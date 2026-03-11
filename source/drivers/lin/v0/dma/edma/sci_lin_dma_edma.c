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

/**
 *  \file sci_lin_dma_edma.c
 *
 *  \brief File containing EDMA Driver APIs implementation for LIN.
 *
 */

#include <string.h>
#include <drivers/soc.h>
#include <drivers/edma/v0/edma.h>
#include <drivers/lin/v0/sci_lin.h>
#include <drivers/lin/v0/internal/sci_lin_internal.h>
#include <drivers/lin/v0/dma/edma/sci_lin_dma_edma.h>
#include <kernel/dpl/CacheP.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */
/** \brief Transmit EDMA channel event queue number */
#define EDMA_LIN_TX_EVT_QUEUE_NO                (0U)
/** \brief Receive EDMA channel event queue number */
#define EDMA_LIN_RX_EVT_QUEUE_NO                (1U)

#define LIN_EDMA_ABC_COUNT_UNIT                 (1U)
#define LIN_EDMA_ABC_IDX_UNIT                   (1)
/* ========================================================================== */
/*                    Internal Function Declarations                          */
/* ========================================================================== */

static void LIN_edmaIsrTx(Edma_IntrHandle intrHandle, void *args);
static void LIN_edmaIsrRx(Edma_IntrHandle intrHandle, void *args);
static void LIN_edmaIsrDummy(Edma_IntrHandle intrHandle, void *args);

static void LIN_HLD_completeCurrentTransferDMA(LIN_Handle handle);

/* ========================================================================== */
/*                           Function Definitions                             */
/* ========================================================================== */

int32_t LIN_WriteLinFrameCommanderDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;
    LIN_EdmaChConfig    *edmaChCfg;
    EDMACCPaRAMEntry    edmaParam_Tx;
    EDMACCPaRAMEntry    edmaDummyParam;
    void                *srcBuffPtr;
    void                *dstBuffPtr;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    edmaChCfg = (LIN_EdmaChConfig *)object->dmaChCfg;

    if(object->state == LIN_HLD_STATE_IDLE)
    {
        /* Wait for Buffers to be completely Empty */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_TXEMPTY) == 0U){};
        /* Wait for Bus to be free */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_BUSY) != 0U){};

        object->state = LIN_HLD_STATE_BUSY;
        object->currentTxnFrame = frame;

        /* Enable data Transmission in case it was disabled */
        LIN_HLD_enableDataTx(hwAttrs->baseAddr);

        /* Generate Parity ID if parity is Enabled */
        if(object->openParams->enableParity == true)
        {
            id = LIN_HLD_gen_ParityID(object->currentTxnFrame->id);
        }
        else
        {
            id = object->currentTxnFrame ->id;
        }

        /* Set The transaction status to Success */
        object->currentTxnFrame->status = LIN_HLD_TXN_STS_SUCCESS;

        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            /* Set The Frame Length */
            LIN_HLD_setFrameLength( hwAttrs->baseAddr,
                                    object->currentTxnFrame->frameLen);
        }

        if(object->openParams->multiBufferMode == true)
        {
            object->writeCountIdx = object->currentTxnFrame->frameLen;
            object->writeBufIdx = (uint8_t*)object->currentTxnFrame->dataBuf;

            int8_t i = 0;
            uint8_t *pData;

            pData = object->writeBufIdx + object->writeCountIdx;
            (void)memset(object->tempBuffer, 0, LIN_FRAME_FIXED_LEN_8U);
            for(i = (int8_t)(object->writeCountIdx); i >= 0; i--)
            {
                object->tempBuffer[(uint32_t)i ^ 3U] = (uint8_t)*pData;
                pData--;
            }

            srcBuffPtr = (void *)(object->tempBuffer);
            dstBuffPtr = (void *)(hwAttrs->baseAddr + (uint32_t)CSL_LIN_LINTD0);

            CacheP_wb((void *)( object->tempBuffer), LIN_FRAME_FIXED_LEN_8U,
                                CacheP_TYPE_ALL);
            CacheP_wb((void *)( hwAttrs->baseAddr + (uint32_t)CSL_LIN_LINTD0),
                                LIN_FRAME_FIXED_LEN_8U , CacheP_TYPE_ALL);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Tx);
            edmaParam_Tx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Tx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Tx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCnt          = (uint16_t) LIN_FRAME_FIXED_LEN_8U;
            edmaParam_Tx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCntReload    = (uint16_t) edmaParam_Tx.bCnt;
            edmaParam_Tx.srcBIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.srcCIdx       = (int16_t) 0;
            edmaParam_Tx.destCIdx      = (int16_t) 0;
            edmaParam_Tx.linkAddr      = 0xFFFFU;
            edmaParam_Tx.opt           = 0;
            edmaParam_Tx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccTx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                            &edmaParam_Tx);

            /* Initialize TX Param Set */
            EDMA_ccPaRAMEntry_init(&edmaDummyParam);

            /* Dummy param set configuration */
            edmaDummyParam.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaDummyParam.linkAddr      = 0xFFFFU;
            edmaDummyParam.opt           =
                (EDMA_OPT_TCINTEN_MASK |
                (   (edmaChCfg->edmaTccDummy << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            /* Write Tx param set */
            EDMA_setPaRAM(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaDummyParam,
                          &edmaDummyParam);

            /* Link  dummy param ID */
            EDMA_linkChannel(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                             edmaChCfg->edmaDummyParam);

            (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                            edmaChCfg->edmaRegionId,
                                            edmaChCfg->edmaTxChId,
                                            EDMA_TRIG_MODE_EVENT);

            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_TX_DMA);
            LIN_HLD_setIDByte(hwAttrs->baseAddr, id);

        }
        else
        {
            /* Store Write parameters */
            if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
            {
                object->writeCountIdx = object->currentTxnFrame->frameLen;
            }
            else
            {
                if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
                {
                    object->writeCountIdx = LIN_FRAME_FIXED_LEN_2U;
                }
                else if(object->currentTxnFrame->id <=
                                                LIN_ID_DATA_LEN_4_RANGE_HIGH)
                {
                    object->writeCountIdx = LIN_FRAME_FIXED_LEN_4U;
                }
                else /* 0x30 - 0x3F */
                {
                    object->writeCountIdx = LIN_FRAME_FIXED_LEN_8U;
                }
            }

            object->writeBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

            /* Write Back User Data Buffer */
            CacheP_wb(  (void *)( object->writeBufIdx), object->writeCountIdx,
                        CacheP_TYPE_ALL);

            srcBuffPtr = (void *) object->writeBufIdx;
            dstBuffPtr = (void *) (hwAttrs->baseAddr +
                                    (uint32_t)CSL_LIN_LINTD0 + (uint32_t)3U);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Tx);
            edmaParam_Tx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Tx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Tx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.cCnt          = (uint16_t) object->writeCountIdx;
            edmaParam_Tx.bCntReload    = (uint16_t) edmaParam_Tx.bCnt;
            edmaParam_Tx.srcBIdx       = (int16_t) 0;
            edmaParam_Tx.destBIdx      = (int16_t) 0;
            edmaParam_Tx.srcCIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.destCIdx      = (int16_t) 0;
            edmaParam_Tx.linkAddr      = 0xFFFFU;
            edmaParam_Tx.opt           = 0;
            edmaParam_Tx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccTx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                            &edmaParam_Tx);

            /* Initialize TX Param Set */
            EDMA_ccPaRAMEntry_init(&edmaDummyParam);

            /* Dummy param set configuration */
            edmaDummyParam.aCnt          = (uint16_t) 1;
            edmaDummyParam.linkAddr      = 0xFFFFU;
            edmaDummyParam.opt           =
                (EDMA_OPT_TCINTEN_MASK |
                (   (edmaChCfg->edmaTccDummy << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            /* Write Tx param set */
            EDMA_setPaRAM(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaDummyParam,
                          &edmaDummyParam);

            /* Link  dummy param ID */
            EDMA_linkChannel(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                             edmaChCfg->edmaDummyParam);

            (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                            edmaChCfg->edmaRegionId,
                                            edmaChCfg->edmaTxChId,
                                            EDMA_TRIG_MODE_EVENT);

            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_TX_DMA);
            LIN_HLD_setIDByte(hwAttrs->baseAddr, id);
        }
    }
    else
    {
        /* Module busy or in Reset State */
        status = SystemP_FAILURE;
        frame->status = LIN_HLD_TXN_STS_FAILURE;
    }

    return status;
}

