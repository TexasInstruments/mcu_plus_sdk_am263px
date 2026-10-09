let common = system.getScript("/common");
let soc = system.getScript(`/drivers/pruicss/soc/pruicss_${common.getSocName()}`);
let helperScript = system.getScript(`/clockTree/helperScript.js`);
let is_am263x_soc = (common.getSocName() == "am263x") ? true : false;
let is_am263px_soc = (common.getSocName() == "am263px") ? true : false;
let is_am261x_soc = (common.getSocName() == "am261x") ? true : false;

/*am261x: R5F clock tree only offers 400MHz or 500MHz; 450MHz is the midpoint used to classify it*/
function isR5Clock400MHz() {
    return helperScript.helperGetFrequencyNamedConnection("R5FSS0_GATED_CLK") <= 450*1000000;
}

/*am261x: Core Clk and IEP Clk being equal is exactly what IEP Clk Sync Mode achieves (IEP sourced from core clock), so auto-enable it*/
function enableSyncModeIfClocksMatch(inst, ui) {
    if(!inst.iepSyncMode && inst.coreClk == inst.iepClk)
    {
        inst.iepSyncMode = true;
        ui.iepClk.readOnly = true;
        ui.coreClk.readOnly = true;
    }
}

function getConfigArr() {
    return soc.getConfigArr();
}

function getInstanceConfig(moduleInstance) {
    let configArr = getConfigArr();
    let config = configArr.find(o => o.name === moduleInstance.instance);
    if(is_am261x_soc)
    {
        /*am261x: changing Core clock id based on user selection*/
        if(moduleInstance.coreClk == 200*1000000)
        {
            config.clockFrequencies[0].clkId = "SOC_RcmPeripheralClockSource_SYS_CLK";
            config.clockFrequencies[0].clkRate = 200*1000000
        }
        else
        {
            config.clockFrequencies[0].clkId = "SOC_RcmPeripheralClockSource_DPLL_ETH_HSDIV0_CLKOUT0";  
            config.clockFrequencies[0].clkRate = 225*1000000  
        }
    }else{       
       /*am263x/am263px : changing Uart clock id based on user selection*/
       if(moduleInstance.uartClk == 192*1000000)
       {
           config.clockFrequencies[0].clkId = "SOC_RcmPeripheralClockSource_DPLL_PER_HSDIV0_CLKOUT1";
           config.clockFrequencies[0].clkRate = 192*1000000
       }
       else
       {
           config.clockFrequencies[0].clkId = "SOC_RcmPeripheralClockSource_DPLL_PER_HSDIV0_CLKOUT0";  
           config.clockFrequencies[0].clkRate = 160*1000000  
       }  
    }
    return {
        ...config,
        ...moduleInstance,
    };
};

function getClockFrequencies(inst) {

    let instConfig = getInstanceConfig(inst);

    return instConfig.clockFrequencies;
}

function getMdioBaseAddr(pruicssInstance)
{
    let configArr = getConfigArr();
    let config = configArr.find(o => o.name === pruicssInstance);

    return config.mdioBaseAddr;
}

