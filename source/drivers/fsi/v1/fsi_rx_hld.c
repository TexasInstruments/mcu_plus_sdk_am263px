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
 *  \file     v1/fsi_rx_hld.c
 *
 *  \brief    This file contains the implementation of the APIs present in the
 *            device abstraction layer file of FSI_RX.
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
#include <drivers/fsi/v1/fsi_rx_hld.h>
#include <drivers/fsi/v1/dma/edma/fsi_dma_edma.h>
#include <kernel/dpl/ClockP.h>

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

static int32_t FSI_Rx_configInstance(FSI_Rx_Handle hFsiRx);
static int32_t FSI_Rx_deConfigInstance(FSI_Rx_Handle hFsiRx);

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
} FSI_Rx_DrvObj;

/** \brief Driver object */
static FSI_Rx_DrvObj     gFsiRxDrvObj =
{
    .lock           = NULL,
};

extern uint32_t gFsiRxConfigNum;
extern FSI_Rx_Config gFsiRxConfig[];
extern FSI_Rx_DmaHandle gFsiRxDmaHandle[];
extern FSI_Rx_DmaChConfig gFsiRxDmaChCfg;
/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void FSI_Rx_init(void)
{
    int32_t       status;
    uint32_t      cnt;
    FSI_Rx_Object    *object;

    /* Init each driver instance object */
    for (cnt = 0U; cnt < gFsiRxConfigNum; cnt++)
    {
        /* initialize object varibles */
        object = gFsiRxConfig[cnt].object;
        DebugP_assert(NULL_PTR != object);
        (void)memset(object, 0, sizeof(FSI_Rx_Object));
        gFsiRxConfig[cnt].attrs->baseAddr = (uint32_t) AddrTranslateP_getLocalAddr((uint64_t)gFsiRxConfig[cnt].attrs->baseAddr);
    }

    /* Create driver lock */
    status = SemaphoreP_constructMutex(&gFsiRxDrvObj.lockObj);
    if(SystemP_SUCCESS == status)
    {
        gFsiRxDrvObj.lock = &gFsiRxDrvObj.lockObj;
    }

    return;
}

void FSI_Rx_deinit(void)
{
    /* Delete driver lock */
    if(NULL != gFsiRxDrvObj.lock)
    {
        SemaphoreP_destruct(&gFsiRxDrvObj.lockObj);
        gFsiRxDrvObj.lock = NULL;
    }

    return;
}

void FSI_HLD_RxParams_init(FSI_Rx_Params *prms)
{
    if(prms != NULL)
    {
        prms->userData = 7;
        prms->frameDataSize = 16;
        prms->numLane = FSI_DATA_WIDTH_1_LANE;
        prms->errorCheck = FSI_RX_NO_ERROR_CHECK;
        prms->delayLineCtrl = FALSE;
        prms->rxTrigger  = false,
        prms->rxTriggerValCycles  = 4,
        prms->udataFilterTest = FALSE;
        prms->rxFrameWDTest = FALSE;
        prms->rxPingWDTest = FALSE;
        prms->intrEvt        = FSI_RX_EVT_DATA_FRAME,
        prms->hwPing        = FALSE,
        prms->transferMode = FSI_RX_TRANSFER_MODE_BLOCKING,
        prms->transferCallbackFxn = NULL;
        prms->errorCallbackFxn = NULL;
    }
}

uint32_t FSI_Rx_getBaseAddr(FSI_Rx_Handle handle)
{
    FSI_Rx_Config       *config;
    FSI_Rx_Attrs        *attrs;
    uint32_t           baseAddr;

    /* Check parameters */
    if (NULL_PTR == handle)
    {
        baseAddr = 0U;
    }
    else
    {
        config = (FSI_Rx_Config *) handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
    }

    return baseAddr;
}

/*
 * Provides delay based on requested count. Needed while performing reset and
 * flush sequence, sufficient delay ensures reliability in operation.
 */
