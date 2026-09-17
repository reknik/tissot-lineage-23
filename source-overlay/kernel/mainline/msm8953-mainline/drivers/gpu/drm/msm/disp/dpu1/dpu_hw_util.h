/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DPU_HW_UTIL_H
#define _DPU_HW_UTIL_H

#include <linux/io.h>
#include <linux/math.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "dpu_hw_mdss.h"
#include "dpu_hw_catalog.h"

#define REG_MASK(n)                     ((BIT(n)) - 1)
#define MISR_FRAME_COUNT                0x1
#define MISR_CTRL_ENABLE                BIT(8)
#define MISR_CTRL_STATUS                BIT(9)
#define MISR_CTRL_STATUS_CLEAR          BIT(10)
#define MISR_CTRL_FREE_RUN_MASK         BIT(31)

#define TO_S15D16(_x_)((_x_) << 7)

#define MDP_TICK_COUNT                    16
#define XO_CLK_RATE                       19200
#define MS_TICKS_IN_SEC                   1000

#define CALCULATE_WD_LOAD_VALUE(fps) \
	((uint32_t)((MS_TICKS_IN_SEC * XO_CLK_RATE)/(MDP_TICK_COUNT * fps)))

extern const struct dpu_csc_cfg dpu_csc_YUV2RGB_601L;
extern const struct dpu_csc_cfg dpu_csc10_YUV2RGB_601L;
extern const struct dpu_csc_cfg dpu_csc10_rgb2yuv_601l;

/*
 * This is the common struct maintained by each sub block
 * for mapping the register offsets in this block to the
 * absoulute IO address
 * @blk_addr:     hw block register mapped address
 * @log_mask:     log mask for this block
 */
struct dpu_hw_blk_reg_map {
	void __iomem *blk_addr;
	u32 log_mask;
};

/**
 * struct dpu_hw_blk - opaque hardware block object
 */
struct dpu_hw_blk {
	/* opaque */
};

/**
 * struct dpu_hw_scaler3_de_cfg : QSEEDv3 detail enhancer configuration
 * @enable:         detail enhancer enable/disable
 * @sharpen_level1: sharpening strength for noise
 * @sharpen_level2: sharpening strength for signal
 * @ clip:          clip shift
 * @ limit:         limit value
 * @ thr_quiet:     quiet threshold
 * @ thr_dieout:    dieout threshold
 * @ thr_high:      low threshold
 * @ thr_high:      high threshold
 * @ prec_shift:    precision shift
 * @ adjust_a:      A-coefficients for mapping curve
 * @ adjust_b:      B-coefficients for mapping curve
 * @ adjust_c:      C-coefficients for mapping curve
 */
struct dpu_hw_scaler3_de_cfg {
	u32 enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[DPU_MAX_DE_CURVES];
	int16_t adjust_b[DPU_MAX_DE_CURVES];
	int16_t adjust_c[DPU_MAX_DE_CURVES];
};


/**
 * struct dpu_hw_scaler3_cfg : QSEEDv3 configuration
 * @enable:        scaler enable
 * @dir_en:        direction detection block enable
 * @ init_phase_x: horizontal initial phase
 * @ phase_step_x: horizontal phase step
 * @ init_phase_y: vertical initial phase
 * @ phase_step_y: vertical phase step
 * @ preload_x:    horizontal preload value
 * @ preload_y:    vertical preload value
 * @ src_width:    source width
 * @ src_height:   source height
 * @ dst_width:    destination width
 * @ dst_height:   destination height
 * @ y_rgb_filter_cfg: y/rgb plane filter configuration
 * @ uv_filter_cfg: uv plane filter configuration
 * @ alpha_filter_cfg: alpha filter configuration
 * @ blend_cfg:    blend coefficients configuration
 * @ lut_flag:     scaler LUT update flags
 *                 0x1 swap LUT bank
 *                 0x2 update 2D filter LUT
 *                 0x4 update y circular filter LUT
 *                 0x8 update uv circular filter LUT
 *                 0x10 update y separable filter LUT
 *                 0x20 update uv separable filter LUT
 * @ dir_lut_idx:  2D filter LUT index
 * @ y_rgb_cir_lut_idx: y circular filter LUT index
 * @ uv_cir_lut_idx: uv circular filter LUT index
 * @ y_rgb_sep_lut_idx: y circular filter LUT index
 * @ uv_sep_lut_idx: uv separable filter LUT index
 * @ dir_lut:      pointer to 2D LUT
 * @ cir_lut:      pointer to circular filter LUT
 * @ sep_lut:      pointer to separable filter LUT
 * @ de: detail enhancer configuration
 * @ dir_weight:   Directional weight
 */