int32_t LIN_WriteLinFrameResponderDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    LIN_EdmaChConfig    *edmaChCfg;
    EDMACCPaRAMEntry    edmaParam_Tx;
    EDMACCPaRAMEntry    edmaDummyParam;
    void                *srcBuffPtr;
    void                *dstBuffPtr;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    edmaChCfg = (LIN_EdmaChConfig *)object->dmaChCfg;

    if(object->state == LIN_HLD_STATE_IDLE)
    {
        /* Wait for Buffers to be completely Empty */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_TXEMPTY) == 0U){};
        /* Wait for Bus to be free */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_BUSY) != 0U){};

        object->state = LIN_HLD_STATE_BUSY;
        object->currentTxnFrame = frame;

        /* Enable data Transmission in case it was disabled */
        LIN_HLD_enableDataTx(hwAttrs->baseAddr);

        /* Set The transaction status to Success */
        object->currentTxnFrame->status = LIN_HLD_TXN_STS_SUCCESS;

        /* Check of HGENCTRL bit, if it is set write ID-slaveTask Byte */
        if (object->openParams->linConfigParams.maskFilteringType ==
                                    LIN_HLD_MSG_FILTER_IDRESPONDER)
        {
            if(object->openParams->enableParity == true)
            {

                LIN_HLD_setIDSlaveTaskByte(hwAttrs->baseAddr, frame->id);
            }
            else
            {
                LIN_HLD_setIDSlaveTaskByte(hwAttrs->baseAddr, frame->id);
            }
        }
        else
        {
            if(object->openParams->enableParity == true)
            {
                LIN_HLD_setIDByte(hwAttrs->baseAddr,
                                  LIN_HLD_gen_ParityID(frame->id));
            }
            else
            {
                LIN_HLD_setIDByte(hwAttrs->baseAddr, frame->id);
            }
        }

        object->writeBufIdx = (uint8_t*)object->currentTxnFrame->dataBuf;

        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            object->writeCountIdx = object->currentTxnFrame->frameLen;
        }
        else
        {
            if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
            {
                object->writeCountIdx = LIN_FRAME_FIXED_LEN_2U;
            }
            else if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
            {
                object->writeCountIdx = LIN_FRAME_FIXED_LEN_4U;
            }
            else /* 0x30 - 0x3F */
            {
                object->writeCountIdx = LIN_FRAME_FIXED_LEN_8U;
            }
        }

        object->writeCountIdx = object->currentTxnFrame->frameLen;
        object->writeBufIdx = (uint8_t*)object->currentTxnFrame->dataBuf;

        LIN_HLD_setFrameLength(hwAttrs->baseAddr, (uint8_t)object->writeCountIdx);

        if(object->openParams->multiBufferMode == true)
        {
            int8_t i = 0;
            uint8_t *pData;

            pData = object->writeBufIdx + object->writeCountIdx;
            (void)memset(object->tempBuffer, 0, LIN_FRAME_FIXED_LEN_8U);
            for(i = (int8_t)(object->writeCountIdx); i >= 0; i--)
            {
                object->tempBuffer[(uint32_t)i ^ 3U] = (uint8_t)*pData;
                pData--;
            }

            srcBuffPtr = (void *) (object->tempBuffer);
            dstBuffPtr = (void *) ( hwAttrs->baseAddr +
                                    (uint32_t)CSL_LIN_LINTD0);

            CacheP_wb((void *)( object->tempBuffer), 8U, CacheP_TYPE_ALL);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Tx);
            edmaParam_Tx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Tx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Tx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCnt          = (uint16_t) LIN_FRAME_FIXED_LEN_8U;
            edmaParam_Tx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCntReload    = (uint16_t) edmaParam_Tx.bCnt;
            edmaParam_Tx.srcBIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.srcCIdx       = (int16_t) 0;
            edmaParam_Tx.destCIdx      = (int16_t) 0;
            edmaParam_Tx.linkAddr      = 0xFFFFU;
            edmaParam_Tx.opt           = 0;
            edmaParam_Tx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccTx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                            &edmaParam_Tx);

            /* Initialize TX Param Set */
            EDMA_ccPaRAMEntry_init(&edmaDummyParam);

            /* Dummy param set configuration */
            edmaDummyParam.aCnt          = (uint16_t) 1;
            edmaDummyParam.linkAddr      = 0xFFFFU;
            edmaDummyParam.opt           =
                (EDMA_OPT_TCINTEN_MASK |
                (   (edmaChCfg->edmaTccDummy << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            /* Write Tx param set */
            EDMA_setPaRAM(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaDummyParam,
                          &edmaDummyParam);

            /* Link  dummy param ID */
            EDMA_linkChannel(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                             edmaChCfg->edmaDummyParam);

            (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                            edmaChCfg->edmaRegionId,
                                            edmaChCfg->edmaTxChId,
                                            EDMA_TRIG_MODE_EVENT);

            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_TX_DMA);

        }
        else
        {
            CacheP_wb((void *)(object->writeBufIdx), object->writeCountIdx,
                        CacheP_TYPE_ALL);

            srcBuffPtr = (void *) object->writeBufIdx;
            dstBuffPtr = (void *) (hwAttrs->baseAddr +
                                    (uint32_t)CSL_LIN_LINTD0 + (uint32_t)3U);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Tx);
            edmaParam_Tx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Tx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Tx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.bCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Tx.cCnt          = (uint16_t) object->writeCountIdx;
            edmaParam_Tx.bCntReload    = (uint16_t) edmaParam_Tx.bCnt;
            edmaParam_Tx.srcBIdx       = (int16_t) 0;
            edmaParam_Tx.destBIdx      = (int16_t) 0;
            edmaParam_Tx.srcCIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Tx.destCIdx      = (int16_t) 0;
            edmaParam_Tx.linkAddr      = 0xFFFFU;
            edmaParam_Tx.opt           = 0;
            edmaParam_Tx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccTx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                            &edmaParam_Tx);

            /* Initialize TX Param Set */
            EDMA_ccPaRAMEntry_init(&edmaDummyParam);

            /* Dummy param set configuration */
            edmaDummyParam.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaDummyParam.linkAddr      = 0xFFFFU;
            edmaDummyParam.opt           =
                (EDMA_OPT_TCINTEN_MASK |
                (   (edmaChCfg->edmaTccDummy << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));

            /* Write Tx param set */
            EDMA_setPaRAM(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaDummyParam,
                          &edmaDummyParam);

            /* Link  dummy param ID */
            EDMA_linkChannel(edmaChCfg->edmaBaseAddr, edmaChCfg->edmaTxParam,
                             edmaChCfg->edmaDummyParam);

            (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                            edmaChCfg->edmaRegionId,
                                            edmaChCfg->edmaTxChId,
                                            EDMA_TRIG_MODE_EVENT);

            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_TX_DMA);
        }
    }
    else
    {
        /* Module busy or in Reset State */
        status = SystemP_FAILURE;
        frame->status = LIN_HLD_TXN_STS_FAILURE;
    }

    return status;
}



int32_t LIN_ReadLinFrameCommanderDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;
    LIN_EdmaChConfig    *edmaChCfg;
    EDMACCPaRAMEntry    edmaParam_Rx;
    void                *srcBuffPtr;
    void                *dstBuffPtr;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    edmaChCfg = (LIN_EdmaChConfig *)object->dmaChCfg;

    if(object->state == LIN_HLD_STATE_IDLE)
    {
        /* Wait for Buffers to be completely Empty */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_TXEMPTY) == 0U){};
        /* Wait for Bus to be free */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_BUSY) != 0U){};

        object->state = LIN_HLD_STATE_BUSY;
        object->currentTxnFrame = frame;

        /* Generate Parity ID if parity is Enabled */
        if(object->openParams->enableParity == true)
        {
            id = LIN_HLD_gen_ParityID(object->currentTxnFrame->id);
        }
        else
        {
            id = object->currentTxnFrame->id;
        }

        /* Set The transaction status to Success */
        object->currentTxnFrame->status = LIN_HLD_TXN_STS_SUCCESS;

        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            /* Set The Frame Length */
            LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                   object->currentTxnFrame->frameLen);
            object->readCountIdx = object->currentTxnFrame->frameLen;
        }
        else
        {
            if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
            {
                LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_2U);
                object->readCountIdx = LIN_FRAME_FIXED_LEN_2U;
            }
            else if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
            {
                LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_4U);
                object->readCountIdx = LIN_FRAME_FIXED_LEN_4U;
            }
            else /* 0x30 - 0x3F */
            {
                LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_8U);
                object->readCountIdx = LIN_FRAME_FIXED_LEN_8U;
            }
        }

        object->readBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

        CacheP_wb((void *)( object->readCountIdx), sizeof(uint32_t),
                            CacheP_TYPE_ALL);
        CacheP_wb((void *)( object->readBufIdx), sizeof(uint32_t),
                            CacheP_TYPE_ALL);

        if(object->openParams->multiBufferMode == true)
        {
            srcBuffPtr = (void *)(hwAttrs->baseAddr + (uint32_t)CSL_LIN_LINRD0);
            dstBuffPtr = (void *)(object->tempBuffer);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Rx);
            edmaParam_Rx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Rx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Rx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCnt          = (uint16_t) LIN_FRAME_FIXED_LEN_8U;
            edmaParam_Rx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCntReload    = (uint16_t) edmaParam_Rx.bCnt;
            edmaParam_Rx.srcBIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.srcCIdx       = (int16_t) 0;
            edmaParam_Rx.destCIdx      = (int16_t) 0;
            edmaParam_Rx.linkAddr      = 0xFFFFU;
            edmaParam_Rx.opt           = 0;
            edmaParam_Rx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccRx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));
        }
        else
        {
            srcBuffPtr = (void *) ( (hwAttrs->baseAddr) +
                                    (uint32_t)CSL_LIN_LINRD0 + 3U);
            dstBuffPtr = (void *) (object->readBufIdx);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Rx);
            edmaParam_Rx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Rx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Rx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCnt          = (uint16_t) object->readCountIdx;
            edmaParam_Rx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCntReload    = (uint16_t) edmaParam_Rx.bCnt;
            edmaParam_Rx.srcBIdx       = (int16_t) 0;
            edmaParam_Rx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.srcCIdx       = (int16_t) 0;
            edmaParam_Rx.destCIdx      = (int16_t) 0;
            edmaParam_Rx.linkAddr      = 0xFFFFU;
            edmaParam_Rx.opt           = 0;
            edmaParam_Rx.opt          |=
                (EDMA_OPT_TCINTEN_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccRx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));
        }

        /* Set Transfer Parameters */
        EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaRxParam,
                        &edmaParam_Rx);
        /* Enable Transfer Region */
        (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                        edmaChCfg->edmaRegionId,
                                        edmaChCfg->edmaRxChId,
                                        EDMA_TRIG_MODE_EVENT);
        /* Enable RX DMA */
        LIN_HLD_enableInterrupt(hwAttrs->baseAddr, LIN_INT_RX_DMA);
        /* Set Message ID to initiate a header transmission */
        LIN_HLD_setIDByte(hwAttrs->baseAddr, id);
        /* Disable data Transmission */
        LIN_HLD_disableDataTx(hwAttrs->baseAddr);
        /* Enable No Response Error interrupt */
        LIN_HLD_enableInterrupt(hwAttrs->baseAddr, LIN_INT_LVL_NRE);
    }
    else
    {
        /* Module Busy or in Reset State */
        status = SystemP_FAILURE;
        frame->status = LIN_HLD_TXN_STS_FAILURE;
    }

    return status;
}

