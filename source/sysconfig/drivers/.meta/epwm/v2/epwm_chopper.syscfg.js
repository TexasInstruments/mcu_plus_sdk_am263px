let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    if (inst.epwmChopper_enableChopper == true)
    {
        ui.epwmChopper_dutyCycle.hidden = false;
        ui.epwmChopper_clkFreq.hidden = false;
        ui.epwmChopper_oneShotWidth.hidden = false;
    }
    else
    {
        ui.epwmChopper_dutyCycle.hidden = true;
        ui.epwmChopper_clkFreq.hidden = true;
        ui.epwmChopper_oneShotWidth.hidden = true;
    }
}

let config = [
    {
        name: "epwmChopper_enableChopper",
        displayName: "Enable PWM Chopper",
        description: 'Enable chopper sub-module',
        longDescription: "PWM Chopper Enable\n\nEnables high-frequency carrier modulation.\n\nPurpose: Creates pulse train for transformer-isolated gate drivers.\n\nRequired for:\n- Transformer-coupled gate drivers\n- Optically-isolated high-voltage drives\n- Legacy pulse transformer systems\n\nNot needed (Modern):\n- Direct gate drive\n- Isolated DC-DC supplies\n- Fiber-optic drivers\n\nNote: Rarely used in modern designs",
        hidden: false,
        default: false,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmChopper_dutyCycle",
        displayName: "Chopping Clock Duty Cycle",
        description: 'Duty cycle of the chopping clock',
        longDescription: "Chopping Clock Duty Cycle\n\nOptions: 12.5% to 87.5%\n\nTypical: 50% (balanced)\n\nProvides equal on/off times for transformer balance.",
        hidden: true,
        default: device_peripheral.EPWM_ChpDutyCycle[3].name,
        options: device_peripheral.EPWM_ChpDutyCycle,
    },
    {
        name: "epwmChopper_clkFreq",
        displayName: "Chopping Clock Frequency Divider",
        description: 'Divider for chopping clock frequency',
        longDescription: "Chopping Clock Frequency\n\nChopping Freq = SYSCLKOUT / (8 × Divider)\n\nFor AM273x: SYSCLKOUT = 200 MHz\n\nDivider 1: 25.0 MHz\nDivider 2: 12.5 MHz\nDivider 3: 8.3 MHz\nDivider 4: 6.25 MHz\nDivider 5: 5.0 MHz (Standard)\nDivider 6: 4.17 MHz\nDivider 7: 3.57 MHz\nDivider 8: 3.125 MHz\n\nTypical: Divider 5 (5 MHz)",
        hidden: true,
        default: device_peripheral.EPWM_ChpClkFreq[4].name,
        options: device_peripheral.EPWM_ChpClkFreq,
    },
    {
        name: "epwmChopper_oneShotWidth",
        displayName: "One-Shot Pulse Width",
        description: 'Width of first pulse in SYSCLKOUT/8 periods (0-15)',
        longDescription: "One-Shot Pulse Width\n\nWidth of first pulse when PWM goes high.\n\nCalculation:\nWidth (ns) = Value × 40 ns (for AM273x 200 MHz)\n\nValue 0: 40 ns (minimal)\nValue 5: 200 ns (short)\nValue 10: 400 ns (standard)\nValue 15: 600 ns (long)\n\nPurpose: Initial pulse for gate driver startup\n\nTypical: 10 (400ns)",
        hidden: true,
        default: 10,
    },
];

let epwmChopperSubmodule = {
    displayName: "EPWM Chopper",
    defaultInstanceName: "EPWM_CHP",
    description: "PWM Chopper Configuration",
    config: config,
};

exports = epwmChopperSubmodule;