struct dpu_hw_scaler3_cfg {
	u32 enable;
	u32 dir_en;
	int32_t init_phase_x[DPU_MAX_PLANES];
	int32_t phase_step_x[DPU_MAX_PLANES];
	int32_t init_phase_y[DPU_MAX_PLANES];
	int32_t phase_step_y[DPU_MAX_PLANES];

	u32 preload_x[DPU_MAX_PLANES];
	u32 preload_y[DPU_MAX_PLANES];
	u32 src_width[DPU_MAX_PLANES];
	u32 src_height[DPU_MAX_PLANES];

	u32 dst_width;
	u32 dst_height;

	u32 y_rgb_filter_cfg;
	u32 uv_filter_cfg;
	u32 alpha_filter_cfg;
	u32 blend_cfg;

	u32 lut_flag;
	u32 dir_lut_idx;

	u32 y_rgb_cir_lut_idx;
	u32 uv_cir_lut_idx;
	u32 y_rgb_sep_lut_idx;
	u32 uv_sep_lut_idx;
	u32 *dir_lut;
	size_t dir_len;
	u32 *cir_lut;
	size_t cir_len;
	u32 *sep_lut;
	size_t sep_len;

	/*
	 * Detail enhancer settings
	 */
	struct dpu_hw_scaler3_de_cfg de;

	u32 dir_weight;
};

/*
 * QSEED2 scaler (DPU 1.x VIG pipes: msm8917, msm8937, msm8953, msm8996).
 *
 * QSEED2 is a different algorithm from QSEED3 and shares neither its
 * register layout nor its bit-field encoding, so it has its own
 * configuration structure, helpers and programming routine instead of
 * being routed through the QSEED3 code.
 *
 * Register offsets, bit layouts and semantics below come from the in-tree
 * mdp5 register description of the same block
 * (drivers/gpu/drm/msm/registers/display/mdp5.xml) and were cross-checked
 * against Qualcomm's downstream mdss_mdp_qseed2_setup() in
 * drivers/video/fbdev/msm/mdss_mdp_pp.c.
 */
#define DPU_QSEED2_PHASE_STEP_SHIFT	21
/* phase step and initial phase are 26-bit (25:0) fixed point registers */
#define DPU_QSEED2_PHASE_MASK		GENMASK(25, 0)
/* maximum reduction the filtered (BIL/PCMN) scaler can do in one step */
#define DPU_QSEED2_MAX_DOWNSCALE	4
#define DPU_QSEED2_MAX_UPSCALE		20
/* SCALE_CONFIG filter selectors */
#define DPU_QSEED2_FILTER_NEAREST	0
#define DPU_QSEED2_FILTER_BIL		1
#define DPU_QSEED2_FILTER_PCMN		2
#define DPU_QSEED2_FILTER_CA		3
/* sharpening block offset, relative to the scaler block base */
#define DPU_QSEED2_SHARP_OFFSET		0x30

/*
 * Component indices of the QSEED2 phase registers.  They intentionally
 * match DPU_SSPP_COMP_* in dpu_hw_sspp.h, which cannot be included here
 * because that header includes this one.
 */
