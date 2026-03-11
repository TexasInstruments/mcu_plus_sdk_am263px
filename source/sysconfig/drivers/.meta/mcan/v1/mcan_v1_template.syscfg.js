exports = {
    config: [],
	templates: {
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/drivers/mcan/templates/mcan_v1_config.c.xdt",
        },
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/mcan/templates/mcan_v1.h.xdt",
        },
        "/drivers/system/power_clock_config.c.xdt": {
        },
        "/drivers/system/drivers_open_close.c.xdt": {
            driver_open_close_config: "/drivers/mcan/templates/mcan_v1_open_close_config.c.xdt",
            driver_open: "/drivers/mcan/templates/mcan_open.c.xdt",
            driver_close: "/drivers/mcan/templates/mcan_close.c.xdt",
        },
        "/drivers/system/drivers_open_close.h.xdt": {
            driver_open_close_config: "/drivers/mcan/templates/mcan_v1_open_close.h.xdt",
        },
    },
};