/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/SystemP.h>
#include <drivers/gpio.h>
#include <drivers/eqep.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/*
 *  CW/CCW Input demonstration to eQEP
 * 
 *  This example emulates CW and CCW pulses using 2 GPIO's and Timer based interrupts, before feeding them to the eQEP module.
 *  QMA MODE 2 (Active Pulse High is Enabled)
 *  We calculate and watch position, direction, frequency and speed post feeding these
 *  signals to eQEP module.
 *  
 * 
 *  The configuration for this example is as follows
 *  - Encoder resolution is configured to 1000 counts/revolution
 *  - GPIO's are routed through PWMXBAR to eQEP module
 *  - GPIO's (simulating CW/CCW encoder signals) are configured for a 2.5kHz frequency
 *    or 150 rpm (= 2500 cnts/sec * 60 sec/min) / 1000 cnts/rev)
 * 
 * 
 *  External Connections (Only needed if you want to view the input CW/CCW Signals)\n
 * 
 *  AM263x-CC, AM263Px-CC
 *  - Scope GPIO43/Hsec 49 (simulates eQEP Phase A signal)
 *  - Scope GPIO44/Hsec 51 (simulates eQEP Phase B signal)
 *  - Scope GPIO48/Hsec 52 (simulates eQEP Index Signal)
 * 
 *  AM263x-LP, AM263Px-LP
 *  - Scope GPIO43/J2.11 (simulates eQEP Phase A signal)
 *  - Scope GPIO44/J6.59 (simulates eQEP Phase B signal)
 *  - Scope GPIO48/J4.40 (simulates eQEP Index Signal)
 *
 *  AM261x-SOM
 *  - Scope GPIO49 (simulates eQEP Phase A signal)
 *  - Scope GPIO50 (simulates eQEP Phase B signal)
 *  - Scope GPIO48 (simulates eQEP Index Signal)
 * 
 *  AM261x-LP
 *  - Scope GPIO49/J4.38 (simulates eQEP Phase A signal)
 *  - Scope GPIO50/J4.37 (simulates eQEP Phase B signal)
 *  - Scope GPIO48/J4.36 (simulates eQEP Index Signal)
 * 
 */ 

/* Configuration Constants */
#define APP_LOOP_CNT          (1000U)     /* Number of interrupt cycles before completion */
#define APP_INT_IS_PULSE      (1U)       /* Configure interrupt as pulse type */
#define APP_ENCODER_SLOTS     (1000U)    /* Encoder resolution (counts per revolution) */
#define APP_UNIT_PERIOD       (10000U)   /* Unit timeout period in microseconds (10ms) */
#define APP_PULSES_PER_GROUP  (50U)      /* Number of pulses in each direction group (CW/CCW) */
#define APP_TOGGLES_PER_PULSE (2U)       /* Each pulse requires 2 toggles (high and low) */

/* 
 * APP_EXPECTED_FREQ_HZ: Derived from the timer interrupt period of 200μs.
 * Each toggle happens every 200μs, and a complete pulse (high-low) takes 2 toggles = 400μs.
 * Frequency = 1/period = 1/0.0004s = 2500 Hz
 */
#define APP_EXPECTED_FREQ_HZ      (2500)    /* Expected frequency in Hz */

/* 
 * APP_EXPECTED_RPM: Derived from the frequency using the encoder resolution.
 * RPM = (Frequency in Hz × 60 seconds/minute) / (Encoder resolution in counts/rev)
 * RPM = (2500 counts/sec × 60 sec/min) / 1000 counts/rev = 150 RPM
 */
#define APP_EXPECTED_RPM          (150U)    /* Expected RPM */

/* 
 * Tolerance values for validation
 * For precision control systems, typical frequency tolerances are ±2-5%
 * and speed tolerances are ±3-5% of the expected value
 */
#define APP_FREQ_TOLERANCE_PERCENT  (5U)    /* Standard 5% tolerance for frequency measurement */
#define APP_SPEED_TOLERANCE_PERCENT (5U)    /* Standard 5% tolerance for speed measurement */

/* Calculate actual tolerance values from percentages */
#define APP_FREQ_TOLERANCE_HZ    ((APP_EXPECTED_FREQ_HZ * APP_FREQ_TOLERANCE_PERCENT) / 100U)   /* 125 Hz */
#define APP_SPEED_TOLERANCE_RPM  ((APP_EXPECTED_RPM * APP_SPEED_TOLERANCE_PERCENT) / 100U)      /* 7.5 RPM */