#define DPU_QSEED2_COMP_0		0
#define DPU_QSEED2_COMP_1_2		1
#define DPU_QSEED2_COMP_2		2
#define DPU_QSEED2_COMP_3		3

#define DPU_QSEED2_DECIMATED_DIM(dim, deci) \
	(((dim) + ((1 << (deci)) - 1)) >> (deci))

/**
 * struct dpu_hw_scaler2_cfg : QSEEDv2 configuration
 * @enable:            master scaler enable
 * @scale_x:           program horizontal scaling (SCALEX_EN)
 * @scale_y:           program vertical scaling (SCALEY_EN)
 * @init_phase_x:      horizontal initial phase per component
 * @phase_step_x:      horizontal phase step per component
 * @init_phase_y:      vertical initial phase per component
 * @phase_step_y:      vertical phase step per component
 * @src_width:         post-decimation source width per component
 * @src_height:        post-decimation source height per component
 * @dst_width:         destination width
 * @dst_height:        destination height
 * @horz_filter_y_rgb: horizontal G/Y filter configuration
 * @horz_filter_uv:    horizontal CrCb filter configuration
 * @horz_filter_alpha: horizontal alpha filter configuration
 * @vert_filter_y_rgb: vertical G/Y filter configuration
 * @vert_filter_uv:    vertical CrCb filter configuration
 * @vert_filter_alpha: vertical alpha filter configuration
 * @horz_decimate:     horizontal decimation exponent (0 disables)
 * @vert_decimate:     vertical decimation exponent (0 disables)
 */
struct dpu_hw_scaler2_cfg {
	u32 enable;
	u32 scale_x;
	u32 scale_y;

	int32_t init_phase_x[DPU_MAX_PLANES];
	int32_t phase_step_x[DPU_MAX_PLANES];
	int32_t init_phase_y[DPU_MAX_PLANES];
	int32_t phase_step_y[DPU_MAX_PLANES];

	u32 src_width[DPU_MAX_PLANES];
	u32 src_height[DPU_MAX_PLANES];

	u32 dst_width;
	u32 dst_height;

	u32 horz_filter_y_rgb;
	u32 horz_filter_uv;
	u32 horz_filter_alpha;
	u32 vert_filter_y_rgb;
	u32 vert_filter_uv;
	u32 vert_filter_alpha;

	u32 horz_decimate;
	u32 vert_decimate;
};

/**
 * struct dpu_hw_scaler2_regs : packed QSEED2 scaler register values
 * @config:           SCALE_CONFIG (scaler base + 0x04)
 * @phase_step_c03_x: component 0/3 horizontal phase step (+ 0x10)
 * @phase_step_c03_y: component 0/3 vertical phase step (+ 0x14)
 * @phase_step_c12_x: component 1/2 horizontal phase step (+ 0x18)
 * @phase_step_c12_y: component 1/2 vertical phase step (+ 0x1c)
 * @init_phase_c03_x: component 0/3 horizontal initial phase (+ 0x20)
 * @init_phase_c03_y: component 0/3 vertical initial phase (+ 0x24)
 * @init_phase_c12_x: component 1/2 horizontal initial phase (+ 0x28)
 * @init_phase_c12_y: component 1/2 vertical initial phase (+ 0x2c)
 */
struct dpu_hw_scaler2_regs {
	u32 config;
	u32 phase_step_c03_x;
	u32 phase_step_c03_y;
	u32 phase_step_c12_x;
	u32 phase_step_c12_y;
	u32 init_phase_c03_x;
	u32 init_phase_c03_y;
	u32 init_phase_c12_x;
	u32 init_phase_c12_y;
};

/**
 * dpu_hw_scaler2_calc_phase_step - calculate a QSEED2 phase step
 * @src: source dimension
 * @dst: destination dimension
 * @out: output phase step
 *
 * QSEED2 phase steps are 26-bit fixed point values with 21 fractional
 * bits, which leaves five integer bits: the filtered scaler can reduce
 * the source by at most DPU_QSEED2_MAX_DOWNSCALE in a single step.
 * Larger ratios have to be brought into range by the decimation block
 * before this is called.
 *
 * Returns 0 on success, -EOVERFLOW when the ratio is still beyond the
 * filtered scaler limit and -EINVAL for empty dimensions.
 */
