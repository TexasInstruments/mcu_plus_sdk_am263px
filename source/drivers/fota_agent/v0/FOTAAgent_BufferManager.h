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

#ifndef __FOTAAGENT_BUFFER_MANAGER_H__
#define __FOTAAGENT_BUFFER_MANAGER_H__

#include <stdint.h>
#include "fota_agent.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FotaAgentP_BufferManager_BUFFERFULL 		((int32_t)-3)
#define FotaAgentP_BufferManager_ADDRESS_DISCONT 	((int32_t)-4)
#define FotaAgentP_BufferManager_ADDRESS_UNALIGNED 	((int32_t)-5)

/**
 * @brief Initilize buffer manager state machine
 *
 * The role of this function is to init all the initial varaibles
 * with some default varaibles and also the state machine to its
 * initial state, if any.
 *
 * @param fotaAgentHandle pointer to fotaAgent handle
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_BufferManager_init(FOTAAgent_Handle *fotaAgentHandle);

/**
 * @brief Update internal buffer with new data
 *
 * Udpate the internal buffer with new information.
 * At any point of time, after calling this function with newData and flashOffset,
 * this API will have the following information:
 * 1. size of internal buffer filled
 * 2. last address at which data is supposed to be written
 *
 * In case, internal buffer is 4K, then, when internal buffer is full, at that time,
 * this API will have data to be written and the address where it is be written.
 *
 * @param fotaAgentHandle pointer to fotaAgent handle.
 * @param newData new data.
 * @param flashOffset offset in flash at which it is supposed to be written.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_BufferManager_update(FOTAAgent_Handle *fotaAgentHandle, uint8_t newData,
                                            uint32_t flashOffset);

/**
 * @brief Calculate Address at which internal buffer to be flushed at.abort
 *
 * Before flush function is called, this function is called internally, to calculate
 * the address at which internal buffer is to be flushed.
 *
 * This address is that this function calculate aligns with erase size.
 *
 * @param fotaAgentHandle
 * @param addr
 * @return int32_t
 */
int32_t FOTAAgent_BufferManager_calculateFlushAddress(FOTAAgent_Handle *fotaAgentHandle, uint32_t * addr);

/**
 * @brief Flush internal buffer to flash.
 *
 * When this function is called, doesn't matter if the internal buffer is full or
 * not, it will calculate the address aligned to erase size and write the interal
 * buffer.
 *
 * @param fotaAgentHandle pointer to fotaAgent handle.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_BufferManager_flush(FOTAAgent_Handle *fotaAgentHandle);

/**
 * @brief Check if the new data's address is discontinuous or not.
 *
 * essentially this function returns (last_address - address) == sizeof(uint8_t).
 *
 * @param fotaAgentHandle pointer to fotaAgent handle.
 * @param address SOC address space of new data
 * @param newData new data byte
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_BufferManager_checkAddressDiscontinuity(
	FOTAAgent_Handle *fotaAgentHandle, uint32_t address, uint8_t newData);

#ifdef __cplusplus
}
#endif


#endif //__FOTAAGENT_BUFFER_MANAGER_H__