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
#include <string.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/SemaphoreP.h>
#include "tx_api.h"

int32_t SemaphoreP_constructBinary(SemaphoreP_Object *pSemaphore, uint32_t initCount)
{
    int32_t status = SystemP_FAILURE;
    UINT tx_ret;

    if(pSemaphore != NULL)
    {
        status = SystemP_SUCCESS;
    }

    if (SystemP_SUCCESS == status)
    {
        pSemaphore->isMutex = 0;
        pSemaphore->isCounting = 0;
        pSemaphore->maxCount = 1;

        tx_ret = tx_semaphore_create(&pSemaphore->semObj, "Binary Sem (DPL)", initCount);
        if(tx_ret != TX_SUCCESS) {
            status = SystemP_FAILURE;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }

    return status;
}

int32_t SemaphoreP_constructCounting(SemaphoreP_Object *pSemaphore, uint32_t initCount, uint32_t maxCount)
{
    int32_t status = SystemP_FAILURE;
    UINT tx_ret;

    if(pSemaphore != NULL)
    {
        status = SystemP_SUCCESS;
    }

    if(status == SystemP_SUCCESS)
    {
        pSemaphore->isMutex = 0;
        pSemaphore->isCounting = 1;
        pSemaphore->maxCount = maxCount;

        tx_ret = tx_semaphore_create(&pSemaphore->semObj, "Counting Sem (DPL)", initCount);
        if(tx_ret != TX_SUCCESS) {
            status = SystemP_FAILURE;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }

    return status;
}

int32_t SemaphoreP_constructMutex(SemaphoreP_Object *pSemaphore)
{
    int32_t status = SystemP_FAILURE;
    UINT tx_ret;

    if(pSemaphore != NULL)
    {
        status = SystemP_SUCCESS;
    }

    if(status == SystemP_SUCCESS)
    {
        pSemaphore->isMutex = 1;
        pSemaphore->isCounting = 0;
        pSemaphore->maxCount = 1;

        tx_ret = tx_mutex_create(&pSemaphore->mutexObj, "Mutex (DPL)", TX_TRUE);
        if(tx_ret != TX_SUCCESS) {
            status = SystemP_FAILURE;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }

    return status;
}

void SemaphoreP_destruct(SemaphoreP_Object *pSemaphore)
{
    if(pSemaphore != NULL)
    {
        if(pSemaphore->isMutex == 1U) {
            (void)tx_mutex_delete(&pSemaphore->mutexObj);
        }
        else
        {
            (void)tx_semaphore_delete(&pSemaphore->semObj);
        }

    }
}


int32_t SemaphoreP_pend(SemaphoreP_Object *pSemaphore, uint32_t timeout)
{
    int32_t status;
    UINT tx_ret;

    if(pSemaphore->isMutex == 1U)
    {
        tx_ret = tx_mutex_get(&pSemaphore->mutexObj, timeout);
    } else {
        tx_ret = tx_semaphore_get(&pSemaphore->semObj, timeout);
    }

    if(tx_ret != TX_SUCCESS)
    {
        if(tx_ret == TX_NO_INSTANCE)
        {
            status = SystemP_TIMEOUT;
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    else
    {
        status = SystemP_SUCCESS;
    }

    return status;
}

void SemaphoreP_post(SemaphoreP_Object *pSemaphore)
{
    UINT tx_ret;

    if(pSemaphore->isMutex == 1U)
    {
        tx_ret = tx_mutex_put(&pSemaphore->mutexObj);
    }
    else if(SemaphoreP_getCount(pSemaphore) < (int32_t)pSemaphore->maxCount)
    {
        /* Semaphore count is below max, proceed to post */
        if(pSemaphore->isCounting == 0U)
        {
            tx_ret = tx_semaphore_ceiling_put(&pSemaphore->semObj, 1U);
        }
        else
        {
            tx_ret = tx_semaphore_put(&pSemaphore->semObj);
        }
    }
    else
    {
        /* Semaphore count is already at max, do not post */
        tx_ret = TX_SEMAPHORE_ERROR;
    }

    (void)tx_ret; // Can't return an error.
}

/*
 *  ======== SemaphoreP_getCount ========
 */
int32_t SemaphoreP_getCount(SemaphoreP_Object *pSemaphore)
{
    ULONG current_value;
    int32_t count = 0;
    UINT tx_ret;

    if(pSemaphore->isMutex == 1U)
    {
        tx_ret = tx_mutex_info_get(&pSemaphore->mutexObj, NULL, &current_value, NULL, NULL, NULL, NULL);
    }
    else
    {
        tx_ret = tx_semaphore_info_get(&pSemaphore->semObj, NULL, &current_value, NULL, NULL, NULL);
    }

    if(tx_ret == TX_SUCCESS)
    {
        count = (int32_t)current_value;
    }

    return count;
}

