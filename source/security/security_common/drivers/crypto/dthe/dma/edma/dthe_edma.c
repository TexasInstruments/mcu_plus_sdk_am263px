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

/**
 *  \file   dthe_dma.c
 *
 *  \brief  This file contains the implementation of Dthe with EDMA driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <security_common/drivers/crypto/dthe/dma/edma/dthe_edma.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define EDMA_TEST_EVT_QUEUE_NO          (0U)
#define EDMA_NUM_OF_CC_PARAMS           (2U)
#define EDMA_NUM_OF_EDMA_PARAMS         (2U)
#define EDMA_A_COUNT_VALUE              (4U)
#define EDMA_B_COUNT_VALUE              (4U)
#define EDMA_BINEX_POSITIVE_VALUE       (4U)
#define EDMA_BINEX_NEGATIVE_VALUE       (-4U)
#define EDMA_NULL_INDEX_VALUE           (0U)
#define EDMA_LINK_ADDRESS               (0xFFFFU)
#define EDMA_TX_CH_PARAMS_INDEX         (0U)
#define EDMA_RX_CH_PARAMS_INDEX         (1U)
#define EDMA_AES_TX_CHANNEL_NUMBER      (16U)
#define EDMA_SHA_TX_CH_NUMBER           (9U)
#define EDMA_AES_RX_CHANNEL_NUMBER      (17U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/** Edma function pointer array for dma cfg */
DMA_Fxns gEdmaFxns =
{
    .cfgDmaTxChFxn = EDMA_Config_TxChannel,
    .enableTxTransferRegionFxn = EDMA_enableTxTransferRegion,
    .waitForTxTranferFxn = EDMA_WaitForTxTransfer,
    .disableTxChFxn = EDMA_disableTxChannel,
    .cfgDmaRxChFxn = EDMA_Config_RxChannel,
    .enableRxTransferRegionFxn = EDMA_enableRxTransferRegion,
    .waitForRxTranferFxn = EDMA_WaitForRxTransfer,
    .disableRxChFxn = EDMA_disableRxChannel,
};

/** Global CC edma params */
EDMACCPaRAMEntry   gEdmaCCParams[2];
/** Global dma params */
DMA_Params         globalEdmaParams[2];

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

