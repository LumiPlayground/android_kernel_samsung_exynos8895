/*
<<<<<<<< HEAD:drivers/misc/modem_5g/modem_link_device_spi_boot.h
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
========
 * Copyright (c) 2015, Linaro Limited
 * Copyright (c) 2016, EPAM Systems
>>>>>>>> ACK/deprecated/android-4.4-p:drivers/tee/optee/shm_pool.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

<<<<<<<< HEAD:drivers/misc/modem_5g/modem_link_device_spi_boot.h
#ifndef __MODEM_LINK_DEVICE_SPI_BOOT_H__
#define __MODEM_LINK_DEVICE_SPI_BOOT_H__

struct spi_boot_link_device {
	struct link_device ld;
	struct spi_device *spi;
	struct sk_buff_head tx_q;
	unsigned int gpio_cp_status;
};
========
#ifndef SHM_POOL_H
#define SHM_POOL_H

#include <linux/tee_drv.h>

struct tee_shm_pool_mgr *optee_shm_pool_alloc_pages(void);
>>>>>>>> ACK/deprecated/android-4.4-p:drivers/tee/optee/shm_pool.h

#endif
