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
 * Alignment of stack to stack size is required with MPU port to add stack as an MPU region. 
 *
 * Task priority, 0 is lowest priority, configMAX_PRIORITIES-1 is highest
 * For this example any valid task priority can be set.
 *
 * See FreeRTOSConfig.h for configMAX_PRIORITIES and StackType_t type.
 * FreeRTOSConfig.h can be found under kernel/freertos/config/${device}/${cpu}/
 *
 * In this example,
 * We create task's, semaphore's, ISR's and stack for the tasks using static allocation.
 * We dont need to delete these semaphore's since static allocation is used.
 *
 * One MUST not return out of a FreeRTOS task instead privileged mode tasks MUST call vTaskDelete
 * and user mode tasks MUST call vTaskSuspend. Later privileged tasks may delete the user mode tasks.
 * 
 * All text and data in this C file is by default placed in privileged functions and privileged data sections. 
 * Hence user mode tasks won't have access to any of the text/data in this C file.
 * Ping/Pong task stacks defined in this file will also be placed in privileged data section. 
 * But as part of user task creation a dedicated MPU region will be configured for the task stack.  
 * 
 * - Any text/data which is not required to be in user mode tasks should be placed in this file.
 * - Any text/data which is required only by ping task should be placed in `task_switch_mpu_ping.c`
 * - Any text/data which is required only by pong task should be placed in `task_switch_mpu_pong.c`
 * - Any text/data which is required by both ping & pong tasks should be placed in `task_switch_mpu_common.c`
 * 
 * Refer linker script for more details on text/data placement in various sections.
 */

#include <kernel/dpl/HwiP.h>

#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "task_switch_mpu_private.h"
 
#define PING_TASK_PRI  (2u)
#define PONG_TASK_PRI  (3u)

#define STACK_DEPTH(size)   ((size) / sizeof(configSTACK_DEPTH_TYPE))

#define PING_TASK_SIZE (1024u*4u)
StackType_t gPingTaskStack[STACK_DEPTH(PING_TASK_SIZE)] __attribute__((aligned(PING_TASK_SIZE)));

#define PONG_TASK_SIZE (1024u*4u)
StackType_t gPongTaskStack[STACK_DEPTH(PONG_TASK_SIZE)] __attribute__((aligned(PONG_TASK_SIZE)));

StaticTask_t gPingTaskObj;
StaticTask_t gPongTaskObj;

StaticSemaphore_t gPingSemObj;
StaticSemaphore_t gPongSemObj;
StaticSemaphore_t gSyncSemObj;

HwiP_Object gPingHwiObj;
HwiP_Object gPongHwiObj;

static void ping_isr(void *arg)
{
    BaseType_t doTaskSwitch = 0;

    xSemaphoreGiveFromISR( gPongSem, &doTaskSwitch); /* wake up pong task */
    portYIELD_FROM_ISR( doTaskSwitch );
}

static void pong_isr(void *arg)
{
    BaseType_t doTaskSwitch = 0;

    xSemaphoreGiveFromISR( gPingSem, &doTaskSwitch); /* wake up ping task */
    portYIELD_FROM_ISR( doTaskSwitch );
}

static void create_semaphores()
{
    /* Create semaphore for ping */
    gPingSem = xSemaphoreCreateBinaryStatic(&gPingSemObj);
    configASSERT(gPingSem != NULL);
    vQueueAddToRegistry(gPingSem, "Ping Sem"); /* This makes the semaphore visible in ROV within CCS IDE */

    /* Create semaphore for pong */
    gPongSem = xSemaphoreCreateBinaryStatic(&gPongSemObj);
    configASSERT(gPongSem != NULL);
    vQueueAddToRegistry(gPongSem, "Pong Sem"); /* This makes the semaphore visible in ROV within CCS IDE */

    /* Create semaphore for synchronisation */
    gSyncSem = xSemaphoreCreateBinaryStatic(&gSyncSemObj);
    configASSERT(gSyncSem != NULL);
    vQueueAddToRegistry(gSyncSem, "Sync Sem"); /* This makes the semaphore visible in ROV within CCS IDE */
}

static void register_interrupts()
{
    HwiP_Params hwiParams;

    HwiP_Params_init(&hwiParams);
    hwiParams.intNum = PING_INT_NUM;
    hwiParams.callback = ping_isr;
    HwiP_construct(&gPingHwiObj, &hwiParams);
    
    HwiP_Params_init(&hwiParams);
    hwiParams.intNum = PONG_INT_NUM;
    hwiParams.callback = pong_isr;
    HwiP_construct(&gPongHwiObj, &hwiParams);
}