int32_t EDMA_Config_TxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, uint16_t blockSize, int32_t operationType)
{
    int32_t         status = SystemP_FAILURE;
    DMA_Config      *dmaCfg;
    EDMA_Handle     edmaHandler; 
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {
        dmaCfg = (DMA_Config *)handle;
        edmaHandler = dmaCfg->dmaHandle;

        globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr = EDMA_getBaseAddr(edmaHandler);

        globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId = EDMA_getRegionId(edmaHandler);

        if( operationType == DMA_AES_ENABLE )
        {
            globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh = EDMA_AES_TX_CHANNEL_NUMBER;
            status = EDMA_allocDmaChannel(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh);
        }
        else if ( operationType == DMA_SHA_ENABLE )
        {
            globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh = EDMA_SHA_TX_CH_NUMBER;
            status = EDMA_allocDmaChannel(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh);
        }
        else
        {
            status  = SystemP_FAILURE;
        }

        globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc = EDMA_RESOURCE_ALLOC_ANY;
        status = EDMA_allocTcc(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc);

        globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].param = EDMA_RESOURCE_ALLOC_ANY;
        status = EDMA_allocParam(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].param);

        /* Request channel */
        EDMA_configureChannelRegion(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId, EDMA_CHANNEL_TYPE_DMA,
        globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].param, EDMA_TEST_EVT_QUEUE_NO);
        
        if( operationType == DMA_AES_ENABLE )
        {
            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX]);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcAddr       = (uint32_t) srcAddress;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destAddr      = (uint32_t) dstAddress;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].aCnt          = (uint16_t) EDMA_A_COUNT_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].bCnt          = (uint16_t) EDMA_B_COUNT_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].cCnt          = (uint16_t) numBlocks;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].bCntReload    = (uint16_t) EDMA_NULL_INDEX_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcBIdx       = (int16_t) EDMA_BINEX_POSITIVE_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destBIdx      = (int16_t) EDMA_BINEX_NEGATIVE_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcCIdx       = (int16_t) (EDMA_A_COUNT_VALUE * EDMA_B_COUNT_VALUE);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destCIdx      = (int16_t) EDMA_NULL_INDEX_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].linkAddr      = EDMA_LINK_ADDRESS;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcBIdxExt    = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_BINEX_POSITIVE_VALUE);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destBIdxExt   = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_BINEX_NEGATIVE_VALUE);

            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].opt          |=
                (EDMA_OPT_TCINTEN_MASK |
                 ((((uint32_t)globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc) << EDMA_OPT_TCC_SHIFT) & EDMA_OPT_TCC_MASK)) | 
                ((uint32_t)1 << EDMA_OPT_SYNCDIM_SHIFT);
       }
       else if( operationType == DMA_SHA_ENABLE )
       {
            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX]);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcAddr       = (uint32_t) srcAddress;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destAddr      = (uint32_t) dstAddress;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].aCnt          = (uint16_t) EDMA_A_COUNT_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].bCnt          = (uint16_t) blockSize;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].cCnt          = (uint16_t) numBlocks;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].bCntReload    = (uint16_t) EDMA_NULL_INDEX_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcBIdx       = (int16_t) EDMA_BINEX_POSITIVE_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destBIdx      = (int16_t) EDMA_NULL_INDEX_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcCIdx       = (int16_t) (blockSize * EDMA_A_COUNT_VALUE);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destCIdx      = (int16_t) EDMA_NULL_INDEX_VALUE;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].linkAddr      = EDMA_LINK_ADDRESS;
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].srcBIdxExt    = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_BINEX_POSITIVE_VALUE);
            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].destBIdxExt   = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_NULL_INDEX_VALUE);

            gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX].opt          |=
                (EDMA_OPT_TCINTEN_MASK |
                 ((((uint32_t)globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc) << EDMA_OPT_TCC_SHIFT) & EDMA_OPT_TCC_MASK)) | 
                ((uint32_t)1 << EDMA_OPT_SYNCDIM_SHIFT);            
       }
       else
       {
           status  = SystemP_FAILURE;
       }

       EDMA_setPaRAM(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].param, &gEdmaCCParams[EDMA_TX_CH_PARAMS_INDEX]);
       status = SystemP_SUCCESS;
       
    }
    return (status);
}

