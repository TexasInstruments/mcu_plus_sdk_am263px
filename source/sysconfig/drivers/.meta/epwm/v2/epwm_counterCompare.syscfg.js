let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    // Show/hide CMPA shadow load mode
    if (inst.epwmCounterCompare_enableShadowLoadModeCMPA == true)
    {
        ui.epwmCounterCompare_shadowLoadModeCMPA.hidden = false;
    }
    else
    {
        ui.epwmCounterCompare_shadowLoadModeCMPA.hidden = true;
    }
    
    // Show/hide CMPB shadow load mode
    if (inst.epwmCounterCompare_enableShadowLoadModeCMPB == true)
    {
        ui.epwmCounterCompare_shadowLoadModeCMPB.hidden = false;
    }
    else
    {
        ui.epwmCounterCompare_shadowLoadModeCMPB.hidden = true;
    }
    
    // Show/hide CMPC shadow load mode
    if (inst.epwmCounterCompare_enableShadowLoadModeCMPC == true)
    {
        ui.epwmCounterCompare_shadowLoadModeCMPC.hidden = false;
    }
    else
    {
        ui.epwmCounterCompare_shadowLoadModeCMPC.hidden = true;
    }
    
    // Show/hide CMPD shadow load mode
    if (inst.epwmCounterCompare_enableShadowLoadModeCMPD == true)
    {
        ui.epwmCounterCompare_shadowLoadModeCMPD.hidden = false;
    }
    else
    {
        ui.epwmCounterCompare_shadowLoadModeCMPD.hidden = true;
    }
}

