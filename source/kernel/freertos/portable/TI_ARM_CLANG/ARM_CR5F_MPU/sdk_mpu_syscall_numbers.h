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

#ifndef SDK_MPU_SYSCALL_NUMBERS_H
#define SDK_MPU_SYSCALL_NUMBERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu_syscall_numbers.h"

/* Numbers assigned to various system calls. */
#define SYSTEM_CALL_SDK_TimerP_setup               (NUM_SYSTEM_CALLS + 0)
#define SYSTEM_CALL_SDK_TimerP_start               (NUM_SYSTEM_CALLS + 1)
#define SYSTEM_CALL_SDK_TimerP_stop                (NUM_SYSTEM_CALLS + 2)
#define SYSTEM_CALL_SDK_TimerP_clearOverflowInt    (NUM_SYSTEM_CALLS + 3)
/* Total number of SDK system calls. */
#define NUM_SDK_SYSTEM_CALLS                       (4) 
/* Total number of system calls (kernel + SDK). */ 
#define TOTAL_NUM_SYSTEM_CALLS                     (NUM_SYSTEM_CALLS +  NUM_SDK_SYSTEM_CALLS)  

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* SDK_MPU_SYSCALL_NUMBERS_H */
 