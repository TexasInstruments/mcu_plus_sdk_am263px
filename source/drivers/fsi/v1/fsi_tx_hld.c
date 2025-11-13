/*
 * Copyright (C) 2024-2025 Texas Instruments Incorporated
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  \file     v1/fsi_tx_hld.c
 *
 *  \brief    This file contains the implementation of the APIs present in the
 *            device abstraction layer file of FSI_TX.
 *            This also contains some related macros.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <drivers/hw_include/csl_types.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/fsi.h>
#include <drivers/fsi/v1/fsi_tx_hld.h>
#include <drivers/fsi/v1/dma/edma/fsi_dma_edma.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                         Structures and Enums                               */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */

static int32_t FSI_Tx_configInstance(FSI_Tx_Handle hFsiTx);
static int32_t FSI_Tx_deConfigInstance(FSI_Tx_Handle hFsiTx);

/* None */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

typedef struct
{
    void                   *lock;
    /**< Driver lock - to protect across open/close */
    SemaphoreP_Object       lockObj;
    /**< Driver lock object */
} FSI_Tx_DrvObj;

/** \brief Driver object */
static FSI_Tx_DrvObj     gFsiTxDrvObj =
{
    .lock           = NULL,
};

extern uint32_t gFsiTxConfigNum;
extern FSI_Tx_Config gFsiTxConfig[];
extern FSI_Tx_DmaHandle gFsiTxDmaHandle[];
extern FSI_Tx_DmaChConfig gFsiTxDmaChCfg;

extern uint32_t fsi_tx_clk;

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void FSI_Tx_init(void)
{
    int32_t       status;
    uint32_t      cnt;
    FSI_Tx_Object    *object;

    /* Init each driver instance object */
    for (cnt = 0U; cnt < gFsiTxConfigNum; cnt++)
    {
        /* initialize object varibles */
        object = gFsiTxConfig[cnt].object;
        DebugP_assert(NULL_PTR != object);
        (void)memset(object, 0, sizeof(FSI_Tx_Object));
        gFsiTxConfig[cnt].attrs->baseAddr = (uint32_t) AddrTranslateP_getLocalAddr((uint64_t)gFsiTxConfig[cnt].attrs->baseAddr);
    }

    /* Create driver lock */
    status = SemaphoreP_constructMutex(&gFsiTxDrvObj.lockObj);
    if(SystemP_SUCCESS == status)
    {
        gFsiTxDrvObj.lock = &gFsiTxDrvObj.lockObj;
    }

    return;
}

void FSI_Tx_deinit(void)
{
    /* Delete driver lock */
    if(NULL != gFsiTxDrvObj.lock)
    {
        SemaphoreP_destruct(&gFsiTxDrvObj.lockObj);
        gFsiTxDrvObj.lock = NULL;
    }

    return;
}

void FSI_HLD_TxParams_init(FSI_Tx_Params *prms)
{
    if(prms != NULL)
    {
        prms->userData = 7;
        prms->frameDataSize = 16;
        prms->numLane = FSI_DATA_WIDTH_1_LANE;
        prms->frameTag = FSI_FRAME_TAG1;
        prms->frameType = FSI_FRAME_TYPE_NWORD_DATA;
        prms->rxTrigger = FALSE;
        prms->rxTriggerVal = 4;
        prms->errorCheck = FSI_TX_NO_ERROR_CHECK;
        prms->delayLineCtrl = FALSE;
        prms->udataFilterTest = FALSE;
        prms->rxFrameWDTest = FALSE;
        prms->rxPingWDTest = FALSE;
        prms->intrEvt     = FSI_TX_EVT_FRAME_DONE;
        prms->transferMode = FSI_TX_TRANSFER_MODE_BLOCKING;
        prms->transferCallbackFxn = NULL;
        prms->errorCallbackFxn = NULL;
    }
}

uint32_t FSI_Tx_getBaseAddr(FSI_Tx_Handle handle)
{
    FSI_Tx_Config       *config;
    FSI_Tx_Attrs        *attrs;
    uint32_t           baseAddr;

    /* Check parameters */
    if (NULL_PTR == handle)
    {
        baseAddr = 0U;
    }
    else
    {
        config = (FSI_Tx_Config *) handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
    }

    return baseAddr;
}

