/*
 *  Copyright (C) 2025 Texas Instruments Incorporated
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

#include <kernel/threadx/dpl/common/ClockP_threadx_priv.h>
#include <kernel/dpl/TimerP.h>



static void ClockP_sleepTicks(uint32_t ticks);

ClockP_Control gClockCtrl;

void ClockP_timerTickIsr(void *args)
{
    VOID _tx_timer_interrupt(VOID);

    /* increment the systick counter */
    gClockCtrl.ticks++;

    /* Call the ThreadX kernel tick handling routine. */
    _tx_timer_interrupt();

    ClockP_timerClearOverflowInt(gClockConfig.timerBaseAddr);
}

void ClockP_timerCallbackFunction( ULONG input )
{
    ClockP_Object *pTimer = (ClockP_Object *)input;

    if((pTimer != NULL) && (pTimer->callback) )
    {
        pTimer->callback(pTimer, pTimer->args);
    }
}

int32_t ClockP_construct(ClockP_Object *pTimer, ClockP_Params *params)
{
    UINT auto_activate = TX_FALSE;
    int32_t status = SystemP_FAILURE;
    UINT tx_ret;

    if(params->start != 0U)
    {
        auto_activate = TX_TRUE;
    }

    pTimer->callback = params->callback;
    pTimer->args = params->args;

    tx_ret = tx_timer_create(&pTimer->timerObj, (CHAR *)params->name, ClockP_timerCallbackFunction, (ULONG)pTimer, params->timeout, params->period, auto_activate);
    
    if(tx_ret == TX_SUCCESS) {
        status = SystemP_SUCCESS;
    }

    return status;
}

void ClockP_destruct(ClockP_Object *pTimer)
{
    (void)tx_timer_delete(&pTimer->timerObj);
}

uint32_t ClockP_usecToTicks(uint64_t usecs)
{
    return (uint32_t)(usecs / gClockCtrl.usecPerTick);
}

uint64_t ClockP_ticksToUsec(uint32_t ticks)
{
    return ((uint64_t)ticks * gClockCtrl.usecPerTick);
}

uint32_t ClockP_getTicks(void)
{
    return ((uint32_t)tx_time_get());
}

uint32_t ClockP_getTimeout(ClockP_Object *pTimer)
{
    UINT active;
    ULONG remaining_ticks;
    uint32_t value = 0;
    UINT tx_ret;

    tx_ret = tx_timer_info_get(&pTimer->timerObj, (CHAR **)TX_NULL, &active, &remaining_ticks, TX_NULL, TX_NULL);
    if(tx_ret == TX_SUCCESS)
    {
        if(active == TX_TRUE)
        {
            value = (uint32_t)remaining_ticks;
        }

    }

    return value;
}

uint32_t ClockP_isActive(ClockP_Object *pTimer)
{
    UINT active;
    UINT tx_ret;
    uint32_t isActive = 0U;

    tx_ret = tx_timer_info_get(&pTimer->timerObj, (CHAR **)TX_NULL, &active, TX_NULL, TX_NULL, TX_NULL);
    if(tx_ret == TX_SUCCESS)
    {
        if(active == TX_TRUE)
         {
             isActive = 1U;
         }
    }

    return isActive;
}

void ClockP_Params_init(ClockP_Params *params)
{
	params->start = 0;
    params->timeout = 0;
    params->period = 0;
    params->callback = NULL;
    params->args = NULL;
	params->name = "Clock (DPL)";

}

void ClockP_setTimeout(ClockP_Object *pTimer, uint32_t timeout)
{
    (void)tx_timer_change(&pTimer->timerObj, timeout, timeout);

}

void ClockP_start(ClockP_Object *pTimer)
{
    (void)tx_timer_activate(&pTimer->timerObj);
}

void ClockP_stop(ClockP_Object *pTimer)
{
    (void)tx_timer_deactivate(&pTimer->timerObj);
}

void ClockP_sleep(uint32_t sec)
{
    uint64_t ticks = (uint64_t)sec * TIME_IN_MICRO_SECONDS / (uint64_t)gClockCtrl.usecPerTick;

    ClockP_sleepTicks((uint32_t)ticks);
}

void ClockP_usleep(uint32_t usec)
{
    uint64_t curTime, endTime;
    uint32_t ticksToSleep;

    curTime = ClockP_getTimeUsec();
    endTime = curTime + usec;

    if (usec >= gClockCtrl.usecPerTick) {
        ticksToSleep = (uint32_t)(usec / gClockCtrl.usecPerTick);
        ClockP_sleepTicks(ticksToSleep);
    }
    else
    {
        curTime = ClockP_getTimeUsec();
        while (curTime < endTime) {
            curTime = ClockP_getTimeUsec();
        }
    }
}

/*
 *  Get the current time in microseconds.
 */
uint64_t ClockP_getTimeUsec(void)
{
    uint64_t ts = 0U;
    uint32_t timerCount1, timerCount2;
    uint64_t ticks1, ticks2;
    uint32_t isOverflowed = 0U;

    do {
        /* Read the current tick count and timer count */
        ticks1 = gClockCtrl.ticks;
        timerCount1 = ClockP_getTimerCount(gClockCtrl.timerBaseAddr);

        /** Check if the timer has overflowed.
         * This is to handle cases in which this function is invoked from say critical sections with
         * interrupts disabled and hence `gClockCtrl.ticks` won't get increment even in case of
         * overflow in timer count.
         * When ISR increments `gClockCtrl.ticks` it will clear the overflow status */
        isOverflowed = TimerP_isOverflowed(gClockCtrl.timerBaseAddr);

        /* Read the timer count and tick count again for consistency */
        timerCount2 = ClockP_getTimerCount(gClockCtrl.timerBaseAddr);
        ticks2 = gClockCtrl.ticks;
    } while ((ticks1 != ticks2) || (timerCount1 > timerCount2));

    /* Handle the overflow case */
    if(isOverflowed != 0U)
    {
        ticks2++;
    }

    /* Get the current time in microseconds */
    ts = (ticks2 * (uint64_t)gClockCtrl.usecPerTick)
             + (uint64_t) ( /* convert timer count to usecs */
                (uint64_t)(((timerCount2 - gClockCtrl.timerReloadCount)*gClockCtrl.usecPerTick)/(MAX_TIMER_COUNT_VALUE - gClockCtrl.timerReloadCount))
                );

    return (ts);
}

/*
 *  Sleep for a given number of ClockP ticks.
 */
static void ClockP_sleepTicks(uint32_t ticks)
{
    tx_thread_sleep(ticks);
}


