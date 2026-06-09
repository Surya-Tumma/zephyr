#define DT_DRV_COMPAT nxp_lpc84x_dac

#include <zephyr/kernel.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>

#include <fsl_dac.h>
#include <fsl_power.h>
#include <fsl_iocon.h>

LOG_MODULE_REGISTER(lpc84x_dac, CONFIG_DAC_LOG_LEVEL);

struct lpc84x_dac_config {
	DAC_Type *base;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
	const struct pinctrl_dev_config *pincfg;
	bool low_power;
};

struct lpc84x_dac_data {
	bool configured;
};

static int lpc84x_dac_init(const struct device *dev)
{
	const struct lpc84x_dac_config *config = dev->config;
	dac_config_t dac_config;
	int err;

	/* 1. Zephyr Clock API */
	if (!device_is_ready(config->clock_dev)) {
		LOG_ERR("Clock device not ready");
		return -ENODEV;
	}

	err = clock_control_on(config->clock_dev, config->clock_subsys);
	if (err) {
		LOG_ERR("Failed to enable clock (Code: %d)", err);
		return err;
	}

	/* 2. NXP HAL Power Domain API */
	if (config->base == DAC0) {
		POWER_DisablePD(kPDRUNCFG_PD_DAC0);
	} else {
		POWER_DisablePD(kPDRUNCFG_PD_DAC1);
	}

	/* 3. Zephyr Pin Control API */
	err = pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	if (err) {
		LOG_ERR("Failed to configure DAC pins via pinctrl");
		return err;
	}

	/* 4. NXP HAL IOCON API (The necessary Analog workaround) */
	CLOCK_EnableClock(kCLOCK_Iocon);

	if (config->base == DAC0) {
		/* Open the DACEN analog gate for the fixed DAC0 pin (17) */
		IOCON_PinMuxSet(IOCON, 17, IOCON_MODE_INACT | (1UL << 16)); 
	} else {
		/* Open the DACEN analog gate for the fixed DAC1 pin (29) */
		IOCON_PinMuxSet(IOCON, 29, IOCON_MODE_INACT | (1UL << 16)); 
	}

	/* 5. NXP HAL DAC Initialization */
	DAC_GetDefaultConfig(&dac_config);
	dac_config.settlingTime = config->low_power ? 
                              kDAC_SettlingTimeIs25us : kDAC_SettlingTimeIs1us;

	DAC_Init(config->base, &dac_config);

	return 0;
}

static int lpc84x_dac_channel_setup(const struct device *dev,
				    const struct dac_channel_cfg *channel_cfg)
{
	struct lpc84x_dac_data *data = dev->data;

	if (channel_cfg->channel_id != 0) {
		LOG_ERR("unsupported channel %d", channel_cfg->channel_id);
		return -ENOTSUP;
	}

	if (channel_cfg->resolution != 10) {
		LOG_ERR("unsupported resolution %d", channel_cfg->resolution);
		return -ENOTSUP;
	}

	if (channel_cfg->internal) {
		LOG_ERR("Internal channels not supported");
		return -ENOTSUP;
	}

	data->configured = true;

	return 0;
}

static int lpc84x_dac_write_value(const struct device *dev, uint8_t channel,
				  uint32_t value)
{
	const struct lpc84x_dac_config *config = dev->config;
	struct lpc84x_dac_data *data = dev->data;

	if (!data->configured) {
		LOG_ERR("channel not initialized");
		return -EINVAL;
	}

	if (channel != 0) {
		LOG_ERR("unsupported channel %d", channel);
		return -ENOTSUP;
	}

	if (value >= 1024) {
		LOG_ERR("value %d out of range", value);
		return -EINVAL;
	}

	DAC_SetBufferValue(config->base, value);

	return 0;
}

static DEVICE_API(dac, lpc84x_dac_driver_api) = {
	.channel_setup = lpc84x_dac_channel_setup,
	.write_value = lpc84x_dac_write_value,
};

#define LPC84X_DAC_INIT(n) \
	PINCTRL_DT_INST_DEFINE(n); \
	static struct lpc84x_dac_data lpc84x_dac_data_##n; \
	static const struct lpc84x_dac_config lpc84x_dac_config_##n = { \
		.base = (DAC_Type *)DT_INST_REG_ADDR(n), \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)), \
		.clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n), \
		.low_power = DT_INST_PROP(n, low_power_mode), \
	}; \
	DEVICE_DT_INST_DEFINE(n, lpc84x_dac_init, NULL, \
		&lpc84x_dac_data_##n, &lpc84x_dac_config_##n, \
		POST_KERNEL, CONFIG_DAC_INIT_PRIORITY, \
		&lpc84x_dac_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LPC84X_DAC_INIT)