/*
 * Provides delay based on requested count. Needed while performing reset and
 * flush sequence, sufficient delay ensures reliability in operation.
 */
FSI_Tx_Handle FSI_Tx_open(uint32_t index, FSI_Tx_Params *prms)
{
    int32_t               status = SystemP_SUCCESS;
    FSI_Tx_Handle         handle = NULL;
    FSI_Tx_Config        *config = NULL;
    FSI_Tx_Object        *obj = NULL;
    const FSI_Tx_Attrs   *attrs;
    HwiP_Params           hwiPrms;

    /* Check index */
    if((index >= gFsiTxConfigNum) && (prms == NULL))
    {
        status = SystemP_FAILURE;
        DebugP_assert(NULL_PTR != prms);
    }
    else
    {
        config = &gFsiTxConfig[index];
    }

    DebugP_assert(NULL_PTR != gFsiTxDrvObj.lock);
    status += SemaphoreP_pend(&gFsiTxDrvObj.lockObj, SystemP_WAIT_FOREVER);

    if((SystemP_SUCCESS == status) && (config != NULL_PTR))
    {
        obj = config->object;
        DebugP_assert(NULL_PTR != obj);
        DebugP_assert(NULL_PTR != config->attrs);
        attrs = config->attrs;
        obj->params = prms;
        if(attrs->operMode == FSI_TX_OPER_MODE_DMA)
        {
            obj->fsiTxDmaHandle = (FSI_Tx_DmaHandle) gFsiTxDmaHandle[0];
            obj->fsiTxDmaChCfg  = gFsiTxDmaChCfg;
        }
        obj->handle = (FSI_Tx_Handle) config;
        handle = obj->handle;
    
        status += FSI_Tx_configInstance(handle);
        if(status == SystemP_SUCCESS)
        {
            /* Create write transfer sync semaphore */
            status += SemaphoreP_constructBinary(&obj->writeTransferSemObj, 0U);
            obj->writeTransferSem = &obj->writeTransferSemObj;

            /* Register interrupt */
            if(FSI_TX_OPER_MODE_INTERRUPT == attrs->operMode)
            {
                HwiP_Params_init(&hwiPrms);
                hwiPrms.intNum      = attrs->intrLine;
                hwiPrms.priority    = attrs->intrPriority;
                hwiPrms.callback    = &FSI_Tx_Isr;
                hwiPrms.args        = (void *) handle;
                status += HwiP_construct(&obj->hwiObj, &hwiPrms);
                status += FSI_enableTxInterrupt(attrs->baseAddr, attrs->intrNum, prms->intrEvt);
            }
        }

        SemaphoreP_post(&gFsiTxDrvObj.lockObj);

        /* Free-up resources in case of error */
        if (SystemP_SUCCESS != status)
        {
            FSI_Tx_close((FSI_Tx_Handle) config);
        }
    }

    return (handle);
}

void FSI_Tx_close(FSI_Tx_Handle handle)
{
    FSI_Tx_Config        *config;
    FSI_Tx_Object        *obj;
    const FSI_Tx_Attrs   *attrs;
    int32_t           status = SystemP_FAILURE;

    if(NULL != handle)
    {
        config = (FSI_Tx_Config *)handle;
        obj    = config->object;
        attrs  = config->attrs;
        DebugP_assert(NULL_PTR != obj);
        DebugP_assert(NULL_PTR != config->attrs);
        DebugP_assert(NULL_PTR != gFsiTxDrvObj.lock);

        status = SemaphoreP_pend(&gFsiTxDrvObj.lockObj, SystemP_WAIT_FOREVER);
        status += FSI_Tx_deConfigInstance(handle);
        DebugP_assert(SystemP_SUCCESS == status);

        if(NULL != obj->writeTransferSem)
        {
            SemaphoreP_destruct(&obj->writeTransferSemObj);
            obj->writeTransferSem = NULL;
        }
        if(NULL != obj->hwiHandle)
        {
            HwiP_destruct(&obj->hwiObj);
            obj->hwiHandle = NULL;
        }
        /* Register interrupt */
        if(FSI_TX_OPER_MODE_INTERRUPT == attrs->operMode)
        {
            status += FSI_disableTxInterrupt(attrs->baseAddr, FSI_INT1, FSI_TX_EVTMASK);
            DebugP_assert(SystemP_SUCCESS == status);
            FSI_clearTxEvents(attrs->baseAddr, FSI_TX_EVTMASK);
        }

        SemaphoreP_post(&gFsiTxDrvObj.lockObj);
    }

    return;
}