function getConfigurables()
{
    let config=new Array();
    if(is_am263x_soc || is_am263px_soc){
        config.push(
            {
                name: "instance",
                displayName: "Instance",
                default: "ICSSM0",
                options: [
                    {
                        name: "ICSSM0",
                    },
                ],
            },
            {
                name: "coreClk",
                displayName: "Core Clk (Hz)",
                default: 200*1000000,
                options: [
                    {
                        name: 200*1000000,
                        description: "ICSS core clock is derived from SYS_CLK(200 MHz)"
                    }
                ],
            }, 
            {
                name: "iepClk",
                displayName: "IEP Clk (Hz)",
                default: 200*1000000,
                options: [
                    {
                        name: 200*1000000,
                        description: "ICSS iep clock is derived from SYS_CLK(200 MHz or 250 MHz)"
                    }
                ],
            }, 
            {
                name: "uartClk",
                displayName: "Uart Clk (Hz)",
                default: 192*1000000,
                options: [
                    {
                        name: 160*1000000,
                        description: "UART clock is derived from DPLL_PER_HSDIV0_CLKOUT0(160 MHz)"
                    },
                    {
                        name: 192*1000000,
                        description: "UART clock is derived from DPLL_PER_HSDIV0_CLKOUT1(192 MHz)"
                    }
                ],
            },  
            {
                name: "iepSyncMode",
                displayName: "IEP Clk Sync Mode",
                longDescription: "In this mode the async IEP bridge is bypassed and the source of IEP CLK is ICSSM0_CORE_CLK. This means all PRU-ICSSM0 IOs which use internal IEP clock will use internal core clock.",
                default: false,
            },
            
        )
    }
    else {
        /*am261x: default Core/IEP clocks to match the current R5F clock frequency (400MHz -> 200MHz, 500MHz -> 225/250MHz)*/
        let defaultCoreClk = isR5Clock400MHz() ? 200*1000000 : 225*1000000;
        let defaultIepClk = isR5Clock400MHz() ? 200*1000000 : 250*1000000;
        /*400MHz defaults land on matching Core/IEP clocks - default Sync Mode on (and lock the fields) to match,
          since onChange only fires on direct user edits, not on instance creation/migration*/
        let defaultIepSyncMode = (defaultCoreClk == defaultIepClk);
        config.push(
            {
                name: "instance",
                displayName: "Instance",
                default: "ICSSM0",
                options: [
                    {
                        name: "ICSSM0",
                    },
                    {
                        name: "ICSSM1",
                    },
                ],
            },
            {
                name: "coreClk",
                displayName: "Core Clk (Hz)",
                default: defaultCoreClk,
                readOnly: defaultIepSyncMode,
                options: [
                    {
                        name: 200*1000000,
                        description: "ICSS core clock is derived from SYS_CLK(200 MHz or 250 MHz) but 250 MHz is not supported with ICSSMn"
                    },
                    {
                        name: 225*1000000,
                        description: "ICSS core clock is derived from DPLL_ETH_HSDIV0_CLKOUT0(450 MHz) with divider 2"
                    }
                ],
                onChange: enableSyncModeIfClocksMatch,
            },
            {
                name: "iepClk",
                displayName: "IEP Clk (Hz)",
                default: defaultIepClk,
                readOnly: defaultIepSyncMode,
                options: [
                    {
                        name: 200*1000000,
                        description: "ICSS iep clock is derived from SYS_CLK(200 MHz or 250 MHz)"
                    },
                    {
                        name: 250*1000000,
                        description: "ICSS iep clock is derived from SYS_CLK(200 MHz or 250 MHz)"
                    },
                    {
                        name: 225*1000000,
                        description: "ICSS iep clock is derived from Core Clk in IEP Clk sync mode"
                    }
                ],
                getDisabledOptions: () => {
                    return [{
                        name: 225*1000000,
                        reason: "This option is auto selected when IEP Clk Sync Mode is enabled"
                    }]
                },
                onChange: enableSyncModeIfClocksMatch,
            },
            {
                name: "uartClk",
                displayName: "Uart Clk (Hz)",
                default: 160*1000000,
                options: [
                    {
                        name: 160*1000000,
                        description: "UART clock is derived from DPLL_PER_HSDIV0_CLKOUT2(160 MHz)"
                    }
                ],
            },
            {
                name: "iepSyncMode",
                displayName: "IEP Clk Sync Mode",
                longDescription: "In this mode the async IEP bridge is bypassed and the source of IEP CLK is ICSSMn_CORE_CLK. This means all PRU-ICSSMn IOs which use internal IEP clock will use internal core clock.",
                default: defaultIepSyncMode,
                onChange: (inst, ui) => {
                    if(inst.iepSyncMode)
                    {
                        inst.iepClk = 225*1000000,
                        inst.coreClk = 225*1000000,
                        ui.iepClk.readOnly =  true,
                        ui.coreClk.readOnly =  true
                    }
                    else
                    {
                        /*Change back IEP to default option (matching current R5F clock frequency) if iepSyncMode is disabled.
                          Uses the defaultIepClk captured when this instance's config was built, since onChange handlers
                          cannot access system.clockTree - only the current instance's configuration.*/
                        inst.iepClk = defaultIepClk,
                        ui.iepClk.readOnly =  false,
                        ui.coreClk.readOnly =  false
                    }     
                },
            },
        )
    }

    if(is_am263x_soc || is_am263px_soc || is_am261x_soc){
        config.push(
            {
                name: "INTC MODE",
                displayName: "INTC MODE",
                default:"mode1",
                options: [
                    {
                        name:"mode1",
                        displayName: "MII_RT_EVENT_ENABLE",
                        description:'In this mode MII_RT_EVENTS are enabled PRU-ICSS Interrupt Controller lines 32 through 55 are mapped to internal events'
                    },
                    {
                        name:"mode0",
                        displayName: "MII_RT_EVENT_DISABLE",
                        description:"In this mode MII_RT_EVENTS are NOT enabled PRU-ICSS Interrupt Controller lines 32 through 55 are mapped to external events"
                    },
                ],
            },
        )
    }
    return config
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
        }
    },

    defaultInstanceName: "CONFIG_PRU_ICSS",
    config: getConfigurables(),
    moduleInstances: moduleInstances,
    moduleStatic: {
        modules: function(inst) {
            return [{
                name: "system_common",
                moduleName: "/system_common",
            }]
        },
    },
    validate: validate,
    getInstanceConfig,
    getMdioBaseAddr,
    getClockFrequencies
};
function moduleInstances(instance) {
    let modInstances = new Array();
    if(is_am263x_soc || is_am263px_soc || is_am261x_soc){
        modInstances.push({
            name: "AdditionalICSSSettings",
            displayName: "Additional ICSS Settings",
            moduleName: '/drivers/pruicss/m_v0/pruicss_m_v0_gpio',
            useArray: true,
            minInstanceCount: 1,
            defaultInstanceCount: 1,
            maxInstanceCount: 1,
            requiredArgs: {
                instance: instance["instance"],
            }
        });
        // Interrupt Mapping:
        let submodule = "/drivers/pruicss/m_v0/icss_intc/";
        if(instance["instance"] == "ICSSM0")
        {
            if(instance["INTC MODE"] === "mode1")
                submodule += "icss0_m_v0_mode1_intc_mapping";
            else if(instance["INTC MODE"] === "mode0")
                submodule += "icss0_m_v0_mode0_intc_mapping";
            else
                submodule += "icss0_m_v0_mode1_intc_mapping";
        }
        if(instance["instance"] == "ICSSM1")
        {
            if(instance["INTC MODE"] === "mode1")
                submodule += "icss1_m_v0_mode1_intc_mapping";
            else if(instance["INTC MODE"] === "mode0")
                submodule += "icss1_m_v0_mode0_intc_mapping";
            else
                submodule += "icss1_m_v0_mode1_intc_mapping";
        }
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
function validate(inst, report) {
    common.validate.checkSameInstanceName(inst, report);
    if(is_am261x_soc){
        let r5ClockmoduleInstance = system.modules["/kernel/dpl/clock"].$static
        let r5ClockFreqInMHz = r5ClockmoduleInstance.r5ClockFreq, tempr5ClockFreqInMHz; 
        let r5ClockFreqInHz,errorMsg;
        if(r5ClockFreqInMHz == "400MHz")
        {
            r5ClockFreqInHz = 400*1000000;
            tempr5ClockFreqInMHz = "500MHz"
        }
        else
        {
            r5ClockFreqInHz = 500*1000000;
            tempr5ClockFreqInMHz = "400MHz"
        }

        if((((r5ClockFreqInHz/2)!=inst.coreClk) && inst.coreClk != 225*1000000 ))
        {
           errorMsg = "change R5F clock frequency to " + tempr5ClockFreqInMHz + " to configure Core Clock at " + inst.coreClk + " in Syconfig Clock module"
           /*report error on icss and r5f clock modules*/
           report.logError(errorMsg, r5ClockmoduleInstance);
           report.logError(errorMsg, inst); 
        } 

        if(((r5ClockFreqInHz/2)!=inst.iepClk && inst.iepClk!=225*1000000))
        {
            errorMsg = "change R5F clock frequency to " + tempr5ClockFreqInMHz + " to configure IEP Clock at " + inst.iepClk + " in Syconfig Clock module"
            /*report error on icss and r5f clock modules*/
            report.logError(errorMsg, r5ClockmoduleInstance);
            report.logError(errorMsg, inst);    
        }
    }
}

exports = pruicss_top_module;
