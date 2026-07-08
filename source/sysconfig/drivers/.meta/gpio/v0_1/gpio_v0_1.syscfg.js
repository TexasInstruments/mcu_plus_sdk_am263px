let common = system.getScript("/common");
let pinmux = system.getScript("/drivers/pinmux/pinmux");
let soc = system.getScript(`/drivers/gpio/soc/gpio_${common.getSocName()}`);
let boardConfig = system.getScript(`/drivers/gpio/soc/k3BoardConfig.json`);

let errorFlag = 0;
let errorLog = '';

function getInstanceConfig(moduleInstance) {
    let coreId = undefined;
    let routerId = undefined;
    if (common.isMcuDomainSupported() || common.isWakeupDomainSupported()) {
        coreId = soc.getTisciDestCoreID();
        routerId = soc.getCpuRouterId();
    }
    let additionalConfig = {
        baseAddr: `CSL_${soc.getInstanceString(moduleInstance)}_BASE`,
        moduleIndex: soc.getInstanceString(moduleInstance),
        pinIndex: soc.getPinIndex(moduleInstance),
    }

    return {
        ...moduleInstance,
        ...additionalConfig,
        coreId,
        routerId,
    };
};

function getInterfaceName(inst) {
    return soc.getInterfaceName(inst);
}

function pinmuxRequirements(inst) {
    let interfaceName = getInterfaceName(inst);

    let resources = [];
    resources.push(pinmux.getGpioPinRequirements(interfaceName, "0"));

    let peripheral = {
        name: interfaceName,
        displayName: "GPIO Peripheral",
        interfaceName: interfaceName,
        resources: resources,
        canShareWith: "/drivers/gpio/gpio",
    };

    return [peripheral];
}

function getPeripheralPinNames(inst) {
    return ["gpioPin"];
}

function validate(inst, report) {
        validateInterruptRouter(inst, report, "intrOut");
        validateBankInterrupt(inst, report);
}



/* Get the GPIO bank identifier from the pad $assign value, using
 * system.deviceData to map the pad signal name to the GPIO peripheral
 * pin. This avoids reading $solution which is stale in validate()
 * when a pin change triggers revalidation.
 *
 * Each GPIO instance has banks of 16 pins (BANK0 = pins 0-15,
 * BANK1 = pins 16-31, etc.). Returns "<instance>/<bankNum>" e.g.
 * "GPIO0/0", or null if not yet assigned. */
function getGpioBank(inst) {
    try {
        let interfaceName = soc.getInterfaceName(inst);
        let padName = inst[interfaceName].gpioPin.$assign;

        if (padName && padName !== "Any") {
            /* Explicit pad assigned: use device data lookup to avoid stale
             * $solution when the user changes the pad selection. */
            for (let key in system.deviceData.devicePins) {
                let dp = system.deviceData.devicePins[key];
                if (dp && dp.name === padName) {
                    let muxSettings = dp.mux && dp.mux.muxSetting;
                    if (muxSettings) {
                        for (let msKey in muxSettings) {
                            let ppName = muxSettings[msKey].peripheralPin.name;
                            if (/^(MCU_)?GPIO\d*_\d+$/.test(ppName)) {
                                let parts = ppName.split("_");
                                let pinIndex = parseInt(parts[parts.length - 1], 10);
                                if (isNaN(pinIndex)) continue;
                                let instancePart = parts.slice(0, parts.length - 1).join("_");
                                let bankNum = Math.floor(pinIndex / 16);
                                return instancePart + "/" + bankNum;
                            }
                        }
                    }
                    break;
                }
            }
            return null;
        } else {
            /* "Any" or unset: solver auto-picks the pad, user is not manually
             * changing it, so $solution is always current — use it directly. */
            let instanceName = soc.getInstanceString(inst);
            let pinIndex = parseInt(soc.getPinIndex(inst), 10);
            if (isNaN(pinIndex)) return null;
            let bankNum = Math.floor(pinIndex / 16);
            return instanceName + "/" + bankNum;
        }
    } catch (e) {
        return null;
    }
}

