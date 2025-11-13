/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
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
#include <middleware/tiELFuParser/tielfup32.h>
#include "fota_agent.h"
#include "FOTAAgent_BufferManager.h"

#define DEFAULT_VALUE 0xA9U

int32_t FOTAAgent_Params_init(FOTAAgent_Params *params)
{
    int32_t status = SystemP_FAILURE;
	if(NULL != params)
	{
		memset((void*)params, 0, sizeof(FOTAAgent_Params));
		status = FLSOPSKD_Params_init(&(params->flsopsParams));
	}
	return status;
}

int32_t FOTAAgent_init(FOTAAgent_Handle *pHandle, FOTAAgent_Params *params)
{
    int32_t status = SystemP_FAILURE;
	if(NULL != params && NULL != pHandle)
	{
		memcpy((void*)&(pHandle->params), (void*)params, sizeof(FOTAAgent_Params));
		status = FLSOPSKD_init(&(pHandle->flopsHandle), &(params->flsopsParams));
	}
	return status;
}

int32_t FOTAAgent_writeStart(FOTAAgent_Handle *pHandle, uint32_t wrOffset, char doFlashSegments)
{
	int32_t status = SystemP_FAILURE;
	if(NULL != pHandle)
	{
		memset(
			(void*)(pHandle->params.pProgramHeader),
			DEFAULT_VALUE,
			(pHandle->params.programHeaderCnt) * sizeof(ELFUP_ELFPH)
		);
		memset(
			(void*)(pHandle->params.pProcessingBuffer),
			0xFF,
			sizeof(uint8_t) * (pHandle->params.flsopsParams.eraseSizeInBytes)
		);
		pHandle->doFlashSegments = doFlashSegments;
		pHandle->filePointer = 0;
		pHandle->flashWritePointer = 0;
		pHandle->wrOffset = wrOffset;
		status = ELFUP_init(&(pHandle->elfupHandle), (pHandle->params).pProgramHeader, (pHandle->params).programHeaderCnt);
		if(SystemP_SUCCESS == status)
		{
			status = FOTAAgent_BufferManager_init(pHandle);
		}
	}
	return status;
}

int32_t FOTAAgent_writeEnd(FOTAAgent_Handle *pHandle)
{
	int32_t status = SystemP_FAILURE;
	if(NULL != pHandle)
	{
		status = FOTAAgent_BufferManager_flush(pHandle);
	}
	return status;
}

int32_t FOTAAgent_writeUpdate(FOTAAgent_Handle *pHandle, uint8_t *buf, size_t bufSizeInBytes)
{
	int32_t status = SystemP_FAILURE;
	size_t bytesProcessed = 0;
	uint8_t newIncomingByte = 0;
	ELFUP_ELFPH programHeader;

	if(NULL != pHandle)
	{
		if((0 == bufSizeInBytes) || (NULL == buf))
		{
			status = SystemP_SUCCESS;
		}
		else
		{
			if(TRUE == pHandle->doFlashSegments)
			{
				bytesProcessed = 0;
				do
				{
					newIncomingByte = *buf++;
					status = ELFUP_update(&(pHandle->elfupHandle), newIncomingByte);
					if(SystemP_SUCCESS == status)
					{
						int16_t gotSegmentData = ELFUP_isPartOfSegment(
							&(pHandle->elfupHandle),
							pHandle->filePointer,
							&programHeader
						);
						if(SystemP_SUCCESS == gotSegmentData)
						{
							if(programHeader.ELFPH.type == PT_LOAD)
							{
								/*
									calculate the destination address of each byte.

									P = start of ELF file which should be always 0.
									A = programHeader.offset = current bytes corresponding data
									    segment starting offset
									B = programHeader.paddr = destination address.
									C = FOTAAgent_Handle->filePointer = offset of current byte.

									|----------------------------------|-----------|
									P(=0)                              A           C

									destiantion address = (C - A) + B
								*/
								uint32_t destFlashOffsetInBytes =
									(pHandle->filePointer - programHeader.ELFPH.offset) + programHeader.ELFPH.paddr;

								status = FOTAAgent_BufferManager_checkAddressDiscontinuity(
									pHandle,
									destFlashOffsetInBytes,
									newIncomingByte
								);
								if(SystemP_SUCCESS == status)
								{
									status = FOTAAgent_BufferManager_update(pHandle, newIncomingByte, destFlashOffsetInBytes);
									if(FotaAgentP_BufferManager_BUFFERFULL == status)
									{
										/* in the current program header's data, buffer got full */
										status = FOTAAgent_BufferManager_flush(pHandle);
										if(SystemP_SUCCESS == status)
										{
											status = FOTAAgent_BufferManager_update(pHandle, newIncomingByte, destFlashOffsetInBytes);
										}
									}
								}
								else if (FotaAgentP_BufferManager_ADDRESS_DISCONT == status)
								{
									/* Address discontinued, flush the current buffer and start again */
									status = FOTAAgent_BufferManager_flush(pHandle);
									if(SystemP_SUCCESS == status)
									{
										status = FOTAAgent_BufferManager_update(pHandle, newIncomingByte, destFlashOffsetInBytes);
									}
								}
							}
						}
					}
					pHandle->filePointer += 1;
				}
				while((++bytesProcessed < bufSizeInBytes) && (SystemP_SUCCESS == status));
			}
			else
			{
				bytesProcessed = 0;
				do
				{
					newIncomingByte = *buf++;
					status = FOTAAgent_BufferManager_update(pHandle, newIncomingByte, pHandle->filePointer);
					if(FotaAgentP_BufferManager_BUFFERFULL == status)
					{
						status = FOTAAgent_BufferManager_flush(pHandle);
						if (SystemP_SUCCESS == status)
						{
							status = FOTAAgent_BufferManager_update(pHandle, newIncomingByte, pHandle->filePointer);
						}
					}
					pHandle->filePointer += 1;
				} while ((++bytesProcessed < bufSizeInBytes) && (SystemP_SUCCESS == status));
			}
		}
	}
	return status;
}

int32_t FOTAAgent_getFLSOPSKDHandle(FOTAAgent_Handle *pHandle, FLSOPSKD_Handle* flsopskdHandle)
{
	int32_t status = SystemP_FAILURE;
	if((NULL != pHandle) && (NULL != flsopskdHandle))
	{
		flsopskdHandle = &(pHandle->flopsHandle);
		status = SystemP_SUCCESS;
	}
	return status;
}
