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

#include "ti_drivers_open_close.h"
#include <kernel/dpl/DebugP.h>

void Drivers_open(void)
{
    Drivers_pmicOpen();

    Drivers_edmaOpen();
    Drivers_i2cOpen();
    Drivers_ospiOpen();
    Drivers_mcspiOpen();
    Drivers_mcanOpen();
    Drivers_uartOpen();
}

void Drivers_close(void)
{
    Drivers_i2cClose();
    Drivers_ospiClose();
    Drivers_mcspiClose();
    Drivers_mcanClose();
    Drivers_uartClose();
    Drivers_edmaClose();
}

/*
 * I2C
 */



/* I2C Driver handles */
I2C_Handle gI2cHandle[CONFIG_I2C_HLD_NUM_INSTANCES];

/* I2C Driver Parameters */
I2C_Params gI2cParams[CONFIG_I2C_HLD_NUM_INSTANCES] =
{
    {
        .transferMode        = I2C_MODE_BLOCKING,
        .transferCallbackFxn = NULL,
        .bitRate             = I2C_400KHZ,
    },
    {
        .transferMode        = I2C_MODE_BLOCKING,
        .transferCallbackFxn = NULL,
        .bitRate             = I2C_400KHZ,
    },
};

void Drivers_i2cOpen(void)
{
    int32_t  status = SystemP_SUCCESS;
    uint32_t instCnt;



    for(instCnt = 0U; instCnt < CONFIG_I2C_HLD_NUM_INSTANCES; instCnt++)
    {
        gI2cHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_I2C_HLD_NUM_INSTANCES; instCnt++)
    {
        gI2cHandle[instCnt] = I2C_open(instCnt, &gI2cParams[instCnt]);
        if(NULL == gI2cHandle[instCnt])
        {
            DebugP_logError("I2C open failed for HLD instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_SUCCESS != status)
    {
        Drivers_i2cClose();   /* Exit gracefully */
    }
    return;
}

void Drivers_i2cClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_I2C_HLD_NUM_INSTANCES; instCnt++)
    {
        if(gI2cHandle[instCnt] != NULL)
        {
            I2C_close(gI2cHandle[instCnt]);
            gI2cHandle[instCnt] = NULL;
        }
    }
    return;
}
/*
 * OSPI
 */
/* OSPI Driver handles */
OSPI_Handle gOspiHandle[CONFIG_OSPI_NUM_INSTANCES];

/* OSPI Driver Parameters */
OSPI_Params gOspiParams[CONFIG_OSPI_NUM_INSTANCES] =
{
    {
        .ospiDmaChIndex = 0,
    },
};

void Drivers_ospiOpen(void)
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_OSPI_NUM_INSTANCES; instCnt++)
    {
        gOspiHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_OSPI_NUM_INSTANCES; instCnt++)
    {
        gOspiHandle[instCnt] = OSPI_open(instCnt, &gOspiParams[instCnt]);
        if(NULL == gOspiHandle[instCnt])
        {
            DebugP_logError("OSPI open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Drivers_ospiClose();   /* Exit gracefully */
    }

    return;
}

void Drivers_ospiClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_OSPI_NUM_INSTANCES; instCnt++)
    {
        if(gOspiHandle[instCnt] != NULL)
        {
            OSPI_close(gOspiHandle[instCnt]);
            gOspiHandle[instCnt] = NULL;
        }
    }

    return;
}

/*
 * MCSPI
 */
/* MCSPI Driver handles */
MCSPI_Handle gMcspiHandle[CONFIG_MCSPI_NUM_INSTANCES];
/* MCSPI Driver Open Parameters */
MCSPI_OpenParams gMcspiOpenParams[CONFIG_MCSPI_NUM_INSTANCES] =
{
    {
        .transferMode           = MCSPI_TRANSFER_MODE_BLOCKING,
        .transferTimeout        = SystemP_WAIT_FOREVER,
        .transferCallbackFxn    = NULL,
        .msMode                 = MCSPI_MS_MODE_CONTROLLER,
        .mcspiDmaIndex = -1,
    },
};
/* MCSPI Driver Channel Configurations */
MCSPI_ChConfig gConfigMcspi0ChCfg[CONFIG_MCSPI0_NUM_CH] =
{
    {
        .chNum              = MCSPI_CHANNEL_0,
        .frameFormat        = MCSPI_FF_POL0_PHA0,
        .bitRate            = 1000000,
        .csPolarity         = MCSPI_CS_POL_LOW,
        .trMode             = MCSPI_TR_MODE_TX_RX,
        .inputSelect        = MCSPI_IS_D1,
        .dpe0               = MCSPI_DPE_ENABLE,
        .dpe1               = MCSPI_DPE_DISABLE,
        .slvCsSelect        = MCSPI_SLV_CS_SELECT_0,
        .startBitEnable     = FALSE,
        .startBitPolarity   = MCSPI_SB_POL_LOW,
        .csIdleTime         = MCSPI_TCS0_0_CLK,
        .turboEnable        = FALSE,
        .defaultTxData      = 0x0U,
        .txFifoTrigLvl      = 16U,
        .rxFifoTrigLvl      = 16U,
    },
};

