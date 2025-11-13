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



    return status;
}

void Board_driversClose(void)
{

    Board_eepromClose();

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

