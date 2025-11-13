/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
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

/*
 * Auto generated file 
 */

#include "ti_board_open_close.h"

int32_t Board_driversOpen(void)
{
    int32_t status = SystemP_SUCCESS;
    if(status==SystemP_SUCCESS)
    {
        status = Board_eepromOpen();
    }

    if(status==SystemP_SUCCESS)
    {
        status = Board_flashOpen();
    }


    return status;
}

void Board_driversClose(void)
{

    Board_eepromClose();

    Board_flashClose();
    Board_pmicClose();

}

/*
 * EEPROM
 */
#include <board/eeprom.h>
/* EEPROM specific includes */
#include <board/eeprom/eeprom_cat24m.h>

/* EEPROM Object - initalized during EEPROM_open() */
EEPROM_Object gEepromObject_CAT24M[CONFIG_EEPROM_NUM_INSTANCES];

/* EEPROM Driver handles - opened during Board_eepromOpen() */
EEPROM_Handle gEepromHandle[CONFIG_EEPROM_NUM_INSTANCES];

/* EEPROM Config */
EEPROM_Config gEepromConfig[CONFIG_EEPROM_NUM_INSTANCES] =
{
    {
        .attrs = &gEepromAttrs_CAT24M,
        .fxns = &gEepromFxns_CAT24M,
        .object = (void *)&gEepromObject_CAT24M[0],
    },
};
uint32_t gEepromConfigNum = CONFIG_EEPROM_NUM_INSTANCES;

/* EEPROM params */
EEPROM_Params gEepromParams[CONFIG_EEPROM_NUM_INSTANCES] =
{
    {
        .driverInstance = CONFIG_I2C1,
        .i2cAddress     = 0x50,
    },
};

int32_t Board_eepromOpen()
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_EEPROM_NUM_INSTANCES; instCnt++)
    {
        gEepromHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_EEPROM_NUM_INSTANCES; instCnt++)
    {
        gEepromHandle[instCnt] = EEPROM_open(instCnt, &gEepromParams[instCnt]);
        if(NULL == gEepromHandle[instCnt])
        {
            DebugP_logError("EEPROM open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Board_eepromClose();   /* Exit gracefully */
    }

    return status;
}

void Board_eepromClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_EEPROM_NUM_INSTANCES; instCnt++)
    {
        if(gEepromHandle[instCnt] != NULL)
        {
            EEPROM_close(gEepromHandle[instCnt]);
            gEepromHandle[instCnt] = NULL;
        }
    }

    return;
}

/*
 * FLASH
 */
/* FLASH Object - initialized during Flash_open() */
Flash_NorOspiObject gFlashObject_IS25LX256;

/* FLASH Driver handles - opened during Board_flashOpen() */
Flash_Handle gFlashHandle[CONFIG_FLASH_NUM_INSTANCES];

/* FLASH Attrs */
Flash_Attrs gFlashAttrs_IS25LX256 =
{
    .flashName = "IS25LX256",
    .deviceId = 0x5A19,
    .manufacturerId = 0x9D,
    .flashSize = 33554432,
    .blockCount = 256,
    .blockSize = 131072,
    .pageCount = 512,
    .pageSize = 256,
    .sectorCount = 8192,
    .sectorSize = 4096,
    .phyTuningOffset = 0x80000,
};

/* FLASH DevConfig */
Flash_DevConfig gFlashDevCfg_IS25LX256 =
{
    .cmdExtType = OSPI_CMD_EXT_TYPE_REPEAT,
    .enable4BAddr = TRUE,
    .addrByteSupport = 1,
    .fourByteAddrEnSeq = 0,
    .cmdWren = 0x06,
    .cmdRdsr = 0x05,
    .srWip = (1 << 0),
    .srWel = (1 << 1),
    .xspiWipRdCmd = 0x05,
    .xspiWipReg = 0x00000000,
    .xspiWipBit = (1 << 0),
    .resetType = 0x30,
    .eraseCfg = {
        .blockSize = 131072,
        .sectorSize = 4096,
        .cmdBlockErase3B = 0xD8,
        .cmdBlockErase4B = 0xDC,
        .cmdSectorErase3B = 0x20,
        .cmdSectorErase4B = 0x21,
        .cmdChipErase = 0xC7,
    },
    .idCfg = {
        .cmd = 0x9F, /* Constant */
        .numBytes = 3,
        .dummy4 = 0,
        .dummy8 = 8,
        .addrSize = 0
    },
    .protocolCfg = {
        .protocol = FLASH_CFG_PROTO_8D_8D_8D,
        .isDtr = TRUE,
        .cmdRd = 0x7C,
        .cmdWr = 0x84,
        .modeClksCmd = 0,
        .modeClksRd = 0,
        .dummyClksCmd = 16,
        .dummyClksRd = 16,
        .enableType = 0,
        .enableSeq = 0x00,
        .protoCfg = {
            .isAddrReg = TRUE,
            .cmdRegRd = 0x85,
            .cmdRegWr = 0x81,
            .cfgReg = 0,
            .shift = 0,
            .mask = 0xff,
            .cfgRegBitP = 231,
        },
        .strDtrCfg = {
            .isAddrReg = TRUE,
            .cmdRegRd = 0x85,
            .cmdRegWr = 0x81,
            .cfgReg = 0x00000000,
            .shift = 255,
            .mask = 0xFF,
            .cfgRegBitP = 0,
        },
        .dummyCfg = {
            .isAddrReg = TRUE,
            .cmdRegRd = 0x85,
            .cmdRegWr = 0x81,
            .cfgReg = 1,
            .shift = 0,
            .mask = 0xFF,
            .cfgRegBitP = 16,
        },
    },
    .flashWriteTimeout = 120,
    .flashBusyTimeout = 72000000,
};

