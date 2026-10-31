/*
 * LED driver - leds-ktd2692.c
 *
 * Copyright (C) 2013 Sunggeun Yim <sunggeun.yim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pwm.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

extern struct class *camera_class; /*sys/class/camera*/
struct device *ktd2692_dev;

struct ktd2692_platform_data *global_ktd2692data = NULL;
struct device *global_dev;

/* Macro for getting offset of flash timeout */
#define GET_TIMEOUT_OFFSET(timeout, step)	((timeout) / (step))

/* Base register address */
#define KTD2692_REG_LVP_BASE			0x00
#define KTD2692_REG_FLASH_TIMEOUT_BASE		0x20
#define KTD2692_REG_MM_MIN_CURR_THRESHOLD_BASE	0x40
#define KTD2692_REG_MOVIE_CURRENT_BASE		0x60
#define KTD2692_REG_FLASH_CURRENT_BASE		0x80
#define KTD2692_REG_MODE_BASE			0xA0

/* Set bit coding time for expresswire interface */
#define KTD2692_TIME_RESET_US			700
#define KTD2692_TIME_DATA_START_TIME_US		10
#define KTD2692_TIME_HIGH_END_OF_DATA_US	350
#define KTD2692_TIME_LOW_END_OF_DATA_US		10
#define KTD2692_TIME_SHORT_BITSET_US		4
#define KTD2692_TIME_LONG_BITSET_US		12

/* KTD2692 default length of name */
#define KTD2692_NAME_LENGTH			20

enum ktd2692_bitset {
	KTD2692_LOW = 0,
	KTD2692_HIGH,
};

/* Movie / Flash Mode Control */
enum ktd2692_led_mode {
	KTD2692_MODE_DISABLE = 0,	/* default */
	KTD2692_MODE_MOVIE,
	KTD2692_MODE_FLASH,
};

struct ktd2692_led_config_data {
	/* maximum LED current in movie mode */
	u32 movie_max_microamp;
	/* maximum LED current in flash mode */
	u32 flash_max_microamp;
	/* maximum flash timeout */
	u32 flash_max_timeout;
	/* max LED brightness level */
	enum led_brightness max_brightness;
};

struct ktd2692_context {
	/* Related LED Flash class device */
	struct led_classdev_flash fled_cdev;

	/* secures access to the device */
	struct mutex lock;
	struct regulator *regulator;

	struct gpio_desc *aux_gpio;
	struct gpio_desc *ctrl_gpio;

	enum ktd2692_led_mode mode;
	enum led_brightness torch_brightness;
};

static struct ktd2692_context *fled_cdev_to_led(
				struct led_classdev_flash *fled_cdev)
{
	if (onoff) {
		__gpio_set_value(global_ktd2692data->flash_control, 1);
	} else {
		__gpio_set_value(global_ktd2692data->flash_control, 0);
	}
}

void ktd2692_set_low_bit(void)
{
	__gpio_set_value(global_ktd2692data->flash_control, 0);
	ndelay(T_L_LB*1000);	/* 12ms */
	__gpio_set_value(global_ktd2692data->flash_control, 1);
	ndelay(T_H_LB*1000);	/* 4ms */
}

void ktd2692_set_high_bit(void)
{
	__gpio_set_value(global_ktd2692data->flash_control, 0);
	ndelay(T_L_HB*1000);	/* 4ms */
	__gpio_set_value(global_ktd2692data->flash_control, 1);
	ndelay(T_H_HB*1000);	/* 12ms */
}

static int ktd2692_set_bit(unsigned int bit)
{
	if (bit) {
		ktd2692_set_high_bit();
	} else {
		ktd2692_set_low_bit();
	}
}

static void ktd2692_expresswire_write(struct ktd2692_context *led, u8 value)
{
	int i;

	ktd2692_expresswire_start(led);
	for (i = 7; i >= 0; i--)
		ktd2692_expresswire_set_bit(led, value & BIT(i));
	ktd2692_expresswire_end(led);
}

static int ktd2692_led_brightness_set(struct led_classdev *led_cdev,
				       enum led_brightness brightness)
{
	struct led_classdev_flash *fled_cdev = lcdev_to_flcdev(led_cdev);
	struct ktd2692_context *led = fled_cdev_to_led(fled_cdev);

