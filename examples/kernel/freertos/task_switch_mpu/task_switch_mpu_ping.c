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

/*
 * IMPORTANT NOTES:
 *
 * All text and data in this C file is by default placed in corresponding sections reserved for the ping task.
 * Hence other user mode tasks won't have access to any of the text/data in this C file.  
 */

#include <kernel/dpl/ClockP.h>

#include "task_switch_mpu_private.h"

static uint32_t gPingCntr;

static void ping_cntr_reset()
{
    gPingCntr = 0U;
}

static void ping_cntr_increment()
{
    gPingCntr++;
}

static uint32_t ping_cntr_get()
{
    return gPingCntr;
}

static void reset_cntr()
{
    ping_cntr_reset();
    common_cntr_reset();
}

static void update_cntr()
{
    ping_cntr_increment();
    common_cntr_increment();
}

static void log_cntr()
{
    DebugP_log("ping count = %" PRId32 "\r\n", ping_cntr_get());
    DebugP_log("common count = %" PRId32 "\r\n", common_cntr_get());
}

static void switch_using_semaphore()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */
    uint64_t curTime;
    
    DebugP_log("\r\n");
    DebugP_log("Performing task switch using semaphores\r\n");

    reset_cntr();

    curTime = ClockP_getTimeUsec();
    while (count--)
    {
        xSemaphoreGive( gPongSem );                 /* wake up pong task */
        update_cntr();                              /* increment ping counter and common counter */
        xSemaphoreTake( gPingSem, portMAX_DELAY );  /* wait for pong to signal */
    }   
    curTime = ClockP_getTimeUsec() - curTime;
    
    log_cntr();

    DebugP_log("\r\n");
    DebugP_log("execution time for task switches = %" PRId64 " us\r\n", curTime);
    DebugP_log("number of task switches = %" PRId32 " \r\n", (uint32_t)NUM_TASK_SWITCHES*2);
    DebugP_log("time per task switch (semaphore give/take) = %" PRId32 " ns\r\n", 
                    (uint32_t)(curTime*1000/(NUM_TASK_SWITCHES*2)));
}

static void switch_using_task_notification()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */
    uint64_t curTime;
    
    DebugP_log("\r\n");
    DebugP_log("Performing task switch using direct-to-task notification\r\n");

    reset_cntr();

    curTime = ClockP_getTimeUsec();
    while (count--)
    {
        xTaskNotifyGive( gPongTask );               /* wake up pong task */
        update_cntr();                              /* increment ping counter and common counter */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* wait for pong to signal */
    }
    curTime = ClockP_getTimeUsec() - curTime;

    log_cntr();

    DebugP_log("\r\n");
    DebugP_log("execution time for task switches = %" PRId64 " us\r\n", curTime);
    DebugP_log("number of task switches = %" PRId32 " \r\n", (uint32_t)NUM_TASK_SWITCHES*2);
    DebugP_log("time per task switch (direct-to-task notification give/take) = %" PRId32 " ns\r\n", 
                    (uint32_t)(curTime*1000/(NUM_TASK_SWITCHES*2)));
}

static void switch_using_isr()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */
    uint64_t curTime;
    
    DebugP_log("\r\n");
    DebugP_log("Performing task switch using interrupts\r\n");

    reset_cntr();

    curTime = ClockP_getTimeUsec();
    while (count--)
    {
        HwiP_post(PING_INT_NUM);                    /* trigger ping interrupt, which will wakeup pong task */
        update_cntr();                              /* increment ping counter and common counter */
        xSemaphoreTake( gPingSem, portMAX_DELAY );  /* wait for ISR to signal */
    }
    curTime = ClockP_getTimeUsec() - curTime;

    log_cntr();

    DebugP_log("\r\n");
    DebugP_log("execution time for task - ISR - task - task switches = %" PRId64 " us\r\n", curTime);
    DebugP_log("number of ISRs = %" PRId32 " \r\n", (uint32_t)NUM_TASK_SWITCHES*2);
    DebugP_log("time per task - ISR - task switch (semaphore give/take) = %" PRId32 " ns\r\n", 
                    (uint32_t)(curTime*1000/(2*NUM_TASK_SWITCHES)));
}

void ping_main(void *args)
{

    DebugP_log("\r\n");
    DebugP_log("[FreeRTOS] ping task ... start !!!\r\n");

    switch_using_semaphore();
    switch_using_task_notification();
    switch_using_isr();

    DebugP_log("\r\n");
    DebugP_log("[FreeRTOS] ping task ... done !!!\r\n");

    /* Signal test completion to main task */
    xSemaphoreGive(gSyncSem);

    /** User mode tasks can't use `vTaskDelete`.
     * Instead privileged task should delete the user mode tasks. */
    vTaskSuspend(NULL);     /* Move to blocked state */
}