static inline int dpu_hw_scaler2_calc_phase_step(u32 src, u32 dst, u32 *out)
{
	if (!src || !dst)
		return -EINVAL;

	if (src > dst * DPU_QSEED2_MAX_DOWNSCALE)
		return -EOVERFLOW;

	*out = mult_frac(1 << DPU_QSEED2_PHASE_STEP_SHIFT, src, dst);

	return 0;
}

/**
 * dpu_hw_scaler2_calc_decimation - pick the smallest decimation exponent
 * @src: source dimension
 * @dst: destination dimension
 * @max_exp: maximum exponent supported by the platform
 *
 * Returns the smallest power-of-two exponent whose decimated source still
 * satisfies the filtered scaler limit, capped at @max_exp.  Callers must
 * still verify the result with dpu_hw_scaler2_calc_phase_step(): when the
 * cap was reached and the ratio is still out of range the layer is not
 * scaler-assignable.
 */
static inline u32 dpu_hw_scaler2_calc_decimation(u32 src, u32 dst,
						 u32 max_exp)
{
	u32 exp = 0;

	if (!dst)
		return max_exp;

	while (exp < max_exp &&
	       DPU_QSEED2_DECIMATED_DIM(src, exp) >
			dst * DPU_QSEED2_MAX_DOWNSCALE)
		exp++;

	return exp;
}

/**
 * dpu_hw_scaler2_build_cfg - build a QSEED2 configuration from plane geometry
 * @src_w: source width
 * @src_h: source height
 * @dst_w: destination width
 * @dst_h: destination height
 * @hsub:  horizontal chroma subsampling factor (1 for RGB)
 * @vsub:  vertical chroma subsampling factor (1 for RGB)
 * @yuv:   whether the source format is YUV
 * @max_hdeci_exp: platform horizontal decimation exponent cap
 * @max_vdeci_exp: platform vertical decimation exponent cap
 * @cfg:   configuration to fill in
 *
 * Calculates the decimation exponents, 26-bit phase steps (component 0/3
 * from the luma/RGB ratio, component 1/2 from the chroma-subsampled
 * ratio) and the per-axis filters (bilinear for enlargement, PCMN for
 * reduction).  Initial phase is left at zero for ordinary DRM atomic
 * states.
 *
 * Returns 0 on success or a negative errno when the geometry cannot be
 * handled by QSEED2 even with decimation.
 */
static inline int dpu_hw_scaler2_build_cfg(u32 src_w, u32 src_h,
					   u32 dst_w, u32 dst_h,
					   u32 hsub, u32 vsub, bool yuv,
					   u32 max_hdeci_exp,
					   u32 max_vdeci_exp,
					   struct dpu_hw_scaler2_cfg *cfg)
{
	u32 src_w_dec, src_h_dec;
	u32 phase_step_x, phase_step_y;
	u32 chroma_div_x, chroma_div_y;
	int ret;

	memset(cfg, 0, sizeof(*cfg));

	if (!src_w || !src_h || !dst_w || !dst_h)
		return -EINVAL;

	if (!hsub)
		hsub = 1;
	if (!vsub)
		vsub = 1;

	cfg->horz_decimate = dpu_hw_scaler2_calc_decimation(src_w, dst_w,
							    max_hdeci_exp);
	cfg->vert_decimate = dpu_hw_scaler2_calc_decimation(src_h, dst_h,
							    max_vdeci_exp);

	src_w_dec = DPU_QSEED2_DECIMATED_DIM(src_w, cfg->horz_decimate);
	src_h_dec = DPU_QSEED2_DECIMATED_DIM(src_h, cfg->vert_decimate);

