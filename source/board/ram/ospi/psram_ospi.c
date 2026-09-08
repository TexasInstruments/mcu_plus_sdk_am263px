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

#include <string.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/soc.h>
#include "psram_ospi.h"

/* Mode Register 1 */
#define OSPI_PSRAM_MR1_ADDRESS            0x00000001U
#define OSPI_PSRAM_MR1_VENDOR_ID_MASK     0x1FU       /* Vendor Identifier                   */

/* Mode Register 2 */
#define OSPI_PSRAM_MR2_ADDRESS            0x00000002U
#define OSPI_PSRAM_MR2_DEVICE_ID_MASK     0x18U       /* Device Identifier                   */

#define OSPI_PSRAM_RD_CAPTURE_DELAY       8U

static uint8_t gReadBuf[OSPI_FLASH_ATTACK_VECTOR_SIZE] = { 0U };

static int32_t Psram_ospiOpen(Ram_Config *config);
static int32_t Psram_ospiRead(Ram_Config *config, uint32_t ramOffset, uint8_t *pbuf, uint32_t pbufLen);
static int32_t Psram_ospiWrite(Ram_Config *config, uint32_t ramOffset,uint8_t *pbuf, uint32_t pbufLen);
static void Psram_ospiClose(Ram_Config *config);
static int32_t Psram_ospiReadCmd(Ram_Config *config, uint8_t cmd, uint32_t readOffset,uint8_t dummyBits,
                            uint8_t numAddrBytes, uint8_t *rxBuf, uint32_t rxBufLen);
static int32_t Psram_ospiReadId(Ram_Config *config, uint32_t *manufacturerId, uint32_t *deviceId);
static int32_t Psram_ospiReset(Ram_Config *config);
static int32_t Psram_ospiWriteCmd(Ram_Config *config, uint8_t cmd, uint32_t writeOffset, uint8_t *txBuf, uint32_t txBufLen);
static int32_t Psram_ospiPhyTune(Ram_Config *config);
static void Psram_ospiDisxipEnable(void);
static void Psram_ospiDisxipDisable(void);

Ram_Fxns gPsramOspiFxns = {
    .openFxn = Psram_ospiOpen,
    .closeFxn = Psram_ospiClose,
    .readFxn = Psram_ospiRead,
    .writeFxn = Psram_ospiWrite,
};

uint32_t gProtocolMap[] =
{
    [OSPI_PROTO_1S_1S_1S] = OSPI_NOR_PROTOCOL(1,1,1,0),
    [OSPI_PROTO_1S_1S_2S] = OSPI_NOR_PROTOCOL(1,1,2,0),
    [OSPI_PROTO_1S_1S_4S] = OSPI_NOR_PROTOCOL(1,1,4,0),
    [OSPI_PROTO_1S_1S_8S] = OSPI_NOR_PROTOCOL(1,1,8,0),
    [OSPI_PROTO_4S_4S_4S] = OSPI_NOR_PROTOCOL(4,4,4,0),
    [OSPI_PROTO_4S_4D_4D] = OSPI_NOR_PROTOCOL(4,4,4,1),
    [OSPI_PROTO_8S_8S_8S] = OSPI_NOR_PROTOCOL(8,8,8,0),
    [OSPI_PROTO_8D_8D_8D] = OSPI_NOR_PROTOCOL(8,8,8,1),
};