	mutex_lock(&led->lock);

	if (brightness == LED_OFF) {
		led->mode = KTD2692_MODE_DISABLE;
		gpiod_direction_output(led->aux_gpio, KTD2692_LOW);
	} else {
		ktd2692_expresswire_write(led, brightness |
					KTD2692_REG_MOVIE_CURRENT_BASE);
		led->mode = KTD2692_MODE_MOVIE;
	}

	ktd2692_expresswire_write(led, led->mode | KTD2692_REG_MODE_BASE);
	mutex_unlock(&led->lock);

	return 0;
}

static int ktd2692_write_data(unsigned data)
{
	int err = 0;
	unsigned int bit = 0;

	/* Data Start Condition */
	__gpio_set_value(global_ktd2692data->flash_control, 1);
	ndelay(T_SOD*1000); //15us

	/* BIT 7*/
	bit = ((data>> 7) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 6 */
	bit = ((data>> 6) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 5*/
	bit = ((data>> 5) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 4 */
	bit = ((data>> 4) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 3*/
	bit = ((data>> 3) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 2 */
	bit = ((data>> 2) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 1*/
	bit = ((data>> 1) & 0x01);
	ktd2692_set_bit(bit);

	/* BIT 0 */
	bit = ((data>> 0) & 0x01);
	ktd2692_set_bit(bit);

	 __gpio_set_value(global_ktd2692data->flash_control, 0);
	ndelay(T_EOD_L*1000); //4us

	/* Data End Condition */
	__gpio_set_value(global_ktd2692data->flash_control, 1);
	udelay(T_EOD_H);

	return err;
}

ssize_t ktd2692_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int value = 0;
	int brightness_value = 0;
	int ret = 0;
	unsigned long flags = 0;
	int torch_intensity = -1;

	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if (global_ktd2692data == NULL) {
		LED_ERROR("KTD2692(%s) global_ktd2692data is not initialized.\n", __func__);
		return -EFAULT;
	}

	global_ktd2692data->sysfs_input_data = value;

	if (value <= 0) {
		ret = gpio_request(global_ktd2692data->flash_control, "ktd2692_led_control");
		if (ret) {
			LED_ERROR("Failed to requeset ktd2692_led_control\n");
		} else {
			LED_INFO("KTD2692-TORCH OFF. : E(%d)\n", value);

			global_ktd2692data->mode_status = KTD2692_DISABLES_MOVIE_FLASH_MODE;
			spin_lock_irqsave(&global_ktd2692data->int_lock, flags);
			ktd2692_write_data(global_ktd2692data->mode_status|
								KTD2692_ADDR_MOVIE_FLASHMODE_CONTROL);
			spin_unlock_irqrestore(&global_ktd2692data->int_lock, flags);

			ktd2692_setGpio(0);
			gpio_free(global_ktd2692data->flash_control);
			global_ktd2692data->is_torch_enable = false;
			LED_INFO("KTD2692-TORCH OFF. : X(%d)\n", value);
		}

	} else {
		ret = gpio_request(global_ktd2692data->flash_control, "ktd2692_led_control");
		if (ret) {
			LED_ERROR("Failed to requeset ktd2692_led_control\n");
		} else {
			LED_INFO("KTD2692-TORCH ON. : E(%d)\n", value);

			global_ktd2692data->mode_status = KTD2692_ENABLE_MOVIE_MODE;
			global_ktd2692data->is_torch_enable = true;
			spin_lock_irqsave(&global_ktd2692data->int_lock, flags);
			ktd2692_write_data(global_ktd2692data->LVP_Voltage|
								KTD2692_ADDR_LVP_SETTING);
#if 0	/* use the internel defualt setting */
			ktd2692_write_data(global_ktd2692data->flash_timeout|
								KTD2692_ADDR_FLASH_TIMEOUT_SETTING);
#endif
			if (value == 100) {
				ktd2692_write_data(global_ktd2692data->factory_current_value|
									KTD2692_ADDR_MOVIE_CURRENT_SETTING);
			} else if (1001 <= value && value <= 1010) {
				brightness_value = value - 1001;
				if (global_ktd2692data->torch_table[brightness_value] != 0) {
					torch_intensity = KTD2692_CAL_MOVIE_CURRENT(KTD2692_TORCH_STEP_LEVEL_CURRENT(global_ktd2692data->torch_table[brightness_value], KTD2692_MAX_CURRENT),
						KTD2692_MAX_CURRENT);
				}
				if (torch_intensity < 0) {
					LED_INFO("KTD2692-force to set as default : %d\n", global_ktd2692data->torch_current_value);
					torch_intensity = global_ktd2692data->torch_current_value;
				}
				ktd2692_write_data(torch_intensity|
									KTD2692_ADDR_MOVIE_CURRENT_SETTING);
			} else {
				ktd2692_write_data(global_ktd2692data->torch_current_value|
									KTD2692_ADDR_MOVIE_CURRENT_SETTING);
			}
			ktd2692_write_data(global_ktd2692data->mode_status|
								KTD2692_ADDR_MOVIE_FLASHMODE_CONTROL);
			spin_unlock_irqrestore(&global_ktd2692data->int_lock, flags);

			gpio_free(global_ktd2692data->flash_control);
			LED_INFO("KTD2692-TORCH ON. : X(%d)\n", value);
		}
	}

	return count;
}

EXPORT_SYMBOL(ktd2692_led_mode_ctrl);

int32_t ktd2692_led_mode_ctrl(int state)
{
	int ret = 0;
	unsigned long flags = 0;

static void ktd2692_init_movie_current_max(struct ktd2692_led_config_data *cfg)
{
	u32 offset, step;
	u32 movie_current_microamp;

	offset = KTD2692_MOVIE_MODE_CURRENT_LEVELS;
	step = KTD2692_MM_TO_FL_RATIO(cfg->flash_max_microamp)
		/ KTD2692_MOVIE_MODE_CURRENT_LEVELS;

	do {
		movie_current_microamp = step * offset;
		offset--;
	} while ((movie_current_microamp > cfg->movie_max_microamp) &&
		(offset > 0));

	cfg->max_brightness = offset;
}

static void ktd2692_init_flash_timeout(struct led_classdev_flash *fled_cdev,
				       struct ktd2692_led_config_data *cfg)
{
	struct led_flash_setting *setting;

	setting = &fled_cdev->timeout;
	setting->min = KTD2692_FLASH_MODE_TIMEOUT_DISABLE;
	setting->max = cfg->flash_max_timeout;
	setting->step = cfg->flash_max_timeout
			/ (KTD2692_FLASH_MODE_TIMEOUT_LEVELS - 1);
	setting->val = cfg->flash_max_timeout;
}

static void ktd2692_setup(struct ktd2692_context *led)
{
	led->mode = KTD2692_MODE_DISABLE;
	ktd2692_expresswire_reset(led);
	gpiod_direction_output(led->aux_gpio, KTD2692_LOW);

	ktd2692_expresswire_write(led, (KTD2962_MM_MIN_CURR_THRESHOLD_SCALE - 1)
				 | KTD2692_REG_MM_MIN_CURR_THRESHOLD_BASE);
	ktd2692_expresswire_write(led, KTD2692_FLASH_MODE_CURR_PERCENT(45)
				 | KTD2692_REG_FLASH_CURRENT_BASE);
}

static int ktd2692_parse_dt(struct ktd2692_context *led, struct device *dev,
			    struct ktd2692_led_config_data *cfg)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_node;
	int ret;

	if (!dev->of_node)
		return -ENXIO;

	led->ctrl_gpio = devm_gpiod_get(dev, "ctrl", GPIOD_ASIS);
	ret = PTR_ERR_OR_ZERO(led->ctrl_gpio);
	if (ret) {
		dev_err(dev, "cannot get ctrl-gpios %d\n", ret);
		return ret;
	}

	led->aux_gpio = devm_gpiod_get(dev, "aux", GPIOD_ASIS);
	ret = PTR_ERR_OR_ZERO(led->aux_gpio);
	if (ret) {
		dev_err(dev, "cannot get aux-gpios %d\n", ret);
		return ret;
	}

	led->regulator = devm_regulator_get(dev, "vin");
	if (IS_ERR(led->regulator))
		led->regulator = NULL;

	if (led->regulator) {
		ret = regulator_enable(led->regulator);
		if (ret)
			dev_err(dev, "Failed to enable supply: %d\n", ret);
	}

	child_node = of_get_next_available_child(np, NULL);
	if (!child_node) {
		dev_err(dev, "No DT child node found for connected LED.\n");
		return -EINVAL;
	}

	led->fled_cdev.led_cdev.name =
		of_get_property(child_node, "label", NULL) ? : child_node->name;

	ret = of_property_read_u32(child_node, "led-max-microamp",
				   &cfg->movie_max_microamp);
	if (ret) {
		dev_err(dev, "failed to parse led-max-microamp\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(child_node, "flash-max-microamp",
				   &cfg->flash_max_microamp);
	if (ret) {
		dev_err(dev, "failed to parse flash-max-microamp\n");
		goto err_parse_dt;
	}

	ret = of_property_read_u32(child_node, "flash-max-timeout-us",
				   &cfg->flash_max_timeout);
	if (ret) {
		dev_err(dev, "failed to parse flash-max-timeout-us\n");
		goto err_parse_dt;
	}

err_parse_dt:
	of_node_put(child_node);
	return ret;
}

ssize_t ktd2692_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", global_ktd2692data->sysfs_input_data);
}

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	ktd2692_show, ktd2692_store);
static DEVICE_ATTR(rear_torch_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	ktd2692_show, ktd2692_store);

static int ktd2692_parse_dt(struct device *dev,
                                struct ktd2692_platform_data *pdata)
{
	struct device_node *dnode = dev->of_node;
	u32 buffer = 0;
	int ret = 0;
	u32 torch_table_enable = 0;

	/* Defulat Value */
	pdata->LVP_Voltage = KTD2692_DISABLE_LVP;
	pdata->flash_timeout = KTD2692_TIMER_1049ms;	/* default */
	pdata->min_current_value = KTD2692_MIN_CURRENT_240mA;
	pdata->flash_current_value = KTD2692_FLASH_CURRENT(KTD2692_FLASH_DEFAULT_CURRENT, KTD2692_MAX_CURRENT);
	pdata->movie_current_value = KTD2692_MOVIE_CURRENT(KTD2692_MOVIE_DEFAULT_CURRENT, KTD2692_MAX_CURRENT);
	pdata->factory_current_value = KTD2692_MOVIE_CURRENT(KTD2692_FACTORY_DEFAULT_CURRENT, KTD2692_MAX_CURRENT);
	pdata->torch_current_value = KTD2692_MOVIE_CURRENT(KTD2692_TORCH_DEFAULT_CURRENT, KTD2692_MAX_CURRENT);
	pdata->mode_status = KTD2692_DISABLES_MOVIE_FLASH_MODE;

	/* get gpio */
	pdata->flash_control = of_get_named_gpio(dnode, "flash_control", 0);
	if (!gpio_is_valid(pdata->flash_control)) {
		dev_err(dev, "failed to get flash_control\n");
		return -1;
	} else {
		gpio_request_one(pdata->flash_control, GPIOF_OUT_INIT_LOW, "FLASH_CONTROL_GPIO_INIT_LOW");
		gpio_free(pdata->flash_control);
    }

	/* get flash current value */
	if (of_property_read_u32(dnode, "flash_current", &buffer) == 0) {
		dev_info(dev, "flash_current = <%d><%d>\n",
			buffer, KTD2692_FLASH_CURRENT(buffer, KTD2692_MAX_CURRENT));
		pdata->flash_current_value = KTD2692_FLASH_CURRENT(buffer, KTD2692_MAX_CURRENT);
	}

	/* get movie current value */
	if (of_property_read_u32(dnode, "movie_current", &buffer) == 0) {
		dev_info(dev, "movie_current = <%d><%d>\n",
			buffer, KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT));
		pdata->movie_current_value = KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT);
	}

	/* get factory current value */
	if (of_property_read_u32(dnode, "factory_current", &buffer) == 0) {
		dev_info(dev, "factory_current = <%d><%d>\n",
			buffer, KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT));
		pdata->factory_current_value = KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT);
	}

	/* get torch current value */
	if (of_property_read_u32(dnode, "torch_current", &buffer) == 0) {
		dev_info(dev, "torch_current = <%d><%d>\n",
			buffer, KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT));
		pdata->torch_current_value = KTD2692_MOVIE_CURRENT(buffer, KTD2692_MAX_CURRENT);
	}

	ret = of_property_read_u32(dnode, "torch_table_enable", &torch_table_enable);
	if (ret) {
		pr_info("%s failed to get a torch_table_enable\n", __func__);
	}
	if (torch_table_enable == 1) {
		pdata->torch_table_enable = torch_table_enable;
		ret = of_property_read_u32_array(dnode, "torch_table", pdata->torch_table, TORCH_STEP);
	} else {
		pdata->torch_table_enable = 0;
	}

	return ret;
}