	ret = dpu_hw_scaler2_calc_phase_step(src_w_dec, dst_w, &phase_step_x);
	if (ret)
		return ret;

	ret = dpu_hw_scaler2_calc_phase_step(src_h_dec, dst_h, &phase_step_y);
	if (ret)
		return ret;

	/*
	 * Chroma planes are subsampled with respect to luma, so their phase
	 * step is smaller by the subsampling factor.  When the decimation
	 * block is active it drops a chroma sample instead of subsampling
	 * it, in which case the luma phase step applies as-is.
	 */
	chroma_div_x = (yuv && !cfg->horz_decimate) ? hsub : 1;
	chroma_div_y = (yuv && !cfg->vert_decimate) ? vsub : 1;

	cfg->phase_step_x[DPU_QSEED2_COMP_0] = phase_step_x;
	cfg->phase_step_y[DPU_QSEED2_COMP_0] = phase_step_y;
	cfg->phase_step_x[DPU_QSEED2_COMP_3] = phase_step_x;
	cfg->phase_step_y[DPU_QSEED2_COMP_3] = phase_step_y;
	cfg->phase_step_x[DPU_QSEED2_COMP_2] =
		cfg->phase_step_x[DPU_QSEED2_COMP_1_2] =
			phase_step_x / chroma_div_x;
	cfg->phase_step_y[DPU_QSEED2_COMP_2] =
		cfg->phase_step_y[DPU_QSEED2_COMP_1_2] =
			phase_step_y / chroma_div_y;

	cfg->src_width[DPU_QSEED2_COMP_0] = src_w_dec;
	cfg->src_height[DPU_QSEED2_COMP_0] = src_h_dec;
	cfg->src_width[DPU_QSEED2_COMP_3] = src_w_dec;
	cfg->src_height[DPU_QSEED2_COMP_3] = src_h_dec;
	cfg->src_width[DPU_QSEED2_COMP_2] =
		cfg->src_width[DPU_QSEED2_COMP_1_2] = src_w_dec / chroma_div_x;
	cfg->src_height[DPU_QSEED2_COMP_2] =
		cfg->src_height[DPU_QSEED2_COMP_1_2] = src_h_dec / chroma_div_y;

	cfg->dst_width = dst_w;
	cfg->dst_height = dst_h;

	cfg->horz_filter_y_rgb = (src_w_dec <= dst_w) ?
		DPU_QSEED2_FILTER_BIL : DPU_QSEED2_FILTER_PCMN;
	cfg->horz_filter_alpha = cfg->horz_filter_y_rgb;
	cfg->horz_filter_uv = (cfg->src_width[DPU_QSEED2_COMP_1_2] <= dst_w) ?
		DPU_QSEED2_FILTER_BIL : DPU_QSEED2_FILTER_PCMN;

	cfg->vert_filter_y_rgb = (src_h_dec <= dst_h) ?
		DPU_QSEED2_FILTER_BIL : DPU_QSEED2_FILTER_PCMN;
	cfg->vert_filter_alpha = cfg->vert_filter_y_rgb;
	cfg->vert_filter_uv = (cfg->src_height[DPU_QSEED2_COMP_1_2] <= dst_h) ?
		DPU_QSEED2_FILTER_BIL : DPU_QSEED2_FILTER_PCMN;

	/* there is no chroma plane for RGB formats, so leave it unselected */
	if (!yuv) {
		cfg->horz_filter_uv = DPU_QSEED2_FILTER_NEAREST;
		cfg->vert_filter_uv = DPU_QSEED2_FILTER_NEAREST;
	}

	/*
	 * QSEED2 also performs chroma upsampling, so a YUV layer at unity
	 * luma size still needs the scaler when chroma is subsampled.
	 */
	cfg->scale_x = (src_w_dec != dst_w) || (yuv && hsub > 1);
	cfg->scale_y = (src_h_dec != dst_h) || (yuv && vsub > 1);
	cfg->enable = cfg->scale_x || cfg->scale_y;

