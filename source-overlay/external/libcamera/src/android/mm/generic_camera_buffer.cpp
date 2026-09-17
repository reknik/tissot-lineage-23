/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2021, Google Inc.
 *
 * generic_camera_buffer.cpp - Generic Android frame buffer backend
 */

#include "../camera_buffer.h"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include <libcamera/base/log.h>

#include "libcamera/internal/formats.h"
#include "libcamera/internal/mapped_framebuffer.h"

using namespace libcamera;

LOG_DECLARE_CATEGORY(HAL)

/*
 * Layout of the buffer handle exported by the minigbm based gralloc
 * (cros_gralloc_handle).  The structure is packed and starts at the first
 * element of native_handle_t::data, after the file descriptors.
 */
struct CrosGrallocHandle {
	int32_t fds[5];
	uint32_t strides[4];
	uint32_t offsets[4];
	uint32_t sizes[4];
	uint32_t id;
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t tiling;
	uint64_t format_modifier;
	uint64_t use_flags;
	uint32_t magic;
	uint32_t pixel_stride;
	int32_t droid_format;
	int64_t usage;
	uint32_t num_planes;
	uint64_t reserved_region_size;
	uint64_t total_size;
} __attribute__((packed));

static constexpr uint32_t kCrosGrallocMagic = 0xABCDDCBA;

class CameraBuffer::Private : public Extensible::Private,
			      public MappedBuffer
{
	LIBCAMERA_DECLARE_PUBLIC(CameraBuffer)

public:
	Private(CameraBuffer *cameraBuffer, buffer_handle_t camera3Buffer,
		PixelFormat pixelFormat, const Size &size, int flags);
	~Private();

	unsigned int numPlanes() const;

	Span<uint8_t> plane(unsigned int plane);

	int fd(unsigned int plane) const;
	unsigned int stride(unsigned int plane) const;
	unsigned int offset(unsigned int plane) const;
	unsigned int size(unsigned int plane) const;

	size_t jpegBufferSize(size_t maxJpegBufferSize) const;

private:
	struct PlaneInfo {
		int fd;
		unsigned int stride;
		unsigned int offset;
		unsigned int size;
	};

	void map();

	int fd_;
	int flags_;
	off_t bufferLength_;
	bool mapped_;
	/*
	 * True when each plane is backed by its own dmabuf instead of the
	 * planes being stored contiguously in a single dmabuf.
	 */
	bool perPlaneFds_;
	/*
	 * True when the plane strides, offsets and sizes were read from the
	 * gralloc handle rather than computed from the pixel format.
	 */
	bool layoutFromGralloc_;
	std::vector<PlaneInfo> planeInfo_;
};