FSI_Rx_Handle FSI_Rx_open(uint32_t index, FSI_Rx_Params *prms)
{
    int32_t               status = SystemP_SUCCESS;
    FSI_Rx_Handle         handle = NULL;
    FSI_Rx_Config        *config = NULL;
    FSI_Rx_Object        *obj = NULL;
    const FSI_Rx_Attrs   *attrs;
    HwiP_Params           hwiPrms;

    /* Check index */
    if((index >= gFsiRxConfigNum) && (prms == NULL))
    {
        status = SystemP_FAILURE;
        DebugP_assert(NULL_PTR != prms);
    }
    else
    {
        config = &gFsiRxConfig[index];
    }

    DebugP_assert(NULL_PTR != gFsiRxDrvObj.lock);
    status += SemaphoreP_pend(&gFsiRxDrvObj.lockObj, SystemP_WAIT_FOREVER);

    if((SystemP_SUCCESS == status) && (config != NULL_PTR))
    {
        obj = config->object;
        DebugP_assert(NULL_PTR != obj);
        DebugP_assert(NULL_PTR != config->attrs);
        attrs = config->attrs;
        obj->params = prms;
        if(attrs->operMode == FSI_RX_OPER_MODE_DMA)
        {
            obj->fsiRxDmaHandle = (FSI_Rx_DmaHandle) gFsiRxDmaHandle[0];
            obj->fsiRxDmaChCfg  = gFsiRxDmaChCfg;
        }
        obj->handle = (FSI_Rx_Handle) config;
        handle = obj->handle;
    
        status += FSI_Rx_configInstance(handle);
        if(status == SystemP_SUCCESS)
        {
            /* Create read transfer sync semaphore */
            status += SemaphoreP_constructBinary(&obj->readTransferSemObj, 0U);
            obj->readTransferSem = &obj->readTransferSemObj;

            if(obj->params->rxFrameWDTest == TRUE)
            {
                /* Performing a reset on frame WD */
                FSI_resetRxModule(attrs->baseAddr, FSI_RX_FRAME_WD_CNT_RESET);
                ClockP_usleep(1U);
                FSI_clearRxModuleReset(attrs->baseAddr, FSI_RX_FRAME_WD_CNT_RESET);
            }
            if(obj->params->rxPingWDTest == TRUE)
            {
                /* Performing a reset on frame WD */
                FSI_resetRxModule(attrs->baseAddr, FSI_RX_PING_WD_CNT_RESET);
                ClockP_usleep(1U);
                FSI_clearRxModuleReset(attrs->baseAddr, FSI_RX_PING_WD_CNT_RESET);
            }

            /* Interrupt Init */
            /* Register interrupt */
            if(FSI_RX_OPER_MODE_INTERRUPT == attrs->operMode)
            {
                HwiP_Params_init(&hwiPrms);
                hwiPrms.intNum      = attrs->intrLine;
                hwiPrms.priority    = attrs->intrPriority;
                hwiPrms.callback    = &FSI_Rx_Isr;
                hwiPrms.args        = (void *) handle;
                status += HwiP_construct(&obj->hwiObj, &hwiPrms);
                status = FSI_enableRxInterrupt(attrs->baseAddr, attrs->intrNum, prms->intrEvt);
            }
        }

        if(obj->params->rxFrameWDTest == TRUE)
        {
            /* Value to generate a Frame WD timeout interrupt event */
            FSI_enableRxFrameWatchdog(attrs->baseAddr, 0x10000);
        }

        if(obj->params->rxPingWDTest == TRUE)
        {
            /* Value to generate a Frame WD timeout interrupt event */
            FSI_enableRxPingWatchdog(attrs->baseAddr, 0x10000);
        }

        SemaphoreP_post(&gFsiRxDrvObj.lockObj);

        /* Free-up resources in case of error */
        if (SystemP_SUCCESS != status)
        {
            FSI_Rx_close((FSI_Rx_Handle) config);
        }
    }

    return (handle);
}

