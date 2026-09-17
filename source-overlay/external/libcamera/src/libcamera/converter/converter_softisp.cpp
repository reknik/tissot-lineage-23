/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * converter_softisp.cpp - CPU Bayer -> NV12 converter for raw sensors
 *
 * The mainline Qualcomm CAMSS pipeline captures raw Bayer only (the VFE has no
 * ISP), and the simple pipeline has no memory-to-memory converter on those
 * SoCs.  As a result the pipeline cannot offer the YUV formats the Android
 * camera HAL needs, and a raw camera cannot be exposed to the framework.
 *
 * This converter implements the missing raw -> NV12/NV21 step in software: it
 * unpacks MIPI CSI-2 packed 10-bit Bayer, bilinearly demosaics it to RGB,
 * scales it to the requested output size and converts it to semi-planar YUV
 * (BT.601 limited range).  It has no hardware backing and therefore uses the
 * media-device-less Converter constructor; the simple pipeline instantiates it
 * by factory name when the device has no hardware converter.
 */

#include "libcamera/internal/converter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

#include <libcamera/base/log.h>

#include <libcamera/formats.h>
#include <libcamera/framebuffer.h>
#include <libcamera/geometry.h>
#include <libcamera/pixel_format.h>
#include <libcamera/stream.h>

#include "libcamera/internal/dma_heaps.h"
#include "libcamera/internal/mapped_framebuffer.h"

