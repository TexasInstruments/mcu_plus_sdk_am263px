let common = system.getScript("/common");
let device_peripheral = system.getScript(`/drivers/epwm/soc/epwm_${common.getSocName()}`);

function onChangeEnableDisable(inst, ui)
{
    // Show/hide interrupt controls
    if (inst.epwmEventTrigger_enableInterrupt == true)
    {
        ui.epwmEventTrigger_interruptSource.hidden = false;
        ui.epwmEventTrigger_interruptPrescale.hidden = false;
    }
    else
    {
        ui.epwmEventTrigger_interruptSource.hidden = true;
        ui.epwmEventTrigger_interruptPrescale.hidden = true;
    }
}

let config = [
    {
        name: "epwmEventTrigger_enableInterrupt",
        displayName: "Enable ePWM Interrupt",
        description: 'Enable CPU interrupt generation',
        longDescription: 
`# ePWM CPU Interrupt

Generates CPU interrupt on selected counter events.

## Purpose:
Periodic interrupt synchronized with PWM operation.

## Use Cases:

### Control Loop Update

Interrupt Source: Counter = Zero
Prescale: Every event (every PWM cycle)
ISR: Update motor speed control, PI loop
Result: Control executes at PWM frequency

**Example**: 10 kHz PWM
- Interrupt every 100 us
- Read encoder position
- Calculate new duty cycle
- Update CMPA/CMPB

### Periodic Monitoring

Interrupt Source: Counter = Period
Prescale: Every 3rd event
ISR: Check temperatures, voltages, status
Result: Monitoring at 1/3 PWM frequency

### Synchronized Data Logging

Interrupt Source: Counter = CMPA (up)
ISR: Log current, voltage at specific PWM phase
Result: Consistent sampling point

### ADC Result Processing

Interrupt Source: Counter = Zero
ADC triggered at same event
ISR: Read ADC results, process data
Result: Minimal latency between ADC and processing

## Performance Considerations:

**Interrupt Frequency**:

Freq = PWM_Freq / Prescale

**Example**: 20 kHz PWM, Prescale = 2
- Interrupt at 10 kHz (every 100 us)
- ISR must complete in less than 100 us

**CPU Load**:

Load = (ISR_Time × Interrupt_Freq) × 100%

**Example**: 10 us ISR, 10 kHz interrupt
- Load = (10 us × 10 kHz) × 100% = 10%

## Prescaler Usage:

**Every Event** (Prescale = 1):
- Use for: Fast control loops
- Example: High-performance motor control

**Every 2nd Event** (Prescale = 2):
- Use for: Moderate CPU load reduction
- Example: Dual-ADC processing

**Every 3rd Event** (Prescale = 3):
- Use for: Slow control loops, monitoring
- Example: Temperature monitoring, state machines

## Interrupt Registration:

In application code:
HwiP_Params hwiParams;
HwiP_Params_init(&hwiParams);
hwiParams.intNum = CONFIG_EPWM0_INTR;
hwiParams.callback = &EPWM_ISR;
HwiP_construct(&gEpwmHwiObject, &hwiParams);

## ISR Template:

void EPWM_ISR(void *args)
{
    // 1. Clear interrupt flag (first!)
    EPWM_etIntrClear(CONFIG_EPWM0_BASE_ADDR);
    
    // 2. Read inputs (ADC, sensors)
    float current = ADC_readResult(...);
    float voltage = ADC_readResult(...);
    
    // 3. Execute control algorithm
    float duty = PI_Controller(current, voltage);
    
    // 4. Update PWM
    uint16_t cmpa = (uint16_t)(duty * TBPRD);
    EPWM_counterComparatorCfg(...);
    
    // 5. Status updates
    controlLoopCounter++;
}

**Performance**: Keep ISR short (less than 10% of period)!
`,
        hidden: false,
        default: false,
        onChange: onChangeEnableDisable
    },
    {
        name: "epwmEventTrigger_interruptSource",
        displayName: "Interrupt Source Event",
        description: 'Counter event that triggers interrupt',
        longDescription: 
`# Interrupt Source Event

Selects which counter event generates the interrupt.

## Options:

### Counter = Zero

Occurs: Start of PWM period (up-count)
        Twice per period (up-down)

**Use for**:
- Start-of-cycle control updates
- Period-based state machines
- Most common choice

### Counter = Period

Occurs: End of PWM period (up-count)
        Once per period (up-down, at peak)

**Use for**:
- End-of-cycle processing
- Down-count mode control

### Counter = CMPA (Counting Up)

Occurs: When counter hits CMPA during up-count

**Use for**:
- Sample current at specific PWM phase
- Custom timing relative to duty cycle

### Counter = CMPA (Counting Down)

Occurs: When counter hits CMPA during down-count

**Use for**:
- Up-down mode mid-cycle processing
- Symmetric sampling points

### Counter = CMPB (Counting Up/Down)
Same as CMPA, but uses CMPB event.

## Selection Guide:

| Application | Source | Reason |
|-------------|--------|--------|
| Motor control | Zero | Start of cycle, consistent timing |
| Buck converter | Zero | Beginning of switching cycle |
| Boost converter | Period | End of switching cycle |
| Current sensing | CMPA | Sample at peak current |
| Multi-phase sync | Zero | Maintain phase relationship |

## Timing Diagram (Up-Down Mode):

Counter:  0 --> CMPA --> TBPRD --> CMPA --> 0
          ^             ^                ^
          Zero          CMPA(up)         CMPA(down)
          INT           INT              INT

**Recommendation**: Start with "Counter = Zero" for simplicity.
`,
        hidden: true,
        default: device_peripheral.EPWM_EtIntrEvt[0].name,
        options: device_peripheral.EPWM_EtIntrEvt,
    },
    {
        name: "epwmEventTrigger_interruptPrescale",
        displayName: "Interrupt Prescale",
        description: 'Number of events before generating interrupt',
        longDescription: 
`# Interrupt Prescaler

Controls how many selected events must occur before generating interrupt.

## Options:

### Disable (0)

No interrupts generated

Use when interrupt not needed (ADC trigger only).

### Generate on First Event (1x)

Every selected event generates interrupt

**Interrupt Frequency** = PWM Frequency

**Use for**:
- Fast control loops
- Cycle-by-cycle updates
- Maximum response speed

**Example**: 20 kHz PWM, source = Zero
- Interrupt every 50 us
- CPU load: Depends on ISR time

### Generate on Second Event (2x)

Every 2nd event generates interrupt

**Interrupt Frequency** = PWM Frequency / 2

**Use for**:
- Reduce CPU load 50%
- Still fast response
- Medium-speed control

**Example**: 20 kHz PWM, prescale = 2
- Interrupt every 100 us
- CPU load reduced by half

### Generate on Third Event (3x)

Every 3rd event generates interrupt

**Interrupt Frequency** = PWM Frequency / 3

**Use for**:
- Slow control loops
- Monitoring tasks
- Minimize CPU load

**Example**: 30 kHz PWM, prescale = 3
- Interrupt every 100 us
- CPU load: 33% of prescale=1

## CPU Load Calculation:

CPU Load = (ISR_Time / Interrupt_Period) × 100%

Example: 
  PWM Freq: 10 kHz (100 us period)
  Prescale: 1 (every event)
  ISR Time: 10 us
  
  Load = (10 us / 100 us) × 100% = 10%
  
With Prescale = 3:
  Load = (10 us / 300 us) × 100% = 3.3%
  (Saved 6.7% CPU for other tasks)

## Event Counter:

The hardware maintains a counter:
- Count: 1, 2, 3, 1, 2, 3, ...
- Interrupt: Generated when counter reaches prescale value
- Readable via EPWM_etGetEventCount() for diagnostics

## Selection Guide:

| Control Loop Speed | Prescale | CPU Load | Use Case |
|--------------------|----------|----------|----------|
| Very Fast | 1 | High | High-perf servo, fast current loop |
| Fast | 2 | Medium | Standard motor control |
| Moderate | 3 | Low | Slow speed control, monitors |

**Recommendation**: 
- **Development**: Prescale = 1 (test at max rate)
- **Production**: Increase prescale if CPU load too high
`,
        hidden: true,
        default: device_peripheral.EPWM_EtIntrPeriod[1].name,
        options: device_peripheral.EPWM_EtIntrPeriod,
    },
];

