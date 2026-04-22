let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    let hasTripSource = (inst.epwmTripZone_oneShotSource != 0 || 
                         inst.epwmTripZone_cbcSource != 0);
    
    ui.epwmTripZone_tzActionA.hidden = !hasTripSource;
    ui.epwmTripZone_tzActionB.hidden = !hasTripSource;
    ui.epwmTripZone_enableOSTIntr.hidden = !hasTripSource;
    ui.epwmTripZone_enableCBCIntr.hidden = !hasTripSource;
}

let config = [
    {
        name: "epwmTripZone_oneShotSource",
        displayName: "One-Shot Trip Source (Pin Number)",
        description: 'Trip zone pin for one-shot trip (0 = disabled, 1-6 = TZ1-TZ6)',
        longDescription: "One-Shot Trip Zone (Latched Fault)\n\nRequires software clear.\n\nPurpose: Protection against major faults\n\nBehavior:\n1. Trip signal asserted\n2. PWM outputs take trip action\n3. Trip latches - stays in trip state\n4. Software must clear to resume\n5. Interrupt notifies CPU\n\nPin Selection:\n0: Disabled\n1-6: TZ1-TZ6 pins\n\nTypical Sources:\n- Overcurrent (sustained)\n- Overtemperature\n- Supply voltage fault\n- Emergency stop button\n\nSoftware Clear:\nEPWM_tzEventStatusClear(base, EPWM_TZ_STS_FLG_OST);",
        hidden: false,
        default: 0,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmTripZone_cbcSource",
        displayName: "Cycle-by-Cycle Trip Source (Pin Number)",
        description: 'Trip zone pin for CBC trip (0 = disabled, 1-6 = TZ1-TZ6)',
        longDescription: "Cycle-by-Cycle Trip Zone (Auto-clearing)\n\nResumes next PWM cycle.\n\nPurpose: Protection against transient faults\n\nBehavior:\n1. Trip signal asserted\n2. PWM outputs take trip action\n3. Auto-clears at counter = zero\n4. If fault cleared: PWM resumes\n5. If fault persists: Trips again\n\nTypical Sources:\n- Peak current limiting\n- Instantaneous overcurrent\n- IGBT desaturation\n- Dynamic load regulation\n\nComparison:\nOST: Manual clear, major faults\nCBC: Auto-clear, transient faults",
        hidden: false,
        default: 0,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmTripZone_tzActionA",
        displayName: "Trip Action on Output A",
        description: 'Action taken on Output A when trip occurs',
        longDescription: "Trip Zone Action - Output A\n\nHigh Impedance (Safest): Output driver disabled, pin high-Z\n  Use for: Motor drives (coast to stop), maximum safety\n\nForce High: Output forced high, driver active\n  Use for: Active-high disable signals\n\nForce Low: Output forced low, driver active\n  Use for: Active-low disable, brake choppers\n\nDo Nothing: Output unchanged (monitoring only)\n  Use for: Trip detection without output change\n\nRecommendation: High Impedance for motor drives",
        hidden: true,
        default: device_peripheral.EPWM_TripZoneAction[0].name,
        options: device_peripheral.EPWM_TripZoneAction,
    },
    {
        name: "epwmTripZone_tzActionB",
        displayName: "Trip Action on Output B",
        description: 'Action taken on Output B when trip occurs',
        longDescription: "Trip Zone Action - Output B\n\nTypical: Match Output A action for complementary outputs.\n\nExample H-bridge:\nOutput A: High-Z (high-side off)\nOutput B: High-Z (low-side off)\nResult: Both switches safely off",
        hidden: true,
        default: device_peripheral.EPWM_TripZoneAction[0].name,
        options: device_peripheral.EPWM_TripZoneAction,
    },
    {
        name: "epwmTripZone_enableOSTIntr",
        displayName: "Enable One-Shot Trip Interrupt",
        description: 'Generate CPU interrupt on one-shot trip',
        longDescription: "One-Shot Trip Interrupt\n\nGenerates CPU interrupt when OST trip occurs.\n\nPurpose: Notify software of major fault\n\nISR Tasks:\n1. Read trip status\n2. Identify fault source\n3. Take corrective action\n4. Clear interrupt flag\n5. Do NOT clear trip until safe\n\nRecommendation: Always enable for OST (safety-critical)",
        hidden: true,
        default: true,
    },
    {
        name: "epwmTripZone_enableCBCIntr",
        displayName: "Enable Cycle-by-Cycle Trip Interrupt",
        description: 'Generate CPU interrupt on CBC trip',
        longDescription: "Cycle-by-Cycle Trip Interrupt\n\nGenerates CPU interrupt when CBC trip occurs.\n\nEnable If:\n- Need to count CBC events\n- Want to log transient faults\n- Implementing adaptive limiting\n\nDisable If (Typical):\n- CBC events frequent (normal limiting)\n- Interrupt overhead too high\n- Trip action alone sufficient\n\nPerformance: CBC can trip every cycle (10k trips/sec at 10kHz PWM)\n\nRecommendation: Disable in production unless logging required",
        hidden: true,
        default: false,
    },
];

let epwmTripZoneSubmodule = {
    displayName: "EPWM Trip Zone",
    defaultInstanceName: "EPWM_TZ",
    description: "Trip Zone Fault Protection Configuration",
    config: config,
};

exports = epwmTripZoneSubmodule;