namespace libcamera {

LOG_DEFINE_CATEGORY(SoftIsp)

namespace {

enum class BayerOrder {
	Grbg,
	Bggr,
	Gbrg,
	Rggb,
};

struct BayerPattern {
	int c00;
	int c01;
	int c10;
	int c11;
};

/* Colour indices: 0 = R, 1 = G, 2 = B. */
constexpr BayerPattern kPatterns[] = {
	{ 1, 0, 2, 1 }, /* Grbg */
	{ 2, 1, 1, 0 }, /* Bggr */
	{ 1, 2, 0, 1 }, /* Gbrg */
	{ 0, 1, 1, 2 }, /* Rggb */
};

inline int colorAt(const BayerPattern &p, int r, int c)
{
	int rr = r & 1;
	int cc = c & 1;

	if (rr == 0)
		return cc == 0 ? p.c00 : p.c01;
	return cc == 0 ? p.c10 : p.c11;
}

inline int to8(uint16_t v)
{
	return v >= 1023 ? 255 : (v * 255 + 511) / 1023;
}

inline int clamp8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/*
 * sRGB display transfer (linear 8-bit -> gamma-encoded 8-bit).  The sensor
 * output is linear light; without this the midtones are ~3x too dark for a
 * display that expects gamma-encoded RGB.
 */
const uint8_t *gammaLut()
{
	static const std::array<uint8_t, 256> lut = [] {
		std::array<uint8_t, 256> t{};
		for (int i = 0; i < 256; i++) {
			double v = i / 255.0;
			double s = v <= 0.0031308 ? v * 12.92
						  : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
			t[i] = static_cast<uint8_t>(std::lround(s * 255.0));
		}
		return t;
	}();
	return lut.data();
}

struct RgbGains {
	double r = 1.0;
	double g = 1.0;
	double b = 1.0;
	bool logged = false;
};

/*
 * White-patch auto white balance: treat the 98th percentile of each channel
 * as the likely-white level and equalise the three, with temporal smoothing
 * so the preview does not pump colour.  Without this the raw Bayer response
 * (two green samples per pixel) leaves a strong green cast that a fully
 * automatic ISP would remove.
 */
void updateWhiteBalance(const std::vector<uint8_t> &rgb, size_t npix, RgbGains &gains)
{
	if (!npix)
		return;

	unsigned int hist[3][256] = {};
	for (size_t i = 0; i < npix; i++) {
		hist[0][rgb[i * 3 + 0]]++;
		hist[1][rgb[i * 3 + 1]]++;
		hist[2][rgb[i * 3 + 2]]++;
	}

	double ref[3];
	/* White patch: the brightest 0.5% of each channel should be neutral. */
	const size_t want = npix * 995 / 1000;
	for (int ch = 0; ch < 3; ch++) {
		size_t acc = 0;
		unsigned int v = 255;
		for (unsigned int i = 0; i < 256; i++) {
			acc += hist[ch][i];
			if (acc >= want) {
				v = i;
				break;
			}
		}
		ref[ch] = std::max(1.0, static_cast<double>(v));
	}

	double avg = (ref[0] + ref[1] + ref[2]) / 3.0;
	double target[3];
	for (int ch = 0; ch < 3; ch++)
		target[ch] = std::clamp(avg / ref[ch], 0.5, 4.0);

	if (!gains.logged) {
		LOG(SoftIsp, Error) << "AWB p99.5 ref=(" << ref[0] << "," << ref[1]
				    << "," << ref[2] << ") target=(" << target[0]
				    << "," << target[1] << "," << target[2] << ")";
		gains.logged = true;
	}

	double *cur[3] = { &gains.r, &gains.g, &gains.b };
	for (int ch = 0; ch < 3; ch++)
		*cur[ch] = *cur[ch] * 0.9 + target[ch] * 0.1;
}

bool bayerOrderFor(PixelFormat format, BayerOrder *order)
{
	if (format == formats::SGRBG10_CSI2P)
		*order = BayerOrder::Grbg;
	else if (format == formats::SBGGR10_CSI2P)
		*order = BayerOrder::Bggr;
	else if (format == formats::SGBRG10_CSI2P)
		*order = BayerOrder::Gbrg;
	else if (format == formats::SRGGB10_CSI2P)
		*order = BayerOrder::Rggb;
	else
		return false;

	return true;
}

/* Unpack MIPI CSI-2 RAW10 rows into 16-bit samples. */
void unpackRow(const uint8_t *src, uint16_t *dst, unsigned int width)
{
	for (unsigned int i = 0, j = 0; j < width; i += 5, j += 4) {
		uint8_t lsb = src[i + 4];
		dst[j + 0] = (src[i + 0] << 2) | ((lsb >> 0) & 0x3);
		dst[j + 1] = (src[i + 1] << 2) | ((lsb >> 2) & 0x3);
		dst[j + 2] = (src[i + 2] << 2) | ((lsb >> 4) & 0x3);
		dst[j + 3] = (src[i + 3] << 2) | ((lsb >> 6) & 0x3);
	}
}

/* Bilinear demosaic of a packed 10-bit Bayer frame into packed RGB888. */
void debayer(const uint8_t *packed, unsigned int stride, unsigned int width,
	     unsigned int height, BayerOrder order, std::vector<uint8_t> &rgb,
	     RgbGains &gains)
{
	const BayerPattern &pattern = kPatterns[static_cast<int>(order)];
	std::vector<uint16_t> bayer(static_cast<size_t>(width) * height);

	for (unsigned int r = 0; r < height; r++)
		unpackRow(packed + static_cast<size_t>(r) * stride,
			  bayer.data() + static_cast<size_t>(r) * width, width);

	rgb.resize(static_cast<size_t>(width) * height * 3);

	const unsigned int workerCount = std::min(4u, height);
	std::vector<std::thread> workers;
	workers.reserve(workerCount);

	auto processRows = [&](unsigned int first, unsigned int last) {
	for (unsigned int r = first; r < last; r++) {
		for (unsigned int c = 0; c < width; c++) {
			uint8_t *out = rgb.data() + (static_cast<size_t>(r) * width + c) * 3;
			int own = colorAt(pattern, r, c);

			for (int target = 0; target < 3; target++) {
				if (target == own) {
					out[target] = to8(bayer[static_cast<size_t>(r) * width + c]);
					continue;
				}

				int sum = 0;
				int count = 0;

				for (int dr = -1; dr <= 1; dr++) {
					for (int dc = -1; dc <= 1; dc++) {
						int rr = static_cast<int>(r) + dr;
						int cc = static_cast<int>(c) + dc;

						if (rr < 0 || rr >= static_cast<int>(height) ||
						    cc < 0 || cc >= static_cast<int>(width))
							continue;
						if (colorAt(pattern, rr, cc) != target)
							continue;

						sum += to8(bayer[static_cast<size_t>(rr) * width + cc]);
						count++;
					}
				}

				out[target] = count ? (sum + count / 2) / count
						    : to8(bayer[static_cast<size_t>(r) * width + c]);
			}
		}
	}
	};

	for (unsigned int worker = 0; worker < workerCount; worker++) {
		unsigned int first = height * worker / workerCount;
		unsigned int last = height * (worker + 1) / workerCount;
		workers.emplace_back(processRows, first, last);
	}

	for (std::thread &worker : workers)
		worker.join();

	updateWhiteBalance(rgb, static_cast<size_t>(width) * height, gains);
}

void sampleBilinear(const std::vector<uint8_t> &rgb, unsigned int sw,
		    unsigned int sh, int fx, int fy, int out[3])
{
	int x = fx >> 16;
	int y = fy >> 16;
	int x1 = std::min(x + 1, static_cast<int>(sw) - 1);
	int y1 = std::min(y + 1, static_cast<int>(sh) - 1);
	int wx = fx & 0xffff;
	int wy = fy & 0xffff;

	const uint8_t *p00 = rgb.data() + (static_cast<size_t>(y) * sw + x) * 3;
	const uint8_t *p01 = rgb.data() + (static_cast<size_t>(y) * sw + x1) * 3;
	const uint8_t *p10 = rgb.data() + (static_cast<size_t>(y1) * sw + x) * 3;
	const uint8_t *p11 = rgb.data() + (static_cast<size_t>(y1) * sw + x1) * 3;

	for (int ch = 0; ch < 3; ch++) {
		int64_t top = static_cast<int64_t>(p00[ch]) * (65536 - wx) +
			      static_cast<int64_t>(p01[ch]) * wx;
		int64_t bot = static_cast<int64_t>(p10[ch]) * (65536 - wx) +
			      static_cast<int64_t>(p11[ch]) * wx;
		out[ch] = static_cast<int>((top * (65536 - wy) + bot * wy) >> 32);
	}
}

/*
 * Bilinear-scale the demosaiced RGB frame into a semi-planar NV12/NV21
 * destination.  Chroma is averaged over each 2x2 output block.
 */
void scaleToNv12(const std::vector<uint8_t> &rgb, const Size &src,
		 uint8_t *yPlane, unsigned int yStride, uint8_t *uvPlane,
		 unsigned int uvStride, const Size &dst, bool nv21,
		 const RgbGains &gains)
{
	const unsigned int sw = src.width;
	const unsigned int sh = src.height;
	const unsigned int dw = dst.width;
	const unsigned int dh = dst.height;

	for (unsigned int dy = 0; dy < dh; dy++) {
		int fy = dh > 1 && sh > 1
			       ? static_cast<int>(static_cast<int64_t>(dy) * (sh - 1) * 65536 / (dh - 1))
			       : 0;

		for (unsigned int dx = 0; dx < dw; dx++) {
			int fx = dw > 1 && sw > 1
				       ? static_cast<int>(static_cast<int64_t>(dx) * (sw - 1) * 65536 / (dw - 1))
				       : 0;
			int pixel[3];

			sampleBilinear(rgb, sw, sh, fx, fy, pixel);

			/* White balance in linear light, then the display transfer. */
			const uint8_t *lut = gammaLut();
			const double gains3[3] = { gains.r, gains.g, gains.b };
			for (int ch = 0; ch < 3; ch++)
				pixel[ch] = lut[clamp8(static_cast<int>(std::lround(pixel[ch] * gains3[ch])))];

			int y = ((66 * pixel[0] + 129 * pixel[1] + 25 * pixel[2] + 128) >> 8) + 16;
			yPlane[static_cast<size_t>(dy) * yStride + dx] = clamp8(y);

			if ((dx & 1) || (dy & 1))
				continue;

			int sum[3] = { pixel[0], pixel[1], pixel[2] };
			int count = 1;

			for (unsigned int oy = 0; oy < 2 && dy + oy < dh; oy++) {
				for (unsigned int ox = 0; ox < 2 && dx + ox < dw; ox++) {
					if (oy == 0 && ox == 0)
						continue;
					int fyy = dh > 1 && sh > 1
						  ? static_cast<int>(static_cast<int64_t>(dy + oy) * (sh - 1) * 65536 / (dh - 1))
						  : 0;
					int fxx = dw > 1 && sw > 1
						  ? static_cast<int>(static_cast<int64_t>(dx + ox) * (sw - 1) * 65536 / (dw - 1))
						  : 0;
					int other[3];

					sampleBilinear(rgb, sw, sh, fxx, fyy, other);
					for (int ch = 0; ch < 3; ch++)
						sum[ch] += other[ch];
					count++;
				}
			}

			int r = (sum[0] + count / 2) / count;
			int g = (sum[1] + count / 2) / count;
			int b = (sum[2] + count / 2) / count;

			int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
			int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

			size_t index = static_cast<size_t>(dy / 2) * uvStride + dx;
			if (nv21) {
				uvPlane[index] = clamp8(v);
				uvPlane[index + 1] = clamp8(u);
			} else {
				uvPlane[index] = clamp8(u);
				uvPlane[index + 1] = clamp8(v);
			}
		}
	}
}

} /* namespace */

class SoftIspConverter : public Converter
{
public:
	SoftIspConverter(MediaDevice *media);
	~SoftIspConverter() override = default;

