
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


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/csl_types.h>
#include <kernel/dpl/DebugP.h>
#include "fss.h"

#define HWREG(x)                                                               \
        (*((volatile uint32_t *)(x)))

#define MSS_OSPI_BOOT_CONFIG_MASK              (0x840ul)
#define MSS_OSPI_BOOT_CONFIG_SEG               (0x844ul)
#define BOOT_REGION_A                          (0u)
#define BOOT_REGION_B                          (1u)

int32_t FSS_addressBitMask(FSS_Handle handle, uint32_t bitMask, uint8_t segment)
{
    int32_t ret = SystemP_FAILURE;

    if(handle != NULL)
    {
        FSS_Config *config = (FSS_Config *)handle;
        if(bitMask != 0)
        {
            HWREG(config->ipBaseAddress + MSS_OSPI_BOOT_CONFIG_MASK) = ~(bitMask >> 12);
            HWREG(config->ipBaseAddress + MSS_OSPI_BOOT_CONFIG_SEG) = (segment * (bitMask + 1)) >> 12;
        }
        else
        {
            HWREG(config->ipBaseAddress + MSS_OSPI_BOOT_CONFIG_MASK) = 0;
            HWREG(config->ipBaseAddress + MSS_OSPI_BOOT_CONFIG_SEG) = 0;
        }
        ret = SystemP_SUCCESS;
    }

    return ret;
}

int32_t FSS_selectRegionA(FSS_Handle handle)
{
    int32_t ret = SystemP_FAILURE;
    if(handle != NULL)
    {
        FSS_Config *config = (FSS_Config *)handle;
        if((config->extFlashSize & (config->extFlashSize - 1)) == 0)
        {
            ret = FSS_addressBitMask(handle, (config->extFlashSize / 2) - 1, 0);
        }
    }
    return ret;
}

int32_t FSS_selectRegionB(FSS_Handle handle)
{
    int32_t ret = SystemP_FAILURE;
    if(handle != NULL)
    {
        FSS_Config *config = (FSS_Config *)handle;
        if((config->extFlashSize & (config->extFlashSize - 1)) == 0)
        {
            ret = FSS_addressBitMask(handle, (config->extFlashSize / 2) - 1, 1);
        }
    }
    return ret;
}

uint32_t FSS_getBootRegion(FSS_Handle handle)
{
    uint32_t ret = 0;
    if(handle != NULL)
    {
        FSS_Config *config = (FSS_Config *)handle;
        ret = (HWREG(config->ipBaseAddress + MSS_OSPI_BOOT_CONFIG_SEG) > 0) ? BOOT_REGION_B : BOOT_REGION_A;
    }
    return ret;
}
int32_t FSS_disableAddressRemap(FSS_Handle handle)
{
    return FSS_addressBitMask(handle, 0, 0);
}


#define MAX_SIZE 103

static void FSS_ECCConvertBytesToBits(uint8_t* bytes, int* bits, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            bits[i * 8 + j] = (bytes[i] >> (7 - j)) & 1;
        }
    }
}

static uint32_t FSS_ECCCalculateRegionSize(uint32_t sizeInBytes)
{
    // If sizeInBytes is less than 4KB, return 1
    if (sizeInBytes < 4096)
    {
        return 1;
    }
    // Calculate the region size based on the provided logic
    return ((sizeInBytes -1) / 4096) + 1;
}

void FSS_enableECC()
{
    HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + CSL_FSS_FSAS_GENREGS_SYSCONFIG, CSL_FSS_FSAS_GENREGS_SYSCONFIG_ECC_EN, 1);
    HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + CSL_FSS_FSAS_GENREGS_SYSCONFIG, CSL_FSS_FSAS_GENREGS_SYSCONFIG_ECC_DISABLE_ADR,1);
}

void FSS_disableECC()
{
    HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + CSL_FSS_FSAS_GENREGS_SYSCONFIG, CSL_FSS_FSAS_GENREGS_SYSCONFIG_ECC_EN, 0);
}

// Set ECC Region Start using region index
void FSS_setECCRegionStart(uint32_t regionIndex, uint32_t ecc_reg_start)
{
    uint32_t regOffset = CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSTRT(regionIndex);
    HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + regOffset, CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSTRT_R_START, (ecc_reg_start)/0x1000);
}

/*Set ECC Region Size using region index*/
void FSS_setECCRegionSize(uint32_t regionIndex, uint32_t size_in_bytes)
{
    uint32_t ecc_reg_size=FSS_ECCCalculateRegionSize(size_in_bytes);
    uint32_t regOffset = CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSIZ(regionIndex);
    HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + regOffset, CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSIZ_R_SIZE, ecc_reg_size);
}

int32_t FSS_configECCMRegion(FSS_ECCRegionConfig *parameter)
{
    int32_t status = SystemP_FAILURE;
    if(NULL != parameter)
    {
        uint32_t regOffset = CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSTRT(parameter->regionIndex);
        uint32_t ecc_reg_size =FSS_ECCCalculateRegionSize(parameter->size);
        uint32_t regOffset1 = CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSIZ(parameter->regionIndex);
        HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + regOffset, CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSTRT_R_START, (parameter->startAddress)/0x1000);
        HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + regOffset1, CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSIZ_R_SIZE, ecc_reg_size);
        HW_WR_FIELD32(CSL_FSS_FSAS_GENREGS_REGS_BASE + regOffset1, CSL_FSS_FSAS_GENREGS_ECC_REGCTRL_ECC_RGSIZ_R_SIZE, ecc_reg_size);
        status = SystemP_SUCCESS;
    }
    return status;
}

int32_t FSS_ConfigEccm(uint32_t numRegion, FSS_ECCRegionConfig *parameter)
{
    int32_t status = SystemP_FAILURE;
    if(parameter != NULL)
    {
        FSS_disableECC();
        for(int i=0; i < numRegion; i++)
        {
            status = FSS_configECCMRegion(&parameter[i]);
        }
        FSS_enableECC();
        status = SystemP_SUCCESS;
    }
    return status;
}