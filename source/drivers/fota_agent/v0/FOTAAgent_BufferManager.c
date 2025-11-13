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

#include <kernel/dpl/SystemP.h>
#include "FOTAAgent_BufferManager.h"

#define SOC_ADDRESS_TO_FSS_DATA_REGION_MASK (~(0xF0000000U))

int32_t FOTAAgent_BufferManager_init(FOTAAgent_Handle *fotaAgentHandle)
{
	int32_t status = SystemP_FAILURE;
	if(NULL != fotaAgentHandle)
	{
		fotaAgentHandle->BufferManager_lastAddress = 0;
        fotaAgentHandle->pProcessingBuffer_len = 0;
		status = SystemP_SUCCESS;
	}
	return status;
}

int32_t FOTAAgent_BufferManager_update(FOTAAgent_Handle *fotaAgentHandle, uint8_t new_data, uint32_t flashOffset)
{
	int32_t status = SystemP_FAILURE;
	uint32_t *bufferLen = (uint32_t *)NULL;
	uint32_t allowedBufferLen = 0;

	if(NULL != fotaAgentHandle)
	{
		bufferLen = &(fotaAgentHandle->pProcessingBuffer_len);
		allowedBufferLen = fotaAgentHandle->params.flsopsParams.eraseSizeInBytes;
		if((*bufferLen) < allowedBufferLen)
		{
			*(fotaAgentHandle->params.pProcessingBuffer + (*bufferLen)) = new_data;
			fotaAgentHandle->flashWritePointer = flashOffset;
			*bufferLen = (*bufferLen) + 1;
			status = SystemP_SUCCESS;
		}
		else
		{
			status = FotaAgentP_BufferManager_BUFFERFULL;
		}
	}
	return status;
}

int32_t FOTAAgent_BufferManager_calculateFlushAddress(FOTAAgent_Handle *fotaAgentHandle, uint32_t * addr)
{
	int32_t status = SystemP_FAILURE;
	uint32_t allowedBufferLen = 0;
	uint32_t destFlashOffsetInBytes = 0;

	if(NULL != fotaAgentHandle && NULL != addr)
	{
        destFlashOffsetInBytes = fotaAgentHandle->flashWritePointer;
        allowedBufferLen = fotaAgentHandle->params.flsopsParams.eraseSizeInBytes;
        destFlashOffsetInBytes = allowedBufferLen * (uint32_t)((double)destFlashOffsetInBytes / (double)allowedBufferLen) ;
        destFlashOffsetInBytes += fotaAgentHandle->wrOffset;
        destFlashOffsetInBytes = destFlashOffsetInBytes & SOC_ADDRESS_TO_FSS_DATA_REGION_MASK;
        *addr = destFlashOffsetInBytes;
        if((destFlashOffsetInBytes % allowedBufferLen) != 0)
        {
            status = FotaAgentP_BufferManager_ADDRESS_UNALIGNED;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }
	return status;
}

int32_t FOTAAgent_BufferManager_flush(FOTAAgent_Handle *fotaAgentHandle)
{
	int32_t status = SystemP_FAILURE;
	uint32_t *bufferLen = (uint32_t *)NULL;
	uint32_t destFlashOffsetInBytes = 0;
	uint32_t allowedBufferLen = 0;

	if(NULL != fotaAgentHandle)
	{
		bufferLen = &(fotaAgentHandle->pProcessingBuffer_len);
		if(*bufferLen > 0)
		{
			allowedBufferLen = fotaAgentHandle->params.flsopsParams.eraseSizeInBytes;
            status = FOTAAgent_BufferManager_calculateFlushAddress(fotaAgentHandle, &destFlashOffsetInBytes);
            if(SystemP_SUCCESS == status)
            {
                status = FLSOPSKD_erase(&(fotaAgentHandle->flopsHandle), destFlashOffsetInBytes);
                if(SystemP_SUCCESS == status)
                {
                    status = FLSOPSKD_write(
                        &(fotaAgentHandle->flopsHandle),
                        destFlashOffsetInBytes,
                        (uint8_t*)(fotaAgentHandle->params.pProcessingBuffer),
                        *bufferLen
                    );
                    if(SystemP_SUCCESS == status)
                    {
                        memset((void*)(fotaAgentHandle->params.pProcessingBuffer), 0xFF, allowedBufferLen);
                        *bufferLen = 0;
                    }
                }
            }
		}
		else
		{
			status = SystemP_SUCCESS;
		}
	}
	return status;
}

int32_t FOTAAgent_BufferManager_checkAddressDiscontinuity(
	FOTAAgent_Handle *fotaAgentHandle, uint32_t address, uint8_t new_data)
{
	int32_t status = SystemP_FAILURE;
	int32_t diff = 0;
	
	if(NULL != fotaAgentHandle)
	{
		diff = address - fotaAgentHandle->BufferManager_lastAddress;
		if(diff == sizeof(new_data))
		{
			status = SystemP_SUCCESS;
		}
		else
		{
			status = FotaAgentP_BufferManager_ADDRESS_DISCONT;
		}
		fotaAgentHandle->BufferManager_lastAddress = address;
	}
	return status;
}