/**
 *  @b Description
 *  @n
 *      Function initializes the FSITx driver instance with the specified hardware attributes.
 *      It resets and configures the FSITX module.
 *
 *
 *  @param[in]  handle
 *      FSITx handle.
 *  @retval
 *      Success  -   SystemP_SUCCESS
 *  @retval
 *      Failure  -   SystemP_FAILURE
 */

static int32_t FSI_Tx_configInstance(FSI_Tx_Handle handle)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr;
    const FSI_Tx_Attrs *attrs;
    FSI_Tx_Config      *config = NULL;
    FSI_Tx_Object      *fsiTxObj = NULL;

    if(handle != NULL)
    {
        config = (FSI_Tx_Config *)handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        fsiTxObj = config->object;

        /* TX init and reset */
        status = FSI_performTxInitialization(baseAddr, attrs->preScalarVal);

        /* Setting for requested transfer params */
        status += FSI_setTxSoftwareFrameSize(baseAddr, fsiTxObj->params->frameDataSize);
        status += FSI_setTxDataWidth(baseAddr, fsiTxObj->params->numLane);

        if(fsiTxObj->params->rxTrigger == TRUE)
        {
            /* RX TRIGGER 0*/
            status += FSI_setTxExtFrameTrigger(baseAddr, fsiTxObj->params->rxTriggerVal);
        }

        if(attrs->operMode != FSI_TX_OPER_MODE_DMA)
        {
            /*In case of NON-DMA transmission */
            /* Setting frame config */
            if (fsiTxObj->params->udataFilterTest == TRUE)
            {
                status += FSI_setTxUserDefinedData(baseAddr, 0xA5);
            }
            else
            {
                status += FSI_setTxUserDefinedData(baseAddr, fsiTxObj->params->userData);
            }
            status += FSI_setTxFrameTag(baseAddr, fsiTxObj->params->frameTag);
        }

        status += FSI_setTxFrameType(baseAddr, fsiTxObj->params->frameType);

        if (fsiTxObj->params->delayLineCtrl == TRUE)
        {
            FSI_configTxDelayLine(baseAddr, FSI_TX_DELAY_CLK, 5U);
            FSI_configTxDelayLine(baseAddr, FSI_TX_DELAY_D0, 5U);
            FSI_configTxDelayLine(baseAddr, FSI_TX_DELAY_D1, 5U);
        }

        if(fsiTxObj->params->intrEvt == FSI_TX_EVT_PING_HW_TRIG)
        {
            status += FSI_resetTxModule(baseAddr, FSI_TX_PING_TIMEOUT_CNT_RESET);
            status += FSI_clearTxModuleReset(baseAddr, FSI_TX_PING_TIMEOUT_CNT_RESET);
        }

        /* Initialize dma mode if the dma handle is not NULL */
        if (status == SystemP_SUCCESS)
        {
            if (attrs->operMode == FSI_TX_OPER_MODE_DMA)
            {
                status  = FSI_Tx_dmaOpen(handle, fsiTxObj->fsiTxDmaChCfg);
            }
        }
    }

    return status;
}

static int32_t FSI_Tx_deConfigInstance(FSI_Tx_Handle handle)
{
    int32_t                 status = SystemP_FAILURE;
    const FSI_Tx_Attrs      *attrs;
    FSI_Tx_Config           *config = NULL;
    FSI_Tx_Object           *fsiTxObj = NULL;
    uint32_t            baseAddr = 0;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the FSI-TX Driver Block */
        config = (FSI_Tx_Config*)handle;
        attrs = config->attrs;
        fsiTxObj = config->object;
        baseAddr = attrs->baseAddr;

        if(FSI_TX_OPER_MODE_DMA == attrs->operMode)
        {
           status = FSI_Tx_dmaClose(handle, fsiTxObj->fsiTxDmaChCfg);
        }
        else
        {
            status = SystemP_SUCCESS;
        }
        if (fsiTxObj->params->errorCheck == FSI_TX_USER_DEFINED_CRC_CHECK)
        {
            /* CRC Value is calculated based on the TX pattern */
            FSI_disableTxUserCRC(baseAddr);
        }
        if(fsiTxObj->params->intrEvt == FSI_TX_EVT_PING_HW_TRIG)
        {
            FSI_disableTxPingTimer(baseAddr);
        }
    }

    return status;
}

