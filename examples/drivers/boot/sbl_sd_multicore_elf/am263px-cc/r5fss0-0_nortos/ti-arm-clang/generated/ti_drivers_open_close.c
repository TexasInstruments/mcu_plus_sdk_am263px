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
    Drivers_mmcsdOpen();
    Drivers_mcspiOpen();
    Drivers_ffOpen();
    Drivers_uartOpen();
}

void Drivers_close(void)
{
    Drivers_mcspiClose();
    Drivers_ffClose();
    Drivers_uartClose();
    Drivers_edmaClose();
    Drivers_mmcsdClose();
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

/*
 * FreeRTOS+FAT
 */
/* FreeRTOS+FAT disk */
FF_Disk_t gFFDisks[CONFIG_FF_NUM_INSTANCES];

/* FreeRTOS+FAT MMCSD Configs */
FF_MMCSD_Config gFFMmcsdConfig[CONFIG_FF_NUM_INSTANCES] =
{
    {
            .mediaType         = MMCSD_CARD_TYPE_SD,
        .mmcsdInstance     = CONFIG_MMCSD0,
    },
};

/* FreeRTOS+FAT partitions */
FF_MMCSD_PartitionId gFFPartitionIds[CONFIG_FF_NUM_INSTANCES] =
{
    {
    	.partitionNumber = 0,
    	.partitionName   = "/sd0",
    },
};

void Drivers_ffOpen(void)
{
	uint32_t instCnt;
	int32_t status = SystemP_SUCCESS;
	FF_Disk_t *diskStatus;

	/* Initialize all instances */
	for(instCnt = 0U; instCnt < CONFIG_FF_NUM_INSTANCES; instCnt++)
	{
		diskStatus = FF_MMCSDDiskInit(gFFPartitionIds[instCnt].partitionName,
		                             &gFFDisks[instCnt],
		                             &gFFMmcsdConfig[instCnt],
		                             gFFPartitionIds[instCnt].partitionNumber);
		if(NULL == diskStatus)
		{
		    DebugP_logError("FF Disk Init failed for instance %d !!!\r\n", instCnt);
		    status = SystemP_FAILURE;
		    break;
		}
	}

	if(SystemP_FAILURE == status)
	{
		Drivers_ffClose(); /* Exit gracefully */
	}
}

void Drivers_ffClose(void)
{
	uint32_t instCnt;

	/* Close all instances that are open */
	for(instCnt = 0U; instCnt < CONFIG_FF_NUM_INSTANCES; instCnt++)
	{
	    FF_MMCSDDiskDelete(&gFFDisks[instCnt]);
	}
}
/*
 * MMCSD
 */
/* MMCSD Device Data structures */
MMCSD_SdDeviceData gSdData0;

/* MMCSD temporary data buffers */
uint8_t gMmcsdDataBuf0[512U] __attribute__((aligned(128U)));;


/* MMCSD Driver handles */
MMCSD_Handle gMmcsdHandle[CONFIG_MMCSD_NUM_INSTANCES];

/* MMCSD Driver Parameters */
MMCSD_Params gMmcsdParams[CONFIG_MMCSD_NUM_INSTANCES] =
{
    {
        .deviceData = &gSdData0,
        .dataBuf    = &gMmcsdDataBuf0[0],
        .transferMode = MMCSD_MODE_BLOCKING,
        .txnCallbackFxn = NULL,
        .edmaInst = -1,
    },
};


MMCSD_DmaChConfig gMmcsdDmaChConfig[1] = 
{
    
        NULL,
};

void Drivers_mmcsdOpen(void)
{
    uint32_t instCnt;
    int32_t  status = SystemP_SUCCESS;

    for(instCnt = 0U; instCnt < CONFIG_MMCSD_NUM_INSTANCES; instCnt++)
    {
        gMmcsdHandle[instCnt] = NULL;   /* Init to NULL so that we can exit gracefully */
    }

    /* Open all instances */
    for(instCnt = 0U; instCnt < CONFIG_MMCSD_NUM_INSTANCES; instCnt++)
    {
        gMmcsdHandle[instCnt] = MMCSD_open(instCnt, &gMmcsdParams[instCnt]);
        if(NULL == gMmcsdHandle[instCnt])
        {
            DebugP_logError("MMCSD open failed for instance %d !!!\r\n", instCnt);
            status = SystemP_FAILURE;
            break;
        }
    }

    if(SystemP_FAILURE == status)
    {
        Drivers_mmcsdClose();   /* Exit gracefully */
    }

    return;
}

void Drivers_mmcsdClose(void)
{
    uint32_t instCnt;

    /* Close all instances that are open */
    for(instCnt = 0U; instCnt < CONFIG_MMCSD_NUM_INSTANCES; instCnt++)
    {
        if(gMmcsdHandle[instCnt] != NULL)
        {
            MMCSD_close(gMmcsdHandle[instCnt]);
            gMmcsdHandle[instCnt] = NULL;
        }
    }

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
