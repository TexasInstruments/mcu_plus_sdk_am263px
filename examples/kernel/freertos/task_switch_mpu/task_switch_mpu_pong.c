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
 * All text and data in this C file is by default placed in corresponding sections reserved for the pong task
 * Hence other user mode tasks won't have access to any of the text/data in this C file.  
 */

#include "task_switch_mpu_private.h"

static uint32_t gPongCntr;

static void pong_cntr_reset()
{
    gPongCntr = 0U;
}

static void pong_cntr_increment()
{
    gPongCntr++;
}

static uint32_t pong_cntr_get()
{
    return gPongCntr;
}

static void reset_cntr()
{
    pong_cntr_reset();
}

static void update_cntr()
{
    pong_cntr_increment();
    common_cntr_increment();
}

static void log_cntr()
{
    DebugP_log("\r\n");
    DebugP_log("pong count = %" PRId32 "\r\n", pong_cntr_get());
}

static void switch_using_semaphore()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */

    reset_cntr();

    while (count--)
    {
        xSemaphoreTake( gPongSem, portMAX_DELAY );  /* wait for pong to signal */
        update_cntr();                              /* increment pong counter and common counter */
        xSemaphoreGive( gPingSem );                 /* wakeup pong task */
    }

    log_cntr();
}

static void switch_using_task_notification()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */
    
    reset_cntr();

    while (count--)
    {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  /* wait for pong to signal */
        update_cntr();                              /* increment pong counter and common counter */
        xTaskNotifyGive( gPingTask );               /* wake up pong task */
    }
    
    log_cntr();
}

static void switch_using_isr()
{
    uint32_t count = NUM_TASK_SWITCHES;             /* loop `count` times */

    reset_cntr();

    while (count--)
    {
        xSemaphoreTake( gPongSem, portMAX_DELAY );  /* wait for ISR to signal */
        update_cntr();                              /* increment pong counter and common counter */
        HwiP_post(PONG_INT_NUM);                    /* trigger pong interrupt, which will wakeup ping task */
    }

    log_cntr();
}

void pong_main(void *args)
{
    switch_using_semaphore();
    switch_using_task_notification();
    switch_using_isr();
    
    /** User mode tasks can't use `vTaskDelete`.
     * Instead privileged task should delete the user mode tasks. */
    vTaskSuspend(NULL); /* Move to blocked state */
}