/* Global Variables for Position and Speed Calculation */
typedef struct {
    uint32_t oldCount;           /* Previous position count at last unit timeout */
    uint32_t newCount;           /* Latest latched position count at unit timeout */
    uint32_t currentEncoderPos;  /* Current absolute encoder position */
    int32_t freq;               /* Pulse frequency in Hz */
    float speed;                /* Motor speed in RPM */
    int32_t direction;          /* Current direction: 1 = CW, -1 = CCW */
} App_eqepEncoderState;

/* Make encoder state globally accessible for debugging */
App_eqepEncoderState gAppEqepEncoderState = {0};

/* Global Variables */
static uint32_t gAppEqepBaseAddr;
static uint16_t gAppEqepInterruptCount = 0;
static HwiP_Object gAppEqepHwiObject;
static SemaphoreP_Object gAppEqepSyncSem;
static bool gAppEqepQmaValid = true;

static volatile int32_t gCurrentPulseDirection = 1;

/* Function Prototypes */
static void App_eqepISR(void *args);
static void App_eqepCalculateSpeed(void);
static inline void App_gpioPinToggle(uint32_t baseAddr, uint32_t pinNum);

/* Main Application Entry Point */
void eqep_cw_ccw_main(void *args)
{
    int32_t status;
    HwiP_Params hwiPrms;
    
    /* Initialize drivers and board */
    Drivers_open();
    Board_driversOpen();

    gAppEqepBaseAddr = CONFIG_EQEP0_BASE_ADDR;

    /* Create synchronization semaphore */
    status = SemaphoreP_constructBinary(&gAppEqepSyncSem, 0);
    DebugP_assert(status == SystemP_SUCCESS);

    DebugP_log("EQEP Position Speed Test Started ...\r\n");
    
    /* Clear any pending interrupts */
    EQEP_clearInterruptStatus(gAppEqepBaseAddr, EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL|EQEP_INT_QMA_ERROR);
    
    /* Configure and register EQEP interrupt handler */
    HwiP_Params_init(&hwiPrms);
    hwiPrms.intNum = CSLR_R5FSS0_CORE0_CONTROLSS_INTRXBAR0_OUT_0;
    hwiPrms.callback = &App_eqepISR;
    hwiPrms.isPulse = APP_INT_IS_PULSE;
    
    status = HwiP_construct(&gAppEqepHwiObject, &hwiPrms);
    DebugP_assert(status == SystemP_SUCCESS);

    /* 
    * Start the timer and wait for completion 
    * Tick period is set to 200usec
    */
    TimerP_start(gTimerBaseAddr[CONFIG_TIMER0]);
    SemaphoreP_pend(&gAppEqepSyncSem, SystemP_WAIT_FOREVER);
    TimerP_stop(gTimerBaseAddr[CONFIG_TIMER0]);

    /* Validate frequency and speed measurements */
    bool freqValid = (gAppEqepEncoderState.freq >= (APP_EXPECTED_FREQ_HZ - APP_FREQ_TOLERANCE_HZ) && 
                     gAppEqepEncoderState.freq <= (APP_EXPECTED_FREQ_HZ + APP_FREQ_TOLERANCE_HZ));
    bool speedValid = (gAppEqepEncoderState.speed >= (APP_EXPECTED_RPM - APP_SPEED_TOLERANCE_RPM) && 
                      gAppEqepEncoderState.speed <= (APP_EXPECTED_RPM + APP_SPEED_TOLERANCE_RPM));
    
    if(!freqValid || !speedValid || !gAppEqepQmaValid) {
        DebugP_log("EQEP Validation Failed!\r\n");
        DebugP_log("Results:\r\n");
        DebugP_log("  Frequency: %d Hz (Expected: %d Hz ±%d)\r\n", 
                  gAppEqepEncoderState.freq, APP_EXPECTED_FREQ_HZ, APP_FREQ_TOLERANCE_HZ);
        DebugP_log("  Speed: %.2f RPM (Expected: %d RPM ±%d)\r\n",
                  gAppEqepEncoderState.speed, APP_EXPECTED_RPM, APP_SPEED_TOLERANCE_RPM);
        DebugP_log("  Direction: %s\r\n", 
                  (gAppEqepEncoderState.direction > 0) ? "Clockwise" : "Counter-clockwise");
        
        if(!gAppEqepQmaValid) {
            DebugP_log("ERROR: QMA Error detectead\r\n");
        }
        if(!freqValid) {
            DebugP_log("ERROR: Frequency out of valid range\r\n");
        }
        if(!speedValid) {
            DebugP_log("ERROR: Speed out of valid range\r\n"); 
        }
        
        DebugP_log("Test Failed!\r\n");
    }
    else {
        DebugP_log("EQEP Validation Passed\r\n");
        DebugP_log("Results:\r\n");
        DebugP_log("  Frequency: %d Hz\r\n", gAppEqepEncoderState.freq);
        DebugP_log("  Speed: %.2f RPM\r\n", gAppEqepEncoderState.speed);
        DebugP_log("  Direction: %s\r\n",
                  (gAppEqepEncoderState.direction > 0) ? "Clockwise" : "Counter-clockwise");
        DebugP_log("All tests passed successfully!\r\n");
    }

    /* Cleanup resources */
    HwiP_destruct(&gAppEqepHwiObject);
    SemaphoreP_destruct(&gAppEqepSyncSem);

    Board_driversClose();
    Drivers_close();
}