MCSPI_ChConfig *gConfigMcspiChCfg[1] =
{
    gConfigMcspi0ChCfg,
};



MCSPI_DmaChConfig gMcspiDmaChConfig[1] =
{
    NULL,
};

void Drivers_mcspiOpen(void)
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_MCSPI_NUM_INSTANCES; instCnt++)
    {
        gMcspiHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_MCSPI_NUM_INSTANCES; instCnt++)
    {
        gMcspiHandle[instCnt] = MCSPI_open(instCnt, &gMcspiOpenParams[instCnt]);
        if(NULL == gMcspiHandle[instCnt])
        {
            DebugP_logError("MCSPI open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Drivers_mcspiClose();   /* Exit gracefully */
    }

    return;
}

void Drivers_mcspiClose(void)
{
    uint32_t instCnt;
    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_MCSPI_NUM_INSTANCES; instCnt++)
    {
        if(gMcspiHandle[instCnt] != NULL)
        {
            MCSPI_close(gMcspiHandle[instCnt]);
            gMcspiHandle[instCnt] = NULL;
        }
    }
    return;
}

/*
 * EDMA
 */
/* EDMA Driver handles */
EDMA_Handle gEdmaHandle[CONFIG_EDMA_NUM_INSTANCES];

/* EDMA Driver Open Parameters */
EDMA_Params gEdmaParams[CONFIG_EDMA_NUM_INSTANCES] =
{
    {
        .intrEnable = TRUE,
        .errIntrEnable = FALSE,
    },
};

void Drivers_edmaOpen(void)
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_EDMA_NUM_INSTANCES; instCnt++)
    {
        gEdmaHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_EDMA_NUM_INSTANCES; instCnt++)
    {
        gEdmaHandle[instCnt] = EDMA_open(instCnt, &gEdmaParams[instCnt]);
        if(NULL == gEdmaHandle[instCnt])
        {
            DebugP_logError("EDMA open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Drivers_edmaClose();   /* Exit gracefully */
    }

    return;
}

void Drivers_edmaClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_EDMA_NUM_INSTANCES; instCnt++)
    {
        if(gEdmaHandle[instCnt] != NULL)
        {
            EDMA_close(gEdmaHandle[instCnt]);
            gEdmaHandle[instCnt] = NULL;
        }
    }

    return;
}

void Drivers_mcanOpen(void)
{
    return;
}

void Drivers_mcanClose(void)
{
    return;
}

/*
 * UART
 */

/* UART Driver handles */
UART_Handle gUartHandle[CONFIG_UART_NUM_INSTANCES];

#include <drivers/uart/v0/lld/dma/uart_dma.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/edma.h>

UART_DmaChConfig gUartDmaChConfig[CONFIG_UART_NUM_INSTANCES] =
{
                NULL,
};

/* UART Driver Parameters */
UART_Params gUartParams[CONFIG_UART_NUM_INSTANCES] =
{
        {
            .baudRate           = 115200,
            .dataLength         = UART_LEN_8,
            .stopBits           = UART_STOPBITS_1,
            .parityType         = UART_PARITY_NONE,
            .readMode           = UART_TRANSFER_MODE_BLOCKING,
            .readReturnMode     = UART_READ_RETURN_MODE_FULL,
            .writeMode          = UART_TRANSFER_MODE_BLOCKING,
            .readCallbackFxn    = NULL,
            .writeCallbackFxn   = NULL,
            .hwFlowControl      = FALSE,
            .hwFlowControlThr   = UART_RXTRIGLVL_16,
            .transferMode       = UART_CONFIG_MODE_POLLED,
            .skipIntrReg        = FALSE,
            .uartDmaIndex = -1,
            .intrNum            = 38U,
            .intrPriority       = 4U,
            .operMode           = UART_OPER_MODE_16X,
            .rxTrigLvl          = 8U,
            .txTrigLvl          = 32U,
            .rxEvtNum           = 0U,
            .txEvtNum           = 0U,
        },
};

void Drivers_uartOpen(void)
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++)
    {
        gUartHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++)
    {
        gUartHandle[instCnt] = UART_open(instCnt, &gUartParams[instCnt]);
        if(NULL == gUartHandle[instCnt])
        {
            DebugP_logError("UART open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Drivers_uartClose();   /* Exit gracefully */
    }

    return;
}

void Drivers_uartClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_UART_NUM_INSTANCES; instCnt++)
    {
        if(gUartHandle[instCnt] != NULL)
        {
            UART_close(gUartHandle[instCnt]);
            gUartHandle[instCnt] = NULL;
        }
    }

    return;
}
