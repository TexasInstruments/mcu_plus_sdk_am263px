let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    if (inst.epwmActionQualifier_EPWM_AQ_OUTPUT_A_swForceActionEnable == true)
    {
        ui.epwmActionQualifier_EPWM_AQ_OUTPUT_A_swForceAction.hidden = false;
    }
    else
    {
        ui.epwmActionQualifier_EPWM_AQ_OUTPUT_A_swForceAction.hidden = true;
    }
    
    if (inst.epwmActionQualifier_EPWM_AQ_OUTPUT_B_swForceActionEnable == true)
    {
        ui.epwmActionQualifier_EPWM_AQ_OUTPUT_B_swForceAction.hidden = false;
    }
    else
    {
        ui.epwmActionQualifier_EPWM_AQ_OUTPUT_B_swForceAction.hidden = true;
    }
    
    if (inst.epwmActionQualifier_EPWM_AQ_OUTPUT_A_continousSwForceAction != "EPWM_AQ_SW_TRIG_CONT_ACTION_NOEFFECT" ||
        inst.epwmActionQualifier_EPWM_AQ_OUTPUT_B_continousSwForceAction != "EPWM_AQ_SW_TRIG_CONT_ACTION_NOEFFECT")
    {
        ui.epwmActionQualifier_continousSwForceReloadMode.hidden = false;
    }
    else
    {
        ui.epwmActionQualifier_continousSwForceReloadMode.hidden = true;
    }
}