let config = [
    // ========================================
    // COUNTER COMPARE A
    // ========================================
    {
        name: "GROUP_CMPA",
        displayName: "Counter Compare A",
        collapsed: false,
        config: [
            {
                name: "epwmCounterCompare_cmpA",
                displayName: "Counter Compare A Value (CMPA)",
                description: 'Compare value for Counter Compare A (0-65535)',
                longDescription: 
`# Counter Compare A (CMPA)

When time-base counter equals this value, CMPA event occurs.

## Primary Use: PWM Duty Cycle Control

### Duty Cycle Calculation:

**For Up-Count or Down-Count Mode:**

Duty Cycle = (CMPA / TBPRD) × 100%

Example: TBPRD = 2000, CMPA = 1000
Duty = (1000 / 2000) × 100% = 50%

**For Up-Down Count Mode:**

Duty Cycle = ((TBPRD - CMPA) / TBPRD) × 100%

Example: TBPRD = 1000, CMPA = 750
Duty = ((1000 - 750) / 1000) × 100% = 25%

## Typical Action Qualifier Setup (Up-Count):

Zero: Set Output HIGH
CMPA (up): Set Output LOW
Result: Duty = CMPA / TBPRD

## Common Applications:

**Motor Speed Control**:
- CMPA determines average voltage
- Higher CMPA = Higher duty = Higher speed
- Real-time update via EPWM_counterComparatorCfg()

**LED Brightness**:
- CMPA sets brightness level
- 0 = Off, TBPRD = Max brightness

**Power Supply Regulation**:
- CMPA adjusted by control loop
- Maintains output voltage/current

## Valid Range: 0 - 65535

**Practical Limits**:
- Minimum: ~1% of TBPRD (driver turn-on time)
- Maximum: ~99% of TBPRD (driver turn-off time)

**Dead-time consideration**:
If using dead-band, effective duty is reduced by dead-time.
`,
                hidden: false,
                default: 0,
            },
            {
                name: "epwmCounterCompare_cmpAGld",
                displayName: "Enable CMPA Global Load",
                description: 'Use global load configuration for CMPA',
                longDescription: 
`# CMPA Global Load

Coordinates CMPA updates with other register changes.

When enabled:
- CMPA shadow to active waits for global load trigger
- Synchronizes with period/CMPB/deadband updates
- Ensures all parameters change together

**Use Cases**:
- Simultaneous duty and frequency changes
- Coordinated multi-parameter updates
- Glitch-free transitions

**Note**: Requires global load module configuration (advanced).
`,
                hidden: false,
                default: false,
            },
            {
                name: "epwmCounterCompare_enableShadowLoadModeCMPA",
                displayName: "Enable CMPA Shadow Mode",
                description: 'Use shadow register for CMPA',
                longDescription: 
`# CMPA Shadow Register

**Shadow Mode (Recommended)**:
- Writes go to shadow register
- Transfer to active at specified event
- Prevents glitches during duty cycle changes
- **Use for**: Dynamic duty cycle control

**Direct Mode**:
- Writes immediately affect active register
- Can cause waveform glitches mid-cycle
- Faster response (no wait for event)
- **Use for**: Static configurations only

## When to use Direct Mode:
- One-time duty setup during initialization
- Duty never changes during operation
- Absolute minimum latency required

## When to use Shadow Mode:
- Motor speed control (frequent duty changes)
- Dynamic load response
- Any application where glitches are unacceptable

**Recommendation**: Always use Shadow Mode in production.
`,
                hidden: false,
                default: true,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmCounterCompare_shadowLoadModeCMPA",
                displayName: "CMPA Shadow Load Mode",
                description: 'Event that triggers shadow to active transfer',
                longDescription: 
`# CMPA Shadow Load Event

When to transfer shadow register to active register.

## Options:

### Load on Counter = Zero (Most Common)

Occurs: Start of PWM cycle
Use for: Up-count or down-count mode
Result: Duty change takes effect at cycle start

**Example**: Motor speed control
- New duty calculated in interrupt
- Shadow write: CMPA = new_duty
- Load event: Counter = Zero
- Result: Smooth speed change at cycle boundary

### Load on Counter = Period

Occurs: End of PWM cycle (up-count peak)
Use for: Down-count mode, or end-of-cycle updates
Result: Duty change at cycle end

### Load on Counter = Zero or Period (Up-Down Mode)

Occurs: Twice per period (start and peak)
Use for: Up-down count mode (symmetric PWM)
Result: Faster response, loads twice per period

**Example**: 10 kHz PWM (100 us period)
- Up-down mode
- Load: Zero or Period
- Update rate: 20 kHz (every 50 us)
- Result: Faster control response

### Freeze (No Load)

Shadow never transfers to active
Use for: Holding duty constant
Manual transfer: Software force

## Selection Guide:

| Counter Mode | Recommended Load Mode | Reason |
|--------------|----------------------|---------|
| Up-Count | Counter = Zero | Start of cycle |
| Down-Count | Counter = Period | End of cycle |
| Up-Down | Counter = Zero or Period | Twice per period |

**Tip**: Match load mode to your counter mode for cleanest operation.
`,
                hidden: true,
                default: device_peripheral.EPWM_CounterCompareLoadMode[0].name,
                options: device_peripheral.EPWM_CounterCompareLoadMode,
            },
            {
                name: "epwmCounterCompare_cmpALink",
                displayName: "CMPA Link",
                description: 'Link CMPA with another EPWM',
                longDescription: 
`# CMPA Register Linking

Simultaneously update CMPA of multiple EPWMs.

## Purpose:
Gang control - one write updates multiple EPWMs.

## Use Cases:

**Parallel Converters** (same duty):

EPWMA: cmpALink = Disable (master)
EPWMB: cmpALink = EPWM_A (slave)
EPWMC: cmpALink = EPWM_A (slave)

Result: Write to EPWMA.CMPA updates all three
        Maintains identical duty cycles

**3-Phase Motor** (balanced control):

All phases linked to master
Change master duty - all phases follow
Maintains balanced 3-phase operation

**Multi-Module Power Supply**:

Multiple boost stages linked
One control loop updates all
Load sharing maintained

## When NOT to link:
- Independent duty cycles needed
- Different operating modes
- Separate control loops

**Default**: Disable (independent operation)
`,
                hidden: false,
                default: device_peripheral.EPWM_CurrentLink[0].name,
                options: device_peripheral.EPWM_CurrentLink,
            },
        ]
    },
    
    // ========================================
    // COUNTER COMPARE B
    // ========================================
    {
        name: "GROUP_CMPB",
        displayName: "Counter Compare B",
        collapsed: false,
        config: [
            {
                name: "epwmCounterCompare_cmpB",
                displayName: "Counter Compare B Value (CMPB)",
                description: 'Compare value for Counter Compare B (0-65535)',
                longDescription: 
`# Counter Compare B (CMPB)

Independent compare value for second PWM output or timing.

## Common Uses:

### 1. Complementary PWM (H-Bridge)

Output A: Duty controlled by CMPA
Output B: Duty controlled by CMPB
Result: Two independent or complementary outputs

**Example**: H-bridge motor drive
- CMPA = 600 (60% duty on high-side)
- CMPB = 1400 (40% duty on low-side)
- With dead-band: Safe complementary switching

### 2. Dual Independent Outputs

Output A: LED dimming (CMPA)
Output B: Fan speed (CMPB)
Result: Two separate PWM signals from one EPWM

### 3. Sync Out Timing

syncOutPulseMode = "Counter = CMPB"
Result: Custom sync pulse at CMPB position

**Example**: ADC sampling at 75% of PWM cycle
- TBPRD = 2000
- CMPB = 1500 (75%)
- ADC SOC triggered at CMPB

### 4. Event Trigger Point

Interrupt source: Counter = CMPB
Result: Interrupt at specific cycle point

## Duty Cycle Calculation:

Duty_B = (CMPB / TBPRD) × 100%

## Valid Range: 0 - 65535
`,
                hidden: false,
                default: 0,
            },
            {
                name: "epwmCounterCompare_cmpBGld",
                displayName: "Enable CMPB Global Load",
                description: 'Use global load for CMPB updates',
                hidden: false,
                default: false,
            },
            {
                name: "epwmCounterCompare_enableShadowLoadModeCMPB",
                displayName: "Enable CMPB Shadow Mode",
                description: 'Use shadow register for CMPB',
                hidden: false,
                default: true,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmCounterCompare_shadowLoadModeCMPB",
                displayName: "CMPB Shadow Load Mode",
                description: 'Event that triggers shadow to active transfer',
                longDescription: 
`# CMPB Shadow Load Event

Same options as CMPA. See CMPA description for details.

## Independent Operation:

CMPA and CMPB can have different load modes.

**Example**: Asymmetric updates

CMPA: Load on Counter = Zero (start of cycle)
CMPB: Load on Counter = Period (end of cycle)

Result: 
  Output A duty updates at cycle start
  Output B duty updates at cycle end
  Staggered updates reduce transients

**Typical**: Match CMPA load mode for simplicity.
`,
                hidden: true,
                default: device_peripheral.EPWM_CounterCompareLoadMode[0].name,
                options: device_peripheral.EPWM_CounterCompareLoadMode,
            },
            {
                name: "epwmCounterCompare_cmpBLink",
                displayName: "CMPB Link",
                description: 'Link CMPB with another EPWM',
                hidden: false,
                default: device_peripheral.EPWM_CurrentLink[0].name,
                options: device_peripheral.EPWM_CurrentLink,
            },
        ]
    },
    
    // ========================================
    // COUNTER COMPARE C
    // ========================================
    {
        name: "GROUP_CMPC",
        displayName: "Counter Compare C",
        collapsed: true,
        config: [
            {
                name: "epwmCounterCompare_cmpC",
                displayName: "Counter Compare C Value (CMPC)",
                description: 'Compare value for Counter Compare C (0-65535)',
                longDescription: 
`# Counter Compare C (CMPC)

Additional compare value for advanced timing.

## Note: No Direct Output Actions in v2

CMPC does NOT have dedicated action qualifier outputs like CMPA/CMPB.

## Use Cases:

**1. Sync Out Pulse Generation**:

syncOutPulseMode = "Counter = CMPC"
Result: Sync pulse at custom position

**2. ADC Trigger Timing**:

Use CMPC event for ADC SOC trigger
Sample at specific PWM phase

**3. Additional Event Marker**:

CMPC = milestone point in PWM cycle
Software checks for CMPC event
State machine trigger

**4. Multi-Level Converter Timing**:

CMPA: Level 1 switching point
CMPB: Level 2 switching point
CMPC: Level 3 switching point
Complex waveform generation

## Limitation:
Cannot directly control Output A/B via action qualifier.
Use for event generation and timing only.

## Valid Range: 0 - 65535
`,
                hidden: false,
                default: 0,
            },
            {
                name: "epwmCounterCompare_cmpCGld",
                displayName: "Enable CMPC Global Load",
                description: 'Use global load for CMPC updates',
                hidden: false,
                default: false,
            },
            {
                name: "epwmCounterCompare_enableShadowLoadModeCMPC",
                displayName: "Enable CMPC Shadow Mode",
                description: 'Use shadow register for CMPC',
                hidden: false,
                default: true,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmCounterCompare_shadowLoadModeCMPC",
                displayName: "CMPC Shadow Load Mode",
                description: 'Event that triggers shadow to active transfer',
                hidden: true,
                default: device_peripheral.EPWM_CounterCompareLoadMode[0].name,
                options: device_peripheral.EPWM_CounterCompareLoadMode,
            },
            {
                name: "epwmCounterCompare_cmpCLink",
                displayName: "CMPC Link",
                description: 'Link CMPC with another EPWM',
                hidden: false,
                default: device_peripheral.EPWM_CurrentLink[0].name,
                options: device_peripheral.EPWM_CurrentLink,
            },
        ]
    },
    
    // ========================================
    // COUNTER COMPARE D
    // ========================================
    {
        name: "GROUP_CMPD",
        displayName: "Counter Compare D",
        collapsed: true,
        config: [
            {
                name: "epwmCounterCompare_cmpD",
                displayName: "Counter Compare D Value (CMPD)",
                description: 'Compare value for Counter Compare D (0-65535)',
                longDescription: 
`# Counter Compare D (CMPD)

Fourth compare value for maximum flexibility.

## Note: No Direct Output Actions in v2

CMPD does NOT have dedicated action qualifier outputs.

## Use Cases:

**1. Fourth Timing Marker**:

CMPA, CMPB, CMPC, CMPD: Four event points
Complex timing sequences
Multi-phase operations

**2. Redundant Sampling**:

CMPC: Primary ADC trigger
CMPD: Backup ADC trigger
Fault detection comparison

**3. State Machine Triggers**:

CMPD event: Change system state
Software polls CMPD flag
Timed state transitions

**4. Advanced Sync Control**:

syncOutPulseMode = "Counter = CMPD"
Fine-grained sync timing
Multi-level system coordination

## Limitation:
Like CMPC, cannot directly control PWM outputs.
Use for event generation and timing only.

## Valid Range: 0 - 65535
`,
                hidden: false,
                default: 0,
            },
            {
                name: "epwmCounterCompare_cmpDGld",
                displayName: "Enable CMPD Global Load",
                description: 'Use global load for CMPD updates',
                hidden: false,
                default: false,
            },
            {
                name: "epwmCounterCompare_enableShadowLoadModeCMPD",
                displayName: "Enable CMPD Shadow Mode",
                description: 'Use shadow register for CMPD',
                hidden: false,
                default: true,
                onChange: onChangeEnableDisable
            },
            {
                name: "epwmCounterCompare_shadowLoadModeCMPD",
                displayName: "CMPD Shadow Load Mode",
                description: 'Event that triggers shadow to active transfer',
                hidden: true,
                default: device_peripheral.EPWM_CounterCompareLoadMode[0].name,
                options: device_peripheral.EPWM_CounterCompareLoadMode,
            },
            {
                name: "epwmCounterCompare_cmpDLink",
                displayName: "CMPD Link",
                description: 'Link CMPD with another EPWM',
                hidden: false,
                default: device_peripheral.EPWM_CurrentLink[0].name,
                options: device_peripheral.EPWM_CurrentLink,
            },
        ]
    },
];

let epwmCounterCompareSubmodule = {
    displayName: "EPWM Counter Compare",
    defaultInstanceName: "EPWM_CC",
    description: "Counter Comparator Configuration",
    longDescription: 
`# Counter Compare Submodule

Generates events when time-base counter matches compare values.

## Key Features:
- Four independent compare registers (A, B, C, D)
- Shadow registers for glitch-free updates
- Flexible load events
- Register linking across EPWMs

## PWM Duty Cycle Control:

Duty_A = (CMPA / TBPRD) × 100%
Duty_B = (CMPB / TBPRD) × 100%

## Common Configurations:

**Single PWM Output**:
- Use CMPA for duty cycle
- Action: Set high on Zero, Set low on CMPA

**Complementary PWM**:
- CMPA controls Output A
- CMPB controls Output B
- Can overlap for dead-time

**Multi-level Inverter**:
- CMPA, CMPB, CMPC for different switching points
- Complex waveform synthesis
`,
    config: config,
};

exports = epwmCounterCompareSubmodule;