void FSI_Rx_close(FSI_Rx_Handle handle)
{
    FSI_Rx_Config        *config;
    FSI_Rx_Object        *obj;
    const FSI_Rx_Attrs   *attrs;
    int32_t           status = SystemP_FAILURE;

    if(NULL != handle)
    {
        config = (FSI_Rx_Config *)handle;
        obj    = config->object;
        attrs  = config->attrs;
        DebugP_assert(NULL_PTR != obj);
        DebugP_assert(NULL_PTR != config->attrs);
        DebugP_assert(NULL_PTR != gFsiRxDrvObj.lock);

        status = SemaphoreP_pend(&gFsiRxDrvObj.lockObj, SystemP_WAIT_FOREVER);
        status += FSI_Rx_deConfigInstance(handle);
        DebugP_assert(SystemP_SUCCESS == status);

        if(NULL != obj->readTransferSem)
        {
            SemaphoreP_destruct(&obj->readTransferSemObj);
            obj->readTransferSem = NULL;
        }
        if(NULL != obj->hwiHandle)
        {
            HwiP_destruct(&obj->hwiObj);
            obj->hwiHandle = NULL;
        }

        /* Register interrupt */
        if(FSI_RX_OPER_MODE_INTERRUPT == attrs->operMode)
        {
            status += FSI_disableRxInterrupt(attrs->baseAddr, FSI_INT1, FSI_RX_EVTMASK);
            FSI_clearRxEvents(attrs->baseAddr, FSI_RX_EVTMASK);
            DebugP_assert(SystemP_SUCCESS == status);
        }

        SemaphoreP_post(&gFsiRxDrvObj.lockObj);
    }

    return;
}

/**
 *  @b Description
 *  @n
 *      Function initializes the FSIRx driver instance with the specified hardware attributes.
 *      It resets and configures the FSIRX module.
 *
 *
 *  @param[in]  handle
 *      FSIRx handle.
 *  @retval
 *      Success  -   SystemP_SUCCESS
 *  @retval
 *      Failure  -   SystemP_FAILURE
 */

static int32_t FSI_Rx_configInstance(FSI_Rx_Handle handle)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr;
    const FSI_Rx_Attrs *attrs;
    FSI_Rx_Config      *config = NULL;
    FSI_Rx_Object      *fsiRxObj = NULL;

    if(handle != NULL)
    {
        config = (FSI_Rx_Config *)handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        fsiRxObj = config->object;

        /* Rx init and reset */
        status = FSI_performRxInitialization(baseAddr);

        if(fsiRxObj->params->udataFilterTest == TRUE)
        {
            FSI_enableRxDataFilter(baseAddr);
            FSI_setRxUserDataRefValue(baseAddr, fsiRxObj->params->userData);
            FSI_setRxUserDataBitMask(baseAddr, 0xF0);
        }
        if(fsiRxObj->params->rxTrigger == TRUE)
        {
            FSI_enableAndConfigRxTrigCtrl(baseAddr, FSI_RX_TRIG_CTRL_REG_0,
                                        FSI_RX_TRIGGER_CTRL_SEL_DATA_PACKET,
                                        fsiRxObj->params->rxTriggerValCycles);
        }

        /* Setting for requested transfer params */
        status += FSI_setRxSoftwareFrameSize(baseAddr, fsiRxObj->params->frameDataSize);
        status += FSI_setRxDataWidth(baseAddr, fsiRxObj->params->numLane);

        if((fsiRxObj->params->intrEvt == FSI_RX_EVT_PING_WD_TIMEOUT) ||
            (fsiRxObj->params->intrEvt == FSI_RX_EVT_PING_FRAME))
        {
            status += FSI_setRxPingTimeoutMode(baseAddr, FSI_PINGTIMEOUT_ON_HWINIT_PING_FRAME);
        }
        else
        {
            if(attrs->operMode != FSI_RX_OPER_MODE_DMA)
            {
                /* Setting frame config */
                status += FSI_setRxBufferPtr(baseAddr, 0U);
            }
        }

        if (fsiRxObj->params->delayLineCtrl == TRUE)
        {
            FSI_configRxDelayLine(baseAddr, FSI_RX_DELAY_CLK, 5U);
            FSI_configRxDelayLine(baseAddr, FSI_RX_DELAY_D0, 5U);
            FSI_configRxDelayLine(baseAddr, FSI_RX_DELAY_D1, 5U);
        }
        if(attrs->operMode == FSI_RX_OPER_MODE_DMA)
        {
            status  = FSI_Rx_dmaOpen(handle, fsiRxObj->fsiRxDmaChCfg);
        }
    }

    return status;
}