static int32_t Psram_ospiOpen(Ram_Config *config)
{
    int32_t status = SystemP_SUCCESS;

    Ram_Attrs *attrs = config->attrs;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject*)(config->object);

    obj->ospiHandle = OSPI_getHandle(attrs->driverInstance);

    if(obj->ospiHandle == NULL)
    {
        status = SystemP_FAILURE;
    }

    if(status == SystemP_SUCCESS)
    {
        const OSPI_Attrs *ospi_attrs = ((OSPI_Config *)obj->ospiHandle)->attrs;

        Psram_ospiReset(config);

        OSPI_configResetPin(obj->ospiHandle, OSPI_RESETPIN_DEDICATED);
        /* Set device size and addressing bytes */
        OSPI_setDeviceSize(obj->ospiHandle, config->attrs->pageSize, config->attrs->ramSize);

        OSPI_setProtocol(obj->ospiHandle, gProtocolMap[ospi_attrs->protocol]);

        OSPI_setReadDummyCycles(obj->ospiHandle,config->devConfig->dummyClksRd);

        OSPI_setWriteDummyCycles(obj->ospiHandle,config->devConfig->dummyClksWr);

        OSPI_setCmdDummyCycles(obj->ospiHandle,config->devConfig->dummyClksCmd);

        OSPI_setCmdExtType(obj->ospiHandle, config->devConfig->cmdExtType);

        Psram_ospiDisxipEnable();

        /* Configure mode registers from syscfg */
        if((config->devConfig->mrCount > 0) && (config->devConfig->mrConfig != NULL))
        {
            uint8_t valWidth = (config->devConfig->mrValWidth > 0U) ? config->devConfig->mrValWidth : 1U;
            for(uint8_t i = 0; (i < config->devConfig->mrCount) && (status == SystemP_SUCCESS); i++)
            {
                /* Value is stored as uint32_t; send only mrValWidth bytes (little-endian) */
                uint32_t mrValue = config->devConfig->mrConfig[i].value;
                status += Psram_ospiWriteCmd(config, config->devConfig->cmdRegWr,
                                              config->devConfig->mrConfig[i].address,
                                              (uint8_t *)&mrValue, valWidth);
            }
        }

        OSPI_setNumAddrBytes(obj->ospiHandle,4);
        obj->numAddrBytes = 4;

    }

    if(status == SystemP_SUCCESS)
    {
        OSPI_setXferOpCodes(obj->ospiHandle, config->devConfig->cmdRd, config->devConfig->cmdWr);
    }

    if(status == SystemP_SUCCESS)
    {
        status = Psram_ospiPhyTune(config);
    }

    return status;
}

static int32_t Psram_ospiReadCmd(Ram_Config *config, uint8_t cmd, uint32_t readOffset, uint8_t dummyBits,
                            uint8_t numAddrBytes, uint8_t *rxBuf, uint32_t rxBufLen)
{
    int32_t status = SystemP_SUCCESS;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject*)(config->object);

    if(obj->ospiHandle == NULL)
    {
        status = SystemP_FAILURE;
    }

    OSPI_ReadCmdParams  rdParams;

    if(status == SystemP_SUCCESS)
    {
        OSPI_ReadCmdParams_init(&rdParams);
        rdParams.cmd           = cmd;
        rdParams.cmdAddr       = readOffset;
        rdParams.rxDataBuf     = rxBuf;
        rdParams.rxDataLen     = rxBufLen;
        rdParams.dummyBits     = dummyBits;
		rdParams.numAddrBytes  = numAddrBytes;
		
        status = OSPI_readCmd(obj->ospiHandle, &rdParams);
    }

    return status;
}

static int32_t Psram_ospiWriteCmd(Ram_Config *config, uint8_t cmd, uint32_t writeOffset, uint8_t *txBuf, uint32_t txBufLen)
{
    int32_t status = SystemP_SUCCESS;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject *)(config->object);

    if(obj->ospiHandle == NULL)
    {
        status = SystemP_FAILURE;
    }

    if(SystemP_SUCCESS == status)
    {
        OSPI_WriteCmdParams wrParams;
        OSPI_WriteCmdParams_init(&wrParams);
        wrParams.cmd        = cmd;
        wrParams.cmdAddr    = writeOffset;
        wrParams.txDataBuf  = txBuf;
        wrParams.txDataLen  = txBufLen;
        wrParams.numAddrBytes = obj->numAddrBytes;
        status += OSPI_writeCmd(obj->ospiHandle , &wrParams);
    }

    return status;
}