let config = [
    {
        name: "GROUP_OUTPUT_A",
        displayName: "Output A (EPWMxA) Actions",
        description: "Configure actions on Output A for counter events",
        collapsed: false,
        config: [
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_ZERO",
                displayName: "Action on Counter = Zero",
                description: 'Action when time-base counter equals zero',
                longDescription: "Action on Counter = Zero\n\nOccurs at start of PWM period (up-count) or twice per period (up-down).\n\nCommon Pattern: Set High on Zero\nCombine with: Set Low on CMPA\nResult: Duty = CMPA / TBPRD",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_PERIOD",
                displayName: "Action on Counter = Period",
                description: 'Action when time-base counter equals period',
                longDescription: "Action on Counter = Period\n\nOccurs at end of PWM period (up-count) or peak (up-down).\n\nLess commonly used than Zero + CMPA events.",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_UP_CMPA",
                displayName: "Action on Counter = CMPA (Counting Up)",
                description: 'Action when counter equals CMPA while incrementing',
                longDescription: "Action on Counter = CMPA (Up-Count)\n\nDuty Cycle Control:\nTypical: Set Low on CMPA (up)\nCombine with: Set High on Zero\nResult: Duty = CMPA / TBPRD\n\nExample: 50% duty with TBPRD = 1000\nCMPA = 500\nZero to High, CMPA(up) to Low",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_DOWN_CMPA",
                displayName: "Action on Counter = CMPA (Counting Down)",
                description: 'Action when counter equals CMPA while decrementing',
                longDescription: "Action on Counter = CMPA (Down-Count)\n\nFor symmetric PWM in up-down mode.\n\nExample Symmetric Pattern:\nZero: Set High\nCMPA (up): Set Low\nCMPA (down): Set High\nResult: Centered pulse at CMPA",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_UP_CMPB",
                displayName: "Action on Counter = CMPB (Counting Up)",
                description: 'Action when counter equals CMPB while incrementing',
                longDescription: "Action on Counter = CMPB (Up)\n\nCross-reference control: Output A can respond to CMPB.\n\nUse for: Multi-level waveforms, complex patterns",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_ON_TIMEBASE_DOWN_CMPB",
                displayName: "Action on Counter = CMPB (Counting Down)",
                description: 'Action when counter equals CMPB while decrementing',
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
        ]
    },
    
    {
        name: "GROUP_OUTPUT_B",
        displayName: "Output B (EPWMxB) Actions",
        description: "Configure actions on Output B for counter events",
        collapsed: false,
        config: [
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_ZERO",
                displayName: "Action on Counter = Zero",
                description: 'Action when time-base counter equals zero',
                longDescription: "Output B: Action on Counter = Zero\n\nCommon Patterns:\nComplementary PWM: Output A High, Output B Low\nIndependent PWM: Different duty cycles\nParallel: Same as Output A (higher current)",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_PERIOD",
                displayName: "Action on Counter = Period",
                description: 'Action when time-base counter equals period',
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_UP_CMPA",
                displayName: "Action on Counter = CMPA (Counting Up)",
                description: 'Action when counter equals CMPA while incrementing',
                longDescription: "Output B: Action on CMPA (Up)\n\nCross-reference: Output B responds to CMPA.\n\nComplementary PWM Example:\nOutput A: Zero to High, CMPA to Low\nOutput B: CMPA to High, Period to Low\nResult: B turns on when A turns off",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_DOWN_CMPA",
                displayName: "Action on Counter = CMPA (Counting Down)",
                description: 'Action when counter equals CMPA while decrementing',
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_UP_CMPB",
                displayName: "Action on Counter = CMPB (Counting Up)",
                description: 'Action when counter equals CMPB while incrementing',
                longDescription: "Output B: Action on CMPB (Up)\n\nPrimary control for Output B duty cycle.\n\nTypical: Zero to High, CMPB to Low\nResult: Duty = CMPB / TBPRD",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_ON_TIMEBASE_DOWN_CMPB",
                displayName: "Action on Counter = CMPB (Counting Down)",
                description: 'Action when counter equals CMPB while decrementing',
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierOutput,
            },
        ]
    },
    
    {
        name: "GROUP_SW_FORCE_ONETIME",
        displayName: "Software Force - One-Time Pulse",
        description: "Trigger single output change via software",
        collapsed: true,
        config: [
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_swForceActionEnable",
                displayName: "Enable SW Force on Output A",
                description: 'Enable one-time software-triggered action',
                longDescription: "Software Force - One-Time (Output A)\n\nTriggers a single output action via software.\n\nUse for:\n- Testing action configuration\n- Manual pulse generation\n- Calibration procedures\n\nExecutes during Drivers_epwmOpen() or when called in runtime code.",
                hidden: false,
                default: false,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_swForceAction",
                displayName: "Output A - SW Force Action",
                description: 'Action to apply on Output A',
                longDescription: "SW Force Action: Do Nothing, Low, High, or Toggle\n\nExecutes once during initialization or manual call.",
                hidden: true,
                default: device_peripheral.EPWM_ActionQualifierSWOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierSWOutput,
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_swForceActionEnable",
                displayName: "Enable SW Force on Output B",
                description: 'Enable one-time software-triggered action',
                hidden: false,
                default: false,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_swForceAction",
                displayName: "Output B - SW Force Action",
                description: 'Action to apply on Output B',
                hidden: true,
                default: device_peripheral.EPWM_ActionQualifierSWOutput[0].name,
                options: device_peripheral.EPWM_ActionQualifierSWOutput,
            },
        ]
    },
    
    {
        name: "GROUP_SW_FORCE_CONTINUOUS",
        displayName: "Software Force - Continuous Override",
        description: "Continuously force output to fixed state",
        collapsed: true,
        config: [
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_A_continousSwForceAction",
                displayName: "Output A - Continuous Force",
                description: 'Continuously force Output A to specific state',
                longDescription: "Software Force - Continuous (Output A)\n\nPermanently overrides output, disabling all counter events.\n\nUse for:\n- Emergency stop (force low)\n- Forced run state (force high)\n- Disable PWM safely (force low, counter still runs)\n\nApplication must explicitly clear to resume normal operation.",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierContForce[0].name,
                options: device_peripheral.EPWM_ActionQualifierContForce,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmActionQualifier_EPWM_AQ_OUTPUT_B_continousSwForceAction",
                displayName: "Output B - Continuous Force",
                description: 'Continuously force Output B to specific state',
                longDescription: "Software Force - Continuous (Output B)\n\nSame as Output A continuous force.\n\nTypical: Force both A and B low for emergency stop.",
                hidden: false,
                default: device_peripheral.EPWM_ActionQualifierContForce[0].name,
                options: device_peripheral.EPWM_ActionQualifierContForce,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmActionQualifier_continousSwForceReloadMode",
                displayName: "Continuous Force Reload Mode",
                description: 'When to apply continuous force action change',
                longDescription: "Continuous Force Shadow Load\n\nLoad on Counter = Zero: Change at cycle start\nLoad on Counter = Period: Change at cycle end\nLoad on Zero or Period: Change at either boundary\nLoad Immediately: Emergency stops (no wait)\n\nRecommendation: Use Immediate for emergency stops",
                hidden: true,
                default: device_peripheral.EPWM_AqCsfrcRegReload[3].name,
                options: device_peripheral.EPWM_AqCsfrcRegReload,
            },
        ]
    },
];

let epwmActionQualifierSubmodule = {
    displayName: "EPWM Action Qualifier",
    defaultInstanceName: "EPWM_AQ",
    description: "PWM Output Action Configuration",
    config: config,
};

exports = epwmActionQualifierSubmodule;