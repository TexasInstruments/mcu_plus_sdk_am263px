/*
 *  Copyright (C) 2022-24 Texas Instruments Incorporated
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

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdlib.h>
#include <stdint.h>
#include <kernel/dpl/SemaphoreP.h>
#include <security_common/drivers/hsmclient/hsmclient.h>

/*==============================================================================*
 *                          Public Function definition.
 *==============================================================================*/

int32_t HsmClient_waitForBootNotify(HsmClient_t* HsmClient, uint32_t timeout)
{
    int32_t status ;

    SemaphoreP_constructBinary(&HsmClient->Semaphore,0);

    status = SemaphoreP_pend(&HsmClient->Semaphore,timeout);

    /* first wait for bootnotify from HsmServer
     * once received return SystemP_SUCCESS */
    if((status == SystemP_TIMEOUT) || (status == SystemP_FAILURE))
    {
        return SystemP_FAILURE;
    }
    else
    {
        /*TODO: check crc latency and add crc checks later */
        if(HsmClient->RespMsg.serType == HSM_MSG_BOOT_NOTIFY )
        {
            return SystemP_SUCCESS;
        }
        /* if message received is not bootnotify */
        else
        {
            return SystemP_FAILURE;
        }
    }
    /* ISR will transfer the response message to HsmClient->RespMsg */
}
