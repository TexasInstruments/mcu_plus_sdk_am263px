exports = {
    config: [],
	templates: {
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/drivers/mcan/templates/mcan_v0_config_hld.c.xdt",
            driver_init: "/drivers/mcan/templates/mcan_init.c.xdt",
            driver_deinit: "/drivers/mcan/templates/mcan_deinit.c.xdt",
        },
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/mcan/templates/mcan_v0_hld.h.xdt",
        },
        "/drivers/system/power_clock_config.c.xdt": {
        },
        "/drivers/system/drivers_open_close.c.xdt": {
            driver_open_close_config: "/drivers/mcan/templates/mcan_v0_open_close_config_hld.c.xdt",
            driver_open: "/drivers/mcan/templates/mcan_open.c.xdt",
            driver_close: "/drivers/mcan/templates/mcan_close.c.xdt",
        },
        "/drivers/system/drivers_open_close.h.xdt": {
            driver_open_close_config: "/drivers/mcan/templates/mcan_v0_open_close_hld.h.xdt",
        },
    },
};