int32_t FSI_Tx_hld(FSI_Tx_Handle handle, uint16_t *txBufData, uint16_t *txBufTagAndUserData, 
                    uint16_t bufIdx)
{
    int32_t                 retVal = SystemP_FAILURE;
    const FSI_Tx_Attrs      *attrs;
    FSI_Tx_Config           *config = NULL;
    FSI_Tx_Object           *object = NULL;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the CAN Driver Block */
        config = (FSI_Tx_Config*)handle;
        attrs = config->attrs;
        object = config->object;
        if((FSI_TX_OPER_MODE_INTERRUPT == attrs->operMode) ||
            (FSI_TX_OPER_MODE_DMA == attrs->operMode))
        {
            if(FSI_TX_OPER_MODE_INTERRUPT == attrs->operMode)
            {
                retVal = FSI_Tx_Intr(handle, txBufData, NULL, bufIdx);
            }
            else
            {
                retVal = FSI_Tx_Dma(handle, txBufData, txBufTagAndUserData, bufIdx);
            }
            if (retVal == SystemP_SUCCESS)
            {
                if(attrs->operMode == FSI_TX_OPER_MODE_INTERRUPT)
                {
                    if (object->params->transferMode == FSI_TX_TRANSFER_MODE_BLOCKING)
                    {
                        /* Block on transferSem till the transfer completion. */
                        if(object->params->rxFrameWDTest != TRUE && object->params->rxPingWDTest != TRUE )
                        {
                            DebugP_assert(NULL_PTR != object->writeTransferSem);
                            SemaphoreP_pend(&object->writeTransferSemObj, SystemP_WAIT_FOREVER);
                        }
                    }

                }
            }
        }
        else
        {
            retVal = FSI_Tx_Poll(handle, txBufData, NULL, bufIdx);
        }

    }

    return retVal;
}

int32_t FSI_Tx_Poll(FSI_Tx_Handle handle, uint16_t *txBufData, uint16_t *txBufTagAndUserData,
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr = 0;
    uint16_t    dataSize = 0;
    const FSI_Tx_Attrs *attrs;
    FSI_Tx_Config *config = NULL;
    FSI_Tx_Object *obj = NULL;
    uint16_t    txEvtSts;

    if(handle != NULL)
    {
        config = (FSI_Tx_Config *)handle;
        attrs = config->attrs;
        obj = config->object;
        baseAddr = attrs->baseAddr;
        dataSize = obj->params->frameDataSize;

        /* Transmit data */
        status = FSI_setTxBufferPtr(baseAddr, bufIdx);
        status += FSI_writeTxBuffer(baseAddr, txBufData, dataSize, bufIdx);
        status += FSI_startTxTransmit(baseAddr);
        DebugP_assert(status == SystemP_SUCCESS);
    }
    /* Wait for TX completion */
    while(1)
    {
        FSI_getTxEventStatus(baseAddr, &txEvtSts);
        if(txEvtSts & FSI_TX_EVT_FRAME_DONE)
        {
            FSI_clearTxEvents(baseAddr, FSI_TX_EVT_FRAME_DONE);
            break;
        }
    }
	return status;
}