/* Timer ISR for Generating Test Signals */
void timerISR(void) {
    static int32_t previousPulseDirection = 1; /* Initialize to starting direction */
    static uint32_t indexPulseCount = 0; /* Counter to manage index pulse width */
    /* Software counter to simulate the eQEP position */
    static uint32_t simulatedPosition = 0; /* Start simulation at position 0 */

    /* Read the current direction (might have been changed by App_eqepISR) */
    int32_t currentDirection = gCurrentPulseDirection;

    /* Check if the direction has changed since the last timer tick */
    if (currentDirection != previousPulseDirection) {
        /* Direction changed: Ensure the pin for the *previous* direction is LOW. (IMPT) */
        if (previousPulseDirection > 0) { /* If previous direction was CW */
            GPIO_pinWriteLow(GPIO_CW_PIN_BASE_ADDR, GPIO_CW_PIN_PIN);
        } else { /* If previous direction was CCW */
            GPIO_pinWriteLow(GPIO_CCW_PIN_BASE_ADDR, GPIO_CCW_PIN_PIN);
        }
        /* Update the previous direction state for the next check */
        previousPulseDirection = currentDirection;
    }

    /* Generate CW or CCW test pulses based on the *current* direction flag */
    if (currentDirection > 0) { /* Generate CW pulse */
        App_gpioPinToggle(GPIO_CW_PIN_BASE_ADDR, GPIO_CW_PIN_PIN);
        /* Update simulated position for CW movement */
        simulatedPosition++;
        /* Handle wrap-around if using a specific max count other than U32_MAX */
        if (simulatedPosition > ((APP_ENCODER_SLOTS-1)) * 2) {
            simulatedPosition = 0;
        }
    } else { /* Generate CCW pulse */
        App_gpioPinToggle(GPIO_CCW_PIN_BASE_ADDR, GPIO_CCW_PIN_PIN);
        /* Update simulated position for CCW movement */
        if (simulatedPosition == 0) {
            /* Wrap around based on the effective max count */
            simulatedPosition = ((APP_ENCODER_SLOTS-1)) * 2; 
        } else {
            simulatedPosition--;
        }
    }

    /* 
    * Generate index pulse based on the simulated position counter 
    * Check if the simulated position counter is at 0 and we are not already generating an index pulse 
    */
    if (simulatedPosition == 0 && indexPulseCount == 0) {
        GPIO_pinWriteHigh(GPIO_INDEX_PIN_BASE_ADDR, GPIO_INDEX_PIN_PIN);
        indexPulseCount = 1; /* Start the index pulse duration counter */
    }
    /* Manage the duration of the index pulse (keep it high for a few timer ticks) */
    else if(indexPulseCount > 0) {
        indexPulseCount++;
        /* Keep the pulse high for approximately 3 * 200us = 600us - to ensure eQEP hardware reliably detects index pulse */
        if(indexPulseCount >= 4) {
            GPIO_pinWriteLow(GPIO_INDEX_PIN_BASE_ADDR, GPIO_INDEX_PIN_PIN);
            indexPulseCount = 0; /* Reset index pulse counter */
        }
    }
}

