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

/**
 *  \defgroup DRV_FLSOPSKD_MODULE APIs for FLSOPSKD
 *  \ingroup DRV_MODULE
 *
 * This module contains APIs for FLSOPSKD.
 * FLSOPSKD stands for **FL**ash **OP**erations Scheduler (SKD)
 *
 * FLSOPSKD is name given to 8051 controller that sits close to external
 * flash controller (OSPI Controller) and monitors the traffic on the
 * data bus. When there is no traffic on the OSPI data bus, 8051 will
 * place the flash command on the bus.
 *
 *
 *  @{
 */

#ifndef __FLSOPSKD__H__
#define __FLSOPSKD__H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdbool.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/hw_include/cslr_flsopskd.h>

/**
 * @brief The version number that this driver expects from the firmware running in 8051.
 *
 * It is good practice to get the firmware version from 8051 and check if it is
 * equal to this macro.
 *
 */
#define FLSOPSKD_EXPECTED_FW_VERSION (0x10000U)

typedef struct FLSOPSKD_Params_s
{
    uint8_t eraseOpCode; /**< Configure Erase OpCode */
    uint8_t eraseExOpCode;/**< Configure Erase ExOpCode*/
    uint16_t pageSizeInBytes; /**< Size of page in bytes during writing. Should be less than 512B */
    uint32_t eraseSizeInBytes; /**< Number of bytes erase at time with above opcode*/
    uint32_t pollTimeout; /**< Amount of time to block waiting, in units of system ticks  */
} FLSOPSKD_Params;

typedef struct FLSOPSKD_Handle_s
{
    uint32_t lastOperationXipDowntime; /**< Keep track of XIP downtime reported for last operation */
    uint32_t lastOperationPollCount; /**< Keep track for status polls sent for last operation */
    FLSOPSKD_Params params; /**< to store all the params */
}FLSOPSKD_Handle;

/**
 * @brief User defined getTicks function
 *
 * User should define this function in their applicaiton.
 *
 * @return system ticks
 */
uint32_t FLSOPSKD_usrGetTicks();

/**
 * @brief Init all params

 * Use this function to initilize the parameters.
 * if FLSOPSKD_Params.pageSizeInBytes is 0 then this function sets it to 256.
 * if FLSOPSKD_Params.eraseSizeInBytes is 0 then this function sets it to 4096.
 * if FLSOPSKD_Params.eraseOpCode is 0 then this function sets it to 0x21.
 * if FLSOPSKD_Params.eraseExOpCode is 0 then this function sets it to 0x21.
 * FLSOPSKD_Params.pollTimeout is set to 1000.
 *
 * @param pParams pointer to FLSOPSKD_Params struct
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_Params_init(FLSOPSKD_Params *pParams);

/**
 * @brief Init the FLSOPSKD IP and driver
 *
 * FLSOPSKD IP is initilized by following procedure:
 * 1.	Put 8051 in reset state
 * 2.	Enable and initilize 8051 memory
 *     1. This step consist of enabling R5F access to 8051 code and data access.
 *     2. wait for memory to finish initilization
 * 3.	Load 8051 firmware in the 8051 code/data memory.
 * 4.	Disable R5F access to 8051 memories to avoid unintentional modification of 8051 firmware.
 * 5.	Enable required interrupts in the IP and R5F.
 * 6.	Lift 8051 from reset
 *
 * Most of the flash paramter are taken from the OSPI IP. However, some other parameter of external flash
 * are not save in the OSPI IP and those needs to be save in the RAM. eraseOpCode and eraseExOpCode are
 * such flash parameters and all of these parameters are in pParams.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param pParams pointer to FLSOPSKD_Params Object.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_init(FLSOPSKD_Handle *pHandle, FLSOPSKD_Params *pParams);

/**
 * @brief Sends write scheduling request to hardware IP.
 *
 * This function will schedule a write command to the flash and
 * sends that command as soon as the OSPI Bus is free. After M8051
 * sends the write command, it also polls for the completion of
 * write. Completion of write is logically determined by the `m8051 done`
 * interrupt which is set only after the WIP bit of the external flash
 * is not active.

 * SOC memory-map maps flash to some address space, for example, 0x60000000
 * and suppose application wants to write to 0x60000000 + 1MB = 0x60100000.
 * In this case, destAddr should be 0x100000 and not 0x60100000.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param destAddr [in] Flash address to which to write this buffer
 * @param pSrcBuffer [in] pointer to source buffer
 * @param bytesToWrite [in] size of the buffer to write
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_write(FLSOPSKD_Handle *pHandle, uint32_t destAddr, uint8_t * pSrcBuffer, uint32_t bytesToWrite);

/**
 * @brief Send erase scheduling request.
 *
 * This function schedules erase command and sends it to the flash as
 * soon as the OSPI lines are free. This function also polls by sending
 * the status check commands and if polling is enabled then will only
 * return when the flash WIP bit is not active.
 *
 * The command which is sent to the external flash requries a field called
 * address. This field specifies the sector/block which is to be
 * erased. Depending on flash, its value can be constraint. Value of that
 * field is sent via eraseOffsetInBytes argument.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param eraseOffsetInBytes [in] block offset in bytes.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_erase(FLSOPSKD_Handle *pHandle, uint32_t eraseOffsetInBytes);


/**
 * @brief Get Firmware version
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param pVersion [out] pointer to memory to save version info.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_getFwVersion(FLSOPSKD_Handle *pHandle, volatile uint32_t *pVersion);

/**
 * @brief Wait till controller is busy.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_busyPoll(FLSOPSKD_Handle *pHandle);

/**
 * @brief Get XIP downtime of last operation.
 *
 * Each Flash operation cause some downtime of XIP. After operation
 * is complete, the FLSOPSKD firmware returns the worst case XIP
 * downtime that application can expect out of the last operation.
 * This function returns the XIP downtime.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param memLoc [out] pointer to location to write the downtime.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_perfGetDowntime(FLSOPSKD_Handle *pHandle, uint32_t *memLoc);

/**
 * @brief Get Busy poll status count.
 *
 * After sending any erase or write operation, FLSOPSKD operation also sends
 * status operation. After the operation is complete, the number of status
 * polls sent is returned.
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @param memLoc [out] pointer to location to write polls count.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_perfGetPollCounts(FLSOPSKD_Handle *pHandle, uint32_t *memLoc);

/**
 * @brief Deinit Handle
 *
 * @param pHandle [in] pointer to FLSOPSKD_Handle instance.
 * @return SystemP_FAILURE
 * @return SystemP_SUCCESS
 */
int32_t FLSOPSKD_deinit(FLSOPSKD_Handle *pHandle);

#endif

#ifdef __cplusplus
}
#endif

/** @} */