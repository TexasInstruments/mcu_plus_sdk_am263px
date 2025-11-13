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
 * ### Communicating with 8051 firmware.
 * \note
 * Before running this driver, make sure that microcontroller is fresh from reset.
 *
 * #### Any operation general format
 *
 * Any operation that is performed by 8051 is generally in format:
 * 1.	wait for go bit
 * 2.	wait for OSPI IP to be IDLE
 * 3.	perform the required operation
 * 4.	Save status/error code in the corresponding registers.
 * 5.	Send operation finish interrupt to R5F cores.
 *
 * 8051 FW supports the following operations
 * 1.	scheduled Write
 * 2.	scheduled Erase
 * 3.	scheduled General Command
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <kernel/dpl/SystemP.h>

#include "flsopskd.h"
#include "fota_fw_arr.h"


#define FLSOPSKD_INITIAL_BUSY_STATE             (0)

#define FLSOPSKD_DRIVER_DEFAULT_TIMEOUT         (100000000U)

#define FLSOPSKD_FW_MEM_SZ                      (2048U)
#define FLSOPSKD_WBUF_SIZE                      (512U)

#define FLSOPSKD_OPCODE_WRITE_TO_FLASH          (0x0000U)
#define FLSOPSKD_OPCODE_ERASE_FLASH             (0x0001U)
#define FLSOPSKD_OPCODE_PROG_FLASH_CMD_FLDS     (0x0002U)
#define FLSOPSKD_OPCODE_SEND_FLASH_CMD          (0x0003U)
#define FLSOPSKD_OPCODE_GET_FW_VERSION          (0x0004U)

#define FLSOPSKD_STIG_SET_OPCODE                (0x0000U | FLSOPSKD_OPCODE_PROG_FLASH_CMD_FLDS)
#define FLSOPSKD_STIG_SET_EXOPCODE              (0x0100U | FLSOPSKD_OPCODE_PROG_FLASH_CMD_FLDS)

#define DEFAULT_ERASE_OP_CODE                   (0x21U)
#define DEFAULT_ERASE_EXOP_CODE                 DEFAULT_ERASE_OP_CODE
#define DEFAULT_FLASH_PAGE_SIZE                 (256U)
#define DEFAULT_ERASE_SECTOR_SIZE               (4096U)

/**
 * @brief function to send fields of generic flash command.
 *
 * @param pHandle pointer to FLSOPSKD_Handle instance.
 * @param fncode function code.
 * @param val value of that corresponding function
 */
static inline int32_t FLSOPSKD_FlashCmd_setGeneric(FLSOPSKD_Handle *pHandle, uint16_t fncode, uint16_t val)
{
    int32_t status = SystemP_FAILURE;
    uint32_t nval = 0;
    if (NULL != pHandle)
    {
        nval = (val << 16) | fncode;
        const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);
        CSL_REG32_WR(&pReg->FOTA_GP0, nval);
        CSL_REG32_WR(&pReg->STS_IRQ.STATUS, TRUE);
        CSL_REG32_FINS(&pReg->FOTA_CTRL, FSS_FOTA_GENREGS_FOTA_CTRL_GO, TRUE);
        status = FLSOPSKD_busyPoll(pHandle);
    }
    return status;
}

/**
 * @brief set OPCode for flash command
 *
 * @param pHandle pointer to FLSOPSKD_Handle instance.
 * @param opcode opcode
 */
static int32_t FLSOPSKD_FlashCmd_setOpcode(FLSOPSKD_Handle *pHandle, uint8_t opcode)
{
    int32_t status = SystemP_FAILURE;
    if (NULL != pHandle)
    {
        status = FLSOPSKD_FlashCmd_setGeneric(pHandle, FLSOPSKD_STIG_SET_OPCODE, opcode);
    }
    return status;
}

/**
 * @brief set extended opcode for flash command
 *
 * @param pHandle pointer to FLSOPSKD_Handle instance.
 * @param exopcode
 */
static int32_t FLSOPSKD_FlashCmd_setExopcode(FLSOPSKD_Handle *pHandle, uint8_t exopcode)
{
    int32_t status = SystemP_FAILURE;
    if (NULL != pHandle)
    {
        status = FLSOPSKD_FlashCmd_setGeneric(pHandle, FLSOPSKD_STIG_SET_EXOPCODE, exopcode);
    }
    return status;
}

int32_t FLSOPSKD_Params_init(FLSOPSKD_Params *pParams)
{
    int32_t status = SystemP_FAILURE;
    if(NULL != pParams)
    {
        pParams->pollTimeout = FLSOPSKD_DRIVER_DEFAULT_TIMEOUT;
        pParams->eraseOpCode = DEFAULT_ERASE_OP_CODE;
        pParams->eraseExOpCode = DEFAULT_ERASE_EXOP_CODE;
        pParams->eraseSizeInBytes = DEFAULT_ERASE_SECTOR_SIZE;
        pParams->pageSizeInBytes = DEFAULT_FLASH_PAGE_SIZE;
        pParams->pageSizeInBytes = FLSOPSKD_WBUF_SIZE;
        status = SystemP_SUCCESS;
    }
    return status;
}