CameraBuffer::Private::Private([[maybe_unused]] CameraBuffer *cameraBuffer,
			       buffer_handle_t camera3Buffer,
			       PixelFormat pixelFormat,
			       const Size &size, int flags)
	: fd_(-1), flags_(flags), bufferLength_(-1), mapped_(false),
	  perPlaneFds_(false), layoutFromGralloc_(false)
{
	error_ = 0;

	const auto &info = PixelFormatInfo::info(pixelFormat);
	if (!info.isValid()) {
		error_ = -EINVAL;
		LOG(HAL, Error) << "Invalid pixel format: " << pixelFormat;
		return;
	}

	const unsigned int numPlanes = info.numPlanes();

	/*
	 * The minigbm based gralloc describes the real layout of the buffers it
	 * allocates in its handle: the number of planes, their file
	 * descriptors, strides, offsets and sizes.  Use it when it is available
	 * and consistent with the pixel format, as the format alone cannot
	 * express the padding and alignment the allocator applies.
	 */
	const CrosGrallocHandle *grallocHandle =
		reinterpret_cast<const CrosGrallocHandle *>(camera3Buffer->data);

	bool grallocLayoutValid =
		camera3Buffer->numFds + camera3Buffer->numInts ==
			static_cast<int>(sizeof(CrosGrallocHandle) / sizeof(int32_t)) &&
		grallocHandle->magic == kCrosGrallocMagic &&
		grallocHandle->num_planes == numPlanes && numPlanes <= 4;

	for (unsigned int i = 0; grallocLayoutValid && i < numPlanes; ++i)
		grallocLayoutValid = grallocHandle->fds[i] >= 0 &&
				     grallocHandle->strides[i] != 0 &&
				     grallocHandle->sizes[i] != 0;

	if (grallocLayoutValid) {
		planeInfo_.resize(numPlanes);

		for (unsigned int i = 0; i < numPlanes; ++i) {
			planeInfo_[i].fd = grallocHandle->fds[i];
			planeInfo_[i].stride = grallocHandle->strides[i];
			planeInfo_[i].offset = grallocHandle->offsets[i];
			planeInfo_[i].size = grallocHandle->sizes[i];
		}

		fd_ = planeInfo_[0].fd;
		bufferLength_ = lseek(fd_, 0, SEEK_END);
		if (bufferLength_ < 0) {
			error_ = -errno;
			LOG(HAL, Error) << "Failed to get buffer length";
			return;
		}

		layoutFromGralloc_ = true;

		LOG(HAL, Debug)
			<< "Camera buffer " << size.toString() << "-" << pixelFormat
			<< ": " << numPlanes << " planes from the gralloc handle"
			<< " (drm format 0x" << std::hex << grallocHandle->format
			<< std::dec << ", buffer length " << bufferLength_ << ")";

		for (unsigned int i = 0; i < numPlanes; ++i)
			LOG(HAL, Debug) << "  plane " << i
					<< ": fd=" << planeInfo_[i].fd
					<< " stride=" << planeInfo_[i].stride
					<< " offset=" << planeInfo_[i].offset
					<< " size=" << planeInfo_[i].size;

		return;
	}

	/*
	 * Android offers no API to query the memory layout of a camera3
	 * buffer, so the layout is deduced from the file descriptors stored in
	 * the buffer handle.  Two layouts are supported:
	 *
	 * - a single dmabuf backing all the planes, which are stored
	 *   contiguously.  This is the historical layout of the Android
	 *   gralloc implementations.
	 * - one dmabuf per plane.
	 */
	std::vector<int> fds;
	for (int i = 0; i < camera3Buffer->numFds; i++) {
		const int fd = camera3Buffer->data[i];
		if (fd < 0)
			continue;
		if (std::find(fds.begin(), fds.end(), fd) == fds.end())
			fds.push_back(fd);
	}

	if (fds.empty()) {
		error_ = -EINVAL;
		LOG(HAL, Error) << "No valid file descriptor"
				<< " (numFds=" << camera3Buffer->numFds
				<< " numInts=" << camera3Buffer->numInts << ")";
		return;
	}

	perPlaneFds_ = numPlanes > 1 && fds.size() >= numPlanes;
	fd_ = fds[0];

	bufferLength_ = lseek(fd_, 0, SEEK_END);
	if (bufferLength_ < 0) {
		error_ = -errno;
		LOG(HAL, Error) << "Failed to get buffer length";
		return;
	}

	planeInfo_.resize(numPlanes);

	unsigned int offset = 0;
	for (unsigned int i = 0; i < numPlanes; ++i) {
		const int planeFd = perPlaneFds_ ? fds[i] : fd_;
		off_t planeLength = bufferLength_;

		if (perPlaneFds_ && planeFd != fd_) {
			planeLength = lseek(planeFd, 0, SEEK_END);
			if (planeLength < 0) {
				error_ = -errno;
				LOG(HAL, Error) << "Failed to get the length of plane " << i;
				return;
			}
		}

		/*
		 * When every plane has its own dmabuf, it starts at offset 0
		 * of that dmabuf.  Otherwise the planes are addressed through
		 * cumulative offsets of the single dmabuf.
		 */
		const unsigned int planeOffset = perPlaneFds_ ? 0 : offset;
		const unsigned int formatPlaneSize = info.planeSize(size, i);
		unsigned int planeSize = formatPlaneSize;

		if (static_cast<off_t>(planeOffset + planeSize) > planeLength) {
			LOG(HAL, Warning)
				<< "Plane " << i << " is larger than the buffer:"
				<< " plane offset=" << planeOffset
				<< ", plane size=" << planeSize
				<< ", buffer length=" << planeLength;
			if (static_cast<off_t>(planeOffset) >= planeLength) {
				error_ = -EINVAL;
				return;
			}
			planeSize = planeLength - planeOffset;
		}

		planeInfo_[i].fd = planeFd;
		planeInfo_[i].stride = info.stride(size.width, i, 1u);
		planeInfo_[i].offset = planeOffset;
		planeInfo_[i].size = planeSize;

		offset += formatPlaneSize;
	}

	LOG(HAL, Debug) << "Camera buffer " << size.toString() << "-" << pixelFormat
			<< ": numFds=" << camera3Buffer->numFds
			<< " numInts=" << camera3Buffer->numInts
			<< " distinct fds=" << fds.size()
			<< " planes=" << numPlanes
			<< (perPlaneFds_ ? " (one dmabuf per plane)"
					 : " (single contiguous dmabuf)");

	for (unsigned int i = 0; i < numPlanes; ++i)
		LOG(HAL, Debug) << "  plane " << i
				<< ": fd=" << planeInfo_[i].fd
				<< " stride=" << planeInfo_[i].stride
				<< " offset=" << planeInfo_[i].offset
				<< " size=" << planeInfo_[i].size;
}

