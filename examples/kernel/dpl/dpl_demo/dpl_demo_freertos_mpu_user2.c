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
 * All text and data in this C file is by default placed in corresponding sections reserved for user 2.
 * Hence other user mode tasks won't have access to any of the text/data in this C file.  
 */

#include <inttypes.h>

#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/TaskP.h>
 
static uint32_t gUser2Var = 0U;

extern uint32_t          gMyVar;
extern SemaphoreP_Object gMpuSyncSem;

void user2_var_increment()
{
    gUser2Var++;    /* Increment user 2 variable */
    gMyVar++;       /* Increment shared unprivileged variable */
}

void user2_main(void *args)
{
    DebugP_log("\r\n");
    DebugP_log("[FreeRTOS MPU] user 2 ... start !!!\r\n");

    user2_var_increment();
    
    DebugP_log("user 2 variable = %" PRId32 "\r\n", gUser2Var);
    DebugP_log("shared variable = %" PRId32 "\r\n", gMyVar);

    DebugP_log("[FreeRTOS MPU] user 2 ... done !!!\r\n");

    SemaphoreP_post(&gMpuSyncSem); /* Signal main thread that user 2 is done */

    TaskP_exit();
}