int32_t LIN_ReadLinFrameResponderDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    LIN_EdmaChConfig    *edmaChCfg;
    EDMACCPaRAMEntry    edmaParam_Rx;
    void                *srcBuffPtr;
    void                *dstBuffPtr;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    edmaChCfg = (LIN_EdmaChConfig *)object->dmaChCfg;

    if(object->state == LIN_HLD_STATE_IDLE)
    {
        /* Wait for Buffers to be completely Empty */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_TXEMPTY) == 0U){};
        /* Wait for Bus to be free */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_BUSY) != 0U){};

        object->state = LIN_HLD_STATE_BUSY;
        object->currentTxnFrame = frame;

        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            /* Set The Frame Length */
            LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                   object->currentTxnFrame->frameLen);
        }

        object->readCountIdx = object->currentTxnFrame->frameLen;
        object->readBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

        if(object->openParams->linConfigParams.maskFilteringType ==
                                                    LIN_HLD_MSG_FILTER_IDBYTE)
        {
            if(object->openParams->enableParity == true)
            {
                LIN_HLD_setIDByte(hwAttrs->baseAddr,
                            LIN_HLD_gen_ParityID(object->currentTxnFrame->id));
            }
            else
            {
                LIN_HLD_setIDByte(hwAttrs->baseAddr,
                                object->currentTxnFrame->id);
            }
        }

        if(object->openParams->multiBufferMode == true)
        {
            srcBuffPtr = (void *)(hwAttrs->baseAddr + (uint32_t)CSL_LIN_LINRD0);
            dstBuffPtr = (void *)(object->tempBuffer);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Rx);
            edmaParam_Rx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Rx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Rx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCnt          = (uint16_t) LIN_FRAME_FIXED_LEN_8U;
            edmaParam_Rx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCntReload    = (uint16_t) edmaParam_Rx.bCnt;
            edmaParam_Rx.srcBIdx       = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.srcCIdx       = (int16_t) 0;
            edmaParam_Rx.destCIdx      = (int16_t) 0;
            edmaParam_Rx.linkAddr      = 0xFFFFU;
            edmaParam_Rx.opt           = 0;
            edmaParam_Rx.opt          |=
                (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_SYNCDIM_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccRx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));
        }
        else
        {
            srcBuffPtr = (void *) ( (hwAttrs->baseAddr) +
                                    (uint32_t)CSL_LIN_LINRD0 + 3U);
            dstBuffPtr = (void *) (object->readBufIdx);

            /* Program Param Set */
            EDMA_ccPaRAMEntry_init(&edmaParam_Rx);
            edmaParam_Rx.srcAddr       = (uint32_t) SOC_virtToPhy(srcBuffPtr);
            edmaParam_Rx.destAddr      = (uint32_t) SOC_virtToPhy(dstBuffPtr);
            edmaParam_Rx.aCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCnt          = (uint16_t) object->readCountIdx;
            edmaParam_Rx.cCnt          = (uint16_t) LIN_EDMA_ABC_COUNT_UNIT;
            edmaParam_Rx.bCntReload    = (uint16_t) edmaParam_Rx.bCnt;
            edmaParam_Rx.srcBIdx       = (int16_t) 0;
            edmaParam_Rx.destBIdx      = (int16_t) LIN_EDMA_ABC_IDX_UNIT;
            edmaParam_Rx.srcCIdx       = (int16_t) 0;
            edmaParam_Rx.destCIdx      = (int16_t) 0;
            edmaParam_Rx.linkAddr      = 0xFFFFU;
            edmaParam_Rx.opt           = 0;
            edmaParam_Rx.opt          |=
                (EDMA_OPT_TCINTEN_MASK |
                (   (((uint32_t) edmaChCfg->edmaTccRx) << EDMA_OPT_TCC_SHIFT) &
                    EDMA_OPT_TCC_MASK));
        }

        /* Set Transfer Parameters */
        EDMA_setPaRAM(  edmaChCfg->edmaBaseAddr, edmaChCfg->edmaRxParam,
                        &edmaParam_Rx);

        /* Enable Transfer Region */
        (void)EDMA_enableTransferRegion(edmaChCfg->edmaBaseAddr,
                                        edmaChCfg->edmaRegionId,
                                        edmaChCfg->edmaRxChId,
                                        EDMA_TRIG_MODE_EVENT);
        /* Enable RX DMA */
        LIN_HLD_enableInterrupt(hwAttrs->baseAddr, LIN_INT_RX_DMA);
    }
    else
    {
        /* Module Busy or in Reset State */
        status = SystemP_FAILURE;
        frame->status = LIN_HLD_TXN_STS_FAILURE;
    }

    return status;
}