static int32_t Psram_ospiReadId(Ram_Config *config, uint32_t *manufacturerId, uint32_t *deviceId)
{
    int32_t status = SystemP_SUCCESS;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject *)(config->object);

    uint8_t cmd = config->devConfig->cmdRegRd;
    uint8_t numAddrBytes = obj->numAddrBytes;
    uint8_t dummyBits = config->devConfig->dummyClksCmd;
    uint32_t cmdAddr = OSPI_PSRAM_MR1_ADDRESS;
    uint8_t idCode[2] = { 0 };

    status += Psram_ospiReadCmd(config, cmd, cmdAddr, dummyBits, numAddrBytes, idCode, sizeof(idCode)/sizeof(uint8_t));

    if(status == SystemP_SUCCESS)
    {
        *manufacturerId = (uint32_t)idCode[0] & (OSPI_PSRAM_MR1_VENDOR_ID_MASK);
		*deviceId = ((uint32_t)idCode[1] & (OSPI_PSRAM_MR2_DEVICE_ID_MASK)) >> (3);
    }

    if((*manufacturerId != config->attrs->manufacturerId) || (*deviceId != config->attrs->deviceId))
    {
        status = SystemP_FAILURE;
    }

    return status;
}

static int32_t Psram_ospiWrite(Ram_Config *config, uint32_t ramOffset, uint8_t *buf, uint32_t bufLen)
{
    int32_t status = SystemP_SUCCESS;
    Ram_Attrs *attrs = config->attrs;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject*)(config->object);

    if(obj->ospiHandle == NULL)
    {
        status = SystemP_FAILURE;
    }

    if(obj->phyEnable)
    {
        OSPI_enablePhy(obj->ospiHandle);
    }

    /* Validate address input */
    if((ramOffset + bufLen) > (attrs->ramSize) || (bufLen < 2))
    {
        status = SystemP_FAILURE;
    }
    
    /* Check for odd address */
    if(ramOffset & 1)
    {
        status = SystemP_FAILURE;
    }

    if(status == SystemP_SUCCESS)
    {
        const uint32_t pageSize = config->attrs->pageSize;
        uint32_t bytesRemaining = bufLen;
        uint32_t currentOffset = ramOffset;
        uint8_t *currentBuf = buf;

        while((bytesRemaining > 0U) && (status == SystemP_SUCCESS))
        {
            /* Calculate bytes to write in this chunk, respecting page boundary */
            uint32_t offsetInPage = currentOffset % pageSize;
            uint32_t bytesLeftInPage = pageSize - offsetInPage;
            uint32_t chunkSize = (bytesRemaining < bytesLeftInPage) ? bytesRemaining : bytesLeftInPage;

            OSPI_Transaction transaction;
            OSPI_Transaction_init(&transaction);
            transaction.addrOffset = currentOffset;
            transaction.buf = (void *)currentBuf;
            transaction.count = chunkSize;
            status = OSPI_writeDirect(obj->ospiHandle, &transaction);

            if(status == SystemP_SUCCESS)
            {
                bytesRemaining -= chunkSize;
                currentOffset += chunkSize;
                currentBuf += chunkSize;
            }
        }
    }

    if(obj->phyEnable)
    {
        OSPI_disablePhy(obj->ospiHandle);
    }

    return status;
}