static int32_t FSI_Rx_deConfigInstance(FSI_Rx_Handle handle)
{
    int32_t                 status = SystemP_FAILURE;
    const FSI_Rx_Attrs      *attrs;
    FSI_Rx_Config           *config = NULL;
    FSI_Rx_Object           *fsiRxObj = NULL;
    uint32_t  baseAddr;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the FSI-RX Driver Block */
        config = (FSI_Rx_Config*)handle;
        attrs = config->attrs;
        fsiRxObj = config->object;
        baseAddr = attrs->baseAddr;

        if(FSI_RX_OPER_MODE_DMA == attrs->operMode)
        {
           status = FSI_Rx_dmaClose(handle, fsiRxObj->fsiRxDmaChCfg);
        }
        else
        {
            if(fsiRxObj->params->udataFilterTest == TRUE)
            {
                FSI_disableRxDataFilter(baseAddr);
            }
            if(fsiRxObj->params->rxFrameWDTest == TRUE)
            {
                FSI_disableRxFrameWatchdog(baseAddr);
            }
            if(fsiRxObj->params->rxPingWDTest == TRUE)
            {
                FSI_disableRxPingWatchdog(baseAddr);
            }
            status = SystemP_SUCCESS;
        }
    }

    return status;
}

int32_t FSI_Rx_hld(FSI_Rx_Handle handle, uint16_t *rxBufData, uint16_t *rxBufTagAndUserData,
                    uint16_t bufIdx)
{
    int32_t                 retVal = SystemP_FAILURE;
    const FSI_Rx_Attrs      *attrs;
    FSI_Rx_Config           *config = NULL;
    FSI_Rx_Object           *object = NULL;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the FSI Driver Block */
        config = (FSI_Rx_Config*)handle;
        attrs = config->attrs;
        object = config->object;
        if((FSI_RX_OPER_MODE_INTERRUPT == attrs->operMode) ||
            (FSI_RX_OPER_MODE_DMA == attrs->operMode))
        {
            if(FSI_RX_OPER_MODE_INTERRUPT == attrs->operMode)
            {
                retVal = FSI_Rx_Intr(handle, rxBufData, NULL, bufIdx);
            }
            else
            {
                retVal = FSI_Rx_Dma(handle, rxBufData, rxBufTagAndUserData, bufIdx);
            }
            if (retVal == SystemP_SUCCESS)
            {
                if(attrs->operMode == FSI_RX_OPER_MODE_INTERRUPT)
                {
                    if (object->params->transferMode == FSI_RX_TRANSFER_MODE_BLOCKING)
                    {
                        /* Block on transferSem till the transfer completion. */
                        DebugP_assert(NULL_PTR != object->readTransferSem);
                        SemaphoreP_pend(&object->readTransferSemObj, SystemP_WAIT_FOREVER);
                    }
                }
            }
        }
        else
        {
            retVal = FSI_Rx_Poll(handle, rxBufData, NULL, bufIdx);
        }

    }

    return retVal;
}

int32_t FSI_Rx_Poll(FSI_Rx_Handle handle, uint16_t *rxBufData, uint16_t *rxBufTagAndUserData, 
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr = 0;
    uint16_t    dataSize = 0;
    const FSI_Rx_Attrs *attrs;
    FSI_Rx_Config *config = NULL;
    FSI_Rx_Object *obj = NULL;
    uint16_t    rxEvtSts;

    if(handle != NULL)
    {
        config = (FSI_Rx_Config *)handle;
        attrs = config->attrs;
        obj = config->object;
        baseAddr = attrs->baseAddr;
        dataSize = obj->params->frameDataSize;
        /* Wait for RX completion */
        while(1)
        {
            FSI_getRxEventStatus(baseAddr, &rxEvtSts);
            if(rxEvtSts & FSI_RX_EVT_FRAME_DONE)
            {
                FSI_clearRxEvents(baseAddr, FSI_RX_EVT_FRAME_DONE);
                break;
            }
        }

        /* Recieve data */
        status = FSI_readRxBuffer(baseAddr, rxBufData, dataSize, bufIdx);
        DebugP_assert(status == SystemP_SUCCESS);
    }

    return status;
}

