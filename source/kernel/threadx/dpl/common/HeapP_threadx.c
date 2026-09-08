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


#include <stdlib.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/HeapP.h>
#include "tx_api.h"

int32_t HeapP_construct( HeapP_Object *heapObj, void *heapAddr, size_t heapSize )
{
    int32_t status = SystemP_FAILURE;

    DebugP_assert(heapObj != NULL);
    DebugP_assert(heapAddr != NULL);
    DebugP_assert(heapSize > 0U);
    UINT tx_status = TX_SUCCESS;

    tx_status = tx_byte_pool_create(&heapObj->heapHndl, "dpl pool", heapAddr, (ULONG)heapSize);
    
    if(tx_status == TX_SUCCESS)
    {
        status = SystemP_SUCCESS;
    }
    return status;
}

void HeapP_destruct(HeapP_Object *heapObj)
{
    DebugP_assert(heapObj != NULL);

    tx_byte_pool_delete(&heapObj->heapHndl);
}

void* HeapP_alloc( HeapP_Object *heapObj, size_t allocSize )
{
    uint32_t tx_status = TX_SUCCESS;
    void *ptr = NULL;

    DebugP_assert(heapObj != NULL);
    DebugP_assert(allocSize > 0U);

    tx_status = tx_byte_allocate(&heapObj->heapHndl, &ptr, (ULONG)allocSize, TX_NO_WAIT);

    if(tx_status != TX_SUCCESS)
    {
        ptr = NULL;
    }

    return ptr;
}

int32_t HeapP_free( HeapP_Object *heapObj, void * ptr )
{
    int32_t status = SystemP_FAILURE;
    uint32_t tx_status = TX_SUCCESS;

    (void)heapObj;
    DebugP_assert(ptr != NULL);

    tx_status = tx_byte_release(ptr);
    
    if(tx_status == TX_SUCCESS)
    {
        status = SystemP_SUCCESS;
    }
    return status;
}

size_t HeapP_getFreeHeapSize( HeapP_Object *heapObj )
{
    uint32_t tx_status = TX_SUCCESS;
    ULONG available_bytes = 0u;

    DebugP_assert(heapObj != NULL);

    tx_status =  tx_byte_pool_info_get(&heapObj->heapHndl, NULL, &available_bytes, NULL, NULL, NULL, NULL);

    if(tx_status != TX_SUCCESS)
    {
        available_bytes = 0u;
    }

    return (size_t)available_bytes;
}

size_t HeapP_getMinimumEverFreeHeapSize( HeapP_Object *heap )
{
    (void)heap;
    // Not supported in ThreadX
    return 0;
}

int32_t HeapP_getHeapStats( HeapP_Object *heap, HeapP_MemStats * pHeapStats )
{
    (void)heap;
    (void)pHeapStats;

    // Not supported in ThreadX
    return SystemP_SUCCESS;
}

