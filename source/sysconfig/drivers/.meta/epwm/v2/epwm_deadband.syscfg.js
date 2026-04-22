let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    if (inst.epwmDeadband_enableDeadBand == true)
    {
        ui.epwmDeadband_inputMode.hidden = false;
        ui.epwmDeadband_outputMode.hidden = false;
        ui.epwmDeadband_polaritySelect.hidden = false;
        ui.epwmDeadband_risingEdgeDelay.hidden = false;
        ui.epwmDeadband_fallingEdgeDelay.hidden = false;
        ui.epwmDeadband_risingEdgeDelayNS.hidden = false;
        ui.epwmDeadband_fallingEdgeDelayNS.hidden = false;
    }
    else
    {
        ui.epwmDeadband_inputMode.hidden = true;
        ui.epwmDeadband_outputMode.hidden = true;
        ui.epwmDeadband_polaritySelect.hidden = true;
        ui.epwmDeadband_risingEdgeDelay.hidden = true;
        ui.epwmDeadband_fallingEdgeDelay.hidden = true;
        ui.epwmDeadband_risingEdgeDelayNS.hidden = true;
        ui.epwmDeadband_fallingEdgeDelayNS.hidden = true;
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

function getDelayNS(cycles, tbClk) {
    if(tbClk == 0) return 0;
    return (cycles / tbClk) * 1e9;
}

let config = [
    {
        name: "epwmDeadband_enableDeadBand",
        displayName: "Enable Dead-Band Generator",
        description: 'Enable dead-band generation for outputs',
        longDescription: "Dead-Band Generator Enable\n\nPrevents shoot-through in H-bridge and inverter circuits.\n\nRequired for:\n- Full H-bridge motor drives\n- Half-bridge power supplies\n- 3-phase inverters\n- Complementary switches\n\nNot needed for:\n- Single-ended PWM\n- Independent outputs\n- Non-power applications\n\nTypical dead-time: 100-500ns for MOSFETs, 1-2us for IGBTs",
        hidden: false,
        default: false,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmDeadband_inputMode",
        displayName: "Dead-Band Input Mode",
        description: 'Select input source for dead-band delays',
        longDescription: "Dead-Band Input Mode\n\nEPWMxA for Both: Standard complementary PWM\nEPWMxB for RED, EPWMxA for FED: Cross-coupled\nEPWMxA for RED, EPWMxB for FED: Independent control\nEPWMxB for Both: Output B as primary\n\nMost Common: EPWMxA for both",
        hidden: true,
        default: device_peripheral.EPWM_DeadBandInputMode[0].name,
        options: device_peripheral.EPWM_DeadBandInputMode,
    },
    {
        name: "epwmDeadband_outputMode",
        displayName: "Dead-Band Output Mode",
        description: 'Select which delays are active',
        longDescription: "Dead-Band Output Mode\n\nBypass: Dead-band disabled\nRising Edge Only: Delayed turn-on only\nFalling Edge Only: Delayed turn-off only\nBoth Delays (Standard): Full protection\n\nRecommendation: Use Both for maximum protection",
        hidden: true,
        default: device_peripheral.EPWM_DeadBandOutputMode[3].name,
        options: device_peripheral.EPWM_DeadBandOutputMode,
    },
    {
        name: "epwmDeadband_polaritySelect",
        displayName: "Dead-Band Polarity",
        description: 'Invert one or both outputs',
        longDescription: "Dead-Band Polarity Control\n\nActive High: No inversion (standard)\nActive Low Complementary: Invert A only\nActive High Complementary: Invert B only\nActive Low: Invert both\n\nTypical: Active High (no inversion)",
        hidden: true,
        default: device_peripheral.EPWM_DeadBandPolarity[0].name,
        options: device_peripheral.EPWM_DeadBandPolarity,
    },
    {
        name: "epwmDeadband_risingEdgeDelay",
        displayName: "Rising Edge Delay (cycles)",
        description: 'Rising edge delay in TBCLK cycles (0-1023)',
        longDescription: "Rising Edge Delay (RED)\n\nDelay applied to rising edges (off to on transitions).\n\nCalculation:\nDelay (ns) = Cycles × (1 / TBCLK) × 1e9\n\nExample: 200ns with 200 MHz TBCLK\nCycles = 200ns × 200MHz = 40\n\nCommon Values:\n50ns = 10 cycles (GaN)\n100ns = 20 cycles (Fast MOSFET)\n200ns = 40 cycles (Standard MOSFET)\n500ns = 100 cycles (IGBT low current)\n1000ns = 200 cycles (IGBT high current)\n\nValid Range: 0-1023",
        hidden: true,
        default: 0,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmDeadband_risingEdgeDelayNS",
        displayName: "Rising Edge Delay (nanoseconds)",
        description: "Calculated rising edge delay in nanoseconds",
        hidden: true,
        default: 0,
        getValue: (inst) => {
            let tbClk = getTBCLK(inst);
            return parseFloat(getDelayNS(inst.epwmDeadband_risingEdgeDelay, tbClk).toFixed(2));
        },
        readOnly: true
    },
    {
        name: "epwmDeadband_fallingEdgeDelay",
        displayName: "Falling Edge Delay (cycles)",
        description: 'Falling edge delay in TBCLK cycles (0-1023)',
        longDescription: "Falling Edge Delay (FED)\n\nDelay applied to falling edges (on to off transitions).\n\nTypical: Set same as Rising Edge Delay for symmetric dead-time.\n\nAsymmetric Example:\nRED = 20 cycles (100ns) - faster turn-on\nFED = 40 cycles (200ns) - slower turn-off\n\nValid Range: 0-1023",
        hidden: true,
        default: 0,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmDeadband_fallingEdgeDelayNS",
        displayName: "Falling Edge Delay (nanoseconds)",
        description: "Calculated falling edge delay in nanoseconds",
        hidden: true,
        default: 0,
        getValue: (inst) => {
            let tbClk = getTBCLK(inst);
            return parseFloat(getDelayNS(inst.epwmDeadband_fallingEdgeDelay, tbClk).toFixed(2));
        },
        readOnly: true
    },
];

let epwmDeadbandSubmodule = {
    displayName: "EPWM Dead-Band Generator",
    defaultInstanceName: "EPWM_DB",
    description: "Dead-Band Configuration",
    config: config,
};

exports = epwmDeadbandSubmodule;