int32_t FSI_Rx_Intr(FSI_Rx_Handle handle, uint16_t *rxBufData, uint16_t *rxBufTagAndUserData,
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr = 0;
    uint16_t dataSize = 0;
    const FSI_Rx_Attrs *attrs;
    FSI_Rx_Config *config = NULL;
    FSI_Rx_Object *obj = NULL;

    if(handle != NULL)
    {
        config = (FSI_Rx_Config *)handle;
        attrs = config->attrs;
        obj = config->object;
        baseAddr = attrs->baseAddr;

        if(obj->params->rxFrameWDTest != TRUE && obj->params->rxPingWDTest != TRUE)
        {
            dataSize = obj->params->frameDataSize;
            /* Recieve data */
            status = FSI_readRxBuffer(baseAddr, rxBufData, dataSize, bufIdx);
            DebugP_assert(status == SystemP_SUCCESS);
        }
    }

    return status;
}

int32_t FSI_Rx_Dma(FSI_Rx_Handle handle, uint16_t *rxBufData, uint16_t *rxBufTagAndUserData, 
                    uint16_t bufIdx)
{
    int32_t     status = SystemP_SUCCESS;
    uint32_t    baseAddr, regionId, rxBufBaseAddr, rxFrameTagData;
    uint16_t dataSize = 0, loopCnt = 0;;
    const FSI_Rx_Attrs *attrs;
    FSI_Rx_Config *config = NULL;
    FSI_Rx_Object *object = NULL;
    FSI_Rx_EdmaChConfig *edmaChCfg = NULL;
    uint32_t    dmaCh0, dmaCh1;
    uint32_t    param0, param1;
    uint32_t    tccRx;

    if(handle != NULL)
    {
        config = (FSI_Rx_Config *)handle;
        attrs = config->attrs;
        object = config->object;
        edmaChCfg = (FSI_Rx_EdmaChConfig *)object->fsiRxDmaChCfg;
        dataSize = object->params->frameDataSize;
        loopCnt = object->params->loopCnt;
        baseAddr = attrs->baseAddr;
        rxBufBaseAddr = baseAddr + CSL_FSI_RX_CFG_RX_BUF_BASE(bufIdx);
        rxFrameTagData = baseAddr + CSL_FSI_RX_CFG_RX_FRAME_TAG_UDATA;

        regionId = edmaChCfg->edmaRegionId;
        dmaCh0   = edmaChCfg->edmaRxChId[0];
        param0 = EDMA_RESOURCE_ALLOC_ANY;
        tccRx = EDMA_RESOURCE_ALLOC_ANY;

        dmaCh1  = edmaChCfg->edmaRxChId[1];
        param1 = EDMA_RESOURCE_ALLOC_ANY;

        /* Create a semaphore to signal EDMA transfer completion */
        status = SemaphoreP_constructBinary(&gFsiDmaRxSemObject, 0);
        DebugP_assert(SystemP_SUCCESS == status);

        FSI_Rx_edmaChInit(object, 0, &param0, NULL);
        FSI_Rx_configureDma(object, &dmaCh0, (void *)rxBufBaseAddr,
                            (void *)rxBufData, NULL, &param0, regionId, sizeof(uint16_t), dataSize, loopCnt,
                         sizeof(uint16_t), sizeof(uint16_t), 0U, sizeof(uint16_t) * dataSize, EDMA_TRIG_MODE_EVENT);

        FSI_Rx_edmaChInit(object, 1, &param1, &tccRx);
        FSI_Rx_configureDma(object, &dmaCh1, (void *)rxFrameTagData,
                        (void *)rxBufTagAndUserData,
                        &tccRx, &param1, regionId, sizeof(uint16_t), 1U, loopCnt, 0U, 0U, 0U, sizeof(uint16_t), EDMA_TRIG_MODE_EVENT);

        status = FSI_Rx_edmaIntrInit(object, tccRx);
        FSI_enableRxDMAEvent(baseAddr);

        /* Restore the structure parameters */
        edmaChCfg->edmaRxChId[0] = dmaCh0;
        edmaChCfg->edmaRxChId[1] = dmaCh1;
        edmaChCfg->edmaRxParam[0] = param0;
        edmaChCfg->edmaRxParam[1] = param1;
        edmaChCfg->edmaTccRx = tccRx;
    }

    /* Wait for RX completion */
    //SemaphoreP_pend(&gFsiDmaRxSemObject, SystemP_WAIT_FOREVER);

    return status;
}

