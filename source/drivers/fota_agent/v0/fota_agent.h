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

/**
 *  \defgroup DRV_FOTA_AGENT_MODULE APIs for FOTA Agent
 *  \ingroup DRV_MODULE
 *
 *  This module contains APIs to program and use the FOTA Agent driver.
 *
 *  @{
 */

/**
 *  \file fota_agent.h
 *
 *  \brief FOTA Agent Driver API/interface file.
 *
 */

#ifndef __FOTAAgent__H_
#define __FOTAAgent__H_

#ifdef __cplusplus
extern "C"
{
#endif

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdio.h>
#include <kernel/dpl/DebugP.h>
#include <stdlib.h>
#include <string.h>
#include <drivers/flsopskd.h>
#include <middleware/tiELFuParser/tielfup32.h>
#include <drivers/spinlock.h>

/* ========================================================================== */
/*                             Macros & Typedefs                              */
/* ========================================================================== */

#define FOTAAGENT_WRITEOFFSET_DNC (0U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/**
 *  \brief FOTA Agent Driver Handle
 */
typedef struct
{
    size_t programHeaderCnt;
    ELFUP_ELFPH *pProgramHeader;
    uint8_t * pProcessingBuffer;
    FLSOPSKD_Params flsopsParams;
}FOTAAgent_Params;

typedef struct
{
    char doFlashSegments;
    uint32_t BufferManager_lastAddress;
    uint32_t pProcessingBuffer_len;
    uint32_t filePointer;
    uint32_t flashWritePointer;
    uint32_t wrOffset;
    FLSOPSKD_Handle flopsHandle;
    ELFUP_Handle elfupHandle;
    FOTAAgent_Params params;
} FOTAAgent_Handle;

/**
 * @brief Initilize params with default values.
 *
 * @param params pointer to parameter struct
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_Params_init(FOTAAgent_Params *params);

/**
 * @brief Initilize FOTA Agent API
 *
 * This API initilize FOTA agent and underneath modules.
 *
 * @param pHandle pointer to FOTA Agent handle
 * @param params pointer to parameter struct
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_init(FOTAAgent_Handle *pHandle, FOTAAgent_Params *params);

/**
 * @brief Logically start writes and initilize state internal machine.
 *
 * Currently, FOTAAgent only support binary file of format .mcelf and .mcelf_xip.
 * if file type is mcelf_xip then doFlashSegments is TRUE else it is FALSE.
 * doFlashSegments here signals to parse the file, extracts the data segments and
 * only flash those data segments, instead of entire file.
 *
 * In case doFlashSegments is TRUE, wrOffset argument is ignored as this information
 * is stored inside the file. However, wrOffset information needs to be provided
 * explicitly given via wrOffset argument.
 *
 * wrOffset argument is the offset (in bytes) at which file needs to be written.
 *
 * @param pHandle pointer to FOTA Agent handle
 * @param wrOffset flash offset at which file to write
 * @param doFlashSegments flash data segments only instead of entire file
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_writeStart(FOTAAgent_Handle *pHandle, uint32_t wrOffset, char doFlashSegments);

/**
 * @brief Update internal machine with new data
 *
 * Update internal state machine with new data. Its not that on providing every one byte
 * this function will write to flash. Flash constraints on erase size. For exmaple, if
 * erase is sector wise of 4096 bytes (as specified in params) then in that case, this
 * function will wait for this amount of data to be recieved and then send it to the
 * flash.
 *
 * @param pHandle pointer to FOTA Agent handle
 * @param buf pointer to buffer with the data
 * @param size size of buf
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_writeUpdate(FOTAAgent_Handle *pHandle, uint8_t *buf, uint32_t size);

/**
 * @brief Logically end writes and end the state machine.
 *
 * Other than ending the internal state machine, this function will also
 * flash any remaining data that remains to be programmed.
 *
 * @param pHandle pointer to FOTA Agent handle
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_writeEnd(FOTAAgent_Handle *pHandle);

/**
 * @brief Retrive FLSOPSKD handle
 *
 * @param pHandle [in] pointer to FOTA Agent handle
 * @param flsopskdHandle [out] pointer to flsopskdHandle
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FOTAAgent_getFLSOPSKDHandle(FOTAAgent_Handle *pHandle, FLSOPSKD_Handle* flsopskdHandle);


#ifdef __cplusplus
}
#endif

#endif /* __FOTAAgent__H_ */

/** @} */