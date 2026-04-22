let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    if (inst.epwmTimebase_phaseEnable == true)
    {
        ui.epwmTimebase_phaseShift.hidden = false;
        ui.epwmTimebase_counterModeAfterSync.hidden = false;
        ui.epwmTimebase_phaseShiftPercent.hidden = false;
    }
    else
    {
        ui.epwmTimebase_phaseShift.hidden = true;
        ui.epwmTimebase_counterModeAfterSync.hidden = true;
        ui.epwmTimebase_phaseShiftPercent.hidden = true;
    }

    if (inst.epwmTimebase_periodLoadMode == "EPWM_SHADOW_REG_CTRL_ENABLE")
    {
        ui.epwmTimebase_periodLoadEvent.hidden = false;
    }
    else
    {
        ui.epwmTimebase_periodLoadEvent.hidden = true;
    }

    if (inst.epwmTimebase_counterMode == "EPWM_TB_COUNTER_DIR_UP_DOWN")
    {
        ui.epwmTimebase_counterModeAfterSync.hidden = false;
    }
    else
    {
        ui.epwmTimebase_counterModeAfterSync.hidden = true;
    }
}

function getTBCLK(inst) {
    let moduleClk = 200000000;
    
    let hspDiv = 1;
    if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_2") hspDiv = 2;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_4") hspDiv = 4;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_6") hspDiv = 6;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_8") hspDiv = 8;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_10") hspDiv = 10;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_12") hspDiv = 12;
    else if(inst.epwmTimebase_hsClockDiv == "EPWM_HSCLOCK_DIVIDER_14") hspDiv = 14;
    
    let clkDiv = 1;
    if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_2") clkDiv = 2;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_4") clkDiv = 4;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_8") clkDiv = 8;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_16") clkDiv = 16;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_32") clkDiv = 32;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_64") clkDiv = 64;
    else if(inst.epwmTimebase_clockDiv == "EPWM_CLOCK_DIVIDER_128") clkDiv = 128;
    
    return moduleClk / (hspDiv * clkDiv);
}

function getPWMFreq(inst) {
    let tbClk = getTBCLK(inst);
    let period = inst.epwmTimebase_period;
    
    if(period == 0) return 0;
    
    if(inst.epwmTimebase_counterMode == "EPWM_TB_COUNTER_DIR_UP_DOWN") {
        return tbClk / (2 * period);
    } else {
        return tbClk / (period + 1);
    }
}

