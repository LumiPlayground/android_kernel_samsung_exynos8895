/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __EXYNOS_PMU_CP_H
#define __EXYNOS_PMU_CP_H __FILE__

#include "modem_prj.h"


#define SMC_ID		0x82000700
#define READ_CTRL	0x3
#define WRITE_CTRL	0x4

enum cp_mode {
	CP_POWER_ON,
	CP_RESET,
	CP_POWER_OFF,
	NUM_CP_MODE,
};

enum reset_mode {
	CP_HW_RESET,
	CP_SW_RESET,
};

enum cp_control {
	CP_CTRL_S,
	CP_CTRL_NS,
};

#if defined(CONFIG_SOC_EXYNOS8895)
extern int exynos_cp_reset(struct modem_ctl *);
extern int exynos_cp_release(struct modem_ctl *);
extern int exynos_cp_init(struct modem_ctl *);
extern int exynos_cp_active_clear(struct modem_ctl *);
extern int exynos_clear_cp_reset(struct modem_ctl *);
extern int exynos_get_cp_power_status(struct modem_ctl *);
extern int exynos_set_cp_power_onoff(struct modem_ctl *, enum cp_mode mode);
extern void exynos_sys_powerdown_conf_cp(struct modem_ctl *);
extern int exynos_pmu_cp_init(struct modem_ctl *);
#endif

#endif /* __EXYNOS_PMU_CP_H */
