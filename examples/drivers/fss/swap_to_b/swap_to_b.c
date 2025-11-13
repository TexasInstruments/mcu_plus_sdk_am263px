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

#include <kernel/dpl/DebugP.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/fss.h>
#include <drivers/ospi.h>

/*
 * This example:
 *  1. writes data to flash at (flash_size/2) + 2MB offset.
 *  2. remaps address from (flash_size/2) and above to 0MB and above.
 *  3. reads back the data from 2MB offset.
 *  4. checks if data that is read back is correct or not.
*/

/**
 * @brief Offset from which tor read.
 *
 */
#define APP_OSPI_FLASH_OFFSET_REIGON_A  (0x200000U)
#if defined (SOC_AM263PX)
/**
 * @brief Write location of the buffer in flash.
 *
 * Assume that flash address from 0 to 4MB is region A
 * and 4MB to 8MB is region B. Write will happen
 * at this location of flash.
*/
#define APP_OSPI_FLASH_OFFSET_REIGON_B  (0x1200000U)
#else
/**
 * @brief Write location of the buffer in flash.
 *
 * Assume that flash address from 0 to 16MB is region A
 * and 16MB to 32MB is region B. Writes will happen at
 * this location of flash.
*/
#define APP_OSPI_FLASH_OFFSET_REIGON_B  (0x600000U)
#endif

/**
 * @brief Size of buffer data that is to be read.
 *
 */
#define BUFFER_DATA_SIZE (4096)

uint8_t gTxBuff[BUFFER_DATA_SIZE] __attribute__((aligned(4096U))) = {0};
uint8_t gRxBuf[BUFFER_DATA_SIZE] __attribute__((aligned(4096U))) = {0};

void board_flash_reset(OSPI_Handle oHandle);

int32_t enable_flash_dac_phy()
{
    int32_t status = SystemP_SUCCESS;
    /* enable Phy and Phy pipeline for XIP execution */
    if (OSPI_isPhyEnable(gOspiHandle[CONFIG_OSPI0]))
    {
        status = OSPI_enablePhy(gOspiHandle[CONFIG_OSPI0]);
        status = OSPI_enablePhyPipeline(gOspiHandle[CONFIG_OSPI0]);
    }
    status = OSPI_enableDacMode(gOspiHandle[CONFIG_OSPI0]);
    return status;
}

void swap_main(void *args)
{
    int32_t status = SystemP_SUCCESS;
    uint32_t offset;
    FSS_Config fssConf;
    Flash_Attrs *flashAttr;
    uint32_t blk, page;

    Drivers_open();

#if defined (SOC_AM263PX)|| defined (SOC_AM261X)
    board_flash_reset(gOspiHandle[CONFIG_OSPI0]);
#endif

    status = Board_driversOpen();
    DebugP_assert(status==SystemP_SUCCESS);

    for(uint32_t i = 0U; i < BUFFER_DATA_SIZE; i++)
    {
        gTxBuff[i] = i % 256;
        gRxBuf[i] = 0;
    }

    offset = APP_OSPI_FLASH_OFFSET_REIGON_B;

    Flash_offsetToBlkPage(gFlashHandle[CONFIG_FLASH0], offset, &blk, &page);
    flashAttr = Flash_getAttrs(CONFIG_FLASH0);
    status = Flash_eraseBlk(gFlashHandle[CONFIG_FLASH0], blk);
    if(status != SystemP_SUCCESS)
    {
        DebugP_log("Block Erase Failed at 0x%X offset !!!", offset);
    }
    else
    {
        status = Flash_write(gFlashHandle[CONFIG_FLASH0], offset, gTxBuff, BUFFER_DATA_SIZE);
        if(status != SystemP_SUCCESS)
        {
            DebugP_log("Flash Write of %d bytes failed at 0x%X offset !!!", BUFFER_DATA_SIZE, offset);
        }
        else
        {
            /*
                Now using Bootseg IP, will switch A and B.
            */
            enable_flash_dac_phy();
            fssConf.ipBaseAddress = CSL_MSS_CTRL_U_BASE;
            fssConf.extFlashSize = flashAttr->flashSize;
            FSS_selectRegionB((FSS_Handle)&fssConf);
            offset = APP_OSPI_FLASH_OFFSET_REIGON_A;
            status = Flash_read(gFlashHandle[CONFIG_FLASH0], offset, gRxBuf, BUFFER_DATA_SIZE);

            if(SystemP_SUCCESS == status)
            {
                for(uint32_t i = 0U; i < BUFFER_DATA_SIZE; i++)
                {
                    DebugP_assert(gTxBuff[i] == gRxBuf[i]);
                }
            }

            DebugP_log("\r\nAll Test Passed\r\n");
        }
    }

    Board_driversClose();
    Drivers_close();
}