CameraBuffer::Private::~Private()
{
}

unsigned int CameraBuffer::Private::numPlanes() const
{
	return planeInfo_.size();
}

int CameraBuffer::Private::fd(unsigned int plane) const
{
	if (plane >= planeInfo_.size())
		return -1;

	return planeInfo_[plane].fd;
}

Span<uint8_t> CameraBuffer::Private::plane(unsigned int plane)
{
	if (!mapped_)
		map();
	if (!mapped_)
		return {};

	return planes_[plane];
}

unsigned int CameraBuffer::Private::stride(unsigned int plane) const
{
	if (plane >= planeInfo_.size())
		return 0;

	return planeInfo_[plane].stride;
}

unsigned int CameraBuffer::Private::offset(unsigned int plane) const
{
	if (plane >= planeInfo_.size())
		return 0;

	return planeInfo_[plane].offset;
}

unsigned int CameraBuffer::Private::size(unsigned int plane) const
{
	if (plane >= planeInfo_.size())
		return 0;

	return planeInfo_[plane].size;
}

size_t CameraBuffer::Private::jpegBufferSize(size_t maxJpegBufferSize) const
{
	ASSERT(bufferLength_ >= 0);

	return std::min<unsigned int>(bufferLength_, maxJpegBufferSize);
}

void CameraBuffer::Private::map()
{
	ASSERT(fd_ != -1);
	ASSERT(bufferLength_ >= 0);

	/*
	 * The planes can be mapped as a whole when they live in the buffer
	 * described by the first file descriptor, either because a single
	 * dmabuf backs all of them or because the gralloc handle reported
	 * their offsets within that buffer.  Buffers made of one unrelated
	 * dmabuf per plane are consumed through the per-plane file descriptors
	 * and are not expected to be mapped here.
	 */
	if (perPlaneFds_ && !layoutFromGralloc_) {
		LOG(HAL, Error) << "Cannot map a buffer with one dmabuf per plane";
		return;
	}

	void *address = mmap(nullptr, bufferLength_, flags_, MAP_SHARED, fd_, 0);
	if (address == MAP_FAILED) {
		error_ = -errno;
		LOG(HAL, Error) << "Failed to mmap plane";
		return;
	}
	maps_.emplace_back(static_cast<uint8_t *>(address), bufferLength_);

	planes_.reserve(planeInfo_.size());
	for (const auto &info : planeInfo_) {
		planes_.emplace_back(
			static_cast<uint8_t *>(address) + info.offset, info.size);
	}

	mapped_ = true;
}

PUBLIC_CAMERA_BUFFER_IMPLEMENTATION