	return 0;
}

/**
 * dpu_hw_scaler2_pack_regs - pack a QSEED2 configuration into registers
 * @cfg:  configuration to pack
 * @regs: packed register values
 *
 * A disabled scaler packs every register as zero so that turning scaling
 * off is a clean transition.  The SCALE_CONFIG layout used here is the
 * mdp5 one: BIT(0)/BIT(1) enable the
 * horizontal/vertical scaler and the G/Y, CrCb and alpha filter selectors
 * live in bits 8/12/16 (horizontal) and 10/14/18 (vertical).
 */
static inline void dpu_hw_scaler2_pack_regs(const struct dpu_hw_scaler2_cfg *cfg,
					    struct dpu_hw_scaler2_regs *regs)
{
	memset(regs, 0, sizeof(*regs));

	if (!cfg->enable)
		return;

	if (cfg->scale_x) {
		regs->config |= BIT(0);
		regs->config |= (cfg->horz_filter_y_rgb & 0x3) << 8;
		regs->config |= (cfg->horz_filter_uv & 0x3) << 12;
		regs->config |= (cfg->horz_filter_alpha & 0x3) << 16;
	}

	if (cfg->scale_y) {
		regs->config |= BIT(1);
		regs->config |= (cfg->vert_filter_y_rgb & 0x3) << 10;
		regs->config |= (cfg->vert_filter_uv & 0x3) << 14;
		regs->config |= (cfg->vert_filter_alpha & 0x3) << 18;
	}

	regs->phase_step_c03_x =
		cfg->phase_step_x[DPU_QSEED2_COMP_0] & DPU_QSEED2_PHASE_MASK;
	regs->phase_step_c03_y =
		cfg->phase_step_y[DPU_QSEED2_COMP_0] & DPU_QSEED2_PHASE_MASK;
	regs->phase_step_c12_x =
		cfg->phase_step_x[DPU_QSEED2_COMP_1_2] & DPU_QSEED2_PHASE_MASK;
	regs->phase_step_c12_y =
		cfg->phase_step_y[DPU_QSEED2_COMP_1_2] & DPU_QSEED2_PHASE_MASK;

	regs->init_phase_c03_x =
		cfg->init_phase_x[DPU_QSEED2_COMP_0] & DPU_QSEED2_PHASE_MASK;
	regs->init_phase_c03_y =
		cfg->init_phase_y[DPU_QSEED2_COMP_0] & DPU_QSEED2_PHASE_MASK;
	regs->init_phase_c12_x =
		cfg->init_phase_x[DPU_QSEED2_COMP_1_2] & DPU_QSEED2_PHASE_MASK;
	regs->init_phase_c12_y =
		cfg->init_phase_y[DPU_QSEED2_COMP_1_2] & DPU_QSEED2_PHASE_MASK;
}

/**
 * struct dpu_drm_pix_ext_v1 - version 1 of pixel ext structure
 * @num_ext_pxls_lr: Number of total horizontal pixels
 * @num_ext_pxls_tb: Number of total vertical lines
 * @left_ftch:       Number of extra pixels to overfetch from left
 * @right_ftch:      Number of extra pixels to overfetch from right
 * @top_ftch:        Number of extra lines to overfetch from top
 * @btm_ftch:        Number of extra lines to overfetch from bottom
 * @left_rpt:        Number of extra pixels to repeat from left
 * @right_rpt:       Number of extra pixels to repeat from right
 * @top_rpt:         Number of extra lines to repeat from top
 * @btm_rpt:         Number of extra lines to repeat from bottom
 */
struct dpu_drm_pix_ext_v1 {
	/*
	 * Number of pixels ext in left, right, top and bottom direction
	 * for all color components.
	 */
	int32_t num_ext_pxls_lr[DPU_MAX_PLANES];
	int32_t num_ext_pxls_tb[DPU_MAX_PLANES];