static int32_t Psram_ospiRead(Ram_Config *config, uint32_t ramOffset, uint8_t *buf, uint32_t bufLen)
{
    int32_t status = SystemP_SUCCESS;
    Ram_Attrs *attrs = config->attrs;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject*)(config->object);

    if(obj->ospiHandle == NULL)
    {
        status = SystemP_FAILURE;
    }

    /* Validate address input */
    if ((ramOffset + bufLen) > (attrs->ramSize))
    {
        status = SystemP_FAILURE;
    }

    if(obj->phyEnable)
    {
        OSPI_enablePhy(obj->ospiHandle);
    }

    if (status == SystemP_SUCCESS)
    {
        uint32_t pageSize = config->attrs->pageSize;
        uint32_t bytesRemaining = bufLen;
        uint32_t currentOffset = ramOffset;
        uint8_t *currentBuf = buf;

        while((bytesRemaining > 0U) && (status == SystemP_SUCCESS))
        {
            /* Calculate bytes to read in this chunk, respecting page boundary */
            uint32_t offsetInPage = currentOffset % pageSize;
            uint32_t bytesLeftInPage = pageSize - offsetInPage;
            uint32_t chunkSize = (bytesRemaining < bytesLeftInPage) ? bytesRemaining : bytesLeftInPage;

            OSPI_Transaction transaction;
            OSPI_Transaction_init(&transaction);
            transaction.addrOffset = currentOffset;
            transaction.buf = (void *)currentBuf;
            transaction.count = chunkSize;
            status = OSPI_readDirect(obj->ospiHandle, &transaction);
            if(status == SystemP_SUCCESS)
            {
                bytesRemaining -= chunkSize;
                currentOffset += chunkSize;
                currentBuf += chunkSize;
            }
        }
    }

    if(obj->phyEnable)
    {
        OSPI_disablePhy(obj->ospiHandle);
    }
    return status;
}

static int32_t Psram_ospiReset(Ram_Config *config)
{
    int32_t status = SystemP_SUCCESS;

    status = Psram_ospiWriteCmd(config, config->devConfig->cmdReset, 0, NULL, 0);

    return status;
}

