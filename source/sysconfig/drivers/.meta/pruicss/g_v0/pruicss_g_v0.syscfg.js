
let common = system.getScript("/common");
let pinmux = system.getScript("/drivers/pinmux/pinmux");
let soc = system.getScript(`/drivers/pruicss/soc/pruicss_${common.getSocName()}`);

function getConfigArr() {
    return soc.getConfigArr();
}

function getInstanceConfig(moduleInstance) {
    let configArr = getConfigArr();
    let config = configArr.find(o => o.name === moduleInstance.instance);
    /*Config array is modified in validation function as per clock setting,
    validation function is triggered by Sysconfig before templates calling this 
    getClockEnableIds function*/
    return {
        ...config,
        ...moduleInstance,
    };
};

function getMdioBaseAddr(pruicssInstance)
{
    let configArr = getConfigArr();
    let config = configArr.find(o => o.name === pruicssInstance);

    return config.mdioBaseAddr;
}

function getClockEnableIds(inst) {

    let instConfig = getInstanceConfig(inst);

    return instConfig.clockIds;
}

function getClockFrequencies(inst) {

    let instConfig = getInstanceConfig(inst);

    return instConfig.clockFrequencies;
}

let pruicss_top_module_name = "/drivers/pruicss/pruicss";

let pruicss_top_module = {
    displayName: "PRU (ICSS)",

    templates: {
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/drivers/pruicss/templates/pruicss_config.c.xdt",
            driver_init: "/drivers/pruicss/templates/pruicss_init.c.xdt",
            driver_deinit: "/drivers/pruicss/templates/pruicss_deinit.c.xdt",
        },
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/pruicss/templates/pruicss.h.xdt",
        },
        "/drivers/system/power_clock_config.c.xdt": {
            moduleName: pruicss_top_module_name,
        },
    },

    defaultInstanceName: "CONFIG_PRU_ICSS",
    config: [
        {
            name: "instance",
            displayName: "Instance",
            default: "ICSSG0",
            options: [
                {
                    name: "ICSSG0",
                },
                {
                    name: "ICSSG1",
                }
            ],
        },
        {
            name: "coreClk",
            displayName: "Core Clk (Hz)",
            default: 200*1000000,
            longDescription: `
| Core Clock Frequency | Clock Source | Description |
|---------------------|-------------|-------------|
| 200 MHz | PLL0_DIV9 (Parent ID 0) / PLL2_DIV0 (Parent ID 1) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 225 MHz | PLL2_DIV0 (Parent ID 1) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 250 MHz | ICLK | Used as PRU core clock (coreSyncMode automatically enabled) |
| 300 MHz | PLL2_DIV0 (Parent ID 1) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 333.33 MHz | PLL0_DIV9 (Parent ID 0)  | Auto-selected after checking conflict with SysConfig instance for other PRU-ICSS |

**Note:** When multiple PRUICSS instances are used, the system automatically resolves clock source conflicts to ensure proper operation, 225MHz + 300MHz is the only frequency combination 
that is blocked by the Sysconfig (since they both require using PLL2_HSDIV0).
`, 
            options: [
                {
                    name: 200*1000000,
                    displayName : "200MHz",
                    description : "PLL0_DIV9 (Parent ID 0) \ PLL2_DIV0 (Parent ID 1) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 225*1000000,
                    displayName : "225MHz",
                    description : "PLL2_DIV0 (Parent ID 1) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 250*1000000,
                    displayName : "250MHz",
                    description : "ICLK is used as PRU core clock"
                },
                {
                    name: 300*1000000,
                    displayName : "300MHz",
                    description : "PLL2_DIV0 (Parent ID 1) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 333333333,
                    displayName : "333.33333...MHz",
                    description : "PLL0_DIV9 (Parent ID 0) is auto-selected after checking conflict with SysConfig instance for other PRU-ICSS"
                },
            ],
            onChange: (inst, ui) => {
                if(inst.iepSyncMode)
                    inst.iepClk = inst.coreClk;
            },
        },
        {
            name: "uartClk",
            displayName: "Uart Clk (Hz)",
            default: 192*1000000,
            options: [
                {
                    name: 192*1000000,
                    displayName : "192MHz",
                }
            ],
        }, 
        {
            name: "iepSyncMode",
            displayName: "IEP Clk Sync Mode",
            longDescription: "In this mode the async IEP bridge is bypassed and the source of IEP CLK is ICSSGn_CORE_CLK. This means all PRU-ICSSG IOs which use internal IEP clock will use internal core clock.",
            default: false,
            onChange: (inst, ui) => {
                ui.iepClk.readOnly = inst.iepSyncMode;
                if(inst.iepSyncMode)
                    inst.iepClk = inst.coreClk;
            },
        },
        {
            name: "coreSyncMode",
            default: false,
            hidden: true,
            getValue: (inst) => {
				if(inst["coreClk"] == 250*1000000)
                {
                    return true;
                }
                return false;
	        }
        },
        {
            name: "iepClk",
            displayName: "IEP Clk (Hz)",
            default: 200*1000000,
            longDescription: `
| IEP Clock Frequency | Clock Source | Description |
|--------------------|-------------|-------------|
| 200 MHz | PLL0_DIV6 (Parent ID 5) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 225 MHz | PLL2_DIV5 (Parent ID 4) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 250 MHz | PLL0_DIV6 (Parent ID 5) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 300 MHz | PLL2_DIV5 (Parent ID 4) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 333.33 MHz | PLL0_DIV6 (Parent ID 5) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |
| 500 MHz | PLL0_DIV1 (Parent ID 11) | Auto-selected based on checking conflict with SysConfig instance for other PRU-ICSS |

**Note:** When IEP Clk Sync Mode is enabled, IEP clock source becomes ICSSGn_CORE_CLK and this configuration is bypassed. When multiple PRUICSS instances are used, the system automatically resolves IEP clock source conflicts to ensure proper operation.
`,
            options: [
                {
                    name: 200*1000000,
                    displayName : "200MHz",
                    description : "PLL0_DIV6 (Parent ID 5) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 225*1000000,
                    displayName : "225MHz",
                    description : "PLL2_DIV5 (Parent ID 4) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 250*1000000,
                    displayName : "250MHz",
                    description : "PLL0_DIV6 (Parent ID 5) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 300*1000000,
                    displayName : "300MHz",
                    description : "PLL2_DIV5 (Parent ID 4) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 333333333,
                    displayName : "333.33333...MHz",
                    description : "PLL0_DIV6 (Parent ID 5) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
                {
                    name: 500*1000000,
                    displayName : "500MHz",
                    description : "PLL0_DIV1 (Parent ID 11) is auto-selected based after checking conflict with SysConfig instance for other PRU-ICSS"
                },
            ],
        },
        {
            name: "EDLoadSharingMode",
            displayName: "EnDat Interface Load Sharing Mode",
            longDescription: "In this mode the RTU_PRU and TX_PRU cores can access and control the EnDat HW. RTU_PRU owns ED Ch-0, PRU owns ED Ch-1, TX_PRU owns ED Ch-2",
            default: "Disabled",
            options: [
                {
                    name: "Disabled",
                    description: "Load Sharing disabled",
                },
                {
                    name: "Slice0",
                    description: "Enable Load Sharing for Slice 0",
                },
                {
                    name: "Slice1",
                    description: "Enable Load Sharing for Slice 1",
                },
                {
                    name: "Slice0 & Slice1",
                    description: "Enable Load Sharing for both Slice0 and Slice 1",
                },
            ],
            hidden: true,
        },
        {
            name: "SDLoadSharingMode",
            displayName: "Sigma-Delta Interface Load Sharing Mode",
            longDescription: "In this mode the RTU_PRU and TX_PRU cores can access and control the Sigma-Delta HW. RTU_PRU owns SD0-SD2, PRU owns SD3-SD5, TX_PRU owns SD6-SD8",
            default: "Disabled",
            options: [
                {
                    name: "Disabled",
                    description: "Load Sharing disabled",
                },
                {
                    name: "Slice0",
                    description: "Enable Load Sharing for Slice 0",
                },
                {
                    name: "Slice1",
                    description: "Enable Load Sharing for Slice 1",
                },
                {
                    name: "Slice0 & Slice1",
                    description: "Enable Load Sharing for both Slice0 and Slice 1",
                },
            ],
            hidden: true,
        },
    ],
    validate: validate,
    moduleStatic: {
        modules: function(inst) {
            return [{
                name: "system_common",
                moduleName: "/system_common",
            }]
        },
    },
    moduleInstances: moduleInstances,
    getInstanceConfig,
    getClockFrequencies,
    getClockEnableIds,
    getMdioBaseAddr,
};