void FSI_Rx_Isr(void* args)
{
    const FSI_Rx_Attrs *attrs;
    FSI_Rx_Config *config = NULL;
    FSI_Rx_Object *object = NULL;
    uint32_t baseAddr = 0;
    uint16_t intrStatus;

    if(args != NULL)
    {
        config = (FSI_Rx_Config *)args;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        object = config->object;

        FSI_getRxEventStatus(baseAddr, &intrStatus);
        if ((intrStatus & FSI_RX_EVT_FRAME_WD_TIMEOUT) == FSI_RX_EVT_FRAME_WD_TIMEOUT)
        {
            FSI_clearRxEvents(baseAddr, FSI_RX_EVT_FRAME_WD_TIMEOUT);
        }
        if ((intrStatus & FSI_RX_EVT_PING_WD_TIMEOUT) == FSI_RX_EVT_PING_WD_TIMEOUT)
        {
            FSI_clearRxEvents(baseAddr, FSI_RX_EVT_PING_WD_TIMEOUT);
        }
        if ((intrStatus & (FSI_RX_EVT_DATA_FRAME | FSI_RX_EVT_FRAME_DONE)) ==
                          (FSI_RX_EVT_DATA_FRAME | FSI_RX_EVT_FRAME_DONE))
        {
            FSI_clearRxEvents(baseAddr,
                              (FSI_RX_EVT_DATA_FRAME | FSI_RX_EVT_FRAME_DONE));
        }

        if ((intrStatus & FSI_RX_EVT_PING_FRAME) == FSI_RX_EVT_PING_FRAME)
        {
            FSI_clearRxEvents(baseAddr, FSI_RX_EVT_PING_FRAME);
        }

        if(object->params->hwPing == TRUE)
        {
            if(object->params->rxPingWDTest != TRUE)
            {
                uint16_t rxPingTag = 0U;
                FSI_FrameType frameType;
                /* Read PinTag which should be same as TXPingTag */
                FSI_getRxFrameType(baseAddr, &frameType);
                DebugP_assert(frameType == FSI_FRAME_TYPE_PING);
                FSI_getRxPingTag(baseAddr, &rxPingTag);
                DebugP_assert(rxPingTag == FSI_FRAME_TAG10);
            }
        }

        SemaphoreP_post(&object->readTransferSemObj);

        return;
    }
}

void FSI_Rx_errorCheck(FSI_Rx_Handle handle, uint16_t *rxBufData)
{
    FSI_Rx_Config *config = NULL;
    FSI_Rx_Object *object = NULL;
    FSI_Rx_Attrs *attrs;
    uint32_t baseAddr;

    if(NULL_PTR != handle)
    {
        /* Get the pointer to the FSI_RX Driver Block */
        config = (FSI_Rx_Config*)handle;
        attrs = config->attrs;
        baseAddr = attrs->baseAddr;
        object = config->object;

       if(object->params->errorCheck == FSI_RX_USER_DEFINED_CRC_CHECK)
       {
            uint16_t receivedCrcVal;
            FSI_getRxReceivedCRC(baseAddr, &receivedCrcVal);
            DebugP_assert(receivedCrcVal == FSI_APP_TX_PATTERN_USER_CRC_VALUE);
       }
       else if(object->params->errorCheck == FSI_RX_ECC_ERROR_CHECK)
       {
            /* ECC computation for 2 words */
            uint16_t getEccVal = 0U, rxEccLog;
            uint32_t rxData = rxBufData[0U] | rxBufData[1U] << 16U;

            FSI_getRxUserDefinedData(baseAddr, &getEccVal);
            FSI_setRxECCComputeWidth(baseAddr, FSI_16BIT_ECC_COMPUTE);
            FSI_setRxECCData(baseAddr, rxData);
            FSI_setRxReceivedECCValue(baseAddr, getEccVal);
            FSI_getRxECCLog(baseAddr, &rxEccLog);
            DebugP_assert(rxEccLog == 0U);
       }
    }
}

void FSI_Rx_pendDmaCompletion()
{
    SemaphoreP_pend(&gFsiDmaRxSemObject, SystemP_WAIT_FOREVER);
}

void FSI_Rx_DmaCompletionCallback(void *args)
{
    SemaphoreP_post(&gFsiDmaRxSemObject);
}
