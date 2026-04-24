/*
 *  Copyright (C) 2026 Texas Instruments Incorporated
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
 *  \file   dthe_rtdma.c
 *
 *  \brief  This file contains the implementation of Dthe with RTDMA driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <security_common/drivers/crypto/dthe/dma/rtdma/dthe_rtdma.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define RTDMA_BLOCK_SIZE                (16U)
#define RTDMA_AES_BURST_SIZE            (16U)
#define RTDMA_WORD_SIZE                 (4U)

/* DTHE AES/SHA/SM3 DMA trigger sources for HSM RTDMA */
#define RTDMA_TRIGGER_DTHE_AES_DATAIN   DMA_TRIGGER_DTHE_AES_DMA_S_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_AES_DATAOUT  DMA_TRIGGER_DTHE_AES_DMA_S_DATAOUT_REQ
#define RTDMA_TRIGGER_DTHE_SHA_DATAIN   DMA_TRIGGER_DTHE_SHA_DMA_S_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_SM3_DATAIN   DMA_TRIGGER_DTHE_SM3_DATAIN_REQ

/* RTDMA channel parameter indices */
#define RTDMA_TX_CH_PARAMS_INDEX        (0U)
#define RTDMA_RX_CH_PARAMS_INDEX        (1U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* Structure to hold RTDMA channel parameters */
typedef struct RTDMA_ChannelParams_s
{
    uint32_t    channelBase;
    bool        isConfigured;
} RTDMA_ChannelParams;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/** RTDMA function pointer array for dma cfg */
DMA_Fxns gRtdmaFxns =
{
    .cfgDmaTxChFxn = RTDMA_Config_TxChannel,
    .enableTxTransferRegionFxn = RTDMA_enableTxTransferRegion,
    .waitForTxTranferFxn = RTDMA_WaitForTxTransfer,
    .disableTxChFxn = RTDMA_disableTxChannel,
    .cfgDmaRxChFxn = RTDMA_Config_RxChannel,
    .enableRxTransferRegionFxn = RTDMA_enableRxTransferRegion,
    .waitForRxTranferFxn = RTDMA_WaitForRxTransfer,
    .disableRxChFxn = RTDMA_disableRxChannel,
    .memCopyFxn = NULL,
};

/** Global RTDMA channel parameters for TX and RX */
static RTDMA_ChannelParams gRtdmaChParams[2] = {{0}, {0}};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void RTDMA_init(void)
{
    /* Perform hard reset of the RTDMA controller */
    DMA_initController(RTDMA1_BASE);

    /* Configure emulation mode to free run */
    DMA_setEmulationMode(RTDMA1_BASE, DMA_EMULATION_FREE_RUN);

    /* Configure priority mode */
    DMA_setPriorityMode(RTDMA1_BASE, DMA_PRIORITY_SOFTWARE_CONFIG);

    /* Set channel priorities */
    DMA_setChannelPriority(RTDMA1_BASE, DMA_CH1, DMA_CHPRIORITY2);
    DMA_setChannelPriority(RTDMA1_BASE, DMA_CH2, DMA_CHPRIORITY2);

    /* Lock the DMA configuration */
    DMA_lockDMAConfig(RTDMA1_BASE);
}

/**
 *  \brief Configure RTDMA TX channel for both AES and SHA operations
 *
 *  This function handles both AES and SHA channel configuration in a unified manner,
 *  similar to EDMA implementation. The operationType parameter determines the specific
 *  configuration:
 *  - DMA_AES_ENABLE: Configures for AES data input with fixed burst size (16 words)
 *  - DMA_SHA_ENABLE: Configures for SHA data input with variable burst size (blockSize)
 *
 *  \param handle        [IN] DMA driver handle
 *  \param srcAddress    [IN] Source buffer address
 *  \param dstAddress    [IN] Destination peripheral register address
 *  \param numBlocks     [IN] Number of blocks/bursts to transfer
 *  \param blockSize     [IN] Size of each block in words (used for SHA)
 *  \param operationType [IN] DMA_AES_ENABLE or DMA_SHA_ENABLE
 *
 *  \return SystemP_SUCCESS on success, SystemP_FAILURE on error
 */