let epwmEventTriggerSubmodule = {
    displayName: "EPWM Event Trigger",
    defaultInstanceName: "EPWM_ET",
    description: "Interrupt and ADC Trigger Configuration",
    longDescription: 
`# Event Trigger Submodule

Generates CPU interrupts synchronized with PWM.

## Key Features:
- CPU Interrupt on selected counter events
- Prescaler to reduce interrupt rate (1x, 2x, 3x)
- Event Counter (hardware-maintained)

## AM273x v2 Note:

The v2 EPWM does NOT have direct ADC trigger outputs like AM263x v1.

**v2 Interrupt Only**:
- EPWM can generate CPU interrupt
- CPU ISR manually triggers ADC

**v1 Has ADC SOC** (not available in v2):
- EPWM directly triggers ADC SOC A/B
- No CPU involvement needed

**Workaround for v2**:

void EPWM_ISR(void)
{
    EPWM_etIntrClear(base);
    
    // Manually trigger ADC
    ADC_forceSOC(ADC_BASE, ADC_SOC_NUMBER0);
    
    // Continue with control...
}

## Typical Control Loop Structure:

PWM Cycle Timing:
- Counter = Zero
  - EPWM Interrupt
    - ISR:
      - Clear interrupt flag
      - Trigger ADC (software)
      - Read previous ADC results
      - Execute control algorithm
      - Update CMPA/CMPB
      - Return

- Counter = CMPA
  - (PWM output changes, no interrupt)

- Counter = TBPRD
  - (cycle repeats)

## Interrupt Latency:

Total latency from event to ISR entry:

Latency = t_detection + t_interrupt_logic + t_CPU_context_switch

AM273x R5F:
  t_detection: 1 clock cycle (5 ns)
  t_interrupt_logic: ~10 cycles (50 ns)
  t_context_switch: ~50 cycles (250 ns)
  
Total: ~300 ns (negligible for most control)

## Performance Tips:

1. Keep ISR short: Aim for less than 10% of PWM period
2. Use prescaler: Reduce interrupt rate if possible
3. Optimize ISR: 
   - Inline small functions
   - Avoid divisions (use shifts)
   - Pre-calculate constants
4. Profile ISR: Measure execution time with GPIO toggle
5. Use DMA: For high-speed data transfers (if available)

## Example: 10 kHz Control Loop

PWM Frequency: 10 kHz (100 us period)
Interrupt Source: Counter = Zero
Prescale: Every event
ISR Time: 8 us

Analysis:
  CPU Load: (8 us / 100 us) × 100% = 8%
  Remaining: 92% for other tasks
  Verdict: Acceptable
`,
    config: config,
};

exports = epwmEventTriggerSubmodule;