/* EQEP Interrupt Service Routine */
static void App_eqepISR(void *args)
{
    int32_t status = EQEP_getInterruptStatus(gAppEqepBaseAddr);

    gAppEqepInterruptCount++;

    /* Check for test completion */
    if(gAppEqepInterruptCount >= APP_LOOP_CNT) {
        SemaphoreP_post(&gAppEqepSyncSem);
    }
    else{
        /* Handle QMA Error */
        if(status & EQEP_INT_QMA_ERROR) {
            gAppEqepQmaValid = false;
            EQEP_clearInterruptStatus(gAppEqepBaseAddr, EQEP_INT_QMA_ERROR|EQEP_INT_GLOBAL);
            SemaphoreP_post(&gAppEqepSyncSem);
        }
        else {
            /* Update encoder state and calculate speed */
            gAppEqepEncoderState.currentEncoderPos = EQEP_getPosition(gAppEqepBaseAddr);
            gAppEqepEncoderState.direction = EQEP_getDirection(gAppEqepBaseAddr);
            gAppEqepEncoderState.newCount = EQEP_getPositionLatch(gAppEqepBaseAddr);

            App_eqepCalculateSpeed();

            /* Switch the pulse direction for the timerISR on the next cycle */
            gCurrentPulseDirection *= -1;
            
            EQEP_clearInterruptStatus(gAppEqepBaseAddr, EQEP_INT_UNIT_TIME_OUT|EQEP_INT_GLOBAL);
        }
    }
}

/* Calculate Speed and Update Encoder State */
static void App_eqepCalculateSpeed(void)
{
    /* 
     * Calculate position change (delta counts) based on rotation direction
     * 
     * For clockwise rotation (direction > 0):
     *   If newCount >= oldCount: Simple subtraction (no counter wraparound)
     *   If newCount < oldCount: Counter wrapped around from max(APP_ENCODER_SLOTS - 1) to 0
     *                           We need to add distance from oldCount to max value,
     *                           plus distance from 0 to newCount
     * 
     * For counter-clockwise rotation (direction < 0):
     *   If newCount <= oldCount: Simple subtraction (no counter wraparound)
     *   If newCount > oldCount: Counter wrapped around from 0 to max(APP_ENCODER_SLOTS - 1)
     *                           We need to add distance from newCount to max value,
     *                           plus distance from 0 to oldCount
     */
    if (gAppEqepEncoderState.direction > 0) {
        /* Clockwise rotation */
        if (gAppEqepEncoderState.newCount >= gAppEqepEncoderState.oldCount)
            gAppEqepEncoderState.newCount -= gAppEqepEncoderState.oldCount;
        else
            gAppEqepEncoderState.newCount = (APP_ENCODER_SLOTS - gAppEqepEncoderState.oldCount) + gAppEqepEncoderState.newCount;
    }
    else {
        /* Counter-clockwise rotation */
        if (gAppEqepEncoderState.newCount <= gAppEqepEncoderState.oldCount)
            gAppEqepEncoderState.newCount = gAppEqepEncoderState.oldCount - gAppEqepEncoderState.newCount;
        else
            gAppEqepEncoderState.newCount = (APP_ENCODER_SLOTS - gAppEqepEncoderState.newCount) + gAppEqepEncoderState.oldCount;
    }

    /* Update old count for next calculation */
    gAppEqepEncoderState.oldCount = gAppEqepEncoderState.currentEncoderPos;

    /* 
     * Calculate frequency in Hz:
     * freq = (position_change * 1000000) / unit_period_in_microseconds
     * 
     * Where:
     * - position_change = number of encoder counts in the time period
     * - 1000000 = conversion factor for microseconds to seconds
     * - APP_UNIT_PERIOD = time period in microseconds (10000μs = 10ms)
     */
    gAppEqepEncoderState.freq = (gAppEqepEncoderState.newCount * (uint32_t)1000000U) / ((uint32_t)APP_UNIT_PERIOD);
    
    /* 
     * Calculate rotational speed in RPM:
     * RPM = (frequency_in_Hz * 60) / encoder_counts_per_revolution
     * 
     * Where:
     * - frequency_in_Hz = counts per second
     * - 60 = seconds per minute
     * - APP_ENCODER_SLOTS = encoder resolution (1000 counts per revolution)
     */
    gAppEqepEncoderState.speed = (gAppEqepEncoderState.freq * 60.0f) / ((float)(APP_ENCODER_SLOTS));
}

/* GPIO Pin Toggle Helper Function */
static inline void App_gpioPinToggle(uint32_t baseAddr, uint32_t pinNum)
{
    /* Read current pin output value */
    if (GPIO_pinOutValueRead(baseAddr, pinNum) == 1U) {
        /* If pin is high, set it low */
        GPIO_pinWriteLow(baseAddr, pinNum);
    } else {
        /* If pin is low, set it high */
        GPIO_pinWriteHigh(baseAddr, pinNum);
    }
}