let config = [
    {
        name: "GROUP_CLOCK",
        displayName: "Clock Configuration",
        collapsed: false,
        config: [
            {
                name: "epwmTimebase_hsClockDiv",
                displayName: "High Speed Clock Divider (HSPCLKDIV)",
                description: 'High-speed time-base clock pre-scale value',
                longDescription: "High Speed Clock Divider: TBCLK = SYSCLKOUT / (HSPCLKDIV × CLKDIV)\n\nFor AM273x: SYSCLKOUT = 200 MHz\n\nTypical Values:\n- 1: For high-frequency PWM (> 1 MHz)\n- 2: For medium-high frequency (500 kHz - 1 MHz)\n- 4-6: For medium frequency (100 kHz - 500 kHz)\n- 8-14: For low frequency (< 100 kHz)",
                hidden: false,
                default: device_peripheral.EPWM_HSClockDivider[0].name,
                options: device_peripheral.EPWM_HSClockDivider,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_clockDiv",
                displayName: "Clock Divider (CLKDIV)",
                description: 'Time base clock pre-scale value',
                longDescription: "Clock Divider: TBCLK = SYSCLKOUT / (HSPCLKDIV × CLKDIV)\n\nPower of 2 scaling: 1, 2, 4, 8, 16, 32, 64, 128\n\nExample: Very low frequency PWM (100 Hz)\n- HSPCLKDIV = 14, CLKDIV = 128\n- TBCLK = 200 MHz / (14 × 128) = 111.6 kHz",
                hidden: false,
                default: device_peripheral.EPWM_ClockDivider[0].name,
                options: device_peripheral.EPWM_ClockDivider,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_tbClkCalc",
                displayName: "Calculated TBCLK (Hz)",
                description: "Calculated time base clock frequency",
                hidden: false,
                default: 200000000,
                getValue: (inst) => { return getTBCLK(inst); },
                readOnly: true
            },
        ]
    },
    
    {
        name: "GROUP_COUNTER",
        displayName: "Counter Configuration",
        collapsed: false,
        config: [
            {
                name: "epwmTimebase_counterMode",
                displayName: "Counter Mode",
                description: 'Direction mode of the Time Base Counter',
                longDescription: "Counter Mode options:\n\nUp Count:\n- Counter: 0 to TBPRD to 0\n- PWM Freq = TBCLK / (TBPRD + 1)\n- Use for: Simple PWM generation\n\nDown Count:\n- Counter: TBPRD to 0 to TBPRD\n- PWM Freq = TBCLK / (TBPRD + 1)\n- Use for: Specialized timing\n\nUp-Down Count (Recommended for Motors):\n- Counter: 0 to TBPRD to 0\n- PWM Freq = TBCLK / (2 × TBPRD)\n- Use for: Motor control (symmetric PWM, reduced harmonics)\n\nStop-Freeze:\n- Counter stopped\n- Use for: Configuration before enabling",
                hidden: false,
                default: "EPWM_TB_COUNTER_DIR_STOP",
                options: device_peripheral.EPWM_TimeBaseCountMode,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_period",
                displayName: "Time Base Period (TBPRD)",
                description: 'Period register value (0-65535)',
                longDescription: "Time Base Period determines PWM frequency.\n\nFor Up or Down Count:\nPWM Freq = TBCLK / (TBPRD + 1)\n\nFor Up-Down Count:\nPWM Freq = TBCLK / (2 × TBPRD)\n\nExample: 10 kHz Motor Control (Up-Down, TBCLK = 200 MHz)\nTBPRD = 200 MHz / (2 × 10 kHz) = 10,000\n\nValid Range: 0 - 65535",
                hidden: false,
                default: 0,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_pwmFreqCalc",
                displayName: "Calculated PWM Frequency (Hz)",
                description: "Calculated PWM output frequency",
                hidden: false,
                default: 0,
                getValue: (inst) => { return getPWMFreq(inst); },
                readOnly: true
            },
            {
                name: "epwmTimebase_counterValue",
                displayName: "Counter Initial Value (TBCTR)",
                description: 'Initial value for the Time Base Counter (0-65535)',
                longDescription: "Counter Initial Value\n\nStarting value when EPWM is initialized.\n\nTypical: 0 (start from beginning of cycle)\n\nSpecial Cases:\n- Mid-cycle startup: Set to TBPRD/2 for testing\n- Phase offset without SYNC: Manually offset counter\n- Debugging: Start from specific point",
                hidden: false,
                default: 0,
            },
            {
                name: "epwmTimebase_counterModeAfterSync",
                displayName: 'Count Direction After Sync',
                description: 'Counter direction after loading phase value',
                longDescription: "Counter Direction After SYNC (Up-Down mode only)\n\nWhen SYNCIN pulse occurs:\n1. Counter loads TBPHS value\n2. Counter resumes in this direction\n\nCount Up After Sync: Normal forward phase\nCount Down After Sync: Inverted phase (rare)",
                hidden: true,
                default: device_peripheral.EPWM_SyncCountMode[1].name,
                options: device_peripheral.EPWM_SyncCountMode,
            },
        ]
    },
    
    {
        name: "GROUP_PERIOD_SHADOW",
        displayName: "Period Shadow Load",
        collapsed: true,
        config: [
            {
                name: "epwmTimebase_periodLoadMode",
                displayName: "Period Load Mode",
                description: 'Period register access mode',
                longDescription: "Period Shadow Load Mode\n\nShadow Mode (Recommended):\n- Prevents glitches during frequency changes\n- Use for: Dynamic frequency control\n\nDirect Mode:\n- Immediate effect, may cause glitches\n- Use for: Static configurations only",
                hidden: false,
                default: device_peripheral.EPWM_PeriodLoadMode[0].name,
                options: device_peripheral.EPWM_PeriodLoadMode,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_periodLoadEvent",
                displayName: "Period Shadow Load Event",
                description: 'Event that triggers shadow to active transfer',
                longDescription: "Period Shadow Load Event\n\nCounter = Zero: Load at start of cycle\nCounter = Zero + SYNC: Load at start or on sync\nSYNC Only: Load only when sync occurs",
                hidden: true,
                default: device_peripheral.EPWM_PeriodShadowLoadMode[0].name,
                options: device_peripheral.EPWM_PeriodShadowLoadMode,
            },
            {
                name: "epwmTimebase_periodGld",
                displayName: "Enable Global Load for Period",
                description: 'Use global load configuration for PRD',
                longDescription: "Global Load for Period\n\nCoordinates period updates with other register changes.\nSynchronizes all parameters to change together.",
                hidden: false,
                default: false,
            },
            {
                name: "epwmTimebase_periodLink",
                displayName: "Period Link",
                description: 'Link period register with another EPWM',
                longDescription: "Period Register Linking\n\nSimultaneous write to multiple EPWM period registers.\nMaintains fixed frequency relationship.\n\nUse for: Interleaved converters, multi-phase systems",
                hidden: false,
                default: device_peripheral.EPWM_CurrentLink[0].name,
                options: device_peripheral.EPWM_CurrentLink,
            },
        ]
    },
    
    {
        name: "GROUP_SYNC_OUT",
        displayName: "Synchronization Output (Master Mode)",
        collapsed: false,
        config: [
            {
                name: "epwmTimebase_syncOutMode",
                displayName: "Sync Out Mode",
                description: 'Event that generates SYNCOUT pulse',
                longDescription: "Sync Out Mode (Master Configuration)\n\nControls when this EPWM generates sync pulses.\n\nDisable: No sync output\nPass-through SYNCIN: Forwards input to output\nCounter = Zero: Master mode (generates sync at period start)\nCounter = CMPB: Custom sync timing\n\nExample 3-Phase Motor:\nEPWMA (Master): syncOutMode = Counter = Zero\nEPWMB (Slave): phaseEnable = true, TBPHS = 833 (120 deg)\nEPWMC (Slave): phaseEnable = true, TBPHS = 1666 (240 deg)",
                hidden: false,
                default: device_peripheral.EPWM_SYNC_OUT_PULSE_ON[0].name,
                options: device_peripheral.EPWM_SYNC_OUT_PULSE_ON,
            },
            {
                name: "epwmTimebase_oneShotSyncOutTrigger",
                displayName: "One-Shot Sync Out Trigger",
                description: 'Trigger for one-shot sync event',
                longDescription: "One-Shot Sync Trigger\n\nOSHT SYNC: Triggered by one-shot trip zone\nOSHT RELOAD: Triggered when one-shot counter reloads\n\nUsed for single synchronization events.",
                hidden: false,
                default: device_peripheral.EPWM_OneShotSyncOutTrigger[0].name,
                options: device_peripheral.EPWM_OneShotSyncOutTrigger,
            },
        ]
    },
    
    {
        name: "GROUP_SYNC_IN",
        displayName: "Phase Synchronization (Slave Mode)",
        collapsed: false,
        config: [
            {
                name: "epwmTimebase_phaseEnable",
                displayName: "Enable Phase Shift Load on SYNCIN",
                description: 'Load phase value when SYNCIN pulse occurs',
                longDescription: "Phase Synchronization (Slave Mode)\n\nWhen enabled, this EPWM loads TBPHS value on SYNCIN.\n\nOperation:\n1. EPWM runs normally\n2. SYNCIN pulse arrives from master\n3. Counter loads TBPHS value\n4. Counter resumes in specified direction\n5. Phase offset established\n\nUse Cases:\n- 3-Phase Motor: 0 deg, 120 deg, 240 deg offsets\n- Interleaved Converters: Reduced ripple current\n- Synchronized Sampling: Precise ADC timing",
                hidden: false,
                default: false,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmTimebase_phaseShift",
                displayName: 'Phase Shift Value (TBPHS)',
                description: 'Counter value to load on SYNCIN (0-65535)',
                longDescription: "Phase Shift Value (TBPHS)\n\nCalculation Formulas:\n\nFrom Electrical Angle:\nTBPHS = (Angle / 360) × TBPRD\n\nFrom Time Delay:\nTBPHS = Delay × TBCLK\n\nFrom Percentage:\nTBPHS = (Percentage / 100) × TBPRD\n\nExample 3-Phase Motor (TBPRD = 2500):\nPhase U: 0 deg = TBPHS 0\nPhase V: 120 deg = TBPHS 833\nPhase W: 240 deg = TBPHS 1666",
                hidden: true,
                default: 0,
            },
            {
                name: "epwmTimebase_phaseShiftPercent",
                displayName: "Phase Shift (% of Period)",
                description: "Phase shift as percentage of period",
                hidden: true,
                default: 0,
                getValue: (inst) => {
                    if(inst.epwmTimebase_period == 0) return 0;
                    return parseFloat(((inst.epwmTimebase_phaseShift / inst.epwmTimebase_period) * 100).toFixed(2));
                },
                readOnly: true
            },
        ]
    },
    
    {
        name: "GROUP_ADVANCED",
        displayName: "Advanced Configuration",
        collapsed: true,
        config: [
            {
                name: "epwmTimebase_emulationMode",
                displayName: "Emulation Mode",
                description: 'Behavior during debugger halt',
                longDescription: "Emulation Mode\n\nStop After Next Increment: Immediate stop on breakpoint\nStop After Complete Cycle: Finishes current period\nFree Run (Recommended for Motors): PWM continues during debug\n\nSafety: Always use Free Run for motor and power applications!",
                hidden: false,
                default: device_peripheral.EPWM_EmulationMode[2].name,
                options: device_peripheral.EPWM_EmulationMode
            },
            {
                name: "epwmTimebase_forceSyncPulse",
                displayName: 'Force Sync Pulse (One-Time)',
                description: 'Software-triggered sync pulse',
                longDescription: "Force Sync Pulse\n\nTriggers a single SYNC pulse during initialization.\n\nUse for:\n- Initial alignment at startup\n- Testing sync chain\n- Manual trigger for debug\n\nNote: One-time action during Drivers_epwmOpen()",
                hidden: false,
                default: false,
            },
            {
                name: "hrpwm_syncSource",
                displayName: "PWMSYNC Source for Peripherals",
                description: 'Source of EPWMSYNCPER signal to CMPSS/GPDAC',
                longDescription: "PWM Sync Peripheral Source\n\nSelects event that generates EPWMSYNCPER signal to:\n- CMPSS (Comparator SubSystem)\n- GPDAC (General Purpose DAC)\n\nTypical: Match to main PWM event (Zero or Period)",
                hidden: false,
                default: device_peripheral.HRPWM_SyncPulseSource[0].name,
                options: device_peripheral.HRPWM_SyncPulseSource
            },
        ]
    },
];

let epwmTimebaseSubmodule = {
    displayName: "EPWM Time Base",
    defaultInstanceName: "EPWM_TB",
    description: "Time Base Counter Configuration",
    config: config,
};

exports = epwmTimebaseSubmodule;