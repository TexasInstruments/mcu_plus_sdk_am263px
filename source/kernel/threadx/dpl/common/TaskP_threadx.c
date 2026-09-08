/*
 *  Copyright (C) 2018-2024 Texas Instruments Incorporated
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

#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/ClockP.h>
#include "tx_api.h"

void vApplicationLoadHook(void);

#define TaskP_LOAD_CPU_LOAD_SCALE   (10000U)
#define TaskP_REGISTRY_MAX_ENTRIES  (32u)
#define TaskP_STACK_SIZE_MIN        (TX_MINIMUM_STACK)


typedef struct {
    TaskP_Object *taskRegistry[TaskP_REGISTRY_MAX_ENTRIES];
    uint32_t lastTotalTime;
    uint64_t accTotalTime;
    uint32_t idleTskLastRunTime;
    uint64_t idleTskAccRunTime;

} TaskP_Ctrl;


TaskP_Ctrl gTaskP_ctrl;
static uint32_t TaskP_adjustPriority(const TaskP_Params *params)
{
    uint32_t priority = params->priority;

    /* if priority is out of range, adjust to bring it in range */
    priority = (priority > TaskP_PRIORITY_HIGHEST) ? TaskP_PRIORITY_HIGHEST : priority;
    priority = (priority <= TaskP_PRIORITY_LOWEST) ? TaskP_PRIORITY_LOWEST : priority;

    return priority;
}

static void TaskP_addToRegistry(TaskP_Object *task)
{
    TX_INTERRUPT_SAVE_AREA
    uint32_t i;

    /* Disable interrupts. */
    TX_DISABLE

    for(i=0; i<TaskP_REGISTRY_MAX_ENTRIES; i++)
    {
        if(gTaskP_ctrl.taskRegistry[i]==NULL)
        {
            gTaskP_ctrl.taskRegistry[i] = task;
            break;
        }
    }

    /* Restore interrupts. */
    TX_RESTORE
}

static void TaskP_removeFromRegistry(TaskP_Object *task)
{
    TX_INTERRUPT_SAVE_AREA
    uint32_t i;

    /* Disable interrupts. */
    TX_DISABLE

    for(i=0; i<TaskP_REGISTRY_MAX_ENTRIES; i++)
    {
        if(gTaskP_ctrl.taskRegistry[i]==task)
        {
            gTaskP_ctrl.taskRegistry[i] = NULL;
            break;
        }
    }

    /* Restore interrupts. */
    TX_RESTORE
}

static uint32_t TaskP_calcCounterDiff(uint32_t cur, uint32_t last)
{
    uint32_t delta;

    if(cur >= last)
    {
        delta = cur - last;
    }
    else
    {
        delta = (  0xFFFFFFFFU - last ) + cur;
    }
    return delta;
}

static uint32_t TaskP_calcCpuLoad(uint64_t taskTime, uint64_t totalTime)
{
    uint32_t cpuLoad;

    cpuLoad = (uint32_t)(taskTime * TaskP_LOAD_CPU_LOAD_SCALE) / totalTime;
    if( cpuLoad > TaskP_LOAD_CPU_LOAD_SCALE)
    {
        cpuLoad = TaskP_LOAD_CPU_LOAD_SCALE;
    }
    return cpuLoad;
}

static VOID TaskP_wrapper(ULONG args)
{
    TaskP_Object *task;

    task = (TaskP_Object *)args;

    task->taskMain(task->args);
}


void TaskP_Params_init(TaskP_Params *params)
{
    params->name = "Task (DPL)";
    params->stackSize = 0;
    params->stack = NULL;
    params->priority = ((TaskP_PRIORITY_HIGHEST - TaskP_PRIORITY_LOWEST) /(uint32_t) 2);
    params->args = NULL;
    params->taskMain = NULL;
}

int32_t TaskP_construct(TaskP_Object *taskObj, const TaskP_Params *params)
{
    int32_t status = SystemP_SUCCESS;
    UINT tx_ret;
    uint32_t priority;

    DebugP_assert(params != NULL);
    DebugP_assert(taskObj != NULL);

    DebugP_assert(params->stackSize >= TaskP_STACK_SIZE_MIN);
    DebugP_assert( (params->stackSize & (8U - 1U)) == 0U);  // Check for 8 byte alignment
    DebugP_assert(params->stack != NULL);
    DebugP_assert( ((uintptr_t)params->stack & (8U - 1U)) == 0U); // Check for 8 byte alignment

    DebugP_assert(params->taskMain != NULL );

    /* if priority is out of range, adjust to bring it in range */
    priority = TaskP_adjustPriority(params);

    taskObj->lastRunTime = 0;
    taskObj->accRunTime = 0;

    TaskP_addToRegistry(taskObj);

    taskObj->taskMain = params->taskMain;
    taskObj->args = params->args;

    tx_ret = tx_thread_create(&taskObj->taskObj,
                              (CHAR *)params->name,
                              TaskP_wrapper,
                              (ULONG)taskObj,
                              params->stack,
                              params->stackSize,
                              priority,
                              priority,
                              TX_NO_TIME_SLICE,
                              TX_AUTO_START);

    if(tx_ret != TX_SUCCESS)
    {
        status = SystemP_FAILURE;
    }

    return status;
}

void TaskP_destruct(TaskP_Object *taskObj)
{
    if(taskObj != NULL)
    {
        tx_thread_delete(&taskObj->taskObj);

        TaskP_removeFromRegistry(taskObj);
    }
}

void* TaskP_getHndl(TaskP_Object *taskObj)
{
    return  (void*)&taskObj->taskObj;
}

void TaskP_yield(void)
{
    tx_thread_relinquish();
}

void TaskP_exit(void)
{
    TX_THREAD *thread = tx_thread_identify();
    tx_thread_delete(thread);
}