int32_t FSI_Tx_Intr(FSI_Tx_Handle handle, uint16_t *txBufData, uint16_t *txBufTagAndUserData,
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr = 0;
    uint16_t dataSize = 0;
    const FSI_Tx_Attrs *attrs;
    FSI_Tx_Config *config = NULL;
    FSI_Tx_Object *obj = NULL;

    if(handle != NULL)
    {
        config = (FSI_Tx_Config *)handle;
        attrs = config->attrs;
        obj = config->object;
        baseAddr = attrs->baseAddr;
        dataSize = obj->params->frameDataSize;

        /* Transmit data */
        if(obj->params->intrEvt != FSI_TX_EVT_PING_HW_TRIG)
        {
            status = FSI_setTxBufferPtr(baseAddr, bufIdx);
            status += FSI_writeTxBuffer(baseAddr, txBufData, dataSize, bufIdx);
            status += FSI_startTxTransmit(baseAddr);
            DebugP_assert(status == SystemP_SUCCESS);
        }
        else
        {
            status = FSI_setTxPingTimeoutMode(baseAddr, FSI_PINGTIMEOUT_ON_HWINIT_PING_FRAME);
            status += FSI_setTxPingTag(baseAddr, FSI_FRAME_TAG10);
            status += FSI_enableTxPingTimer(baseAddr, ((fsi_tx_clk / (attrs->preScalarVal * 2)) / 2), FSI_FRAME_TAG10);
        }

        if(obj->params->rxFrameWDTest == TRUE || obj->params->rxPingWDTest == TRUE)
        {
            FSI_disableTxClock(baseAddr);
        }
    }

    return status;
}

int32_t FSI_Tx_Dma(FSI_Tx_Handle handle, uint16_t *txBufData, uint16_t *txBufTagAndUserData, 
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr, regionId, txBufBaseAddr, txFrameTagData;
    uint16_t dataSize = 0, loopCnt = 0;
    const FSI_Tx_Attrs *attrs;
    FSI_Tx_Config *config = NULL;
    FSI_Tx_Object *object = NULL;
    FSI_Tx_EdmaChConfig *edmaChCfg = NULL;
    uint32_t    dmaCh0, dmaCh1;
    uint32_t    param0, param1;
    uint32_t    tccTx;

    if(handle != NULL)
    {
        config = (FSI_Tx_Config *)handle;
        attrs = config->attrs;
        object = config->object;
        dataSize = object->params->frameDataSize;
        loopCnt = object->params->loopCnt;
        edmaChCfg = (FSI_Tx_EdmaChConfig *)object->fsiTxDmaChCfg;
        baseAddr = attrs->baseAddr;
        txBufBaseAddr = baseAddr + CSL_FSI_TX_CFG_TX_BUF_BASE(bufIdx);
        txFrameTagData = baseAddr + CSL_FSI_TX_CFG_TX_FRAME_TAG_UDATA;

        regionId = edmaChCfg->edmaRegionId;
        dmaCh0   = edmaChCfg->edmaTxChId[0];
        param0 = EDMA_RESOURCE_ALLOC_ANY;
        tccTx = EDMA_RESOURCE_ALLOC_ANY;

        dmaCh1  = edmaChCfg->edmaTxChId[1];
        param1 = EDMA_RESOURCE_ALLOC_ANY;

        /* Create a semaphore to signal EDMA transfer completion */
        status = SemaphoreP_constructBinary(&gFsiDmaTxSemObject, 0);
        DebugP_assert(SystemP_SUCCESS == status);
    
        if(status == SystemP_SUCCESS)
        {
            status += FSI_Tx_edmaChInit(object, 0, &param0, NULL);
            status += FSI_Tx_configureDma(object, &dmaCh0, (void *)txBufData,
                        (void *)txBufBaseAddr,
                            NULL, &param0, regionId, sizeof(uint16_t), dataSize, 1U,
                            sizeof(uint16_t), sizeof(uint16_t), 0U, 0U, EDMA_TRIG_MODE_MANUAL);

            status += FSI_Tx_edmaChInit(object, 1, &param1, &tccTx);
            status += FSI_Tx_configureDma(object, &dmaCh1, (void *)txBufTagAndUserData,
                            (void *)txFrameTagData,
                            NULL, &param1, regionId, sizeof(uint16_t), 1U, 1U, 0U, 0U, sizeof(uint16_t), 0U, EDMA_TRIG_MODE_MANUAL);

            /* Copy rest of the data with event based */
            status += FSI_Tx_configureDma(object, &dmaCh0, (void *)(txBufData + dataSize),
                        (void *)txBufBaseAddr,
                            NULL, &param0, regionId, sizeof(uint16_t), dataSize, loopCnt - 1U,
                            sizeof(uint16_t), sizeof(uint16_t), sizeof(uint16_t) * dataSize, 0U, EDMA_TRIG_MODE_EVENT);
            status += FSI_Tx_configureDma(object, &dmaCh1, (void *)&txBufTagAndUserData[1U],
                            (void *)txFrameTagData,
                            &tccTx, &param1, regionId, sizeof(uint16_t), 1U, loopCnt - 1U, 0U, 0U, sizeof(uint16_t), 0U, EDMA_TRIG_MODE_EVENT);
        }

        status = FSI_Tx_edmaIntrInit(object, tccTx);

        if(status == SystemP_SUCCESS)
        {
            /* Restore the structure parameters */
            edmaChCfg->edmaTxChId[0] = dmaCh0;
            edmaChCfg->edmaTxChId[1] = dmaCh1;
            edmaChCfg->edmaTxParam[0] = param0;
            edmaChCfg->edmaTxParam[1] = param1;
            edmaChCfg->edmaTccTx = tccTx;

            /* Transmit data */
            /* Send Flush Sequence to sync, after every rx soft reset */
            status = FSI_executeTxFlushSequence(baseAddr, attrs->preScalarVal);
            DebugP_assert(status == SystemP_SUCCESS);
            status = FSI_setTxStartMode(baseAddr, FSI_TX_START_FRAME_CTRL_OR_UDATA_TAG);
            FSI_enableTxDMAEvent(baseAddr);
        }
    }

       /* Wait for TX completion */
   // SemaphoreP_pend(&gFsiDmaTxSemObject, SystemP_WAIT_FOREVER);

    return status;
}

