let common = system.getScript("/common");
let pinmux = system.getScript("/drivers/pinmux/pinmux");
let soc = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

// Import all submodules
let epwmTimebase = system.getScript("/drivers/epwm/v2/epwm_timebase");
let epwmCounterCompare = system.getScript("/drivers/epwm/v2/epwm_counterCompare");
let epwmActionQualifier = system.getScript("/drivers/epwm/v2/epwm_actionQualifier");
let epwmDeadband = system.getScript("/drivers/epwm/v2/epwm_deadband");
let epwmTripZone = system.getScript("/drivers/epwm/v2/epwm_tripZone");
let epwmEventTrigger = system.getScript("/drivers/epwm/v2/epwm_eventTrigger");
let epwmChopper = system.getScript("/drivers/epwm/v2/epwm_chopper");

function getStaticConfigArr() {
    return system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`).getStaticConfigArr();
}

function getInstanceConfig(moduleInstance) {
    let solution = moduleInstance[getInterfaceName(moduleInstance)].$solution;
    let staticConfigArr = getStaticConfigArr();
    let staticConfig = staticConfigArr.find( o => o.name === solution.peripheralName);

    return {
        ...staticConfig,
        ...moduleInstance
    }
}

function epwmFilter(instance,peripheral) {
    /* EPWM appears in SYSCFG by mistake - fix it by this workaround */
    let blocked_epwm =[getInterfaceName(instance)];
    let found = blocked_epwm.find(
        function(str) {
            return str == peripheral.name;
        }
    );
    return !found;
}

function pinmuxRequirements(instance) {
    let interfaceName = getInterfaceName(instance);
    let resources = [];
    let pinResource = {};

    pinResource = pinmux.getPinRequirements(interfaceName, "0", "Output Channel 0");
    pinmux.setConfigurableDefault( pinResource, "rx", false );
    resources.push( pinResource);

    pinResource = pinmux.getPinRequirements(interfaceName, "1", "Output Channel 1");
    pinmux.setConfigurableDefault( pinResource, "rx", false );
    resources.push( pinResource);

    pinResource = pinmux.getPinRequirements(interfaceName, "SYNCO", "SYNC OUT")
    pinmux.setConfigurableDefault( pinResource, "rx", false );
    resources.push( pinResource);

    pinResource = pinmux.getPinRequirements(interfaceName, "SYNCI", "SYNC IN");
    pinmux.setConfigurableDefault( pinResource, "rx", true );
    resources.push( pinResource);

    let peripheral = {
        name          : interfaceName,
        displayName   : "EPWM",
        interfaceName : interfaceName,
        filter        : epwmFilter,
        resources     : resources,
    };

    return [peripheral];
}

function getInterfaceName(instance) {
     return soc.getInterfaceName(instance);
}

function getPeripheralPinNames(instance) {
    return [ "0", "1", "SYNCO", "SYNCI" ];
}

function getClockEnableIds(instance) {
    let instConfig = getInstanceConfig(instance);
    return instConfig.clockIds;
}

function validate(instance, report) {
    /* Validate time base configuration */
    if(instance.epwmTimebase_period > 65535) {
        report.logError("Time Base Period must be between 0 and 65535", instance, "epwmTimebase_period");
    }
    
    if(instance.epwmTimebase_phaseEnable && instance.epwmTimebase_phaseShift > 65535) {
        report.logError("Phase Shift must be between 0 and 65535", instance, "epwmTimebase_phaseShift");
    }
    
    /* Validate counter compare */
    if(instance.epwmCounterCompare_cmpA > 65535) {
        report.logError("Counter Compare A must be between 0 and 65535", instance, "epwmCounterCompare_cmpA");
    }
    
    if(instance.epwmCounterCompare_cmpB > 65535) {
        report.logError("Counter Compare B must be between 0 and 65535", instance, "epwmCounterCompare_cmpB");
    }

    /* CMP C/D: v2 limitation - they do not directly control outputs via Action Qualifier */
    if(instance.epwmCounterCompare_cmpC > 0) {
        report.logWarning("CMPC (Counter Compare C) does not directly control EPWM outputs in v2; use it only for event generation or sync/trigger purposes.", instance, "epwmCounterCompare_cmpC");
    }

    if(instance.epwmCounterCompare_cmpD > 0) {
        report.logWarning("CMPD (Counter Compare D) does not directly control EPWM outputs in v2; use it only for event generation or sync/trigger purposes.", instance, "epwmCounterCompare_cmpD");
    }
    
    /* Validate deadband delays */
    if(instance.epwmDeadband_risingEdgeDelay > 1023) {
        report.logError("Rising Edge Delay must be between 0 and 1023", instance, "epwmDeadband_risingEdgeDelay");
    }
    
    if(instance.epwmDeadband_fallingEdgeDelay > 1023) {
        report.logError("Falling Edge Delay must be between 0 and 1023", instance, "epwmDeadband_fallingEdgeDelay");
    }
}

// Build complete configuration array
let config = [
    {
        name: "GROUP_TIMEBASE",
        displayName: "Time Base",
        description: "Time Base Counter Configuration",
        longDescription: "Configure the time base counter which generates the time reference for all submodules",
        collapsed: false,
        config: epwmTimebase.config
    },
    {
        name: "GROUP_COUNTER_COMPARE",
        displayName: "Counter Compare",
        description: "Counter Comparator Configuration",
        longDescription: "Configure compare values that trigger actions when counter matches",
        collapsed: false,
        config: epwmCounterCompare.config
    },
    {
        name: "GROUP_ACTION_QUALIFIER",
        displayName: "Action Qualifier",
        description: "PWM Output Action Configuration",
        longDescription: "Define what actions to take on PWM outputs when events occur",
        collapsed: false,
        config: epwmActionQualifier.config
    },
    {
        name: "GROUP_DEADBAND",
        displayName: "Dead-Band Generator",
        description: "Dead-Band Configuration",
        longDescription: "Insert delays between complementary PWM signals to prevent shoot-through",
        collapsed: true,
        config: epwmDeadband.config
    },
    {
        name: "GROUP_TRIPZONE",
        displayName: "Trip Zone",
        description: "Trip Zone Configuration",
        longDescription: "Configure fault protection to safely handle overcurrent and other faults",
        collapsed: true,
        config: epwmTripZone.config
    },
    {
        name: "GROUP_EVENT_TRIGGER",
        displayName: "Event Trigger",
        description: "Interrupt and ADC Trigger Configuration",
        longDescription: "Configure CPU interrupts and ADC start-of-conversion triggers",
        collapsed: true,
        config: epwmEventTrigger.config
    },
    {
        name: "GROUP_CHOPPER",
        displayName: "PWM Chopper",
        description: "PWM Chopper Configuration",
        longDescription: "High-frequency carrier modulation for transformer-isolated gate drives",
        collapsed: true,
        config: epwmChopper.config
    }
];

let epwm_module_name = "/drivers/epwm/epwm";

let epwm_module = {
    displayName: "EPWM",
    templates: {
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/epwm/templates/v2/epwm_v2.h.xdt",
            moduleName: epwm_module_name,
        },
        "/drivers/pinmux/pinmux_config.c.xdt": {
            moduleName: epwm_module_name,
        },
        "/drivers/system/system_config.c.xdt": {
            driver_init: "/drivers/epwm/templates/v2/epwm_v2_init.c.xdt",
            driver_deinit: "/drivers/epwm/templates/v2/epwm_v2_deinit.c.xdt",
            moduleName: epwm_module_name,
        },
        "/drivers/system/drivers_open_close.h.xdt": {
            driver_open_close_config: "/drivers/epwm/templates/v2/epwm_v2_open_close_config.h.xdt",
        },
        "/drivers/system/drivers_open_close.c.xdt": {
            driver_open_close_config: "/drivers/epwm/templates/v2/epwm_v2_open_close_config.c.xdt",
            driver_open: "/drivers/epwm/templates/v2/epwm_v2_open.c.xdt",
            driver_close: "/drivers/epwm/templates/v2/epwm_v2_close.c.xdt",
        },
    },
    defaultInstanceName: "CONFIG_EPWM",
    config: config,
    validate: validate,
    modules: function(instance) {
        return [{
            name: "system_common",
            moduleName: "/system_common",
        }]
    },
    getInstanceConfig,
    pinmuxRequirements,
    getInterfaceName,
    getPeripheralPinNames,
    getClockEnableIds,
};

exports = epwm_module;