function resolveCoreClockConflicts(inst) {
    let possibleConfigurations = getConfigArr()[2]["coreClockParentIdCombinations"];
    let instance0 = inst.$module.$instances[0];
    let config0;
    let config1;
    /*if instance0 coreSyncMode is enabled then conflicting PLL_DIVs : TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_HSDIV4_16FFT_MAIN_2_HSDIVOUT0_CLK
    & TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_POSTDIV4_16FF_MAIN_0_HSDIVOUT9_CLK are not used for instance0*/
    if(!(instance0.coreSyncMode))
    {
        config0 = getInstanceConfig(instance0).clockFrequencies[0];
        config0.clkRate = instance0.coreClk;
        config0.used = 0;
    }
    if(inst.$module.$instances.length == 2)
    {
        let instance1 = inst.$module.$instances[1];
        /*if instance1 coreSyncMode is enabled then conflicting PLL_DIVs : TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_HSDIV4_16FFT_MAIN_2_HSDIVOUT0_CLK
        & TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_POSTDIV4_16FF_MAIN_0_HSDIVOUT9_CLK are not used for instance1*/
        if(!(instance1.coreSyncMode))
        {
            config1 = getInstanceConfig(instance1).clockFrequencies[0];
            config1.clkRate = instance1.coreClk;
            config1.used = 0;
        }
    }
    assignNonConflictingClockParents(config0, config1, possibleConfigurations);
}