static int32_t Psram_ospiPhyTune(Ram_Config *config)
{
    int32_t status = SystemP_SUCCESS;
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject *)(config->object);
    uint32_t readDataCapDelay;
    uint32_t phyTuningData = 0U, phyTuningDataSize = 0U;

    /* Disable PHY by default; enable only after successful tuning */
    obj->phyEnable = FALSE;

    if(OSPI_isPhyEnable(obj->ospiHandle) == (uint32_t)TRUE)
    {
        /*
         * PSRAM is volatile ΓÇö the attack vector must be written fresh each
         * power-on.  Write it at offset 0, then ask the OSPI PHY layer to
         * verify it (OSPI_phyReadAttackVector reads back and compares via the
         * PHY pipeline).
         */
        OSPI_phyGetTuningData(&phyTuningData, &phyTuningDataSize);
        status = Psram_ospiWrite(config, 0U, (uint8_t *)phyTuningData, phyTuningDataSize);

        if(status == SystemP_SUCCESS)
        {
            status = OSPI_phyReadAttackVector(obj->ospiHandle, 0U);
        }

        /* If verification failed, sweep read-data-capture delay and retry */
        if(status != SystemP_SUCCESS)
        {
            readDataCapDelay = OSPI_PSRAM_RD_CAPTURE_DELAY;
            while((status != SystemP_SUCCESS) && (readDataCapDelay > 0U))
            {
                OSPI_setRdDataCaptureDelay(obj->ospiHandle, readDataCapDelay);
                status = OSPI_phyReadAttackVector(obj->ospiHandle, 0U);
                readDataCapDelay--;
            }
        }

        /* Run DDR PHY tuning algorithm */
        if(status == SystemP_SUCCESS)
        {
            status = OSPI_phyTuneDDR(obj->ospiHandle, 0U);
        }

        if(status == SystemP_SUCCESS)
        {
            obj->phyEnable = TRUE;
            OSPI_setPhyEnableSuccess(obj->ospiHandle, TRUE);
        }
        else
        {
            DebugP_logError("%s : PHY enabling failed!!! Falling back to non-PHY mode...\r\n", __func__);
            obj->phyEnable = FALSE;
            OSPI_setPhyEnableSuccess(obj->ospiHandle, FALSE);

            /*
             * Re-establish a working non-PHY configuration so that the PSRAM
             * is accessible without PHY.  Mirror the fallback pattern used in
             * flash_nor_ospi.c for AM263PX / AM261X: restore dummy-cycle
             * settings in the controller and sweep read-capture-delay to find
             * a passing window.
             */
            OSPI_setReadDummyCycles(obj->ospiHandle, config->devConfig->dummyClksRd);
            OSPI_setCmdDummyCycles(obj->ospiHandle,  config->devConfig->dummyClksCmd);

            readDataCapDelay = 0U;
            status = OSPI_phyReadAttackVector(obj->ospiHandle, 0U);
            while((status != SystemP_SUCCESS) && (readDataCapDelay < OSPI_PSRAM_RD_CAPTURE_DELAY))
            {
                readDataCapDelay++;
                OSPI_setRdDataCaptureDelay(obj->ospiHandle, readDataCapDelay);
                status = OSPI_phyReadAttackVector(obj->ospiHandle, 0U);
            }

            if(status == SystemP_SUCCESS)
            {
                /* Return success - PSRAM usable without PHY at reduced bandwidth */
                status = SystemP_SUCCESS;
            }
            else
            {
                DebugP_logError("%s : Non-PHY fallback also failed!!!\r\n", __func__);
            }
        }
    }
    else
    {
        /*
         * PHY not requested in OSPI config.  Fall back to the simple
         * read-data-capture-delay sweep so that the PSRAM is accessible
         * without PHY.
         */
        uint8_t readCaptureDelay = 0U;

        status = Psram_ospiWrite(config, 0U, gOspiFlashAttackVector, OSPI_FLASH_ATTACK_VECTOR_SIZE);

        if(status == SystemP_SUCCESS)
        {
            OSPI_setRdDataCaptureDelay(obj->ospiHandle, readCaptureDelay);
            status = Psram_ospiRead(config, 0U, gReadBuf, OSPI_FLASH_ATTACK_VECTOR_SIZE);
            if(memcmp(gReadBuf, gOspiFlashAttackVector, OSPI_FLASH_ATTACK_VECTOR_SIZE) != 0)
            {
                status = SystemP_FAILURE;
            }
        }

        while((status != SystemP_SUCCESS) && (readCaptureDelay < OSPI_PSRAM_RD_CAPTURE_DELAY))
        {
            readCaptureDelay++;
            OSPI_setRdDataCaptureDelay(obj->ospiHandle, readCaptureDelay);
            status = Psram_ospiRead(config, 0U, gReadBuf, OSPI_FLASH_ATTACK_VECTOR_SIZE);
            if(memcmp(gReadBuf, gOspiFlashAttackVector, OSPI_FLASH_ATTACK_VECTOR_SIZE) != 0)
            {
                status = SystemP_FAILURE;
            }
        }
    }

    return status;
}

static void Psram_ospiDisxipEnable(void)
{
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, MSS_CTRL_PARTITION0);
    CSL_mss_ctrlRegs *mss = (CSL_mss_ctrlRegs *)CSL_MSS_CTRL_U_BASE;
    mss->OSPI1_CONFIG_CONTROL = ((1U << 31U) | CSL_MSS_CTRL_OSPI1_CONFIG_CONTROL_OSPI_DDR_MODE_MASK);
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, MSS_CTRL_PARTITION0);
}

static void Psram_ospiDisxipDisable(void)
{
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, MSS_CTRL_PARTITION0);
    CSL_mss_ctrlRegs *mss = (CSL_mss_ctrlRegs *)CSL_MSS_CTRL_U_BASE;
    mss->OSPI1_CONFIG_CONTROL = CSL_MSS_CTRL_OSPI1_CONFIG_CONTROL_OSPI_DDR_MODE_MASK;
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, MSS_CTRL_PARTITION0);
}

static void Psram_ospiClose(Ram_Config *config)
{
    Ram_OspiPsramObject *obj = (Ram_OspiPsramObject *)(config->object);

    (void)Psram_ospiReset(config);

    OSPI_disablePhy(obj->ospiHandle);

    obj->ospiHandle = NULL;
    obj->phyEnable = FALSE;
    return;
}