static int ktd2692_probe(struct platform_device *pdev)
{
	struct ktd2692_platform_data *pdata;
	int ret = 0;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	fled_cdev = &led->fled_cdev;
	led_cdev = &fled_cdev->led_cdev;

	ret = ktd2692_parse_dt(led, &pdev->dev, &led_cfg);
	if (ret)
		return ret;

	ktd2692_init_flash_timeout(fled_cdev, &led_cfg);
	ktd2692_init_movie_current_max(&led_cfg);

	fled_cdev->ops = &flash_ops;

	led_cdev->max_brightness = led_cfg.max_brightness;
	led_cdev->brightness_set_blocking = ktd2692_led_brightness_set;
	led_cdev->flags |= LED_CORE_SUSPENDRESUME | LED_DEV_CAP_FLASH;

	mutex_init(&led->lock);

	platform_set_drvdata(pdev, led);

	ret = led_classdev_flash_register(&pdev->dev, fled_cdev);
	if (ret) {
		dev_err(&pdev->dev, "can't register LED %s\n", led_cdev->name);
		mutex_destroy(&led->lock);
		return ret;
	}

	global_ktd2692data = pdata;
	global_dev = &pdev->dev;

	LED_INFO("KTD2692_LED Probe\n");

	global_ktd2692data->is_torch_enable = false;

	ktd2692_dev = device_create(camera_class, NULL, 0, NULL, "flash");
	if (IS_ERR(ktd2692_dev)) {
		LED_ERROR("Failed to create device(flash)!\n");
	}

	if (device_create_file(ktd2692_dev, &dev_attr_rear_flash) < 0) {
		LED_ERROR("failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
	}
	if (device_create_file(ktd2692_dev, &dev_attr_rear_torch_flash) < 0) {
		LED_ERROR("failed to create device file, %s\n",
				dev_attr_rear_torch_flash.attr.name);
	}
	spin_lock_init(&pdata->int_lock);

	return 0;
}
static int ktd2692_remove(struct platform_device *pdev)
{
	struct ktd2692_context *led = platform_get_drvdata(pdev);
	int ret;

	led_classdev_flash_unregister(&led->fled_cdev);

	if (led->regulator) {
		ret = regulator_disable(led->regulator);
		if (ret)
			dev_err(&pdev->dev,
				"Failed to disable supply: %d\n", ret);
	}

	mutex_destroy(&led->lock);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id ktd2692_dt_ids[] = {
	{ .compatible = "ktd2692",},
	{},
};
/*MODULE_DEVICE_TABLE(of, ktd2692_dt_ids);*/
#endif

static struct platform_driver ktd2692_driver = {
	.driver = {
		   .name = ktd2692_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ktd2692_dt_ids,
#endif
		   },
	.probe = ktd2692_probe,
	.remove = ktd2692_remove,
};

static int __init ktd2692_init(void)
{
	return platform_driver_register(&ktd2692_driver);
}

static void __exit ktd2692_exit(void)
{
	platform_driver_unregister(&ktd2692_driver);
}

module_init(ktd2692_init);
module_exit(ktd2692_exit);

MODULE_AUTHOR("sunggeun yim <sunggeun.yim@samsung.com>");
MODULE_DESCRIPTION("KTD2692 driver");
MODULE_LICENSE("GPL");