function resolveIepClockConflicts(inst) {
    let possibleConfigurations = getConfigArr()[2]["iepClockParentIdCombinations"];
    let instance0 = inst.$module.$instances[0];
    let config0;
    let config1;
    if(!(instance0.iepSyncMode))
    {
        config0 = getInstanceConfig(instance0).clockFrequencies[2];
        config0.clkRate = instance0.iepClk;
        config0.used = 0;
    }

    if(inst.$module.$instances.length == 2)
    {
        let instance1 = inst.$module.$instances[1];
        if(!(instance1.iepSyncMode))
        {
            config1 = getInstanceConfig(instance1).clockFrequencies[2];
            config1.clkRate = instance1.iepClk;
            config1.used = 0;
        }
    }
    assignNonConflictingClockParents(config0, config1, possibleConfigurations);
}

function assignNonConflictingClockParents(config0, config1, possibleConfigurations) {
    let parentIds = possibleConfigurations.PossibleClkParentId;
    let parentRates = possibleConfigurations.PossibleClkRate;

    // Find which parents can support each rate
    let config0ParentOptions = [];
    let config1ParentOptions = [];
    let clkRate0, clkRate1, clkParentId0, clkParentId1;
    if(config0 && config1)
    {
        clkRate0 = config0.clkRate;
        clkRate1 = config1.clkRate;
        for (let i = 0; i < parentIds.length; i++) {
            if (parentRates[i].includes(clkRate1)) {
                config1ParentOptions.push(parentIds[i]);
            }
        }
        for (let i = 0; i < parentIds.length; i++) {
            if (parentRates[i].includes(clkRate0)) {
                config0ParentOptions.push(parentIds[i]);
            }
        }
        for (let i = 0; i < config0ParentOptions.length; i++) {
            for (let j = 0; j < config1ParentOptions.length; j++) {
                clkParentId0 = config0ParentOptions[i];
                clkParentId1 = config1ParentOptions[j];
                if ((clkParentId0!== clkParentId1) || (clkRate0 === clkRate1 && ((clkParentId0 == clkParentId1)))) {
                    config0.clkParentId = clkParentId0;
                    config1.clkParentId = clkParentId1;
                    config0.used = 1;
                    config1.used = 1;
                    break;
                }
            }
            if(config0.used == 1 && config1.used == 1)
            {
                break;
            }
        }
    }
    else if(config0)
    {
        clkRate0 = config0.clkRate;
        for (let i = 0; i < parentIds.length; i++) {
            if (parentRates[i].includes(clkRate0)) {
                config0ParentOptions.push(parentIds[i]);
            }
        }
        if(config0ParentOptions.length != 0)
        {
            clkParentId0 = config0ParentOptions[0];
            config0.used = 1;
            config0.clkParentId = clkParentId0;
        }
    }
    else if(config1)
    {
        clkRate1 = config1.clkRate;
        for (let i = 0; i < parentIds.length; i++) {
            if (parentRates[i].includes(clkRate1)) {
                config1ParentOptions.push(parentIds[i]);
            }
        }
        if(config1ParentOptions.length != 0)
        {
            clkParentId1 = config1ParentOptions[0];
            config1.used = 1;
            config1.clkParentId = clkParentId1;
        }
    }
}