static void create_user_tasks()
{
    BaseType_t xResult;

    /* Variables exported from linker script */
    extern uint32_t __TEXT_PING_START[], __TEXT_PING_END[], __DATA_PING_START[], __DATA_PING_END[];
    extern uint32_t __TEXT_PONG_START[], __TEXT_PONG_END[], __DATA_PONG_START[], __DATA_PONG_END[];

    xTaskParameters xPongTaskParams = {
        .pvTaskCode     = pong_main,
        .pcName         = "pong",
        .usStackDepth   = STACK_DEPTH(PONG_TASK_SIZE),
        .pvParameters   = NULL,
        .uxPriority     = PONG_TASK_PRI,
        .puxStackBuffer = (StackType_t *)gPongTaskStack,
        .pxTaskBuffer   = &gPongTaskObj,
        .xRegions       = {
            /** Task stack will be added as an MPU entry @ `portSTACK_REGION` as part of 
             * `xTaskCreateRestrictedStatic` API itself.
             * Hence, no need to explicitly add the same as part of task specific MPU regions below */
            [0U] = {
                /* Text for pong task */
                .pvBaseAddress   = (void *)__TEXT_PONG_START,
                .ulLengthInBytes = ((uint32_t)__TEXT_PONG_END - (uint32_t)__TEXT_PONG_START),
                .ulParameters    = ( portMPU_REGION_PRIV_RO_USER_RO_EXEC | 
                                     portMPU_REGION_NORMAL_OIWBWA_NONSHARED ),
            },
            [1U] = {
                /* Data for pong task */
                .pvBaseAddress   = (void *)__DATA_PONG_START,
                .ulLengthInBytes = ((uint32_t)__DATA_PONG_END - (uint32_t)__DATA_PONG_START),
                .ulParameters    = ( portMPU_REGION_PRIV_RW_USER_RW_NOEXEC |
                                     portMPU_REGION_NORMAL_OIWBWA_NONSHARED ),
            },
            [2U] = { 0, 0, 0 },
            [3U] = { 0, 0, 0 },
            [4U] = { 0, 0, 0 },
            [5U] = { 0, 0, 0 },
            [6U] = { 0, 0, 0 }
        }
    };

    xTaskParameters xPingTaskParams = {
        .pvTaskCode     = ping_main,
        .pcName         = "ping",
        .usStackDepth   = STACK_DEPTH(PING_TASK_SIZE),
        .pvParameters   = NULL,
        .uxPriority     = PING_TASK_PRI,
        .puxStackBuffer = (StackType_t *)gPingTaskStack,
        .pxTaskBuffer   = &gPingTaskObj,
        .xRegions       = {
            /** Task stack will be added as an MPU entry @ `portSTACK_REGION` as part of 
             * `xTaskCreateRestrictedStatic` API itself.
             * Hence, no need to explicitly add the same as part of task specific MPU regions below */
            [0U] = {
                /* Text for ping task */
                .pvBaseAddress   = (void *)__TEXT_PING_START,
                .ulLengthInBytes = ((uint32_t)__TEXT_PING_END - (uint32_t)__TEXT_PING_START),
                .ulParameters    = ( portMPU_REGION_PRIV_RO_USER_RO_EXEC | 
                                     portMPU_REGION_NORMAL_OIWBWA_NONSHARED ),
            },
            [1U] = {
                /* Data for ping task */
                .pvBaseAddress   = (void *)__DATA_PING_START,
                .ulLengthInBytes = ((uint32_t)__DATA_PING_END - (uint32_t)__DATA_PING_START),
                .ulParameters    = ( portMPU_REGION_PRIV_RW_USER_RW_NOEXEC |
                                     portMPU_REGION_NORMAL_OIWBWA_NONSHARED ),
            },
            [2U] = { 0, 0, 0 },
            [3U] = { 0, 0, 0 },
            [4U] = { 0, 0, 0 },
            [5U] = { 0, 0, 0 },
            [6U] = { 0, 0, 0 }
        }
    };

    /* Create the tasks, order of task creation does not matter for this example */
    xResult = xTaskCreateRestrictedStatic(&xPongTaskParams, &gPongTask);
    configASSERT((xResult == pdPASS)  && (gPongTask != NULL));

    xResult = xTaskCreateRestrictedStatic(&xPingTaskParams, &gPingTask);
    configASSERT((xResult == pdPASS)  && (gPingTask != NULL));
}

static void wait_for_completion()
{
    xSemaphoreTake(gSyncSem, portMAX_DELAY); /* Wait for ping task to signal test completion */
}

static void delete_user_tasks()
{
    vTaskDelete(gPingTask);
    vTaskDelete(gPongTask);
}

static void deregister_interrupts()
{
    HwiP_destruct(&gPingHwiObj);
    HwiP_destruct(&gPongHwiObj);
}

static void delete_semaphores()
{
    vSemaphoreDelete(gPingSem);
    vSemaphoreDelete(gPongSem);
    vSemaphoreDelete(gSyncSem);
}

void task_switch_mpu_main(void *args)
{
    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n");
    DebugP_log("FreeRTOS MPU Task Switch example ... start !!!\r\n");

    create_semaphores();
    register_interrupts();
    create_user_tasks();
    wait_for_completion();
    delete_user_tasks();
    deregister_interrupts();
    delete_semaphores();
    
    DebugP_log("\r\n");
    DebugP_log("All tests have passed!!\r\n");

    Board_driversClose();
    /* Dont close drivers to keep the UART driver open for console */
    /* Drivers_close(); */
}
