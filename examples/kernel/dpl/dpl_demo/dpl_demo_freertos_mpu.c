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
 * All text and data in this C file is by default placed in privileged functions and privileged data sections. 
 * Hence user mode tasks won't have access to any of the text/data in this C file.
 * User1/2 task stacks defined in this file will also be placed in privileged data section. 
 * But as part of user task creation a dedicated MPU region will be configured for the task stack.  
 * 
 * - Any text/data which is not required to be in user mode tasks should be placed in this file.
 * - Any text/data which is required only by user 1 should be placed in `dpl_demo_freertos_mpu_user1.c`
 * - Any text/data which is required only by user 2 should be placed in `dpl_demo_freertos_mpu_user2.c`
 * - All other text and data is by default placed in generic text and data segments. 
 *   These will be accessible by user mode tasks unless explicitly specified by an attribute to place in a 
 *   different dedicated section with reduced access permissions.
 * 
 * Refer linker script for more details on text/data placement in various sections.
 */

 #include <stddef.h>

 #include <kernel/dpl/DebugP.h>
 #include <kernel/dpl/MpuP_armv7.h>
 #include <kernel/dpl/SemaphoreP.h>
 #include <kernel/dpl/TaskP.h>

#define USER1_TASK_PRI  (3u)
#define USER2_TASK_PRI  (2u)

#define USER1_TASK_SIZE (1024u)
uint8_t gUser1TaskStack[USER1_TASK_SIZE] __attribute__((aligned(USER1_TASK_SIZE)));

#define USER2_TASK_SIZE (1024u)
uint8_t gUser2TaskStack[USER2_TASK_SIZE] __attribute__((aligned(USER2_TASK_SIZE)));

TaskP_Object gUser1TaskObj;
TaskP_Object gUser2TaskObj;

extern SemaphoreP_Object gMpuSyncSem;

void user1_main(void *args);
void user2_main(void *args);

void dpl_demo_freertos_mpu_main(void *args)
{
    int32_t                status;
    TaskP_ParamsRestricted taskParams; 

    /* Variables exported from linker script */
    extern uint32_t __TEXT_USER1_START[], __TEXT_USER1_END[], __DATA_USER1_START[], __DATA_USER1_END[];
    extern uint32_t __TEXT_USER2_START[], __TEXT_USER2_END[], __DATA_USER2_START[], __DATA_USER2_END[];

    /* Create semaphore for user threads to signal current main task */
    status = SemaphoreP_constructCounting(&gMpuSyncSem, 0U, 2U); /* Max count 2 for two user threads */
    DebugP_assert(status==SystemP_SUCCESS);

    DebugP_log("\r\n");
    DebugP_log("[FreeRTOS MPU] Creating User mode tasks... !!!\r\n");

    TaskP_ParamsRestricted_init(&taskParams);
    taskParams.params.name      = "user1",
    taskParams.params.stackSize = USER1_TASK_SIZE,
    taskParams.params.stack     = gUser1TaskStack,
    taskParams.params.priority  = USER1_TASK_PRI,
    taskParams.params.args      = NULL,
    taskParams.params.taskMain  = user1_main,
    /* Text for user 1 task */
    taskParams.regionConfig[0U].baseAddr  = (uint32_t)__TEXT_USER1_START,
    taskParams.regionConfig[0U].sizeBytes = ((uint32_t)__TEXT_USER1_END - (uint32_t)__TEXT_USER1_START),
    taskParams.regionConfig[0U].attrs.isCacheable    = 1U,
    taskParams.regionConfig[0U].attrs.isBufferable   = 1U,
    taskParams.regionConfig[0U].attrs.isSharable     = 0U,
    taskParams.regionConfig[0U].attrs.isExecuteNever = 0U,
    taskParams.regionConfig[0U].attrs.tex            = 1U,
    taskParams.regionConfig[0U].attrs.accessPerm     = MpuP_AP_ALL_R,
    /* Data for user 1 task */
    taskParams.regionConfig[1U].baseAddr  = (uint32_t)__DATA_USER1_START,
    taskParams.regionConfig[1U].sizeBytes = ((uint32_t)__DATA_USER1_END - (uint32_t)__DATA_USER1_START),
    taskParams.regionConfig[1U].attrs.isCacheable    = 1U,
    taskParams.regionConfig[1U].attrs.isBufferable   = 1U,
    taskParams.regionConfig[1U].attrs.isSharable     = 0U,
    taskParams.regionConfig[1U].attrs.isExecuteNever = 1U,
    taskParams.regionConfig[1U].attrs.tex            = 1U,
    taskParams.regionConfig[1U].attrs.accessPerm     = MpuP_AP_ALL_RW,
    /** Task stack will be added as an MPU entry @ `portSTACK_REGION` by the task create API on its own.
     * Hence, no need to explicitly add the same as part of task specific MPU regions */
    status = TaskP_constructRestricted(&gUser1TaskObj, &taskParams);
    DebugP_assert(status==SystemP_SUCCESS);

    TaskP_ParamsRestricted_init(&taskParams);
    taskParams.params.name      = "user2",
    taskParams.params.stackSize = USER2_TASK_SIZE,
    taskParams.params.stack     = gUser2TaskStack,
    taskParams.params.priority  = USER2_TASK_PRI,
    taskParams.params.args      = NULL,
    taskParams.params.taskMain  = user2_main,
    /* Text for user 2 task */
    taskParams.regionConfig[0U].baseAddr  = (uint32_t)__TEXT_USER2_START,
    taskParams.regionConfig[0U].sizeBytes = ((uint32_t)__TEXT_USER2_END - (uint32_t)__TEXT_USER2_START),
    taskParams.regionConfig[0U].attrs.isCacheable    = 1U,
    taskParams.regionConfig[0U].attrs.isBufferable   = 1U,
    taskParams.regionConfig[0U].attrs.isSharable     = 0U,
    taskParams.regionConfig[0U].attrs.isExecuteNever = 0U,
    taskParams.regionConfig[0U].attrs.tex            = 1U,
    taskParams.regionConfig[0U].attrs.accessPerm     = MpuP_AP_ALL_R,
    /* Data for user 2 task */
    taskParams.regionConfig[1U].baseAddr  = (uint32_t)__DATA_USER2_START,
    taskParams.regionConfig[1U].sizeBytes = ((uint32_t)__DATA_USER2_END - (uint32_t)__DATA_USER2_START),
    taskParams.regionConfig[1U].attrs.isCacheable    = 1U,
    taskParams.regionConfig[1U].attrs.isBufferable   = 1U,
    taskParams.regionConfig[1U].attrs.isSharable     = 0U,
    taskParams.regionConfig[1U].attrs.isExecuteNever = 1U,
    taskParams.regionConfig[1U].attrs.tex            = 1U,
    taskParams.regionConfig[1U].attrs.accessPerm     = MpuP_AP_ALL_RW,
    /** Task stack will be added as an MPU entry @ `portSTACK_REGION` by the task create API on its own.
     * Hence, no need to explicitly add the same as part of task specific MPU regions */
    status = TaskP_constructRestricted(&gUser2TaskObj, &taskParams);
    DebugP_assert(status==SystemP_SUCCESS);

    /* Wait twice for both the user tasks to run to completion */
    SemaphoreP_pend(&gMpuSyncSem, SystemP_WAIT_FOREVER); 
    SemaphoreP_pend(&gMpuSyncSem, SystemP_WAIT_FOREVER);

    TaskP_destruct(&gUser1TaskObj);
    TaskP_destruct(&gUser2TaskObj);
}