/* Convert internal bank key "GPIO0/2" to display string "GPIO0 BANK2" */
function bankKeyToStr(bankKey) {
    let parts = bankKey.split("/");
    return parts[0] + " BANK" + parts[1];
}

/* Validate that only one pin per GPIO bank has interrupt enabled.
 * Applicable for am64x only. Uses getGpioBank() which reads $assign
 * (not $solution), so it is always current even during pin changes. */
function validateBankInterrupt(instance, report) {
    if (common.getSocName() != "am64x") return;
    if (instance.enableIntr) {
        let myBank = getGpioBank(instance);
        if (myBank !== null) {
            let moduleInstances = instance.$module.$instances;
            for (let i = 0; i < moduleInstances.length; i++) {
                if (moduleInstances[i] !== instance &&
                    moduleInstances[i].enableIntr === true) {
                    let otherBank = getGpioBank(moduleInstances[i]);
                    if (otherBank !== null && otherBank === myBank) {
                        report.logError(
                            "Conflicts with '" + moduleInstances[i].$name +
                            "': both pins are in " + bankKeyToStr(myBank) +
                            ". Only one interrupt per GPIO bank (16 pins) is allowed.",
                            instance, "enableIntr");
                        return;
                    }
                }
            }
        }
    }
}

//Function to validate if same interrupt router is selected for other instances
function validateInterruptRouter(instance, report, fieldname) {
    /* Verified by SYSCFG based on selected pin */
    if (instance.enableIntr && common.isSciClientSupported()) {
        common.validate.checkNumberRange(instance, report, fieldname, 0, soc.getMaxInterruptRouters(), "dec");
        if(errorFlag == 1) {
            report.logWarning(errorLog, instance, fieldname);
        }
        let moduleInstances = instance.$module.$instances;
        let validOptions = instance.$module.$configByName.intrOut.options(instance);
        let selectedOptions = instance.intrOut;
        let found = _.find(validOptions, (o) => o.name === selectedOptions)
        if (found === undefined || found === null) {
            report.logError("Selected option is invalid, please reselect.", instance, fieldname);
        }
        for (let i = 0; i < moduleInstances.length; i++) {
            if (instance[fieldname] === moduleInstances[i][fieldname] && instance !== moduleInstances[i]
                && instance.enableIntr === true && moduleInstances[i].enableIntr === true) {
                report.logError("Same Interrupt Router lines cannot be selected", instance, fieldname);
                return
            }
        }
    }
}

let routerDescription =
`The interrupt router input to the Core are shared for different resources.
 Although many output pins were available for the GPIO MUX interrupt router,
 only resource pin that are allocated in board configuration is available for use`;
let buttonDescription =
`If you manually changed the resource management (RM) data in
source/drivers/sciclient/sciclient_default_boardcfg/am64x_am243x/sciclient_defaultBoardcfg_rm.c,
click this button to reflect it in SysConfig`;

//function to get router pin data from boardConfig
function getRouterPins() {
    let socName = common.getSocName();
    let core = common.getSelfSysCfgCoreName();
    return boardConfig[socName][core]["outPinCfg"];
}