int32_t FLSOPSKD_init(FLSOPSKD_Handle *pHandle, FLSOPSKD_Params *pParams)
{
    int32_t status = SystemP_FAILURE;
    const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);

    if ((NULL != pHandle) && (NULL != pParams))
    {
        memcpy((void*)&(pHandle->params), (void*)pParams, sizeof(FLSOPSKD_Params));

        /* Put m8051 in reset */
        CSL_REG32_FINS(&pReg->FOTA_INIT, FSS_FOTA_GENREGS_FOTA_INIT_RESET, 1);
        /* Make memaccess = 1 */
        CSL_REG32_FINS(&pReg->FOTA_INIT, FSS_FOTA_GENREGS_FOTA_INIT_MEMACCESS, 1);

        /* Poll for mem init done */
        uint32_t mem_init = CSL_REG32_RD(&pReg->FOTA_INIT);
        while ((mem_init & CSL_FSS_FOTA_GENREGS_FOTA_INIT_PDMEM_INIT_DONE_MASK)
                != CSL_FSS_FOTA_GENREGS_FOTA_INIT_PDMEM_INIT_DONE_MASK)
        {
            mem_init = CSL_REG32_RD(&pReg->FOTA_INIT);
        }

        /* load 8051 FW */
        status = SystemP_SUCCESS;
        memcpy((void *)CSL_FSS_PDMEM_GENREGS_REGS_BASE, FOTA_FW_ARR, FOTA_FW_SIZE);

        /* lock back the HW */
        if(SystemP_SUCCESS == status)
        {
            /* Make memaccess = 0 such that CPU can't access m8051 memories now */
            CSL_REG32_FINS(&pReg->FOTA_INIT, FSS_FOTA_GENREGS_FOTA_INIT_MEMACCESS, 0);
            /* lift m8051 from reset */
            CSL_REG32_FINS(&pReg->FOTA_INIT, FSS_FOTA_GENREGS_FOTA_INIT_RESET, 0);
        }
    }

    return status;
}

int32_t FLSOPSKD_erase(FLSOPSKD_Handle *pHandle, uint32_t eraseOffsetInBytes)
{
    /*
    * eraseOffsetInBytes (address) should be aligned to block size (128KB) or
    * sector size (4KB) otherwise there will be undefined behavior.
    * Once the GO bit is set, then R5F should wait for `m8051 DONE` interrupt and check for error
    * codes or status code.
    * Current implementation only supports `m8051 done` bit in `IRQ_STATUS_RAW.fota_done`.
    * It does not provide any error code and status code. It also assumes that flash is already
    * configured.
    */
    /* any non zero value will make 8051 to erase */
    int32_t status = SystemP_FAILURE;
    FLSOPSKD_Params *params = (FLSOPSKD_Params *)NULL;

    const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);

    if (NULL != pHandle)
    {
        params = &(pHandle->params);
        pHandle->lastOperationXipDowntime = 0;
        pHandle->lastOperationXipDowntime = 0;
        status = FLSOPSKD_FlashCmd_setOpcode(pHandle, params->eraseOpCode);
        status |= FLSOPSKD_FlashCmd_setExopcode(pHandle, params->eraseExOpCode);
        if(SystemP_SUCCESS == status)
        {
            CSL_REG32_WR(&pReg->FOTA_GP0, FLSOPSKD_OPCODE_ERASE_FLASH);
            CSL_REG32_WR(&pReg->FOTA_ADDR, eraseOffsetInBytes);
            CSL_REG32_WR(&pReg->STS_IRQ.STATUS, TRUE);
            CSL_REG32_FINS(&pReg->FOTA_CTRL, FSS_FOTA_GENREGS_FOTA_CTRL_GO, TRUE);
            status = FLSOPSKD_busyPoll(pHandle);
            if(SystemP_SUCCESS == status)
            {
                pHandle->lastOperationXipDowntime = ((pReg->FOTA_GP1) & 0xffff) * 10;
                pHandle->lastOperationPollCount = ((pReg->FOTA_GP1) >> 16) & 0xffff;
            }
        }
    }
    return status;
}