int32_t RTDMA_Config_TxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, uint16_t blockSize, int32_t operationType)
{
    int32_t         status = SystemP_FAILURE;
    uint32_t        rtdmaChannelBase;
    DMA_Trigger     trigger;
    uint16_t        burstSize;
    uint16_t        transferSize;
    int16_t         srcBurstStep;
    int16_t         destBurstStep;
    int16_t         srcTransferStep;
    int16_t         destTransferStep;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else if(((uint32_t)srcAddress & 0x3U) != 0U)
    {
        /* DMA_CFG_SIZE_32BIT requires a 4-byte-aligned source address.
         * Return failure so the caller can fall back to the CPU path. */
        status = SystemP_FAILURE;
    }
    else
    {
        /* Handle is already the pointer to gRtdmaHandle array, not DMA_Config */
        uint32_t *rtdmaHandleArray = (uint32_t *)handle;
        rtdmaChannelBase = rtdmaHandleArray[RTDMA_TX_CH_PARAMS_INDEX];

        /* Configure based on operation type */
        if(operationType == DMA_AES_ENABLE)
        {
            /* AES configuration */
            trigger = RTDMA_TRIGGER_DTHE_AES_DATAIN;
            burstSize = RTDMA_AES_BURST_SIZE;
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment source by 4 bytes per word */
            destBurstStep = (int16_t)(-RTDMA_WORD_SIZE);   /* Destination stays at same location (peripheral register) */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);  /* Move to next burst */
            destTransferStep = (int16_t)(3 * RTDMA_WORD_SIZE); /* Destination doesn't change between transfers */
            status = SystemP_SUCCESS;
        }
        else if(operationType == DMA_SHA_ENABLE)
        {
            /* SHA configuration */
            trigger = RTDMA_TRIGGER_DTHE_SHA_DATAIN;
            burstSize = blockSize*4U;
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment source by 4 bytes per word */
            destBurstStep = 0;                              /* Destination stays at same location (peripheral register) */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);  /* Move to next block */
            destTransferStep = 0;                           /* Destination doesn't change between transfers */
            status = SystemP_SUCCESS;

        }
        else if(operationType == DMA_SM3_ENABLE)
        {
            /* SM3 configuration - SM3 has a 16-word register array (not FIFO like SHA)
             * Data must be written to SM3_DATA_IN[0] through SM3_DATA_IN[15] sequentially.
             * Burst step is applied (blockSize-1) times, so transfer step must compensate. */
            trigger = RTDMA_TRIGGER_DTHE_SM3_DATAIN;
            burstSize = blockSize*4U;                        /* Burst size in bytes (16 words × 4 = 64 bytes) */
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;        /* Increment source by 4 bytes per word */
            destBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment dest to write to SM3_DATA_IN[0], [1], ..., [15] */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);   /* Continue to next block in source buffer */
            destTransferStep = (int16_t)(-((int16_t)(blockSize - 1U) * (int16_t)RTDMA_WORD_SIZE)); /* Reset dest back to SM3_DATA_IN[0] */
            status = SystemP_SUCCESS;
        }
        else
        {
            status = SystemP_FAILURE;
        }

        if(status != SystemP_FAILURE)
        {
            /* Configure source and destination addresses */
            DMA_configAddresses(rtdmaChannelBase, dstAddress, srcAddress);

            /* Configure burst parameters */
            DMA_configBurst(rtdmaChannelBase,
                           burstSize,
                           srcBurstStep,
                           destBurstStep);

            /* Configure transfer parameters */
            DMA_configTransfer(rtdmaChannelBase,
                              transferSize,
                              srcTransferStep,
                              destTransferStep);

            /* Configure mode and trigger */
            DMA_configMode(rtdmaChannelBase,
                          trigger,
                          DMA_CFG_ONESHOT_DISABLE |
                          DMA_CFG_CONTINUOUS_DISABLE |
                          DMA_CFG_SIZE_32BIT);

            DMA_setBurstSignalingMode(rtdmaChannelBase, DMA_BURST_SIGNALING_DISABLE);

            DMA_setInterruptMode(rtdmaChannelBase, DMA_INT_AT_END);

            DMA_disableOverrunInterrupt(rtdmaChannelBase);

            /* Save TX channel parameters */
            gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase = rtdmaChannelBase;
            gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].isConfigured = true;

            status = SystemP_SUCCESS;
        }
    }

    return (status);
}

