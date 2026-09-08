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

#include<kernel/dpl/EventP.h>
#include<kernel/dpl/HwiP.h>
#include "tx_api.h"


int32_t EventP_construct(EventP_Object *pEvent)
{
    int32_t status = SystemP_SUCCESS;
    UINT tx_ret;

    tx_ret = tx_event_flags_create(&pEvent->eventObj, (char *)"dpl event");

    if(tx_ret != TX_SUCCESS) {
        status = SystemP_FAILURE;
    }

    return status;
}

void EventP_destruct(EventP_Object *pEvent)
{

    if(pEvent != NULL)
    {
        tx_event_flags_delete(&pEvent->eventObj);
    }
}

int32_t EventP_waitBits(EventP_Object  *pEvent,
                        uint32_t       bitsToWaitFor,
                        uint8_t        clearOnExit,
                        uint8_t        waitForAll,
                        uint32_t       timeToWaitInTicks,
                        uint32_t       *eventBits)
{
    int32_t         status;
    UINT            tx_ret;
    UINT            get_option;
    ULONG           actual_flags_ptr;

    if((pEvent == NULL) || (clearOnExit >= 2U) || (waitForAll >= 2U) || (eventBits == NULL))
    {
        status = SystemP_FAILURE;
    }
    else
    {
        *eventBits = 0U;

        if(waitForAll == 1U) {
            if(clearOnExit == 1U) {
                get_option = TX_AND_CLEAR;
            }
            else
            {
                get_option = TX_AND;
            }
        } else {
            if(clearOnExit == 1U) {
                get_option = TX_OR_CLEAR;
            }
            else
            {
                get_option = TX_OR;
            }
        }

        tx_ret = tx_event_flags_get(&pEvent->eventObj, bitsToWaitFor, get_option, &actual_flags_ptr, timeToWaitInTicks);


        if(tx_ret == TX_SUCCESS) {
            *eventBits = (uint32_t)actual_flags_ptr;
            status = SystemP_SUCCESS;
        }
        else
        {
            status = SystemP_FAILURE;
        }

    }

    return status;
}

int32_t EventP_setBits(EventP_Object *pEvent, uint32_t bitsToSet)
{
    int32_t         status;
    UINT            tx_ret;

    if(pEvent == NULL)
    {
        status = SystemP_FAILURE;
    }
    else
    {

        tx_ret = tx_event_flags_set(&pEvent->eventObj, bitsToSet, TX_OR);
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

int32_t EventP_clearBits(EventP_Object *pEvent, uint32_t bitsToClear)
{
    int32_t         status;
    UINT            tx_ret;

    if(pEvent == NULL)
    {
        status = SystemP_FAILURE;
    }
    else
    {

        tx_ret = tx_event_flags_set(&pEvent->eventObj, ~bitsToClear, TX_AND);
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

int32_t EventP_getBits(EventP_Object *pEvent, uint32_t *eventBits)
{
    int32_t         status;
    UINT            tx_ret;
    ULONG           flags = 0u;

    if((pEvent == NULL) || (eventBits == NULL))
    {
        status = SystemP_FAILURE;
    }
    else
    {
        *eventBits = 0U;

        tx_ret = tx_event_flags_info_get(&pEvent->eventObj, NULL, &flags, NULL, NULL, NULL);
        if(tx_ret != TX_SUCCESS) {
            status = SystemP_FAILURE;
        }
        else
        {
            *eventBits = (uint32_t)flags;
            status = SystemP_SUCCESS;
        }
    }
    return status;
}