	/*
	 * Number of pixels needs to be overfetched in left, right, top
	 * and bottom directions from source image for scaling.
	 */
	int32_t left_ftch[DPU_MAX_PLANES];
	int32_t right_ftch[DPU_MAX_PLANES];
	int32_t top_ftch[DPU_MAX_PLANES];
	int32_t btm_ftch[DPU_MAX_PLANES];
	/*
	 * Number of pixels needs to be repeated in left, right, top and
	 * bottom directions for scaling.
	 */
	int32_t left_rpt[DPU_MAX_PLANES];
	int32_t right_rpt[DPU_MAX_PLANES];
	int32_t top_rpt[DPU_MAX_PLANES];
	int32_t btm_rpt[DPU_MAX_PLANES];

};

/**
 * struct dpu_drm_de_v1 - version 1 of detail enhancer structure
 * @enable:         Enables/disables detail enhancer
 * @sharpen_level1: Sharpening strength for noise
 * @sharpen_level2: Sharpening strength for context
 * @clip:           Clip coefficient
 * @limit:          Detail enhancer limit factor
 * @thr_quiet:      Quite zone threshold
 * @thr_dieout:     Die-out zone threshold
 * @thr_low:        Linear zone left threshold
 * @thr_high:       Linear zone right threshold
 * @prec_shift:     Detail enhancer precision
 * @adjust_a:       Mapping curves A coefficients
 * @adjust_b:       Mapping curves B coefficients
 * @adjust_c:       Mapping curves C coefficients
 */
struct dpu_drm_de_v1 {
	uint32_t enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[DPU_MAX_DE_CURVES];
	int16_t adjust_b[DPU_MAX_DE_CURVES];
	int16_t adjust_c[DPU_MAX_DE_CURVES];
};

/**
 * struct dpu_drm_scaler_v2 - version 2 of struct dpu_drm_scaler
 * @enable:            Scaler enable
 * @dir_en:            Detail enhancer enable
 * @pe:                Pixel extension settings
 * @horz_decimate:     Horizontal decimation factor
 * @vert_decimate:     Vertical decimation factor
 * @init_phase_x:      Initial scaler phase values for x
 * @phase_step_x:      Phase step values for x
 * @init_phase_y:      Initial scaler phase values for y
 * @phase_step_y:      Phase step values for y
 * @preload_x:         Horizontal preload value
 * @preload_y:         Vertical preload value
 * @src_width:         Source width
 * @src_height:        Source height
 * @dst_width:         Destination width
 * @dst_height:        Destination height
 * @y_rgb_filter_cfg:  Y/RGB plane filter configuration
 * @uv_filter_cfg:     UV plane filter configuration
 * @alpha_filter_cfg:  Alpha filter configuration
 * @blend_cfg:         Selection of blend coefficients
 * @lut_flag:          LUT configuration flags
 * @dir_lut_idx:       2d 4x4 LUT index
 * @y_rgb_cir_lut_idx: Y/RGB circular LUT index
 * @uv_cir_lut_idx:    UV circular LUT index
 * @y_rgb_sep_lut_idx: Y/RGB separable LUT index
 * @uv_sep_lut_idx:    UV separable LUT index
 * @de:                Detail enhancer settings
 */
struct dpu_drm_scaler_v2 {
	/*
	 * General definitions
	 */
	uint32_t enable;
	uint32_t dir_en;

	/*
	 * Pix ext settings
	 */
	struct dpu_drm_pix_ext_v1 pe;

	/*
	 * Decimation settings
	 */
	uint32_t horz_decimate;
	uint32_t vert_decimate;

	/*
	 * Phase settings
	 */
	int32_t init_phase_x[DPU_MAX_PLANES];
	int32_t phase_step_x[DPU_MAX_PLANES];
	int32_t init_phase_y[DPU_MAX_PLANES];
	int32_t phase_step_y[DPU_MAX_PLANES];