function validate(inst, report) {
    common.validate.checkSameInstanceName(inst, report);
    let device = common.getDeviceName();
    let configArr = getConfigArr();
    let config = configArr.find(o => o.name === inst.instance);
    /*If PRU clock frequency is 250MHZ then coreSyncMode is auto enabled, So TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_HSDIV4_16FFT_MAIN_2_HSDIVOUT0_CLK,
    TISCI_DEV_PRU_ICSSG0_CORE_CLK_PARENT_POSTDIV4_16FF_MAIN_0_HSDIVOUT9_CLK configuration is not required*/
    if((inst["coreSyncMode"]))
    {
        config.clockFrequencies[0].used = 0;   
    }
    else
    {
        if(inst.$name == inst.$module.$instances[0].$name)
        {
            resolveCoreClockConflicts(inst);
        }
        if(config.clockFrequencies[0].used == 0)
        {
            let errorMsg = "PRU Clock sources required for PRU-ICSSG0 and PRU-ICSSG1 is same which can not support different clock frequecies for each PRU-ICSSG."+
            "Check the description of Core Clk (Hz) by hovering over for more information";
            report.logError(errorMsg, inst);
        }
    }
    /*If IEP clock sync is enabled then TISCI_DEV_PRU_ICSSG0_IEP_CLK_PARENT_POSTDIV4_16FF_MAIN_2_HSDIVOUT5_CLK,
    TISCI_DEV_PRU_ICSSG0_IEP_CLK_PARENT_POSTDIV4_16FF_MAIN_0_HSDIVOUT6_CLK, 
    TISCI_DEV_PRU_ICSSG0_IEP_CLK_PARENT_K3_PLL_CTRL_WRAP_MAIN_0_CHIP_DIV1_CLK_CLK configuration is not required*/
    if((inst["iepSyncMode"]))
    {
        config.clockFrequencies[2].used = 0;   
    }
    else
    {
        if(inst.$name == inst.$module.$instances[0].$name)
        {
            resolveIepClockConflicts(inst);
        }
        if(config.clockFrequencies[2].used == 0)
        {
            let errorMsg = "IEP Clock sources required for PRU-ICSSG0-IEP and PRU-ICSSG1-IEP is same which can not support different clock frequecies for each PRU-ICSSG-IEP."+
            "Check the description of IEP Clk (Hz) by hovering over for more information";
            report.logError(errorMsg, inst);
        }
    }
}

function moduleInstances(instance) {
    let device = common.getDeviceName();
    let modInstances = new Array();
    if((device === "am64x-evm") || (device === "am243x-evm") || (device === "am243x-lp"))
    {
         modInstances.push({
             name: "AdditionalICSSSettings",
             displayName: "Additional ICSS Settings",
             moduleName: '/drivers/pruicss/g_v0/pruicss_g_v0_gpio',
             requiredArgs: {
                instance: instance["instance"],
             },
             
             useArray: true,
             minInstanceCount: 1,
             defaultInstanceCount: 1,
             maxInstanceCount: 1,
         });
    }

    if((device === "am64x-evm") || (device === "am243x-evm") || (device === "am243x-lp"))
    {
        // Interrupt Mapping:
        let submodule = "/drivers/pruicss/icss_intc/";
        if(instance.instance === "ICSSG0")
            submodule += "icss0_intc_mapping";
        else if(instance.instance === "ICSSG1")
            submodule += "icss1_intc_mapping";
        else
            submodule += "icss0_intc_mapping";
        modInstances.push({
            name: "intcMapping",
            displayName: instance.instance + " INTC Internal Signals Mapping",
            moduleName: submodule,
            useArray: true,
            defaultInstanceCount: 0,
        });
    }
    return (modInstances);
}
exports = pruicss_top_module;