/* FLASH Config */
Flash_Config gFlashConfig[CONFIG_FLASH_NUM_INSTANCES] =
{
    {
        .attrs = &gFlashAttrs_IS25LX256,
        .fxns = &gFlashNorOspiFxns,
        .skipHwInit = FALSE,
        .devConfig = &gFlashDevCfg_IS25LX256,
        .object = (void *)&gFlashObject_IS25LX256,
        .rwOffset = 0,
    },
};

/* Externs of quirk functions and custom protocol functions */
extern int32_t Flash_quirkSpansionUNHYSADisable(Flash_Config *config);

/* FLASH Open Params - populated from SysConfig options */
Flash_Params gFlashParams[CONFIG_FLASH_NUM_INSTANCES] =
{
    {
        .quirksFxn = Flash_quirkSpansionUNHYSADisable,
        .custProtoFxn = NULL,
    },
};

uint32_t gFlashConfigNum = CONFIG_FLASH_NUM_INSTANCES;

int32_t Board_flashOpen()
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    /* Set the peripheral instance to use in the under lying flash implementation */
    gFlashConfig[CONFIG_FLASH0].attrs->driverInstance = CONFIG_OSPI0;

    for(instCnt = 0U; instCnt < CONFIG_FLASH_NUM_INSTANCES; instCnt++)
    {
        gFlashHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Set the underlying driver instance to the FLASH config */
    gFlashAttrs_IS25LX256.driverInstance = CONFIG_OSPI0;

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_FLASH_NUM_INSTANCES; instCnt++)
    {

        gFlashHandle[instCnt] = Flash_open(instCnt, &gFlashParams[instCnt]);
        if(NULL == gFlashHandle[instCnt])
        {
            DebugP_logError("FLASH open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Board_flashClose();   /* Exit gracefully */
    }
    return status;
}

void Board_flashClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_FLASH_NUM_INSTANCES; instCnt++)
    {
        if(gFlashHandle[instCnt] != NULL)
        {
            Flash_close(gFlashHandle[instCnt]);
            gFlashHandle[instCnt] = NULL;
        }
    }
    return;
}
/*
 * PMIC
 */
/* PMIC specific includes */
#include <board/pmic/pmic_tps653860xx.h>

/* PMIC Object - initalized during PMIC_open() */
PMIC_Object gPmicObject[CONFIG_PMIC_NUM_INSTANCES];
/* PMIC Driver handles - opened during Board_pmicOpen() */
PMIC_Handle gPmicHandle[CONFIG_PMIC_NUM_INSTANCES];

/* PMIC Config */
PMIC_Config gPmicConfig[CONFIG_PMIC_NUM_INSTANCES] =
{
    {
        .fxns = &gPmicFxns_TPS653860xx,
        .object = (void *)&gPmicObject[0],
    },
};
uint32_t gPmicConfigNum = CONFIG_PMIC_NUM_INSTANCES;

/* PMIC params */
PMIC_Params gPmicParams[CONFIG_PMIC_NUM_INSTANCES] =
{
    {
        .deviceType  = PMIC_DEV_BB_TPS65386X,
        .commMode    = PMIC_INTF_SPI,
        .instType    = PMIC_MAIN_INST,
        .instance    = CONFIG_MCSPI0,
    },
};

int32_t Board_pmicOpen()
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_PMIC_NUM_INSTANCES; instCnt++)
    {
        gPmicHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_PMIC_NUM_INSTANCES; instCnt++)
    {
        gPmicHandle[instCnt] = PMIC_open(instCnt, &gPmicParams[instCnt]);
        if(NULL == gPmicHandle[instCnt])
        {
            DebugP_logError("PMIC open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }

        status = PMIC_configure(gPmicHandle[instCnt]);

        if(status == SystemP_FAILURE)
        {
            DebugP_logError("PMIC configure failed for instance %d !!!\r\n", instCnt);
            break;
        }
    }
    if(SystemP_FAILURE == status)
    {
        Board_pmicClose();   /* Exit gracefully */
    }

    return status;
}

void Board_pmicClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_PMIC_NUM_INSTANCES; instCnt++)
    {
        if(gPmicHandle[instCnt] != NULL)
        {
            PMIC_close(gPmicHandle[instCnt]);
            gPmicHandle[instCnt] = NULL;
        }
    }

    return;
}

void Drivers_pmicOpen()
{
    Drivers_mcspiOpen();
    Board_pmicOpen();
    Drivers_mcspiClose();
    /*Wait 5ms for LDO rail to get initilized*/
    ClockP_usleep(5000);
}