	uint32_t preload_x[DPU_MAX_PLANES];
	uint32_t preload_y[DPU_MAX_PLANES];
	uint32_t src_width[DPU_MAX_PLANES];
	uint32_t src_height[DPU_MAX_PLANES];

	uint32_t dst_width;
	uint32_t dst_height;

	uint32_t y_rgb_filter_cfg;
	uint32_t uv_filter_cfg;
	uint32_t alpha_filter_cfg;
	uint32_t blend_cfg;

	uint32_t lut_flag;
	uint32_t dir_lut_idx;

	/* for Y(RGB) and UV planes*/
	uint32_t y_rgb_cir_lut_idx;
	uint32_t uv_cir_lut_idx;
	uint32_t y_rgb_sep_lut_idx;
	uint32_t uv_sep_lut_idx;

	/*
	 * Detail enhancer settings
	 */
	struct dpu_drm_de_v1 de;
};

/**
 * struct dpu_hw_qos_cfg: pipe QoS configuration
 * @danger_lut: LUT for generate danger level based on fill level
 * @safe_lut: LUT for generate safe level based on fill level
 * @creq_lut: LUT for generate creq level based on fill level
 * @creq_vblank: creq value generated to vbif during vertical blanking
 * @danger_vblank: danger value generated during vertical blanking
 * @vblank_en: enable creq_vblank and danger_vblank during vblank
 * @danger_safe_en: enable danger safe generation
 */
struct dpu_hw_qos_cfg {
	u32 danger_lut;
	u32 safe_lut;
	u64 creq_lut;
	bool danger_safe_en;
};

u32 *dpu_hw_util_get_log_mask_ptr(void);

void dpu_reg_write(struct dpu_hw_blk_reg_map *c,
		u32 reg_off,
		u32 val,
		const char *name);
int dpu_reg_read(struct dpu_hw_blk_reg_map *c, u32 reg_off);

#define DPU_REG_WRITE(c, off, val) dpu_reg_write(c, off, val, #off)
#define DPU_REG_READ(c, off) dpu_reg_read(c, off)

void *dpu_hw_util_get_dir(void);

void dpu_hw_setup_scaler3(struct dpu_hw_blk_reg_map *c,
		struct dpu_hw_scaler3_cfg *scaler3_cfg,
		u32 scaler_offset, u32 scaler_version,
		const struct msm_format *format);

void dpu_hw_setup_scaler2(struct dpu_hw_blk_reg_map *c,
		const struct dpu_hw_scaler2_cfg *scaler2_cfg,
		u32 scaler_offset);

void dpu_hw_csc_setup(struct dpu_hw_blk_reg_map  *c,
		u32 csc_reg_off,
		const struct dpu_csc_cfg *data, bool csc10);

void dpu_setup_cdp(struct dpu_hw_blk_reg_map *c, u32 offset,
		   const struct msm_format *fmt, bool enable);

u64 _dpu_hw_get_qos_lut(const struct dpu_qos_lut_tbl *tbl,
		u32 total_fl);

void _dpu_hw_setup_qos_lut(struct dpu_hw_blk_reg_map *c, u32 offset,
			   bool qos_8lvl,
			   const struct dpu_hw_qos_cfg *cfg);

void dpu_hw_setup_qos_lut_v13(struct dpu_hw_blk_reg_map *c,
			       const struct dpu_hw_qos_cfg *cfg);

void dpu_hw_setup_misr(struct dpu_hw_blk_reg_map *c,
		u32 misr_ctrl_offset, u8 input_sel);

int dpu_hw_collect_misr(struct dpu_hw_blk_reg_map *c,
		u32 misr_ctrl_offset,
		u32 misr_signature_offset,
		u32 *misr_value);

bool dpu_hw_clk_force_ctrl(struct dpu_hw_blk_reg_map *c,
			   const struct dpu_clk_ctrl_reg *clk_ctrl_reg,
			   bool enable);

#endif /* _DPU_HW_UTIL_H */