int32_t RTDMA_enableTxTransferRegion(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Enable trigger */
        DMA_enableTrigger(gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase);

        /* Start the channel */
        DMA_startChannel(gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase);

        status = SystemP_SUCCESS;
    }

    return (status);
}

int32_t RTDMA_WaitForTxTransfer(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    /* Poll until transfer is complete (transfer count reaches zero) */
    while(DMA_getRunStatusFlag(gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase))
    {
        /* Wait for transfer to complete */
    }

    status = SystemP_SUCCESS;

    return (status);
}


int32_t RTDMA_Config_RxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks)
{
    int32_t         status = SystemP_FAILURE;
    uint32_t        rtdmaChannelBase;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Handle is already the pointer to gRtdmaHandle array, not DMA_Config */
        uint32_t *rtdmaHandleArray = (uint32_t *)handle;
        rtdmaChannelBase = rtdmaHandleArray[RTDMA_RX_CH_PARAMS_INDEX];

        /* Configure source and destination addresses */
        DMA_configAddresses(rtdmaChannelBase, dstAddress, srcAddress);

        /* Configure burst parameters for AES RX */
        DMA_configBurst(rtdmaChannelBase,
                       RTDMA_AES_BURST_SIZE,    /* burst size in words */
                       (int16_t)(-RTDMA_WORD_SIZE),                        /* source step (no increment for peripheral) */
                       (int16_t)(RTDMA_WORD_SIZE)); /* dest step per word */

        /* Configure transfer parameters */
        DMA_configTransfer(rtdmaChannelBase,
                          numBlocks,        /* number of bursts */
                          (int16_t)(3 * RTDMA_WORD_SIZE),                /* source transfer step */
                          (int16_t)(RTDMA_WORD_SIZE));               /* dest transfer step */

        /* Configure mode and trigger for AES data output */
        DMA_configMode(rtdmaChannelBase,
                      RTDMA_TRIGGER_DTHE_AES_DATAOUT,
                      DMA_CFG_ONESHOT_DISABLE |
                      DMA_CFG_CONTINUOUS_DISABLE |
                      DMA_CFG_SIZE_32BIT);

        DMA_setBurstSignalingMode(rtdmaChannelBase, DMA_BURST_SIGNALING_DISABLE);

        DMA_setInterruptMode(rtdmaChannelBase, DMA_INT_AT_END);

        DMA_disableOverrunInterrupt(rtdmaChannelBase);

        /* Save RX channel parameters */
        gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase = rtdmaChannelBase;
        gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].isConfigured = true;

        status = SystemP_SUCCESS;
    }

    return (status);
}

int32_t RTDMA_enableRxTransferRegion(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Enable trigger */
        DMA_enableTrigger(gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase);

        /* Start the channel */
        DMA_startChannel(gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase);

        status = SystemP_SUCCESS;
    }

    return (status);
}

int32_t RTDMA_WaitForRxTransfer(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    /* Poll until transfer is complete (transfer count reaches zero) */
    while(DMA_getRunStatusFlag(gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase))
    {
        /* Wait for transfer to complete */
    }

    status = SystemP_SUCCESS;

    return (status);
}

int32_t RTDMA_disableTxChannel(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Stop the channel */
        DMA_stopChannel(gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase);

        /* Clear configured flag */
        gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].isConfigured = false;

        status = SystemP_SUCCESS;
    }

    return (status);
}

int32_t RTDMA_disableRxChannel(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Stop the channel */
        DMA_stopChannel(gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase);

        /* Clear configured flag */
        gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].isConfigured = false;

        status = SystemP_SUCCESS;
    }

    return (status);
}
