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

#include "FreeRTOS.h"
#include "sdk_mpu_syscall_numbers.h"

#include <kernel/nortos/dpl/common/TimerP_rti_priv.h>

/**
 * @brief Array of SDK system call implementation functions.
 *
 * The index in the array MUST match the corresponding SDK system call number (without offset)
 * defined in sdk_mpu_syscall_numbers.h.
 */
PRIVILEGED_DATA UBaseType_t uxSdkSystemCallImplementations[ NUM_SDK_SYSTEM_CALLS ] =
{
    ( UBaseType_t ) TimerP_setupPriv,               /* SYSTEM_CALL_SDK_TimerP_setup. */
    ( UBaseType_t ) TimerP_startPriv,               /* SYSTEM_CALL_SDK_TimerP_start. */
    ( UBaseType_t ) TimerP_stopPriv,                /* SYSTEM_CALL_SDK_TimerP_stop. */
    ( UBaseType_t ) TimerP_clearOverflowIntPriv,    /* SYSTEM_CALL_SDK_TimerP_clearOverflowInt. */
};
 