	int loadConfiguration([[maybe_unused]] const std::string &filename) override
	{
		return 0;
	}

	bool isValid() const override { return valid_; }

	std::vector<PixelFormat> formats(PixelFormat input) override;
	SizeRange sizes(const Size &input) override;
	std::tuple<unsigned int, unsigned int>
	strideAndFrameSize(const PixelFormat &pixelFormat, const Size &size) override;

	int configure(const StreamConfiguration &inputCfg,
		      const std::vector<std::reference_wrapper<StreamConfiguration>> &outputCfgs) override;
	int exportBuffers(unsigned int output, unsigned int count,
			  std::vector<std::unique_ptr<FrameBuffer>> *buffers) override;

	int start() override { return 0; }
	void stop() override {}

	int queueBuffers(FrameBuffer *input,
			 const std::map<unsigned int, FrameBuffer *> &outputs) override;

private:
	int writeOutput(unsigned int index, FrameBuffer *output);

	bool valid_ = false;
	BayerOrder order_ = BayerOrder::Grbg;
	Size inputSize_;
	PixelFormat inputFormat_;
	struct OutputConfig {
		PixelFormat format;
		Size size;
	};
	std::vector<OutputConfig> outputs_;
	std::vector<uint8_t> rgb_;
	RgbGains gains_;
};

SoftIspConverter::SoftIspConverter([[maybe_unused]] MediaDevice *media)
	: Converter("softisp")
{
	valid_ = true;
}

std::vector<PixelFormat> SoftIspConverter::formats(PixelFormat input)
{
	BayerOrder order;

	if (!bayerOrderFor(input, &order))
		return {};

	return { formats::NV12, formats::NV21 };
}

SizeRange SoftIspConverter::sizes(const Size &input)
{
	return SizeRange(Size(16, 16), input);
}

std::tuple<unsigned int, unsigned int>
SoftIspConverter::strideAndFrameSize(const PixelFormat &pixelFormat,
				     const Size &size)
{
	if (pixelFormat != formats::NV12 && pixelFormat != formats::NV21)
		return std::make_tuple(0, 0);

	unsigned int stride = (size.width + 15) & ~15u;

	return std::make_tuple(stride, stride * size.height * 3 / 2);
}

int SoftIspConverter::configure(const StreamConfiguration &inputCfg,
				const std::vector<std::reference_wrapper<StreamConfiguration>> &outputCfgs)
{
	if (!bayerOrderFor(inputCfg.pixelFormat, &order_)) {
		LOG(SoftIsp, Error) << "Unsupported input format " << inputCfg.pixelFormat;
		return -EINVAL;
	}

	if (inputCfg.size.width < 16 || inputCfg.size.height < 16) {
		LOG(SoftIsp, Error) << "Input size too small: " << inputCfg.size;
		return -EINVAL;
	}

	inputSize_ = inputCfg.size;
	inputFormat_ = inputCfg.pixelFormat;
	outputs_.clear();

	for (const auto &cfgRef : outputCfgs) {
		const StreamConfiguration &cfg = cfgRef;

		if (cfg.pixelFormat != formats::NV12 && cfg.pixelFormat != formats::NV21) {
			LOG(SoftIsp, Error) << "Unsupported output format " << cfg.pixelFormat;
			return -EINVAL;
		}

		outputs_.push_back({ cfg.pixelFormat, cfg.size });
	}

	return 0;
}

int SoftIspConverter::exportBuffers(unsigned int output, unsigned int count,
				    std::vector<std::unique_ptr<FrameBuffer>> *buffers)
{
	if (output >= outputs_.size())
		return -EINVAL;

	auto [stride, frameSize] = strideAndFrameSize(outputs_[output].format,
						      outputs_[output].size);
	if (!stride)
		return -EINVAL;

	DmaHeap heap;
	if (!heap.isValid()) {
		LOG(SoftIsp, Error) << "No DMA heap available";
		return -EINVAL;
	}

	return heap.exportBuffers(count, { frameSize }, buffers, DmaHeap::System);
}

int SoftIspConverter::writeOutput(unsigned int index, FrameBuffer *output)
{
	if (index >= outputs_.size())
		return -EINVAL;

	const OutputConfig &cfg = outputs_[index];

	MappedFrameBuffer out(output, MappedFrameBuffer::MapFlag::Write);
	if (!out.isValid()) {
		LOG(SoftIsp, Error) << "Failed to map output buffer";
		return -EINVAL;
	}

	unsigned int yStride = output->planes()[0].stride;
	if (!yStride)
		yStride = cfg.size.width;

	uint8_t *yPlane = out.planes()[0].data();
	uint8_t *uvPlane;
	unsigned int uvStride;

	if (out.planes().size() > 1) {
		uvPlane = out.planes()[1].data();
		uvStride = output->planes()[1].stride;
		if (!uvStride)
			uvStride = yStride;
	} else {
		uvPlane = yPlane + static_cast<size_t>(yStride) * cfg.size.height;
		uvStride = yStride;
	}

	scaleToNv12(rgb_, inputSize_, yPlane, yStride, uvPlane, uvStride,
		    cfg.size, cfg.format == formats::NV21, gains_);

	return 0;
}

int SoftIspConverter::queueBuffers(FrameBuffer *input,
				   const std::map<unsigned int, FrameBuffer *> &outputs)
{
	int ret = 0;

	if (outputs.empty()) {
		LOG(SoftIsp, Error) << "No output buffer";
		return -EINVAL;
	}

	if (!input || !input->planes().size()) {
		LOG(SoftIsp, Error) << "No input buffer";
		return -EINVAL;
	}

	/*
	 * Demosaic the frame once, then produce every requested output from
	 * the shared RGB image.
	 */
	MappedFrameBuffer in(input, MappedFrameBuffer::MapFlag::Read);
	if (!in.isValid()) {
		LOG(SoftIsp, Error) << "Failed to map input buffer";
		return -EINVAL;
	}

	unsigned int inStride = input->planes()[0].stride;
	if (!inStride)
		inStride = inputSize_.width * 10 / 8;

	debayer(in.planes()[0].data(), inStride, inputSize_.width,
		inputSize_.height, order_, rgb_, gains_);

	for (const auto &[index, output] : outputs) {
		if (!output)
			continue;

		ret = writeOutput(index, output);
		if (ret)
			break;
	}

	/*
	 * The input buffer is free again once conversion is done, and each
	 * output buffer is ready to be completed.
	 */
	inputBufferReady.emit(input);

	for (const auto &[index, output] : outputs) {
		if (output)
			outputBufferReady.emit(output);
	}

	return ret;
}

REGISTER_CONVERTER("softisp", SoftIspConverter, {})

} /* namespace libcamera */
