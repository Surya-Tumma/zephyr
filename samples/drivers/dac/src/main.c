/*
 * Copyright (c) 2020 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/dac.h>

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

#if (DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac) && \
	DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac_channel_id) && \
	DT_NODE_HAS_PROP(ZEPHYR_USER_NODE, dac_resolution))
#define DAC_NODE DT_PHANDLE(ZEPHYR_USER_NODE, dac)
#define DAC_CHANNEL_ID DT_PROP(ZEPHYR_USER_NODE, dac_channel_id)
#define DAC_RESOLUTION DT_PROP(ZEPHYR_USER_NODE, dac_resolution)
#else
#error "Unsupported board: see README and check /zephyr,user node"
#define DAC_NODE DT_INVALID_NODE
#define DAC_CHANNEL_ID 0
#define DAC_RESOLUTION 0
#endif

static const struct device *const dac_dev = DEVICE_DT_GET(DAC_NODE);

static const struct dac_channel_cfg dac_ch_cfg = {
	.channel_id  = DAC_CHANNEL_ID,
	.resolution  = DAC_RESOLUTION,
#if defined(CONFIG_DAC_BUFFER_NOT_SUPPORT)
	.buffered = false,
#else
	.buffered = true,
#endif /* CONFIG_DAC_BUFFER_NOT_SUPPORT */
};

int main(void)
{
	if (!device_is_ready(dac_dev)) {
		printk("DAC device %s is not ready\n", dac_dev->name);
		return 0;
	}

	int ret = dac_channel_setup(dac_dev, &dac_ch_cfg);

	if (ret != 0) {
		printk("Setting up of DAC channel failed with code %d\n", ret);
		return 0;
	}

printk("Generating slow square wave at DAC channel %d.\n", DAC_CHANNEL_ID);

	while (1) {
		/* Step 1: Force 0 Volts (0%) */
		printk("Outputting 0.00V...\n");
		ret = dac_write_value(dac_dev, DAC_CHANNEL_ID, 0);
		if (ret != 0) {
			printk("dac_write_value() failed: %d\n", ret);
		}
		k_msleep(2000); /* Freeze for 2 full seconds */

		/* Step 2: Force 3.3 Volts (100%) */
		printk("Outputting 3.30V...\n");
		ret = dac_write_value(dac_dev, DAC_CHANNEL_ID, 1023);
		if (ret != 0) {
			printk("dac_write_value() failed: %d\n", ret);
		}
		k_msleep(2000); /* Freeze for 2 full seconds */
	}

	return 0;
}