int32_t FLSOPSKD_write(FLSOPSKD_Handle *pHandle, uint32_t destAddr, uint8_t *pSrcBuffer, uint32_t bytesToWrite)
{
    /*
    * Current 8051 implements <512B page writes.
    * Completion of this is notified by IRQ_STATUS_RAW.fota_done bit and does not writes any error code and status code.
    * Once the GO bit is set, then R5F should wait for `m8051 DONE` interrupt and check for error codes or status code
    */
    int32_t status = SystemP_FAILURE;
    uint32_t bytesWritten = 0;
    FLSOPSKD_Params *params = (FLSOPSKD_Params *)NULL;
    uint16_t flashPageSize = 0;

    const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);

    if ((NULL != pHandle) && (bytesToWrite > 0) && (NULL != pSrcBuffer))
    {
        params = &(pHandle->params);
        flashPageSize = params->pageSizeInBytes;
        pHandle->lastOperationXipDowntime = 0;
        pHandle->lastOperationXipDowntime = 0;

        do
        {
            uint32_t newDestAddr = destAddr + bytesWritten;
            uint8_t* pSrc = pSrcBuffer + bytesWritten;
            CSL_REG32_WR(&pReg->FOTA_GP0, FLSOPSKD_OPCODE_WRITE_TO_FLASH);
            CSL_REG32_WR(&pReg->FOTA_GP1, 0);
            CSL_REG32_WR(&pReg->FOTA_ADDR, newDestAddr);
            CSL_REG32_WR(&pReg->FOTA_CNT, flashPageSize);
            memcpy((void *)CSL_FSS_WBUF_GENREGS_REGS_BASE, pSrc, flashPageSize);
            memcpy((void *)(CSL_FSS_WBUF_GENREGS_REGS_BASE + flashPageSize), pSrc, flashPageSize);
            CSL_REG32_WR(&pReg->STS_IRQ.STATUS, TRUE);
            CSL_REG32_FINS(&pReg->FOTA_CTRL, FSS_FOTA_GENREGS_FOTA_CTRL_GO, TRUE);
            status = FLSOPSKD_busyPoll(pHandle);
            if(SystemP_SUCCESS == status)
            {
                bytesWritten += flashPageSize;
                pHandle->lastOperationXipDowntime += ((pReg->FOTA_GP1) & 0xffff) * 10;
                pHandle->lastOperationPollCount += ((pReg->FOTA_GP1) >> 16) & 0xffff;
            }
        } while (bytesWritten < bytesToWrite && SystemP_SUCCESS == status);
    }
    return status;
}

int32_t FLSOPSKD_getFwVersion(FLSOPSKD_Handle *pHandle, volatile uint32_t *pVersion)
{
    int32_t status = SystemP_FAILURE;
    const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);

    if ((NULL != pHandle) && (NULL != pVersion))
    {
        CSL_REG32_WR(&pReg->FOTA_GP0, FLSOPSKD_OPCODE_GET_FW_VERSION);
        CSL_REG32_WR(&pReg->STS_IRQ.STATUS, TRUE);
        CSL_REG32_FINS(&pReg->FOTA_CTRL, FSS_FOTA_GENREGS_FOTA_CTRL_GO, TRUE);
        status = FLSOPSKD_busyPoll(pHandle);
        if(SystemP_SUCCESS == status)
        {
            *pVersion = pReg->FOTA_GP1;
        }
    }
    return status;
}

int32_t FLSOPSKD_deinit(FLSOPSKD_Handle *pHandle)
{
    int32_t status = SystemP_FAILURE;
    if(NULL != pHandle)
    {
        status = SystemP_SUCCESS;
    }
    return status;
}

int32_t FLSOPSKD_busyPoll(FLSOPSKD_Handle *pHandle)
{
    int32_t status = SystemP_FAILURE;
    uint32_t start_ticks = 0;
    if(NULL != pHandle)
    {
        const CSL_fss_fota_genregsRegs *pReg = (const CSL_fss_fota_genregsRegs *)(CSL_FSS_FOTA_GENREGS_REGS_BASE);
        start_ticks = FLSOPSKD_usrGetTicks();
        while (1)
        {
            volatile unsigned short operationDoneStatus = CSL_REG32_RD(&pReg->STS_IRQ.STATUS_RAW);
            if(operationDoneStatus != 0)
            {
                status = SystemP_SUCCESS;
                break;
            }
            else if((FLSOPSKD_usrGetTicks() - start_ticks) >= pHandle->params.pollTimeout)
            {
                status = SystemP_TIMEOUT;
                break;
            }
        }
    }
    return status;
}

int32_t FLSOPSKD_perfGetDowntime(FLSOPSKD_Handle *pHandle, uint32_t *memLoc)
{
    int32_t status = SystemP_FAILURE;
    if((NULL != pHandle) && (NULL != memLoc))
    {
        *memLoc = pHandle->lastOperationXipDowntime;
        status = SystemP_SUCCESS;
    }
    return status;
}

int32_t FLSOPSKD_perfGetPollCounts(FLSOPSKD_Handle *pHandle, uint32_t *memLoc)
{
    int32_t status = SystemP_FAILURE;
    if((NULL != pHandle) && (NULL != memLoc))
    {
        *memLoc = pHandle->lastOperationPollCount;
        status = SystemP_SUCCESS;
    }
    return status;
}