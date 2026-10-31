/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_BTS_H_
#define __EXYNOS_BTS_H_

#if defined(CONFIG_EXYNOS8895_BTS)
#define BUS_WIDTH		16
#define DISP_UTIL		75

enum bts_scen_type {
	BS_DEFAULT,
	BS_MIF_CHANGE,
	BS_MFC_UHD,
	BS_MFC_UHD_10BIT,
	BS_G3D_PEFORMANCE,
	BS_DP_DEFAULT,
	BS_CAMERA_DEFAULT,
	BS_MAX,
};

enum bts_bw_type {
	/* RT */
	BTS_BW_DECON0,
	BTS_BW_DECON1,
	BTS_BW_DECON2,
	BTS_BW_CAMERA,
	BTS_BW_AUDIO,
	BTS_BW_CP,
	/* non-RT */
	BTS_BW_G2D,
	BTS_BW_MFC,
	BTS_BW_MCSL,
#ifndef CONFIG_SOC_EMULATOR8895
	BTS_BW_IVA,
	BTS_BW_DSP,
#endif
	BTS_BW_MAX,
	BTS_BW_RT = BTS_BW_G2D,
};

enum bts_dpp_type {
	BTS_DPP0,
	BTS_DPP1,
	BTS_DPP2,
	BTS_DPP3,
	BTS_DPP4,
	BTS_DPP5,
	BTS_DPP_MAX,
};

enum bts_dpu_type {
	BTS_DPU0,
	BTS_DPU1,
	BTS_DPU2,
	BTS_DPU_MAX,
};

struct bts_layer_position {
	unsigned int x1;
	/* x2 = x1 + width */
	unsigned int x2;
	unsigned int y1;
	/* y2 = y1 + height */
	unsigned int y2;
};

struct bts_dpp_info {
	bool used;
	unsigned int bpp;
	unsigned int src_h;
	unsigned int src_w;
	struct bts_layer_position dst;
	unsigned int bw;
};

struct bts_decon_info {
	struct bts_dpp_info dpp[BTS_DPP_MAX];
	/* Khz */
	unsigned int vclk;
	unsigned int lcd_w;
	unsigned int lcd_h;
};

struct bts_bw {
	unsigned int peak;
	unsigned int read;
	unsigned int write;
};

void bts_update_scen(enum bts_scen_type type, unsigned int val);
/* bandwidth (KB/s) */
void bts_update_bw(enum bts_bw_type type, struct bts_bw bw);
unsigned int bts_calc_bw(enum bts_bw_type type, void *data);
#else
#define bts_update_scen(a, b) do {} while(0)
#define bts_update_bw(a, b) do {} while(0)
#define bts_calc_bw(a, b) do {} while(0)
#endif

#if defined(CONFIG_EXYNOS5422_BTS) || defined(CONFIG_EXYNOS5433_BTS)	 || defined(CONFIG_EXYNOS7420_BTS) || defined(CONFIG_EXYNOS7890_BTS)
enum bts_scen_type {
	TYPE_MFC_UD_ENCODING = 0,
	TYPE_MFC_UD_DECODING,
	TYPE_LAYERS,
	TYPE_G3D_FREQ,
	TYPE_G3D_SCENARIO,
	TYPE_ROTATION,
	TYPE_HIGHPERF,
	TYPE_URGENT_OFF,
};

void bts_scen_update(enum bts_scen_type type, unsigned int val);
#else
#define bts_scen_update(a, b) do {} while(0)
#endif


#if   defined(CONFIG_EXYNOS7420_BTS) || defined(CONFIG_EXYNOS7890_BTS)
#define DECON_NOCNT		10
#define VPP_ROT			4
#define CAM_FACTOR		3
#define MIF_UTIL		50
#define MIF_UTIL2		60
#define MIF_UPPER_UTIL		44
#define MIF_DECODING		828000
#define MIF_ENCODING		1068000
#define INT_UTIL		65
#define NO_CNT_TH		100
#define BUS_WIDTH		16
#define SIZE_FACTOR(a)		a = ((a) * 16 / 10)
#define FULLHD_SRC		1920 * 1080
#else
#define FULLHD_SRC		1920 * 1080
#endif

enum bts_media_type {
	TYPE_DECON_INT,
	TYPE_DECON_EXT,
	TYPE_VPP0,
	TYPE_VPP1,
	TYPE_VPP2,
	TYPE_VPP3,
	TYPE_VPP4,
	TYPE_VPP5,
	TYPE_VPP6,
	TYPE_VPP7,
	TYPE_VPP8,
	TYPE_CAM,
	TYPE_YUV,
	TYPE_UD_ENC,
	TYPE_UD_DEC,
	TYPE_SPDMA,
};

enum vpp_bw_type {
	BW_DEFAULT,
	BW_ROT,
	BW_FULLHD_ROT,
};

#define exynos_update_media_scenario(a, b, c) do {} while (0)
#define bts_update_gpu_mif(a) do {} while (0)


#define exynos_bts_scitoken_setting(a) do {} while (0)

#if defined(CONFIG_EXYNOS7420_BTS) || defined(CONFIG_EXYNOS7890_BTS)
void exynos7_update_media_scenario(enum bts_media_type media_type,
		unsigned int bw, int bw_type);
int exynos7_update_bts_param(int target_idx, int work);
int exynos7_bts_register_notifier(struct notifier_block *nb);
int exynos7_bts_unregister_notifier(struct notifier_block *nb);
#else
#define exynos7_update_media_scenario(a, b, c) do {} while (0)
#define exynos7_update_bts_param(a, b) do {} while (0)
#define exynos7_bts_register_notifier(a) do {} while (0)
#define exynos7_bts_unregister_notifier(a) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)	 || defined(CONFIG_EXYNOS5433_BTS)|| defined(CONFIG_EXYNOS7420_BTS)  || defined(CONFIG_EXYNOS7890_BTS)
void bts_initialize(const char *pd_name, bool on);
#else
#define bts_initialize(a, b) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS7420_BTS) || defined(CONFIG_EXYNOS7890_BTS)
void exynos7_bts_show_mo_status(void);
#else
#define exynos7_bts_show_mo_status() do {} while (0)
#endif

#if defined(CONFIG_EXYNOS5430_BTS)
void exynos5_bts_show_mo_status(void);
#else
#define exynos5_bts_show_mo_status() do { } while (0)
#endif

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5433_BTS)
void bts_otf_initialize(unsigned int id, bool on);
#else
#define bts_otf_initialize(a, b) do {} while (0)
#endif

/* Keep the original Exynos8895 BTS ID usable by the unchanged Mali driver. */
#if defined(CONFIG_SOC_EXYNOS8895)
#define BS_G3D_PERFORMANCE BS_G3D_PEFORMANCE
#endif

#endif