function getConfigurables() {
    let config = [];
    config.push({
        name: "pinDir",
        displayName: "PIN Direction",
        default: "INPUT",
        options: [{
            name: "INPUT",
            displayName: "Input"
        },
        {
            name: "OUTPUT",
            displayName: "Output"
        },
        ],
        description: "Direction of GPIO Pin. Can be either input or output",
        onChange: function(inst, ui) {
            if(inst.pinDir == "OUTPUT"){
                ui.defaultValue.hidden = false;
            }
            else {
                ui.defaultValue.hidden = true;
            }
        }
    },
    {
        name: "defaultValue",
        displayName: "Default Value",
        default: "0",
        options: [
            { name: "0" },
            { name: "1" },
        ],
        description: "Default value of GPIO OUT register",
        hidden: true,
    },
    {
        name: "trigType",
        displayName: "Trigger Type",
        default: "NONE",
        options: [{
            name: "NONE",
            displayName: "None",
        },
        {
            name: "RISE_EDGE",
            displayName: "Rising Edge",
        },
        {
            name: "FALL_EDGE",
            displayName: "Falling Edge",
        },
        {
            name: "BOTH_EDGE",
            displayName: "Rising and Falling",
        },
        ],
        description: "GPIO Trigger type for interrupt generation",
    },
    {
        name: "enableIntr",
        displayName: "Enable Interrupt Configuration",
        description: "Enable this option to do the interrupt configuration for GPIO Pin",
        default: false,
        onChange: function (inst, ui) {
            if (common.isSciClientSupported())
            {
                let hideConfigs = true;
                if (inst.enableIntr == true) {
                    hideConfigs = false;
                }
                ui.intrOut.hidden = hideConfigs;
                ui.getBoardCfg.hidden = hideConfigs;
            }
        },
    },
    {
        name: "gpioBank",
        displayName: "GPIO Bank",
        description: "GPIO bank this pin belongs to (16 pins per bank). Read-only, computed from pin assignment.",
        default: "Not determined",
        readOnly: true,
        getValue: (inst) => {
            let bank = getGpioBank(inst);
            if (bank !== null) {
                let parts = bank.split("/");
                return parts[0] + " BANK" + parts[1];
            }
            return "Not determined";
        },
    },
    )
    if (common.isSciClientSupported()) {
        config.push(
            {
                name: "getBoardCfg",
                displayName: "Fetch Board Configuration",
                description: 'Click this button to fetch GPIO RM data automatically',
                longDescription: buttonDescription,
                buttonText: "GET RM DATA",
                hidden: true,
                onLaunch: (inst) => {
                    let nodeCmd = common.getNodePath()
                    let products = system.getProducts()
                    let sdkPath = products[0].path.split("/.metadata/product.json")[0]
                    let filePath = sdkPath + "/source/drivers/.meta/gpio/soc/getBoardConfigRm.js"
                    let socName = common.getSocName();
                    if (system.getOS() == "win") {
                        sdkPath = products[0].path.split("\\.metadata\\product.json")[0]
                        filePath = sdkPath + "//source//drivers//.meta//gpio//soc//getBoardConfigRm.js"
                    }
                    return {
                        command: nodeCmd,
                        args: [filePath, "$comFile", socName],
                        initialData: "initialData",
                        inSystemPath: true,
                    };
                },
                onComplete: (inst, _ui, result) => {
                    if (result.data === "error") {
                        return;
                    } else if (result.data === "initialData") {
                        return;
                    } else {
                        try {
                            errorFlag = 0;
                            boardConfig = JSON.parse(result.data);
                        } catch (e) {
                            errorFlag = 1;
                            errorLog = result.data;
                            return;
                        }
                        return;
                    }
                },
            },
            {
                name: "intrOut",
                displayName: "Interrupt Router Output",
                description: 'GPIO-MUX interrupt Router Output to the destination Core',
                longDescription: routerDescription,
                hidden: true,
                default: getRouterPins()[0].name,
                options: getRouterPins,
            },
        )
    }
    if ((common.isMcuDomainSupported()) && (common.getSocName() != "am65x")) {
        config.push(common.getUseMcuDomainPeripheralsConfig());
    }
    if(common.isWakeupDomainSupported())
    {
      config.push(common.getUseWakeupDomainPeripheralsConfig());
    }
    return config;
}

let gpio_module_name = "/drivers/gpio/gpio";

let gpio_module = {
    displayName: "GPIO",
    templates: {
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/gpio/templates/gpio.h.xdt",
            moduleName: gpio_module_name,
        },
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/drivers/gpio/templates/gpio_config.c.xdt",
            driver_init: "/drivers/gpio/templates/gpio_init.c.xdt",
            driver_deinit: "/drivers/gpio/templates/gpio_deinit.c.xdt",
            moduleName: gpio_module_name,
        },
        "/drivers/pinmux/pinmux_config.c.xdt": {
            moduleName: gpio_module_name,
        },
    },
    defaultInstanceName: "CONFIG_GPIO",
    config: getConfigurables(),
    validate: validate,
    modules: function (inst) {
        return [{
            name: "system_common",
            moduleName: "/system_common",
        }]
    },
    getInstanceConfig,
    pinmuxRequirements,
    getInterfaceName,
    getPeripheralPinNames,
};

exports = gpio_module;