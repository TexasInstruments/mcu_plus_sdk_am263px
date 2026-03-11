/*
 *  Copyright (C) 2018-2023 Texas Instruments Incorporated
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

#include <kernel/nortos/dpl/common/TimerP_rti_priv.h>
#include <FreeRTOS.h>

#if ( portUSING_MPU_WRAPPERS == 1 )
/** Some APIs modifies RTI registers which is allowed only in privilege mode.
 * Hence use MPU wrapper based implementation. */
#include "sdk_mpu_prototypes.h"
#endif

/* RTI timer implementation for clock tick */

void TimerP_Params_init(TimerP_Params *params)
{
    TimerP_Params_initPriv(params);
}

void TimerP_setup(uint32_t baseAddr, TimerP_Params *params)
{
    /** This API modifies RTI registers which is allowed only in privilege mode.
     * Hence use MPU wrapper based implementation. */
#if ( portUSING_MPU_WRAPPERS == 1 )
    /* This will eventually end up in `TimerP_setupPriv` in privileged mode */
    MPU_TimerP_setup(baseAddr, params);
#else
    TimerP_setupPriv(baseAddr, params);
#endif
}

void TimerP_start(uint32_t baseAddr)
{
    /** This API modifies RTI registers which is allowed only in privilege mode.
     * Hence use MPU wrapper based implementation. */
#if ( portUSING_MPU_WRAPPERS == 1 )
    /* This will eventually end up in `TimerP_startPriv` in privileged mode */
    MPU_TimerP_start(baseAddr);
#else
    TimerP_startPriv(baseAddr);
#endif
}

void TimerP_stop(uint32_t baseAddr)
{
    /** This API modifies RTI registers which is allowed only in privilege mode.
     * Hence use MPU wrapper based implementation. */
#if ( portUSING_MPU_WRAPPERS == 1 )
    /* This will eventually end up in `TimerP_stopPriv` in privileged mode */
    MPU_TimerP_stop(baseAddr);
#else
    TimerP_stopPriv(baseAddr);
#endif
}

uint32_t TimerP_getCount(uint32_t baseAddr)
{
    return TimerP_getCountPriv(baseAddr);
}

uint32_t TimerP_getReloadCount(uint32_t baseAddr)
{
    return TimerP_getReloadCountPriv(baseAddr);
}

void TimerP_clearOverflowInt(uint32_t baseAddr)
{
    /** This API modifies RTI registers which is allowed only in privilege mode.
     * Hence use MPU wrapper based implementation. */
#if ( portUSING_MPU_WRAPPERS == 1 )
    /* This will eventually end up in `TimerP_clearOverflowIntPriv` in privileged mode */
    MPU_TimerP_clearOverflowInt(baseAddr);
#else
    TimerP_clearOverflowIntPriv(baseAddr);
#endif
}

uint32_t TimerP_isOverflowed(uint32_t baseAddr)
{
    return TimerP_isOverflowedPriv(baseAddr);
}