void FSI_Tx_Isr(void* args)
{
    const FSI_Tx_Attrs *attrs;
    FSI_Tx_Config *config = NULL;
    FSI_Tx_Object *object = NULL;
    uint32_t baseAddr = 0;
    uint16_t intrStatus;

    if(args != NULL)
    {
        config = (FSI_Tx_Config *)args;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        object = config->object;

        FSI_getTxEventStatus(baseAddr, &intrStatus);
        if ((intrStatus & FSI_TX_EVT_FRAME_DONE) == FSI_TX_EVT_FRAME_DONE)
        {
            FSI_clearTxEvents(baseAddr, FSI_TX_EVT_FRAME_DONE);
        }
        if ((intrStatus & FSI_TX_EVT_PING_HW_TRIG) == FSI_TX_EVT_PING_HW_TRIG)
        {
            FSI_clearTxEvents(baseAddr, FSI_TX_EVT_PING_HW_TRIG);
        }

        SemaphoreP_post(&object->writeTransferSemObj);
    }
    return;
}

void FSI_Tx_errorCheck(FSI_Tx_Handle handle, uint16_t *txBufData)
{
    FSI_Tx_Config *config = NULL;
    FSI_Tx_Object *object = NULL;
    FSI_Tx_Attrs *attrs;
    uint32_t baseAddr;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the CAN Driver Block */
        config = (FSI_Tx_Config*)handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        object = config->object;

       if(object->params->errorCheck == FSI_TX_USER_DEFINED_CRC_CHECK)
       {
            FSI_enableTxUserCRC(baseAddr, FSI_APP_TX_PATTERN_USER_CRC_VALUE);
       }
       else if(object->params->errorCheck == FSI_TX_ECC_ERROR_CHECK)
       {
            /* ECC computation for 2 words */
            uint16_t getEccVal = 0U;
            uint32_t txData = txBufData[0U] | txBufData[1U] << 16U;

            FSI_setTxECCComputeWidth(baseAddr, FSI_16BIT_ECC_COMPUTE);
            FSI_setTxECCdata(baseAddr, txData);
            FSI_getTxECCValue(baseAddr, &getEccVal);
            FSI_setTxUserDefinedData(baseAddr, getEccVal);
       }
    }
}

void FSI_Tx_pendDmaCompletion()
{
    SemaphoreP_pend(&gFsiDmaTxSemObject, SystemP_WAIT_FOREVER);
}

void FSI_Tx_DmaCompletionCallback(void *args)
{
    SemaphoreP_post(&gFsiDmaTxSemObject);
}