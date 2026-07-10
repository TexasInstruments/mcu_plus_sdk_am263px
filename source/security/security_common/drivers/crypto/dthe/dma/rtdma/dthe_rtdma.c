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

#define RTDMA_AES_SM4_BURST_SIZE        (16U)
#define RTDMA_WORD_SIZE                 (4U)
#define RTDMA_WORD_SIZE_NEG             ((int16_t)-4)   /* Negative word size step for DMA burst/transfer step config */
#define RTDMA_WORD_SIZE_NEG_12          ((int16_t)-12)  /* Negative (wordSize-1)*wordSize = -(3*4) = -12 for AES/SM4 transfer step reset */
#define RTDMA_SM3_TRANSFER_STEP_NEG     ((int16_t)-60)  /* Negative (SM3_blockSize-1)*wordSize = -(15*4) = -60 for SM3 transfer step reset */

/* DTHE AES/SHA/SM3/SM4 DMA trigger sources for HSM RTDMA */
#define RTDMA_TRIGGER_DTHE_AES_DATAIN   DMA_TRIGGER_DTHE_AES_DMA_S_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_AES_DATAOUT  DMA_TRIGGER_DTHE_AES_DMA_S_DATAOUT_REQ
#define RTDMA_TRIGGER_DTHE_SHA_DATAIN   DMA_TRIGGER_DTHE_SHA_DMA_S_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_SM3_DATAIN   DMA_TRIGGER_DTHE_SM3_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_SM4_DATAIN   DMA_TRIGGER_DTHE_SM4_DATAIN_REQ
#define RTDMA_TRIGGER_DTHE_SM4_DATAOUT  DMA_TRIGGER_DTHE_SM4_DATAOUT_REQ

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
    .startTxChannelFxn = RTDMA_startTxChannel,
    .enableTxTransferRegionFxn = RTDMA_enableTxTransferRegion,
    .waitForTxTranferFxn = RTDMA_WaitForTxTransfer,
    .disableTxChFxn = RTDMA_disableTxChannel,
    .cfgDmaRxChFxn = RTDMA_Config_RxChannel,
    .startRxChannelFxn = RTDMA_startRxChannel,
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
            burstSize = RTDMA_AES_SM4_BURST_SIZE;
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment source by 4 bytes per word */
            destBurstStep = RTDMA_WORD_SIZE_NEG;   /* Destination stays at same location (peripheral register) */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);  /* Move to next burst */
            destTransferStep = (int16_t)((RTDMA_WORD_SIZE - 1U) * RTDMA_WORD_SIZE); /* Destination doesn't change between transfers */
            status = SystemP_SUCCESS;
        }
        else if(operationType == DMA_SHA_ENABLE)
        {
            /* SHA configuration */
            trigger = RTDMA_TRIGGER_DTHE_SHA_DATAIN;
            burstSize = blockSize*RTDMA_WORD_SIZE;
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment source by 4 bytes per word */
            destBurstStep = 0U;                              /* Destination stays at same location (peripheral register) */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);  /* Move to next block */
            destTransferStep = 0U;                           /* Destination doesn't change between transfers */
            status = SystemP_SUCCESS;

        }
        else if(operationType == DMA_SM3_ENABLE)
        {
            /* SM3 configuration - SM3 has a 16-word register array (not FIFO like SHA)
             * Data must be written to SM3_DATA_IN[0] through SM3_DATA_IN[15] sequentially.
             * Burst step is applied (blockSize-1) times, so transfer step must compensate. */
            trigger = RTDMA_TRIGGER_DTHE_SM3_DATAIN;
            burstSize = blockSize*RTDMA_WORD_SIZE;                        /* Burst size in bytes (16 words × 4 = 64 bytes) */
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;        /* Increment source by 4 bytes per word */
            destBurstStep = (int16_t)RTDMA_WORD_SIZE;       /* Increment dest to write to SM3_DATA_IN[0], [1], ..., [15] */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);   /* Continue to next block in source buffer */
            destTransferStep = RTDMA_SM3_TRANSFER_STEP_NEG; /* Reset dest back to SM3_DATA_IN[0] */
            status = SystemP_SUCCESS;
        }
        else if(operationType == DMA_SM4_ENABLE)
        {
            /* SM4 configuration - SM4 has 4 individual registers (SM4_DATA_IN_0/1/2/3)
             * Opposite to AES, writes from DATA_IN_0 up to DATA_IN_3 using positive step.
             * Block size: 16 bytes (4 words) */
            trigger = RTDMA_TRIGGER_DTHE_SM4_DATAIN;
            burstSize = RTDMA_AES_SM4_BURST_SIZE;               /* 16 bytes (4 words) per block */
            transferSize = numBlocks;
            srcBurstStep = (int16_t)RTDMA_WORD_SIZE;        /* Increment source by 4 bytes per word */
            destBurstStep = (int16_t)(RTDMA_WORD_SIZE);     /* Increment dest: DATA_IN_0 → DATA_IN_1 → DATA_IN_2 → DATA_IN_3 */
            srcTransferStep = (int16_t)(RTDMA_WORD_SIZE);   /* Continue to next block in source buffer */
            destTransferStep = RTDMA_WORD_SIZE_NEG_12; /* Reset dest back to DATA_IN_0 */
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

int32_t RTDMA_startTxChannel(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Start the channel */
        DMA_startChannel(gRtdmaChParams[RTDMA_TX_CH_PARAMS_INDEX].channelBase);

        status = SystemP_SUCCESS;
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

int32_t RTDMA_Config_RxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, int32_t operationType)
{
    int32_t         status = SystemP_FAILURE;
    uint32_t        rtdmaChannelBase;
    DMA_Trigger     trigger;
    uint16_t        burstSize;
    int16_t         srcBurstStep;
    int16_t         destBurstStep;
    int16_t         srcTransferStep;
    int16_t         destTransferStep;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Handle is already the pointer to gRtdmaHandle array, not DMA_Config */
        uint32_t *rtdmaHandleArray = (uint32_t *)handle;
        rtdmaChannelBase = rtdmaHandleArray[RTDMA_RX_CH_PARAMS_INDEX];

        /* Select trigger, burst size, and step values based on operation type */
        if(operationType == DMA_AES_ENABLE)
        {
            /* AES RX: reads DATA_OUT_3 → DATA_OUT_0 (negative step) */
            trigger = RTDMA_TRIGGER_DTHE_AES_DATAOUT;
            burstSize = RTDMA_AES_SM4_BURST_SIZE;
            srcBurstStep = RTDMA_WORD_SIZE_NEG;      /* Decrement: DATA_OUT_3 → DATA_OUT_0 */
            destBurstStep = (int16_t)(RTDMA_WORD_SIZE);      /* Increment destination buffer */
            srcTransferStep = (int16_t)((RTDMA_WORD_SIZE - 1U) * RTDMA_WORD_SIZE); /* Reset back to DATA_OUT_3 */
            destTransferStep = (int16_t)(RTDMA_WORD_SIZE);   /* Continue in destination buffer */
            status = SystemP_SUCCESS;
        }
        else if(operationType == DMA_SM4_ENABLE)
        {
            /* SM4 RX: reads DATA_OUT_0 → DATA_OUT_3 (positive step) */
            trigger = RTDMA_TRIGGER_DTHE_SM4_DATAOUT;
            burstSize = RTDMA_AES_SM4_BURST_SIZE; 
            srcBurstStep = (int16_t)(RTDMA_WORD_SIZE);        /* Increment: DATA_OUT_0 → DATA_OUT_3 */
            destBurstStep = (int16_t)(RTDMA_WORD_SIZE);       /* Increment destination buffer */
            srcTransferStep = RTDMA_WORD_SIZE_NEG_12; /* Reset back to DATA_OUT_0 */
            destTransferStep = (int16_t)(RTDMA_WORD_SIZE);    /* Continue in destination buffer */
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

            /* Configure burst parameters for RX */
            DMA_configBurst(rtdmaChannelBase,
                        burstSize,
                        srcBurstStep,
                        destBurstStep);

            /* Configure transfer parameters */
            DMA_configTransfer(rtdmaChannelBase,
                            numBlocks,
                            srcTransferStep,
                            destTransferStep);

            /* Configure mode and trigger for data output */
            DMA_configMode(rtdmaChannelBase,
                        trigger,
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
    }

    return (status);
}

int32_t RTDMA_startRxChannel(DMA_Handle handle)
{
    int32_t status = SystemP_FAILURE;

    if(NULL == handle)
    {
        status = SystemP_FAILURE;
    }
    else
    {
        /* Start the channel */
        DMA_startChannel(gRtdmaChParams[RTDMA_RX_CH_PARAMS_INDEX].channelBase);

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