int32_t LIN_WriteSciFrameDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    return SystemP_FAILURE;
}

int32_t LIN_ReadSciFrameDMA(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    return SystemP_FAILURE;
}

int32_t LIN_dmaOpen(LIN_Handle handle, LIN_DmaChConfig dmaChCfg)
{
    int32_t             status = SystemP_SUCCESS;
    int32_t             chAllocStatus = SystemP_SUCCESS;
    uint32_t            retVal = 0U;
    LIN_EdmaChConfig    *edmaChCfg = NULL;
    uint32_t            isEdmaInterruptEnabled = 0U;
    EDMA_Handle         linEdmaHandle;
    LIN_Object          *object;

    if((NULL != handle) && (NULL != dmaChCfg))
    {
        edmaChCfg = (LIN_EdmaChConfig *)dmaChCfg;
        object = (LIN_Object *)handle->object;
        linEdmaHandle = (EDMA_Handle) object->linDmaHandle;

        if(linEdmaHandle != NULL)
        {
            /* Read base address of allocated EDMA instance */
            edmaChCfg->edmaBaseAddr = EDMA_getBaseAddr(linEdmaHandle);
            /* Read the region ID of the EDMA instance */
            edmaChCfg->edmaRegionId = EDMA_getRegionId(linEdmaHandle);

            /* Check if interrupt is enabled */
            isEdmaInterruptEnabled = EDMA_isInterruptEnabled(linEdmaHandle);

            if( (edmaChCfg->edmaBaseAddr != (uint32_t)0U) &&
                (edmaChCfg->edmaRegionId < (uint32_t)SOC_EDMA_NUM_REGIONS) &&
                (isEdmaInterruptEnabled == (uint32_t)TRUE))
            {
                /************************************************************ */
                /** RX Config                                                 */
                /************************************************************ */

                /* Allocate EDMA channel for LIN RX Transfer */
                chAllocStatus = EDMA_allocDmaChannel(linEdmaHandle,
                                                     &(edmaChCfg->edmaRxChId));
                status += chAllocStatus;

                /* Allocate EDMA TCC for LIN RX transfer */
                edmaChCfg->edmaTccRx = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocTcc(linEdmaHandle, &(edmaChCfg->edmaTccRx));

                /* Allocate a Param ID for LIN RX transfer */
                edmaChCfg->edmaRxParam = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocParam(linEdmaHandle,
                                          &(edmaChCfg->edmaRxParam));

                if(status == SystemP_SUCCESS)
                {
                    retVal = EDMA_configureChannelRegion(
                    edmaChCfg->edmaBaseAddr,edmaChCfg->edmaRegionId,
                    EDMA_CHANNEL_TYPE_DMA, edmaChCfg->edmaRxChId,
                    edmaChCfg->edmaTccRx, edmaChCfg->edmaRxParam,
                    EDMA_LIN_RX_EVT_QUEUE_NO);

                    if(retVal == FALSE)
                    {
                        status = SystemP_FAILURE;
                    }
                    else
                    {
                        status = SystemP_SUCCESS;
                    }

                    if(status == SystemP_SUCCESS)
                    {
                        /* Register RX interrupt */
                        edmaChCfg->edmaIntrObjRx.tccNum = edmaChCfg->edmaTccRx;
                        edmaChCfg->edmaIntrObjRx.cbFxn  = &LIN_edmaIsrRx;
                        edmaChCfg->edmaIntrObjRx.appData = (void *)handle;
                        status += EDMA_registerIntr(linEdmaHandle,
                                                &(edmaChCfg->edmaIntrObjRx));
                    }
                    else { /* No Code */ }
                }

                if(status != SystemP_SUCCESS)
                {
                    /* Free all allocated resources of EDMA */
                    if ((chAllocStatus == SystemP_SUCCESS) &&
                        (edmaChCfg->edmaRxChId != EDMA_RESOURCE_ALLOC_ANY))
                    {
                        status += EDMA_freeDmaChannel(linEdmaHandle,
                                                      &(edmaChCfg->edmaRxChId));
                    }
                    if (edmaChCfg->edmaTccRx != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeTcc(linEdmaHandle,
                                               &(edmaChCfg->edmaTccRx));
                    }
                    if (edmaChCfg->edmaTxParam != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeParam(linEdmaHandle,
                                                 &(edmaChCfg->edmaRxParam));
                    }
                }

                /************************************************************ */
                /** TX Config                                                 */
                /************************************************************ */

                /* Allocate EDMA channel for LIN TX Transfer */
                chAllocStatus = EDMA_allocDmaChannel(linEdmaHandle,
                                                     &(edmaChCfg->edmaTxChId));
                status += chAllocStatus;

                /* Allocate EDMA TCC for LIN RX transfer */
                edmaChCfg->edmaTccTx = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocTcc(linEdmaHandle, &(edmaChCfg->edmaTccTx));

                /* Allocate a Param ID for LIN RX transfer */
                edmaChCfg->edmaTxParam = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocParam(linEdmaHandle,
                                          &(edmaChCfg->edmaTxParam));

                if(status == SystemP_SUCCESS)
                {
                    retVal = EDMA_configureChannelRegion(
                    edmaChCfg->edmaBaseAddr,edmaChCfg->edmaRegionId,
                    EDMA_CHANNEL_TYPE_DMA, edmaChCfg->edmaTxChId,
                    edmaChCfg->edmaTccTx, edmaChCfg->edmaTxParam,
                    EDMA_LIN_TX_EVT_QUEUE_NO);

                    if(retVal == FALSE)
                    {
                        status = SystemP_FAILURE;
                    }
                    else
                    {
                        status = SystemP_SUCCESS;
                    }

                    if(status == SystemP_SUCCESS)
                    {
                        /* Register TX interrupt */
                        edmaChCfg->edmaIntrObjTx.tccNum = edmaChCfg->edmaTccTx;
                        edmaChCfg->edmaIntrObjTx.cbFxn  = &LIN_edmaIsrTx;
                        edmaChCfg->edmaIntrObjTx.appData = (void *)handle;
                        status += EDMA_registerIntr(linEdmaHandle,
                                                &(edmaChCfg->edmaIntrObjTx));
                    }
                    else { /* No Code */ }
                }

                if(status != SystemP_SUCCESS)
                {
                    /* Free all allocated resources of EDMA */
                    if ((chAllocStatus == SystemP_SUCCESS) &&
                        (edmaChCfg->edmaTxChId != EDMA_RESOURCE_ALLOC_ANY))
                    {
                        status += EDMA_freeDmaChannel(linEdmaHandle,
                                                      &(edmaChCfg->edmaTxChId));
                    }
                    if (edmaChCfg->edmaTccTx != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeTcc(linEdmaHandle,
                                               &(edmaChCfg->edmaTccTx));
                    }
                    if (edmaChCfg->edmaTxParam != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeParam(linEdmaHandle,
                                                 &(edmaChCfg->edmaTxParam));
                    }
                }

                /* Allocate EDMA TCC for Dummy Param Set */
                edmaChCfg->edmaTccDummy = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocTcc(linEdmaHandle,
                                        &(edmaChCfg->edmaTccDummy));

                /* Allocate a Param ID for UART TX transfer */
                edmaChCfg->edmaDummyParam = EDMA_RESOURCE_ALLOC_ANY;
                status += EDMA_allocParam(linEdmaHandle,
                                          &(edmaChCfg->edmaDummyParam));

                if(status == SystemP_SUCCESS)
                {
                    /* Register Dummy interrupt */
                    edmaChCfg->edmaIntrObjDummy.tccNum =
                                                        edmaChCfg->edmaTccDummy;
                    edmaChCfg->edmaIntrObjDummy.cbFxn = &LIN_edmaIsrDummy;
                    edmaChCfg->edmaIntrObjDummy.appData = (void *) handle;
                    status += EDMA_registerIntr(linEdmaHandle,
                                                &(edmaChCfg->edmaIntrObjDummy));
                }
                else { /* No Code */ }

                if(status != SystemP_SUCCESS)
                {
                    /* Free all allocated resources of EDMA */
                    if (edmaChCfg->edmaTccTx != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeTcc(linEdmaHandle,
                                               &(edmaChCfg->edmaTccDummy));
                    }
                    if (edmaChCfg->edmaTxParam != EDMA_RESOURCE_ALLOC_ANY)
                    {
                        status += EDMA_freeParam(linEdmaHandle,
                                                 &(edmaChCfg->edmaDummyParam));
                    }
                }

                if(status == SystemP_SUCCESS)
                {
                    edmaChCfg->isOpen = true;
                }
                else
                {
                    status = SystemP_FAILURE;
                }
            }
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    else
    {
        status = SystemP_FAILURE;
    }

    return status;
}

int32_t LIN_dmaClose(LIN_Handle handle)
{
    int32_t             status = SystemP_SUCCESS;
    uint32_t            retVal = 0U;
    LIN_EdmaChConfig    *edmaChCfg = NULL;
    uint32_t            baseAddr, regionId;
    EDMA_Handle         linEdmaHandle;
    LIN_Object          *object;

    if(NULL != handle)
    {
        object = (LIN_Object *)handle->object;
        linEdmaHandle = (EDMA_Handle) object->linDmaHandle;
        edmaChCfg = (LIN_EdmaChConfig *)(object->dmaChCfg);

        if((linEdmaHandle != NULL) && (edmaChCfg != NULL))
        {
            if(edmaChCfg->isOpen == true)
            {
                /* Get base address and region ID */
                baseAddr = edmaChCfg->edmaBaseAddr;
                regionId = edmaChCfg->edmaRegionId;

                /* Unregister Rx Interrupt */
                status += EDMA_unregisterIntr(linEdmaHandle,
                                                    &edmaChCfg->edmaIntrObjRx);
                /* Free Rx channel */
                retVal = EDMA_freeChannelRegion(baseAddr, regionId,
                EDMA_CHANNEL_TYPE_DMA, edmaChCfg->edmaRxChId,
                EDMA_TRIG_MODE_EVENT, edmaChCfg->edmaTccRx,
                EDMA_LIN_RX_EVT_QUEUE_NO);

                if (retVal == FALSE)
                {
                    status--;
                } else { /* No Code */ }

                /* Free DMA Channel */
                if(edmaChCfg->edmaRxChId != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeDmaChannel(linEdmaHandle,
                                                    &edmaChCfg->edmaRxChId);
                } else { /* No Code */ }

                if(edmaChCfg->edmaTccRx != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeTcc(linEdmaHandle,
                                                    &edmaChCfg->edmaTccRx);
                } else { /* No Code */ }

                if(edmaChCfg->edmaRxParam != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeParam(linEdmaHandle,
                                                    &edmaChCfg->edmaRxParam);
                } else { /* No Code */ }


                /* Unregister Tx Interrupt */
                status += EDMA_unregisterIntr(linEdmaHandle,
                                                    &edmaChCfg->edmaIntrObjTx);
                /* Free Tx channel */
                retVal = EDMA_freeChannelRegion(baseAddr, regionId,
                EDMA_CHANNEL_TYPE_DMA, edmaChCfg->edmaTxChId,
                EDMA_TRIG_MODE_EVENT, edmaChCfg->edmaTccTx,
                EDMA_LIN_RX_EVT_QUEUE_NO);

                if (retVal == FALSE)
                {
                    status--;
                } else { /* No Code */ }

                /* Free DMA Channel */
                if(edmaChCfg->edmaTxChId != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeDmaChannel(linEdmaHandle,
                                                    &edmaChCfg->edmaTxChId);
                } else { /* No Code */ }

                if(edmaChCfg->edmaTccTx != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeTcc(linEdmaHandle,
                                                    &edmaChCfg->edmaTccTx);
                } else { /* No Code */ }

                if(edmaChCfg->edmaTxParam != EDMA_RESOURCE_ALLOC_ANY)
                {
                    status += EDMA_freeParam(linEdmaHandle,
                                                    &edmaChCfg->edmaTxParam);
                } else { /* No Code */ }

                edmaChCfg->isOpen = false;
            }
            else
            {
                status = SystemP_FAILURE;
            }
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    else
    {
        status = SystemP_FAILURE;
    }

    return status;
}

/* ========================================================================== */
/*                    Internal Function Definitions                           */
/* ========================================================================== */

static void LIN_HLD_completeCurrentTransferDMA(LIN_Handle handle)
{
    LIN_Object      *object = NULL;
    LIN_SCI_Frame   *frame = NULL;

    object = handle->object;
    frame = object->currentTxnFrame;

    if(object->openParams->transferMode == LIN_TRANSFER_MODE_BLOCKING)
    {
        /* Blocking Mode of Operation */
        /* Post Semaphore */
        if(object->currentTxnFrame->txnType == LIN_HLD_TXN_TYPE_WRITE)
        {
            SemaphoreP_post(&object->writeFrmCompSemObj);
        }
        else
        {
            SemaphoreP_post(&object->readFrmCompSemObj);
        }

            /* Change driver state back to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
        /* Clear Current Transaction Pointer */
        object->currentTxnFrame = NULL;
    }
    else
    {
        /* Callback Mode of Operation */
        /* Change driver state back to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
        /* Clear Current Transaction Pointer */
        object->currentTxnFrame = NULL;
        /* Call the Callback function */
        object->openParams->linConfigParams.transferCompleteCallbackFxn(handle,
                                                                        frame);
    }
}

static void LIN_edmaIsrTx(Edma_IntrHandle intrHandle, void *args)
{
    LIN_Handle          handle = (LIN_Handle)args;
    LIN_HwAttrs const   *hwAttrs = NULL;


    /* Input parameter validation */
    if (handle != NULL)
    {
        /* Get the pointer to the object */
        hwAttrs = (LIN_HwAttrs *)handle->hwAttrs;

        if((LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                             LIN_FLAG_MASK_TXRDY)) != 0U)
        {
            /* Transfer Completed */
            LIN_HLD_disableInterrupt(hwAttrs->baseAddr,  LIN_INT_TX_DMA);
            /* If Blocking Post Semaphore */
            /* If Callback Call the Callback */
            LIN_HLD_completeCurrentTransferDMA(handle);

        } else { /* Last Byte Written, Transfer Not Complete */ }


    } else { /* No Code */ }
}

static void LIN_edmaIsrRx(Edma_IntrHandle intrHandle, void *args)
{
    LIN_Handle          handle = (LIN_Handle)args;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             i = 0;
    uint8_t             *dataBuff = NULL;
    uint8_t             tempVal = 0x00;
    uint32_t            count = 0U;

    if (handle != NULL)
    {
        /* Get the pointer to the object */
        object = (LIN_Object *)handle->object;
        hwAttrs = (LIN_HwAttrs *)handle->hwAttrs;

        if(object->openParams->multiBufferMode == true)
        {
            CacheP_inv( (void *)( object->tempBuffer), LIN_FRAME_FIXED_LEN_8U,
                        CacheP_TYPE_ALL);

            dataBuff = (object->readBufIdx);
            count = object->readCountIdx;

            /* Read each 8-bit piece of data. */
            for(i = 0U; i <= count ; i++)
            {
                tempVal = object->tempBuffer[(uint32_t)i ^ 3U];
                *(dataBuff) = tempVal;
                dataBuff++;

            }
            CacheP_wb((void *)( object->readBufIdx), count, CacheP_TYPE_ALL);

        }
        else
        {
            CacheP_inv((void *)(object->readBufIdx), object->readCountIdx,
                                CacheP_TYPE_ALL);
        }

        if(object->openParams->linConfigParams.linMode ==
                                                    LIN_MODE_HLD_LIN_COMMANDER)
        {
            /* Disable NRE */
            LIN_HLD_disableInterrupt(hwAttrs->baseAddr, LIN_INT_LVL_NRE);
            /* Disable RX DMA */
            LIN_HLD_disableInterrupt(hwAttrs->baseAddr,  LIN_INT_RX_DMA);
            /* Complete Current Transfer */
            LIN_HLD_completeCurrentTransferDMA(handle);
        }
        else
        {
            if(object->openParams->enableParity == true)
            {
                object->currentTxnFrame->id =
                            (LIN_HLD_getReceivedID( hwAttrs->baseAddr) &
                                                    LIN_FRAME_ID_BIT_MASK);
            }
            else
            {
                object->currentTxnFrame->id =
                                    LIN_HLD_getReceivedID(hwAttrs->baseAddr);
            }

            /* Disable RX DMA */
            LIN_HLD_disableInterrupt(hwAttrs->baseAddr,  LIN_INT_RX_DMA);
            /* Complete Current Transfer */
            LIN_HLD_completeCurrentTransferDMA(handle);
        }
    }
}

static void LIN_edmaIsrDummy(Edma_IntrHandle intrHandle, void *args)
{
    /* No Code */
}