int32_t EDMA_enableTxTransferRegion(DMA_Handle handle)
{
    int32_t         status = SystemP_FAILURE;
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {

        status = EDMA_enableTransferRegion(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh,
                                     EDMA_TRIG_MODE_EVENT);

        if ( 0 == status )
        {
            status = SystemP_FAILURE;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }

    return (status);
}
int32_t EDMA_WaitForTxTransfer(DMA_Handle handle)
{
    int32_t         status = SystemP_FAILURE;

    while(1U != EDMA_readIntrStatusRegion(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc));
    EDMA_clrIntrRegion(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc);
    status = SystemP_SUCCESS;

    return (status);
}
int32_t EDMA_Config_RxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks)
{
    int32_t         status = SystemP_FAILURE;
    DMA_Config      *dmaCfg;
    EDMA_Handle     edmaHandler; 
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {
        dmaCfg = (DMA_Config *)handle;
        edmaHandler = dmaCfg->dmaHandle;

        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr = EDMA_getBaseAddr(edmaHandler);

        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId = EDMA_getRegionId(edmaHandler);

        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh = EDMA_AES_RX_CHANNEL_NUMBER;
        status = EDMA_allocDmaChannel(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh);

        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc = EDMA_RESOURCE_ALLOC_ANY;
        status = EDMA_allocTcc(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc);

        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].param = EDMA_RESOURCE_ALLOC_ANY;
        status = EDMA_allocParam(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].param);

        /* Request channel */
        EDMA_configureChannelRegion(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId, EDMA_CHANNEL_TYPE_DMA,
        globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].param, EDMA_TEST_EVT_QUEUE_NO);
        /* Program Param Set */
        EDMA_ccPaRAMEntry_init(&gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX]);
        
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].srcAddr       = (uint32_t) srcAddress;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].destAddr      = (uint32_t) dstAddress;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].aCnt          = (uint16_t) EDMA_A_COUNT_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].bCnt          = (uint16_t) EDMA_B_COUNT_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].cCnt          = (uint16_t) numBlocks;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].bCntReload    = (uint16_t) EDMA_NULL_INDEX_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].srcBIdx       = (int16_t) EDMA_BINEX_NEGATIVE_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].destBIdx      = (int16_t) EDMA_BINEX_POSITIVE_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].srcCIdx       = (int16_t) EDMA_NULL_INDEX_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].destCIdx      = (int16_t) EDMA_A_COUNT_VALUE * EDMA_B_COUNT_VALUE;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].linkAddr      = EDMA_LINK_ADDRESS;
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].srcBIdxExt    = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_BINEX_NEGATIVE_VALUE);
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].destBIdxExt   = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_BINEX_POSITIVE_VALUE);
        gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX].opt          |=
            (EDMA_OPT_TCINTEN_MASK |
             ((((uint32_t)globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc) << EDMA_OPT_TCC_SHIFT) & EDMA_OPT_TCC_MASK)) | 
            ((uint32_t)1 << EDMA_OPT_SYNCDIM_SHIFT);
        EDMA_setPaRAM(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].param, &gEdmaCCParams[EDMA_RX_CH_PARAMS_INDEX]);
        status = SystemP_SUCCESS;
    }
    return (status);
}
int32_t EDMA_enableRxTransferRegion(DMA_Handle handle)
{
    int32_t         status = SystemP_FAILURE;
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {

        status = EDMA_enableTransferRegion(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh,
                                     EDMA_TRIG_MODE_EVENT);
        if ( 0 == status )
        {
            status = SystemP_FAILURE;
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }

    return (status);
}
int32_t EDMA_WaitForRxTransfer(DMA_Handle handle)
{
    int32_t         status = SystemP_SUCCESS;

    while(1U != EDMA_readIntrStatusRegion(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc));
    EDMA_clrIntrRegion(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc);

    return (status);
}

int32_t EDMA_disableTxChannel(DMA_Handle handle)
{
    int32_t         status = SystemP_FAILURE;
    DMA_Config      *dmaCfg;
    EDMA_Handle     edmaHandler;
    
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {
        dmaCfg = (DMA_Config *)handle;
        edmaHandler = dmaCfg->dmaHandle;
        
        /* Free channel */
        EDMA_freeChannelRegion(globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].regionId, EDMA_CHANNEL_TYPE_DMA,
            globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh, EDMA_TRIG_MODE_MANUAL, globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc, EDMA_TEST_EVT_QUEUE_NO);

        /* Free the EDMA resources managed by driver. */
        EDMA_freeDmaChannel(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].dmaCh);
        EDMA_freeTcc(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].tcc);
        EDMA_freeParam(edmaHandler, &globalEdmaParams[EDMA_TX_CH_PARAMS_INDEX].param);
        status = SystemP_SUCCESS;
    }

    return (status);
}

int32_t EDMA_disableRxChannel(DMA_Handle handle)
{
    int32_t         status = SystemP_FAILURE;
    DMA_Config      *dmaCfg;
    EDMA_Handle     edmaHandler;
    
    if(NULL == handle)
    {
        status  = SystemP_FAILURE;
    }
    else
    {
        dmaCfg = (DMA_Config *)handle;
        edmaHandler = dmaCfg->dmaHandle;
        
        /* Free channel */
        EDMA_freeChannelRegion(globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].baseAddr, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].regionId, EDMA_CHANNEL_TYPE_DMA,
            globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh, EDMA_TRIG_MODE_MANUAL, globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc, EDMA_TEST_EVT_QUEUE_NO);

        /* Free the EDMA resources managed by driver. */
        EDMA_freeDmaChannel(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].dmaCh);
        EDMA_freeTcc(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].tcc);
        EDMA_freeParam(edmaHandler, &globalEdmaParams[EDMA_RX_CH_PARAMS_INDEX].param);
        status = SystemP_SUCCESS;
    }

    return (status);
}
