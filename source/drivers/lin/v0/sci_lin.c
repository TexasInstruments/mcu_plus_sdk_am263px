/*
 * Copyright (C) 2025 Texas Instruments Incorporated
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
 *  \file   sci_lin.c
 *
 *  \brief  File containing LIN Driver APIs implementation for V0.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <drivers/lin/v0/sci_lin.h>
#include <drivers/lin/v0/internal/sci_lin_internal.h>
#include <drivers/lin/v0/dma/sci_lin_dma.h>
#include <drivers/edma/v0/edma.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/soc.h>

/* ========================================================================== */
/*                           Structure Declarations                           */
/* ========================================================================== */

typedef struct
{
    void                    *lock;
    /**< Driver lock - to protect across open/close */
    SemaphoreP_Object       lockObj;
    /**< Driver lock object */

} LIN_DrvLockObj;

const LIN_SCI_Frame LIN_SCI_defaultFrame = {

    0x00U,                          /* ID */
    0x00U,                          /* frameLen */
    NULL,                           /* dataBuf(Pointer) */
    LIN_HLD_TXN_TYPE_WRITE,         /* txnType */
    SystemP_WAIT_FOREVER,           /* timeout */
    LIN_HLD_TXN_STS_SUCCESS,        /* status */
    NULL                            /* args(Pointer) */
};

const LIN_OpenParams LIN_SCI_default_open_param = {

    .transferMode                           = LIN_TRANSFER_MODE_BLOCKING,
    .moduleMode                             = LIN_MODULE_OP_MODE_LIN,
    .enableParity                           = true,
    .commMode                               = LIN_COMM_HLD_LIN_USELENGTHVAL,
    .multiBufferMode                        = true,
    .baudConfigParams.preScaler             = 650,
    .baudConfigParams.fracDivSel_M          = 0,
    .baudConfigParams.supFracDivSel_U       = 0,
    .linConfigParams.linMode                = LIN_MODE_HLD_LIN_RESPONDER,
    .linConfigParams.maskFilteringType      = LIN_HLD_MSG_FILTER_IDRESPONDER,
    .linConfigParams.linTxMask              = 255U,
    .linConfigParams.linRxMask              = 255U,
    .linConfigParams.checksumType           = LIN_HLD_CHECKSUM_ENHANCED,
    .linConfigParams.adaptModeEnable        = false,
    .linConfigParams.maxBaudRate            = 20000,
    .linConfigParams.syncDelimiter          = LIN_HLD_SYNC_DELIMITER_LEN_3,
    .linConfigParams.syncBreak              = LIN_HLD_SYNC_BREAK_LEN_18,
    .linConfigParams.idMatchCallbackFxn     = NULL,
    .linConfigParams.transferCompleteCallbackFxn
                                            = NULL,
};

/* ========================================================================== */
/*                     Internal Function Declarations                         */
/* ========================================================================== */

static int32_t LIN_SCI_writeFrame(LIN_Handle handle, LIN_SCI_Frame *frame);
static int32_t LIN_SCI_readFrame(LIN_Handle handle, LIN_SCI_Frame *frame);

/* HWI Function */
static void LIN_hwiFxn0(void* arg);
static void LIN_hwiFxn1(void* arg);

/* Internal API Support Function */
static int32_t LIN_WriteLinFrameCommanderPolling(LIN_Handle handle,
                                                 LIN_SCI_Frame *frame);
static int32_t LIN_WriteLinFrameCommanderInterrupt(LIN_Handle handle,
                                                   LIN_SCI_Frame *frame);
static int32_t LIN_WriteLinFrameResponderPolling(LIN_Handle handle,
                                                 LIN_SCI_Frame *frame);
static int32_t LIN_WriteLinFrameResponderInterrupt(LIN_Handle handle,
                                                   LIN_SCI_Frame *frame);
static int32_t LIN_WriteSciFramePolling(LIN_Handle handle,
                                        LIN_SCI_Frame *frame);
static int32_t LIN_WriteSciFrameInterrupt(LIN_Handle handle,
                                          LIN_SCI_Frame *frame);
static int32_t LIN_ReadLinFrameCommanderPolling(LIN_Handle handle,
                                                LIN_SCI_Frame *frame);
static int32_t LIN_ReadLinFrameCommanderInterrupt(LIN_Handle handle,
                                                  LIN_SCI_Frame *frame);
static int32_t LIN_ReadLinFrameResponderPolling(LIN_Handle handle,
                                                LIN_SCI_Frame *frame);
static int32_t LIN_ReadLinFrameResponderInterrupt(LIN_Handle handle,
                                                  LIN_SCI_Frame *frame);
static int32_t LIN_ReadSciFramePolling(LIN_Handle handle,
                                       LIN_SCI_Frame *frame);
static int32_t LIN_ReadSciFrameInterrupt(LIN_Handle handle,
                                         LIN_SCI_Frame *frame);

static void LIN_HLD_completeCurrentTransfer(LIN_Handle handle);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* Externs */
extern LIN_Config           gLinConfig[];
extern uint32_t             gLinConfigNum;
extern uint32_t             gLinDmaConfigNum;

extern LIN_DmaHandle        gLinDmaHandle[];
extern LIN_DmaChConfig      gLinDmaChCfg[];

/** \brief Driver Lock object */
static LIN_DrvLockObj       gLinDrvObj =
{
    .lock = NULL,
};

/* ========================================================================== */
/*                         API Function Definitions                           */
/* ========================================================================== */

void LIN_init(void)
{
    LIN_Object      *object;
    uint32_t        count;

    /* Init function for each config*/
    for(count = 0; count < gLinConfigNum; count++)
    {
        object = gLinConfig[count].object;
        /* Input parameter validation */
        if (object != NULL_PTR)
        {
            (void)memset(object, 0, sizeof(LIN_Object));
            /* Mark the object as available */
            object->isOpen = (bool)false;
        }
    }

    /* Create driver lock */
    (void)SemaphoreP_constructMutex(&gLinDrvObj.lockObj);
    gLinDrvObj.lock = &gLinDrvObj.lockObj;
}

void LIN_deinit(void)
{
    /* Delete driver lock */
    if(NULL != gLinDrvObj.lock)
    {
        SemaphoreP_destruct(&gLinDrvObj.lockObj);
        gLinDrvObj.lock = NULL;
    }
}

void LIN_Params_init(LIN_OpenParams *openParams)
{
    if (openParams != NULL)
    {
        /* Fill Structure with Default Values */
        *openParams = LIN_SCI_default_open_param;
    }
}

LIN_Handle LIN_open(uint32_t index, LIN_OpenParams *openParams)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Handle          handle = NULL;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    HwiP_Params         hwiPrms0, hwiPrms1;

    /* Check Whether index and openParams is valid */
    if ((index >= gLinConfigNum) || (openParams == NULL))
    {
        status = SystemP_FAILURE;
    }
    else
    {
        handle      = (LIN_Handle)&(gLinConfig[index]);
        object      = (LIN_Object *)handle->object;
        hwAttrs     = (LIN_HwAttrs const *)handle->hwAttrs;

        DebugP_assert(NULL_PTR != object);
        DebugP_assert(NULL_PTR != hwAttrs);

        if(object->isOpen == true)
        {
            status = SystemP_FAILURE;
            handle = NULL;
        }
        else
        {
            DebugP_assert(NULL != gLinDrvObj.lock);
            status = SemaphoreP_pend(&gLinDrvObj.lockObj, SystemP_WAIT_FOREVER);
        }
    }
    
    if (status == SystemP_SUCCESS)
    {
        /* Mark the handle as being used */
        object->isOpen = true;
        /* Store pointer to openParams structure in driver object */
        object->openParams = openParams;

        /* Disable All Global Interrupts */
        LIN_HLD_disableGlobalInterrupt0(hwAttrs->baseAddr);
        LIN_HLD_disableGlobalInterrupt1(hwAttrs->baseAddr);

        /* Clear All Global Interrupts */
        LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
        LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);

        /* Hard Reset Module */
        LIN_HLD_ModuleResetEnter(hwAttrs->baseAddr);
        LIN_HLD_ModuleResetExit(hwAttrs->baseAddr);

        /* Enter Soft Reset Mode */
        LIN_HLD_ModuleEnterSoftReset(hwAttrs->baseAddr);

        /* Configure with openParams */
        /* Check if mode is LIN or SCI */
        if (openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
        {
            /* Lin Mode Configurations */
            LIN_HLD_setModuleMode(hwAttrs->baseAddr, openParams->moduleMode);
            /* Set Lin Mode ( Commander / Responder ) LIN only */
            LIN_HLD_setLinOpMode(hwAttrs->baseAddr,
                                 openParams->linConfigParams.linMode);
            /* Set auto baud Rate configuration LIN Only */
            if(openParams->linConfigParams.adaptModeEnable)
            {
                LIN_HLD_enableAutoBaud(hwAttrs->baseAddr);
            }
            else
            {
                LIN_HLD_disableAutoBaud(hwAttrs->baseAddr);
            }
            /* Set Comm Mode */
            LIN_HLD_setCommMode(hwAttrs->baseAddr, openParams->commMode);
            /* Set Debug Configuration Mode */
            LIN_HLD_setDebugMode(hwAttrs->baseAddr, hwAttrs->debugMode);
            /* Set Mask Filtering Configuration */
            LIN_HLD_setMaskFilterType(hwAttrs->baseAddr,
                            openParams->linConfigParams.maskFilteringType);
            /* Mask Filter Configuration */
            LIN_HLD_setTxMask(hwAttrs->baseAddr,
                              openParams->linConfigParams.linTxMask);
            /* Mask Filter Configuration */
            LIN_HLD_setRxMask(hwAttrs->baseAddr,
                              openParams->linConfigParams.linRxMask);

            /* Set Checksum Type Configuration */
            LIN_HLD_setChecksumType(hwAttrs->baseAddr,
                            openParams->linConfigParams.checksumType);

            /* Set Loopback Configuration */
            if(hwAttrs->enableLoopback)
            {
                if(hwAttrs->loopBackMode == LIN_HLD_LOOPBACK_INTERNAL)
                {
                    LIN_HLD_enableInternalLoopback(hwAttrs->baseAddr);
                }
                else
                {
                    LIN_HLD_enableExternalLoopback(hwAttrs->baseAddr,
                                                   hwAttrs->loopBackType,
                                                   LIN_HLD_ANALOG_LOOP_RX);
                }
            }
            else
            {
                LIN_HLD_disableInternalLoopback(hwAttrs->baseAddr);
                LIN_HLD_disableExternalLoopback(hwAttrs->baseAddr);
            }

            /* Set Multi-buffer Mode */
            if(openParams->multiBufferMode)
            {
                LIN_HLD_enableMultiBufferMode(hwAttrs->baseAddr);
            }
            else
            {
                LIN_HLD_disableMultiBufferMode(hwAttrs->baseAddr);
            }

            /* SCI Configurations for LIN */
            /* SCI Parity Configuration */
            if(openParams->enableParity)
            {
                LIN_HLD_enableParity(hwAttrs->baseAddr);
            }
            else
            {
                LIN_HLD_disableParity(hwAttrs->baseAddr);
            }

            /* Set Baud Configuration Parameters */
            LIN_HLD_setBaudParams(hwAttrs->baseAddr,
                                  &(openParams->baudConfigParams));
            /* Set Max Baud Rate */
            LIN_HLD_setMaxBaudRate(hwAttrs->baseAddr, hwAttrs->linClk,
                                   openParams->linConfigParams.maxBaudRate);
            /* Enable Data Lines */
            LIN_HLD_enableDataTx(hwAttrs->baseAddr);
            LIN_HLD_enableDataRx(hwAttrs->baseAddr);
            /* Configure Sync Field */
            LIN_HLD_setSyncFields(hwAttrs->baseAddr,
                                  (uint8_t)openParams->linConfigParams.syncBreak,
                                  (uint8_t)openParams->linConfigParams.syncDelimiter);
            /* Set the trig Checksum bit */
            LIN_HLD_trigChecksumCompare(hwAttrs->baseAddr);
        }
        else
        {
            /* SCI mode Configurations */
            LIN_HLD_setModuleMode(hwAttrs->baseAddr, openParams->moduleMode);
            /* Set Comm Mode, Default Idle-line */
            LIN_HLD_setCommMode(hwAttrs->baseAddr, openParams->commMode);
            /* Set Debug configuration Mode TODO: can be made common */
            LIN_HLD_setDebugMode(hwAttrs->baseAddr, hwAttrs->debugMode);
            /* Set Loopback Configuration */
            /* Set Loopback Configuration */
            if(hwAttrs->enableLoopback)
            {
                if(hwAttrs->loopBackMode == LIN_HLD_LOOPBACK_INTERNAL)
                {
                    LIN_HLD_enableInternalLoopback(hwAttrs->baseAddr);
                }
                else
                {
                    // TODO Check if loopback path can be an option in syscfg
                    LIN_HLD_enableExternalLoopback(hwAttrs->baseAddr,
                                                   hwAttrs->loopBackType,
                                                   LIN_HLD_ANALOG_LOOP_RX);
                }
            }
            else
            {
                LIN_HLD_disableInternalLoopback(hwAttrs->baseAddr);
                LIN_HLD_disableExternalLoopback(hwAttrs->baseAddr);
            }

            /* Set Multi-buffer Mode */
            if(openParams->multiBufferMode)
            {
                LIN_HLD_enableMultiBufferMode(hwAttrs->baseAddr);
            }
            else
            {
                LIN_HLD_disableMultiBufferMode(hwAttrs->baseAddr);
            }

            /* SCI Configurations */
            /* SCI Parity Configuration */
            if(openParams->enableParity)
            {
                LIN_HLD_enableParity(hwAttrs->baseAddr);
            }
            else
            {
                LIN_HLD_disableParity(hwAttrs->baseAddr);
            }

            /* Set Parity Type SCI Mode only */
            LIN_HLD_setParityType(hwAttrs->baseAddr,
                                openParams->sciConfigParams.parityType);
            /* Set Stop Bits Configuration */
            LIN_HLD_setStopBits(hwAttrs->baseAddr,
                                openParams->sciConfigParams.stopBits);
            /* Set Data bits/char length Count SCI Mode only */
            LIN_HLD_setCharLength(hwAttrs->baseAddr,
                                openParams->sciConfigParams.dataBits);
            /* Set Frame length to 1 by default */
            LIN_HLD_setFrameLength(hwAttrs->baseAddr, 1U);

            /* Set Baud Configuration Parameters TODO: can be made common */
            LIN_HLD_setBaudParams(hwAttrs->baseAddr,
                                  &(openParams->baudConfigParams));

            /* Enable Data Lines */
            LIN_HLD_enableDataTx(hwAttrs->baseAddr);
            LIN_HLD_enableDataRx(hwAttrs->baseAddr);
        }

        /* Enable TX RX Pin */
        LIN_HLD_EnableTxRxPin(hwAttrs->baseAddr);
        DebugP_assert(status==SystemP_SUCCESS);

        /* Exit Soft Reset Mode */
        LIN_HLD_ModuleExitSoftReset(hwAttrs->baseAddr);

        /* Register Interrupts if Enabled */
        if (hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
        {
            /* Initialize with defaults */
            HwiP_Params_init(&hwiPrms0);
            hwiPrms0.intNum     = hwAttrs->intrNum0;
            hwiPrms0.priority   = hwAttrs->intrPriority;
            hwiPrms0.callback   = &LIN_hwiFxn0;
            hwiPrms0.args       = (void *)handle;
            hwiPrms0.isPulse    = 0;
            status = HwiP_construct(&object->hwiObj0, &hwiPrms0);
            DebugP_assert(status==SystemP_SUCCESS);

            /* Initialize with defaults */
            HwiP_Params_init(&hwiPrms1);
            hwiPrms1.intNum     = hwAttrs->intrNum1;
            hwiPrms1.priority   = hwAttrs->intrPriority;
            hwiPrms1.callback   = &LIN_hwiFxn1;
            hwiPrms1.args       = (void *)handle;
            hwiPrms1.isPulse    = 0;
            status = HwiP_construct(&object->hwiObj1, &hwiPrms1);
            DebugP_assert(status==SystemP_SUCCESS);

            if(object->openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
            {
                /* LIN Mode Operation */
                LIN_HLD_setInterruptLevel0( hwAttrs->baseAddr,
                                            LIN_INT_LVL_BE      |
                                            LIN_INT_LVL_PBE     |
                                            LIN_INT_LVL_CE      |
                                            LIN_INT_LVL_ISFE    |
                                            LIN_INT_LVL_NRE     |
                                            LIN_INT_LVL_FE      |
                                            LIN_INT_LVL_OE      |
                                            LIN_INT_LVL_PE      |
                                            LIN_INT_LVL_TO      );

                LIN_HLD_setInterruptLevel1( hwAttrs->baseAddr,
                                            LIN_INT_LVL_TX      |
                                            LIN_INT_LVL_RX      |
                                            LIN_INT_LVL_ID      );

                LIN_HLD_enableGlobalInterrupt0(hwAttrs->baseAddr);
                LIN_HLD_enableGlobalInterrupt1(hwAttrs->baseAddr);
            }
            else
            {
                /* SCI Mode of Operation */

            }
        }
        else if (hwAttrs->opMode == LIN_OPER_MODE_DMA)
        {
            /* Assign DMA Handle to Driver */
            object->linDmaHandle = gLinDmaHandle[index];
            /* Assign DMA Channel to Driver */
            object->dmaChCfg = gLinDmaChCfg[index];

            /* Open DMA and Configure Channel */
            (void)LIN_dmaOpen(handle, object->dmaChCfg);

            /* LIN mode specific Configurations for DMA */
            if(object->openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
            {
                /* Register Error Interrupt */
                HwiP_Params_init(&hwiPrms0);
                hwiPrms0.intNum     = hwAttrs->intrNum0;
                hwiPrms0.priority   = hwAttrs->intrPriority;
                hwiPrms0.callback   = &LIN_hwiFxn0;
                hwiPrms0.args       = (void *)handle;
                hwiPrms0.isPulse    = 0;
                status = HwiP_construct(&object->hwiObj0, &hwiPrms0);
                DebugP_assert(status==SystemP_SUCCESS);

                /* Set Interrupt Levels */
                LIN_HLD_setInterruptLevel0( hwAttrs->baseAddr,
                                            LIN_INT_LVL_BE      |
                                            LIN_INT_LVL_PBE     |
                                            LIN_INT_LVL_CE      |
                                            LIN_INT_LVL_ISFE    |
                                            LIN_INT_LVL_NRE     |
                                            LIN_INT_LVL_FE      |
                                            LIN_INT_LVL_OE      |
                                            LIN_INT_LVL_PE      |
                                            LIN_INT_LVL_TO      );

                 LIN_HLD_enableGlobalInterrupt0(hwAttrs->baseAddr);
            }
            else
            {
                /* SCI Mode of Operation */
            }
        }
        else { /* No Code */ }

        /* Construct Semaphore Objects if necessary */
        if( (hwAttrs->opMode != LIN_OPER_MODE_POLLING) &&
            (object->openParams->transferMode == LIN_TRANSFER_MODE_BLOCKING))
        {
                status = SemaphoreP_constructBinary(&object->readFrmCompSemObj,
                                                                            0U);
                DebugP_assert(status==SystemP_SUCCESS);
                status = SemaphoreP_constructBinary(&object->writeFrmCompSemObj,
                                                                            0U);
                DebugP_assert(status==SystemP_SUCCESS);
        }

        /*
        * Construct thread safe handles for this LIN Instance
        * Mutex to provide exclusive access to the LIN Instance
        */
        status = SemaphoreP_constructMutex(&object->mutex);
        DebugP_assert(status==SystemP_SUCCESS);

        if(handle != NULL)
        {
            object->state = LIN_HLD_STATE_IDLE;
        }
    }

    SemaphoreP_post(&gLinDrvObj.lockObj);

    /* Return the handle */
    return (handle);
}

void LIN_close(LIN_Handle handle)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;

    /* Input parameter validation */
    if (handle != NULL)
    {
        /* Get the pointer to the object */
        object = (LIN_Object*)handle->object;
        hwAttrs = (LIN_HwAttrs const *)handle->hwAttrs;

        DebugP_assert(NULL != gLinDrvObj.lock);
        status = SemaphoreP_pend(&gLinDrvObj.lockObj, SystemP_WAIT_FOREVER);

        DebugP_assert(status == SystemP_SUCCESS);

        /* Reset Module */
        LIN_HLD_ModuleResetEnter(hwAttrs->baseAddr);
        LIN_HLD_ModuleResetExit(hwAttrs->baseAddr);

        if (hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
        {
            /* Destruct the Hwi */
            HwiP_destruct(&object->hwiObj0);
            HwiP_destruct(&object->hwiObj1);

            if(object->openParams->transferMode == LIN_TRANSFER_MODE_BLOCKING)
            {
                (void)SemaphoreP_destruct(&object->readFrmCompSemObj);
                (void)SemaphoreP_destruct(&object->writeFrmCompSemObj);
            }
        }
        else if(hwAttrs->opMode == LIN_OPER_MODE_DMA)
        {
            /* Destruct the Hwi */
            HwiP_destruct(&object->hwiObj0);

            /* Close DMA and deallocate Resources */
            (void)LIN_dmaClose(handle);

            if(object->openParams->transferMode == LIN_TRANSFER_MODE_BLOCKING)
            {
                (void)SemaphoreP_destruct(&object->readFrmCompSemObj);
                (void)SemaphoreP_destruct(&object->writeFrmCompSemObj);
            }
        }
        else { /* No Code */ }

        if(hwAttrs->opMode != LIN_OPER_MODE_POLLING)
        {
            if(object->openParams->transferMode == LIN_TRANSFER_MODE_BLOCKING)
            {
                (void)SemaphoreP_destruct(&object->readFrmCompSemObj);
                (void)SemaphoreP_destruct(&object->writeFrmCompSemObj);
            }
        }

        /* Destruct the instance lock */
        SemaphoreP_destruct(&object->mutex);

        object->state = LIN_HLD_STATE_RESET;
        object->openParams = NULL;
        object->isOpen = false;

        SemaphoreP_post(&gLinDrvObj.lockObj);
    }
}

void LIN_SCI_Frame_init(LIN_SCI_Frame *frame)
{
    /* Fill Structure with Default Values */
    *frame = LIN_SCI_defaultFrame;
}

int32_t LIN_SCI_transferFrame(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;

    if ((handle == NULL) || (frame == NULL))
    {
        status = SystemP_FAILURE;
    }

    if(status == SystemP_SUCCESS)
    {
        /* Check Frame parameters for bad values */
    }

    if(status == SystemP_SUCCESS)
    {
        /* Get the pointer to the Driver Object */
        object = (LIN_Object *)handle->object;

        if(object->state == LIN_HLD_STATE_ERROR)
        {
            /* Last Transaction ended with an Error Case */
            /* TODO: If Required Check Error and take action in IP */


            /* Turn State back to IDLE */
            object->state = LIN_HLD_STATE_IDLE;
        }
        else if((object->state == LIN_HLD_STATE_RESET) ||
                (object->state == LIN_HLD_STATE_BUSY))
        {
            status = SystemP_FAILURE;
        }
        else { /* No Code */ }
    }

    if(status == SystemP_SUCCESS)
    {
        /* Lock Mutex */
        (void)SemaphoreP_pend(&object->mutex, SystemP_WAIT_FOREVER);

        if (frame->txnType == LIN_HLD_TXN_TYPE_WRITE)
        {
            status = LIN_SCI_writeFrame(handle, frame);
        }
        else
        {
            status = LIN_SCI_readFrame(handle, frame);
        }

        /* Unlock Mutex */
        SemaphoreP_post(&object->mutex);
    }

    return status;
}

static int32_t LIN_SCI_writeFrame(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    int32_t             retVal = 0;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;

    /* Get the pointer to the object */
    object = (LIN_Object *)handle->object;
    hwAttrs = (LIN_HwAttrs const *)handle->hwAttrs;

    if(object->openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
    {
        /* Module Configured in LIN Mode */
        if(object->openParams->linConfigParams.linMode ==
                                        LIN_MODE_HLD_LIN_COMMANDER)
        {
            /* LIN Node Configured as Commander/Master */
            if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
            {
                /* Polling Mode Selected */
                status = LIN_WriteLinFrameCommanderPolling(handle, frame);
            }
            else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
            {
                /* Interrupt Mode Selected */
                status = LIN_WriteLinFrameCommanderInterrupt(handle, frame);

                if(status == SystemP_SUCCESS)
                {
                    if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                    {
                        retVal = SemaphoreP_pend(&(object->writeFrmCompSemObj),
                                                 frame->timeout);

                        if(retVal != SystemP_SUCCESS)
                        {
                            /* Semaphore Timed Out */
                            status = SystemP_TIMEOUT;
                        }
                        else
                        {
                            /* Semaphore Got posted before Timeout */
                            if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                            {
                                status = SystemP_FAILURE;
                            }
                        }
                    }
                    else { /* No Code (Nothing to do in Callback Mode) */ }
                }
            }
            else
            {
                /* DMA Mode Selected */
                status = LIN_WriteLinFrameCommanderDMA(handle, frame);

                if(status == SystemP_SUCCESS)
                {
                    if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                    {
                        retVal = SemaphoreP_pend(&(object->writeFrmCompSemObj),
                                                 frame->timeout);

                        if(retVal != SystemP_SUCCESS)
                        {
                            /* Semaphore Timed Out */
                            status = SystemP_TIMEOUT;
                        }
                        else
                        {
                            /* Semaphore Got posted before Timeout */
                            if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                            {
                                status = SystemP_FAILURE;
                            }
                        }
                    }
                    else { /* No Code (Nothing to do in Callback Mode) */ }
                }
            }
        }
        else
        {
            /* LIN Node Configured as Responder/Slave */
            if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
            {
                /* Polling Mode Selected */
                status = LIN_WriteLinFrameResponderPolling(handle, frame);
            }
            else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
            {
                /* Interrupt Mode Selected */
                status = LIN_WriteLinFrameResponderInterrupt(handle, frame);

                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->writeFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode )*/ }
            }
            else
            {
                /* DMA Mode Selected */
                status = LIN_WriteLinFrameResponderDMA(handle, frame);

                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->writeFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode )*/ }
            }
        }
    }
    else
    {
        /* Module Configured in SCI Mode */
        if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
        {
            /* Polling Mode Selected */
            status = LIN_WriteSciFramePolling(handle, frame);
        }
        else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
        {
            /* Interrupt Mode Selected */
            status = LIN_WriteSciFrameInterrupt(handle, frame);
        }
        else
        {
            /* DMA Mode Selected */
            status = LIN_WriteSciFrameDMA(handle, frame);
        }
    }

    return status;
}

static int32_t LIN_SCI_readFrame(LIN_Handle handle, LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    int32_t             retVal = 0;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;

    /* Get the pointer to the object */
    object = (LIN_Object *)handle->object;
    hwAttrs = (LIN_HwAttrs const *)handle->hwAttrs;

    if(object->openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
    {
        /* Module Configured in LIN Mode */
        if(object->openParams->linConfigParams.linMode ==
                                        LIN_MODE_HLD_LIN_COMMANDER)
        {
            /* LIN Node Configured as Commander/Master */
            if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
            {
                /* Polling Mode Selected */
                status = LIN_ReadLinFrameCommanderPolling(handle, frame);
            }
            else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
            {
                /* Interrupt Mode Selected */
                status = LIN_ReadLinFrameCommanderInterrupt(handle, frame);

                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->readFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode ) */ }
            }
            else
            {
                /* DMA Mode Selected */
                status = LIN_ReadLinFrameCommanderDMA(handle, frame);

                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->readFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode ) */ }
            }
        }
        else
        {
            /* LIN Node Configured as Responder/Slave */
            if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
            {
                /* Polling Mode Selected */
                status = LIN_ReadLinFrameResponderPolling(handle, frame);
            }
            else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
            {
                /* Interrupt Mode Selected */
                status = LIN_ReadLinFrameResponderInterrupt(handle, frame);

                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->readFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode )*/ }
            }
            else
            {
                /* DMA Mode Selected */
                status = LIN_ReadLinFrameResponderDMA(handle, frame);
                if(object->openParams->transferMode ==
                                                    LIN_TRANSFER_MODE_BLOCKING)
                {
                    retVal = SemaphoreP_pend(   &(object->readFrmCompSemObj),
                                                frame->timeout);

                    if(retVal != SystemP_SUCCESS)
                    {
                        /* Semaphore Timed out */
                        status = SystemP_TIMEOUT;
                    }
                    else
                    {
                        /* Semaphore Got posted before Timeout */
                        if(frame->status !=  LIN_HLD_TXN_STS_SUCCESS)
                        {
                            status = SystemP_FAILURE;
                        }
                    }
                }
                else { /* No Code ( Nothing to do in Callback Mode )*/ }
            }
        }
    }
    else
    {
        /* Module Configured in SCI Mode */
        if(hwAttrs->opMode == LIN_OPER_MODE_POLLING)
        {
            /* Polling Mode Selected */
            status = LIN_ReadSciFramePolling(handle, frame);
        }
        else if(hwAttrs->opMode == LIN_OPER_MODE_INTERRUPT)
        {
            /* Interrupt Mode Selected */
            status = LIN_ReadSciFrameInterrupt(handle, frame);
        }
        else
        {
            /* DMA Mode Selected */
            status = LIN_ReadSciFrameDMA(handle, frame);
        }
    }

    return status;
}

/* ========================================================================== */
/*                         ISR Function Definitions                           */
/* ========================================================================== */

/*
 *  Hwi interrupt handler 0 to service the LIN Peripheral interrupt 0
 */
static void LIN_hwiFxn0 (void* arg)
{
    LIN_Handle          handle = (LIN_Handle)arg;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            intrVect0Offset = 0U;

    /* Input parameter validation */
    if (handle != NULL)
    {
        /* Get the pointer to the object */
        object = (LIN_Object *)handle->object;
        hwAttrs = (LIN_HwAttrs *)handle->hwAttrs;

        object->state = LIN_HLD_STATE_ERROR;

        intrVect0Offset = LIN_HLD_getIntVect0ffset(hwAttrs->baseAddr);

        LIN_HLD_disableInterrupt(hwAttrs->baseAddr, LIN_INT_LVL_ALL);

        switch(intrVect0Offset)
        {
            case LIN_VECT_ISFE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_STS_FAILURE;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_PE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_PARITY_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_PBE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_FE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_FRAMING_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_CE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_CHECKSUM_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_BE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_BIT_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_OE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_OVERRUN_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_NRE:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_NO_RES_ERR;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            case LIN_VECT_TO:
            {
                /* Set Status based on Highest Priority interrupt */
                object->currentTxnFrame->status = LIN_HLD_TXN_STS_TIMEOUT;
                /* Clear Global interrupt 1 */
                LIN_HLD_clearGlobalInterrupt0(hwAttrs->baseAddr);
            }
            break;

            default:
            break;
        }

        /* Complete transfer */

        /* Disable all interrupts */
        LIN_HLD_disableInterrupt(hwAttrs->baseAddr, LIN_INT_LVL_ALL);
        /* Clear all interrupt from interrupt line 1 */
        LIN_HLD_clearInterrupt(hwAttrs->baseAddr, LIN_INTR_STS_MASK_ALL);
        /* Clear Global interrupt flag for interrupt line for interrupt line 1 */
        LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);

        /* If Blocking Post Semaphore */
        /* If Callback Call the Callback */
        LIN_HLD_completeCurrentTransfer(handle);
    }

    return;
}

/*
 *  Hwi interrupt handler 1 to service the LIN Peripheral interrupt 1
 */
static void LIN_hwiFxn1 (void* arg)
{
    LIN_Handle          handle = (LIN_Handle)arg;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            intrVect1offset = 0U;

    /* Input parameter validation */
    if (handle != NULL)
    {
        /* Get the pointer to the object */
        object = (LIN_Object *)handle->object;
        hwAttrs = (LIN_HwAttrs *)handle->hwAttrs;

        if(object->openParams->moduleMode == LIN_MODULE_OP_MODE_LIN)
        {
            intrVect1offset = LIN_HLD_getIntVect1ffset(hwAttrs->baseAddr);

            switch(intrVect1offset)
            {
                case LIN_VECT_TX:
                {
                    /* Multi buffer mode of operation */
                    if(object->openParams->multiBufferMode == true)
                    {
                        /* Multibuffer Mode of operation */
                        /* Disable Interrupts */
                        if(object->openParams->linConfigParams.linMode ==
                                                    LIN_MODE_HLD_LIN_RESPONDER)
                        {
                            LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                        LIN_INT_LVL_ID      |
                                                        LIN_INT_LVL_TX      |
                                                        LIN_INT_LVL_PBE     |
                                                        LIN_INT_LVL_BE      |
                                                        LIN_INT_LVL_PE      |
                                                        LIN_INT_LVL_ISFE    |
                                                        LIN_INT_LVL_FE      );
                        }
                        else
                        {
                            LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                        LIN_INT_LVL_TX  |
                                                        LIN_INT_LVL_PBE |
                                                        LIN_INT_LVL_BE  );
                        }

                        /* Clear All Interrupts (SCIFLR) */
                        LIN_HLD_clearInterrupt(hwAttrs->baseAddr,
                                               LIN_INTR_STS_MASK_ALL);
                        /* Clear Global interrupt 1 */
                        LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);
                        /* TODO: Check if Global interrupt 0 should also be cleared */
                        LIN_HLD_completeCurrentTransfer(handle);
                    }
                    else
                    {
                        /* Single Buffer mode */
                        if(object->writeCountIdx != 0U)
                        {
                            LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr,
                                                     object->writeBufIdx, 1U);
                            object->writeBufIdx++;
                            object->writeCountIdx--;

                            /* Clear The TXRDY Interrupt */
                            LIN_HLD_clearInterrupt( hwAttrs->baseAddr,
                                                    LIN_FLAG_MASK_TXRDY);
                            /* Clear Global interrupt 1 */
                            LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);
                        }
                        else
                        {
                            /* Disable interrupts */
                            if(object->openParams->linConfigParams.linMode ==
                                                    LIN_MODE_HLD_LIN_RESPONDER)
                            {
                                LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                        LIN_INT_LVL_ID      |
                                                        LIN_INT_LVL_TX      |
                                                        LIN_INT_LVL_PBE     |
                                                        LIN_INT_LVL_BE      |
                                                        LIN_INT_LVL_ISFE    |
                                                        LIN_INT_LVL_FE      );
                            }
                            else
                            {
                                LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                         LIN_INT_LVL_TX  |
                                                         LIN_INT_LVL_PBE |
                                                         LIN_INT_LVL_BE  );
                            }

                            /* Clear All Interrupts(SCIFLR) */
                            LIN_HLD_clearInterrupt( hwAttrs->baseAddr,
                                                    LIN_INTR_STS_MASK_ALL);
                            /* Clear Global interrupt 1 */
                            LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);
                            /* TODO: Check if Global interrupt 0 should also be cleared */
                            LIN_HLD_completeCurrentTransfer(handle);
                        }
                    }
                }
                break;

                case LIN_VECT_RX:
                {
                    if(object->openParams->multiBufferMode == true)
                    {
                        /* Multibuffer Mode of Operation */
                        if( object->openParams->linConfigParams.linMode ==
                            LIN_MODE_HLD_LIN_RESPONDER)
                        {
                            /*  When Node is Configured as a Responder and
                                ID4ID5 based length count is selected */
                            if( object->openParams->commMode ==
                                LIN_COMM_HLD_LIN_ID4ID5LENCTL)
                            {
                                if(object->currentTxnFrame->id <=
                                                LIN_ID_DATA_LEN_2_RANGE_HIGH)
                                {
                                    object->readCountIdx =
                                                        LIN_FRAME_FIXED_LEN_2U;
                                }
                                else if(object->currentTxnFrame->id <=
                                                LIN_ID_DATA_LEN_4_RANGE_HIGH)
                                {
                                    object->readCountIdx =
                                                        LIN_FRAME_FIXED_LEN_4U;
                                }
                                else /* 0x30 - 0x3F */
                                {
                                    object->readCountIdx =
                                                        LIN_FRAME_FIXED_LEN_8U;
                                }
                            }
                            else { /* No Code */ }

                            if( object->currentTxnFrame->txnType ==
                                LIN_HLD_TXN_TYPE_READ)
                            {
                                if(object->openParams->enableParity == true)
                                {
                                    object->currentTxnFrame->id =
                                    (LIN_HLD_getReceivedID(hwAttrs->baseAddr) &
                                    (LIN_FRAME_ID_BIT_MASK));
                                }
                                else
                                {
                                    object->currentTxnFrame->id =
                                    LIN_HLD_getReceivedID(hwAttrs->baseAddr);
                                }
                            }
                            else { /* No Code */ }
                        }
                        else { /* No Code */ }

                        /* Store Data */
                        LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                                object->readBufIdx,
                                                (uint8_t)object->readCountIdx);
                        /* Disable Interrupts */
                        LIN_HLD_disableInterrupt(   hwAttrs->baseAddr,
                                                    LIN_INT_LVL_RX  |
                                                    LIN_INT_LVL_PBE |
                                                    LIN_INT_LVL_NRE |
                                                    LIN_INT_LVL_FE  |
                                                    LIN_INT_LVL_PE  |
                                                    LIN_INT_LVL_OE  |
                                                    LIN_INT_LVL_CE  |
                                                    LIN_INT_LVL_BE  );

                        /* Clear All Interrupts (SCIFLR) */
                        LIN_HLD_clearInterrupt(hwAttrs->baseAddr,
                                               LIN_INTR_STS_MASK_ALL);
                        /* Clear Global interrupt 1 */
                        LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);
                        /* TODO: Check if Global interrupt 0 should also be cleared */
                        LIN_HLD_completeCurrentTransfer(handle);
                    }
                    else
                    {
                        /* Single Buffer Mode */
                        if(object->readCountIdx != 0U)
                        {
                            LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr, object->readBufIdx, 1U);
                            object->readBufIdx++;
                            object->readCountIdx--;

                            /* Clear The TXRDY Interrupt */
                            LIN_HLD_clearInterrupt(hwAttrs->baseAddr, LIN_FLAG_MASK_RXRDY);
                            /* Clear Global Interrupt 1 */
                            LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);

                            if(object->readCountIdx == 0U)
                            {
                                /* Disable Interrupts */

                                if(object->openParams->linConfigParams.linMode
                                                == LIN_MODE_HLD_LIN_RESPONDER)
                                {
                                    LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                            LIN_INT_LVL_ID  |
                                                            LIN_INT_LVL_RX  |
                                                            LIN_INT_LVL_PBE |
                                                            LIN_INT_LVL_FE  |
                                                            LIN_INT_LVL_PE  |
                                                            LIN_INT_LVL_OE  |
                                                            LIN_INT_LVL_CE  |
                                                            LIN_INT_LVL_BE  );
                                }
                                else
                                {
                                    LIN_HLD_disableInterrupt(hwAttrs->baseAddr,
                                                            LIN_INT_LVL_RX  |
                                                            LIN_INT_LVL_PBE |
                                                            LIN_INT_LVL_NRE |
                                                            LIN_INT_LVL_FE  |
                                                            LIN_INT_LVL_PE  |
                                                            LIN_INT_LVL_OE  |
                                                            LIN_INT_LVL_CE  |
                                                            LIN_INT_LVL_BE  );
                                }

                                /* Clear All Interrupts(SCIFLR) */
                                LIN_HLD_clearInterrupt( hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);
                                /* Clear Global interrupt 1 */
                                LIN_HLD_clearGlobalInterrupt1(
                                                            hwAttrs->baseAddr);
                                /* TODO: Check if Global interrupt 0 should also be cleared */
                                LIN_HLD_completeCurrentTransfer(handle);
                            }
                        }
                        else { /* No Code */ }
                    }
                }
                break;

                case LIN_VECT_ID:
                {
                    if(object->currentTxnFrame != NULL)
                    {
                        if(object->currentTxnFrame->txnType ==
                                                        LIN_HLD_TXN_TYPE_READ)
                        {
                            if(object->openParams->enableParity == true)
                            {
                                object->currentTxnFrame->id =
                                (LIN_HLD_getReceivedID(hwAttrs->baseAddr) &
                                (LIN_FRAME_ID_BIT_MASK));
                            }
                            else
                            {
                                object->currentTxnFrame->id =
                                LIN_HLD_getReceivedID(hwAttrs->baseAddr);
                            }
                        }
                        else
                        {
                            /* Set the response Length */
                            LIN_HLD_setFrameLength( hwAttrs->baseAddr,
                                                    (uint8_t)object->writeCountIdx);

                            /* Callback Function */

                            /* Write data in multibuffer Mode */
                            if(object->openParams->multiBufferMode == true)
                            {
                                LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr,
                                                         object->writeBufIdx,
                                                         (uint8_t)object->writeCountIdx);
                            }
                        }
                    }
                    else { /* No Code */ }

                    /* Clear The TXRDY Interrupt */
                    LIN_HLD_clearInterrupt(hwAttrs->baseAddr,
                                                            LIN_FLAG_MASK_RXID);
                    /* Clear Global Interrupt 1 */
                    LIN_HLD_clearGlobalInterrupt1(hwAttrs->baseAddr);
                }
                break;

                default:
                break;

            }
        }
    }

    return;
}

/* Internal Support Function */


static int32_t LIN_WriteLinFrameCommanderPolling(LIN_Handle handle,
                                                 LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;
    uint32_t            intrStatus = 0x00U;
    uint8_t             writeCount = 0U;
    uint8_t             *writeIndex = NULL;
    uint32_t            startTicks = 0U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    /* Store start Time */
    startTicks = ClockP_getTicks();

    /* Generate Parity ID if parity is Enabled */
    if(object->openParams->enableParity == true)
    {
        id = LIN_HLD_gen_ParityID(frame->id);
    }
    else
    {
        id = frame->id;
    }

    /* Set Object State to BUSY */
    object->state = LIN_HLD_STATE_BUSY;

    /* Enable data Transmission in case it was disabled */
    LIN_HLD_enableDataTx(hwAttrs->baseAddr);

    if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
    {
        /* Set The Frame Length */
        LIN_HLD_setFrameLength(hwAttrs->baseAddr, frame->frameLen);
    }

    if(object->openParams->multiBufferMode == true)
    {
        /* Multibuffer Mode is enabled, Write All the data bytes in buffer */
        LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr, (uint8_t *)frame->dataBuf,
                                 frame->frameLen);
        /* Set Message ID to initiate a header transmission */
        /* Data will be transmitted automatically followed by the Header */
        LIN_HLD_setIDByte(hwAttrs->baseAddr, id);

        /* Wait for transmission to complete */
        while(!LIN_HLD_isTxBufferEmpty(hwAttrs->baseAddr))
        {
            intrStatus = LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);
            if ((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
            {
                status = SystemP_FAILURE;
                frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
            }
            else if ((intrStatus & LIN_FLAG_MASK_BE) != 0U)
            {
                status = SystemP_FAILURE;
                frame->status = LIN_HLD_TXN_BIT_ERR;
            }
            else { /* No Code */ }

            if (frame->status != LIN_HLD_TXN_STS_SUCCESS)
            {
                break;
            }
            else
            {
                continue;
            }
        }
    }
    else
    {
        /* Store Write parameters */
        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            writeCount = frame->frameLen;
        }
        else
        {
            if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
            {
                writeCount = LIN_FRAME_FIXED_LEN_2U;
            }
            else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
            {
                writeCount = LIN_FRAME_FIXED_LEN_4U;
            }
            else /* 0x30 - 0x3F */
            {
                writeCount = LIN_FRAME_FIXED_LEN_8U;
            }
        }

        writeIndex = (uint8_t *)frame->dataBuf;

        /* Set Message ID to initiate a header transmission */
        LIN_HLD_setIDByte(hwAttrs->baseAddr, id);

        while ((status == SystemP_SUCCESS) && (writeCount != 0U))
        {
            while(status == SystemP_SUCCESS)
            {
                intrStatus = LIN_HLD_getInterruptStatusSCIFLR(
                                                        hwAttrs->baseAddr,
                                                        LIN_FLAG_MASK_TXRDY  |
                                                        LIN_FLAG_MASK_PBE    );
                if((intrStatus & LIN_FLAG_MASK_TXRDY) != 0U)
                {
                    LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr, writeIndex, 1U);
                    writeIndex++;
                    writeCount--;
                    break;
                }
                else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_BIT_ERR;
                }
                else
                {
                    /* No Code */
                }

                /* Check Time */
                if ((ClockP_getTicks() - startTicks) >
                    (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                {
                    status = SystemP_TIMEOUT;
                    frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                }
            }
        }

        /* Wait for transmission completion */
        while(!LIN_HLD_isTxBufferEmpty(hwAttrs->baseAddr))
        {
            /* Check Time */
            if ((ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
            {
                status = SystemP_TIMEOUT;
                frame->status = LIN_HLD_TXN_STS_TIMEOUT;
            }
        }
    }

    if(status == SystemP_SUCCESS)
    {
        /* Set Object state to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
    }
    else
    {
        object->state = LIN_HLD_STATE_ERROR;
    }

    return status;
}

static int32_t LIN_WriteLinFrameCommanderInterrupt(LIN_Handle handle,
                                                   LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

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
            /* Multibuffer Mode, Write All the data bytes in buffer */
            LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr,
                                    (uint8_t *)object->currentTxnFrame->dataBuf,
                                    object->currentTxnFrame->frameLen);

            object->writeCountIdx = object->currentTxnFrame->frameLen;
            object->writeBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

            /* Enable necessary Interrupts */
            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_TX  |
                                                        LIN_INT_LVL_PBE |
                                                        LIN_INT_LVL_BE  );

            /* Set Message ID to initiate a header transmission */
            /* Data will be transmitted automatically followed by the Header */
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
                else if(object->currentTxnFrame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
                {
                    object->writeCountIdx = LIN_FRAME_FIXED_LEN_4U;
                }
                else /* 0x30 - 0x3F */
                {
                    object->writeCountIdx = LIN_FRAME_FIXED_LEN_8U;
                }
            }

            object->writeBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

            LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr, object->writeBufIdx,
                                     1U);
            object->writeBufIdx++;
            object->writeCountIdx--;


            /* Enable necessary Interrupts */
            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_TX  |
                                                        LIN_INT_LVL_PBE |
                                                        LIN_INT_LVL_BE  );

            /* Set Message ID to initiate a header transmission */
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

static int32_t LIN_WriteLinFrameResponderPolling(LIN_Handle handle,
                                                 LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            intrStatus = 0x00U;
    uint32_t            startTicks = 0U;
    uint8_t             writeCount = 0U;
    uint8_t             *writeIndex = NULL;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    startTicks = ClockP_getTicks();

    /* Set Object State to Busy */
    object->state = LIN_HLD_STATE_BUSY;

    /* Check of HGENCTRL bit, if it is set write ID-slaveTask Byte */
    if (object->openParams->linConfigParams.maskFilteringType ==
                                LIN_HLD_MSG_FILTER_IDRESPONDER)
    {
        LIN_HLD_setIDSlaveTaskByte(hwAttrs->baseAddr, frame->id);
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

    /* Wait for RxID Match */
    while(status == SystemP_SUCCESS)
    {
        intrStatus = LIN_HLD_getInterruptStatusSCIFLR(  hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);

        if(( intrStatus & ( LIN_FLAG_MASK_RXID  |
                            LIN_FLAG_MASK_FE    |
                            LIN_FLAG_MASK_PE    |
                            LIN_FLAG_MASK_PBE   |
                            LIN_FLAG_MASK_BE    |
                            LIN_INT_LVL_ISFE    )) != 0U )
        {
            break;
        } else { /* No Code */ }

        /* Check Time */
        if ((ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
        {
            status = SystemP_TIMEOUT;
            frame->status = LIN_HLD_TXN_STS_TIMEOUT;

        } else { /* No Code */ }
    }

    if(object->openParams->multiBufferMode == true)
    {
        /* Check status */
        if(status == SystemP_SUCCESS)
        {
            if((intrStatus & LIN_FLAG_MASK_RXID) != 0U)
            {
                if(object->openParams->commMode ==
                                                LIN_COMM_HLD_LIN_USELENGTHVAL)
                {
                    /* Set Frame Length to respond with */
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, frame->frameLen);
                }
                else
                {
                    if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_2U);
                    }
                    else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_4U);
                    }
                    else /* 0x30 - 0x3F */
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_8U);
                    }
                }

                /** Write data in the data buffer */
                LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr,
                                         (uint8_t *)frame->dataBuf,
                                         frame->frameLen);

                /** Wait for TX Completion */
                while(!LIN_HLD_isTxBufferEmpty(hwAttrs->baseAddr))
                {
                    if ((ClockP_getTicks() - startTicks) >
                    (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                    {
                        status = SystemP_TIMEOUT;
                        frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                    }
                }
            }
            else
            {
                /* Handle Error and set Transaction Status */
                status = SystemP_FAILURE;

                if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                {
                    /* Set Transaction Status */
                    frame->status = LIN_HLD_TXN_FRAMING_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                {
                    /* Set Transaction Status */
                    frame->status = LIN_HLD_TXN_PARITY_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                {
                    /* Set Transaction Status */
                    frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                {
                    /* Set Transaction Status */
                    frame->status = LIN_HLD_TXN_BIT_ERR;
                }
                else
                {
                    /* Set Transaction Status */
                    /* ISF Error */
                    frame->status = LIN_HLD_TXN_STS_FAILURE;
                }
            }
        }
        else { /* No Code */ }
    }
    else
    {
        /* Single Buffer Mode of Operation */
        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            writeCount = frame->frameLen;
        }
        else
        {
            if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
            {
                writeCount = LIN_FRAME_FIXED_LEN_2U;
            }
            else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
            {
                writeCount = LIN_FRAME_FIXED_LEN_4U;
            }
            else /* 0x30 - 0x3F */
            {
                writeCount = LIN_FRAME_FIXED_LEN_8U;
            }
        }

        writeIndex = (uint8_t *)frame->dataBuf;

        if(status == SystemP_SUCCESS)
        {
            if((intrStatus & LIN_FLAG_MASK_RXID) != 0U)
            {
                if(object->openParams->commMode ==
                                                LIN_COMM_HLD_LIN_USELENGTHVAL)
                {
                    /* Set Frame Length to respond with */
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, frame->frameLen);
                }
                else
                {
                    if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_2U);
                    }
                    else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_4U);
                    }
                    else /* 0x30 - 0x3F */
                    {
                        LIN_HLD_setFrameLength(hwAttrs->baseAddr,
                                                        LIN_FRAME_FIXED_LEN_8U);
                    }
                }

                while ((status == SystemP_SUCCESS) && (writeCount != 0U))
                {
                    while(status == SystemP_SUCCESS)
                    {
                        intrStatus = LIN_HLD_getInterruptStatusSCIFLR(
                                                        hwAttrs->baseAddr,
                                                        LIN_FLAG_MASK_TXRDY |
                                                        LIN_FLAG_MASK_BE    |
                                                        LIN_FLAG_MASK_FE    |
                                                        LIN_FLAG_MASK_OE    |
                                                        LIN_FLAG_MASK_PBE   );

                        if((intrStatus & LIN_FLAG_MASK_TXRDY) != 0U)
                        {
                            LIN_HLD_writeLinTxBuffer(hwAttrs->baseAddr,
                                                     writeIndex, 1U);
                            writeIndex++;
                            writeCount--;
                            break;
                        }
                        else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                        {
                            status = SystemP_FAILURE;
                            frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                        }
                        else if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                        {
                            status = SystemP_FAILURE;
                            frame->status = LIN_HLD_TXN_FRAMING_ERR;
                        }
                        else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                        {
                            status = SystemP_FAILURE;
                            frame->status = LIN_HLD_TXN_BIT_ERR;
                        }
                        else if((intrStatus & LIN_FLAG_MASK_OE) != 0U)
                        {
                            status = SystemP_FAILURE;
                            frame->status = LIN_HLD_TXN_OVERRUN_ERR;
                        }
                        else
                        {
                            /* No Code */
                        }

                        /* Check Time */
                        if ((ClockP_getTicks() - startTicks) >
                            (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                        {
                            status = SystemP_TIMEOUT;
                            frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                        }
                    }
                }

                /** Wait for TX Completion */
                while(!LIN_HLD_isTxBufferEmpty(hwAttrs->baseAddr))
                {
                    if ((ClockP_getTicks() - startTicks) >
                    (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                    {
                        status = SystemP_TIMEOUT;
                        frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                    }
                }
            }
            else
            {
                /* Handle Error and set Transaction Status */
                status = SystemP_FAILURE;

                /* Set Transaction Status */
                if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_FRAMING_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_PARITY_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_BIT_ERR;
                }
                else
                {
                    /* ISF Error */
                    frame->status = LIN_HLD_TXN_STS_FAILURE;
                }
            }
        }
        else { /* No Code */ }
    }

    if(status == SystemP_SUCCESS)
    {
        /* Set Object state to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
    }
    else
    {
        object->state = LIN_HLD_STATE_ERROR;
    }

    return status;
}

static int32_t LIN_WriteLinFrameResponderInterrupt(LIN_Handle handle,
                                                   LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    if(object->state == LIN_HLD_STATE_IDLE)
    {
        /* Wait for Buffers to be completetely Empty */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_TXEMPTY) == 0U){};
        /* Wait for Bus to be free */
        while(LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                               LIN_FLAG_MASK_BUSY) != 0U){};

        object->state = LIN_HLD_STATE_BUSY;
        object->currentTxnFrame = frame;

        /* Check of HGENCTRL bit, if it is set write ID-slaveTask Byte */
        if (object->openParams->linConfigParams.maskFilteringType ==
                                    LIN_HLD_MSG_FILTER_IDRESPONDER)
        {
            LIN_HLD_setIDSlaveTaskByte(hwAttrs->baseAddr, frame->id);
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

        object->writeBufIdx = (uint8_t *)object->currentTxnFrame->dataBuf;

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

        /* Enable necessary Interrupts */
        LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_ID      |
                                                    LIN_INT_LVL_TX      |
                                                    LIN_INT_LVL_PBE     |
                                                    LIN_INT_LVL_BE      |
                                                    LIN_INT_LVL_PE      |
                                                    LIN_INT_LVL_ISFE    |
                                                    LIN_INT_LVL_FE      );
    }

    return status;
}

static int32_t LIN_WriteSciFramePolling(LIN_Handle handle,
                                        LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            startTicks = 0x00U;
    uint32_t            writeCount = frame->frameLen;
    uint8_t             *writeBufIdx = (uint8_t *)frame->dataBuf;
    uint8_t             count = 0U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    startTicks = ClockP_getTicks();

    /* Wait for bus IDLE */
    while ( (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                              LIN_FLAG_MASK_IDLE) != 0U)
            && (status == SystemP_SUCCESS))
    {
        if  (   (ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
        {
            status = SystemP_TIMEOUT;
        }
    }

    while((writeCount != 0U) && (status == SystemP_SUCCESS))
    {
        while(  (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                                  LIN_FLAG_MASK_TXRDY) != 0U)
                && (status == SystemP_SUCCESS))
        {
            if  (   (ClockP_getTicks() - startTicks) >
                    (ClockP_usecToTicks((uint64_t)(frame->timeout))))
            {
                status = SystemP_TIMEOUT;
            }
        }

        if(status == SystemP_SUCCESS)
        {
            /* Write Char or Frame */
            if(object->openParams->multiBufferMode == true)
            {
                if (writeCount < 8U)
                {
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, (uint8_t)writeCount);
                    while(writeCount != 0U)
                    {
                        LIN_HLD_writeSciTxReg(hwAttrs->baseAddr, writeBufIdx);
                        writeBufIdx++;
                        writeCount--;
                    }
                }
                else
                {
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, 8U);
                    for(count = 0; count < 8U; count++)
                    {
                        LIN_HLD_writeSciTxReg(hwAttrs->baseAddr, writeBufIdx);
                        writeBufIdx++;
                        writeCount--;
                    }
                }
            }
            else
            {
                /* Write Char */
                LIN_HLD_writeSciTxReg(hwAttrs->baseAddr, writeBufIdx);
                writeBufIdx++;
                writeCount--;
            }
        }
    }

    return status;
}

static int32_t LIN_WriteSciFrameInterrupt(LIN_Handle handle,
                                          LIN_SCI_Frame *frame)
{
    return SystemP_FAILURE;
}

static int32_t LIN_ReadLinFrameCommanderPolling(LIN_Handle handle,
                                                LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;
    uint32_t            intrStatus = 0x00U;
    uint8_t             readCount = 0U;
    uint8_t             *readIndex = NULL;
    uint32_t            startTicks = 0U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    startTicks = ClockP_getTicks();

    /* Generate Parity ID if parity is Enabled */
    if(object->openParams->enableParity == true)
    {
        id = LIN_HLD_gen_ParityID(frame->id);
    }
    else
    {
        id = frame->id;
    }

    /* Set Object State to BUSY */
    object->state = LIN_HLD_STATE_BUSY;

    if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
    {
        /* Set The Frame Length */
        LIN_HLD_setFrameLength(hwAttrs->baseAddr, frame->frameLen);
    }
    else
    {
        if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
        {
            LIN_HLD_setFrameLength(hwAttrs->baseAddr, LIN_FRAME_FIXED_LEN_2U);
        }
        else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
        {
            LIN_HLD_setFrameLength(hwAttrs->baseAddr, LIN_FRAME_FIXED_LEN_4U);
        }
        else /* 0x30 - 0x3F */
        {
            LIN_HLD_setFrameLength(hwAttrs->baseAddr, LIN_FRAME_FIXED_LEN_8U);
        }
    }

    /* Set Message ID to initiate a header transmission */
    LIN_HLD_setIDByte(hwAttrs->baseAddr, id);

    /* Disable data Transmission */
    LIN_HLD_disableDataTx(hwAttrs->baseAddr);

    if(object->openParams->multiBufferMode == true)
    {
        /* Wait for Frame Reception or Error */
        while(status == SystemP_SUCCESS)
        {
            intrStatus = LIN_HLD_getInterruptStatusSCIFLR(  hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);

            if(( intrStatus & ( LIN_FLAG_MASK_RXRDY |
                                LIN_FLAG_MASK_FE    |
                                LIN_FLAG_MASK_PE    |
                                LIN_FLAG_MASK_OE    |
                                LIN_FLAG_MASK_NRE   |
                                LIN_FLAG_MASK_PBE   )) != 0U )
            {
                break;
            }

            /* Check Time */
            if ((ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
            {
                status = SystemP_TIMEOUT;
                frame->status = LIN_HLD_TXN_STS_TIMEOUT;
            }
        }

        if(status == SystemP_SUCCESS)
        {
            if((intrStatus & LIN_FLAG_MASK_RXRDY) == LIN_FLAG_MASK_RXRDY)
            {
                /* Store Data */
                LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                        (uint8_t *)frame->dataBuf,
                                        frame->frameLen);
            }
            else
            {
                status = SystemP_FAILURE;

                /* Set Transaction Status */
                if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_FRAMING_ERR;
                }
                if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_PARITY_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_OE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_OVERRUN_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_NRE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_NO_RES_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                }
                else
                {
                    /* Set Transaction Status */
                    frame->status = LIN_HLD_TXN_STS_FAILURE;
                }
            }
        }
        else
        {
            frame->status = LIN_HLD_TXN_STS_TIMEOUT;
        }
    }
    else
    {
        if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
        {
            readCount = frame->frameLen;
        }
        else
        {
            if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
            {
                readCount = LIN_FRAME_FIXED_LEN_2U;
            }
            else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
            {
                readCount = LIN_FRAME_FIXED_LEN_4U;
            }
            else /* 0x30 - 0x3F */
            {
                readCount = LIN_FRAME_FIXED_LEN_8U;
            }
        }

        /* Set The Frame Length */
        LIN_HLD_setFrameLength(hwAttrs->baseAddr, readCount);

        readIndex = (uint8_t *)frame->dataBuf;

        while ((status == SystemP_SUCCESS) && (readCount != 0U))
        {
            while(status == SystemP_SUCCESS)
            {
                intrStatus = LIN_HLD_getInterruptStatusSCIFLR(
                                                        hwAttrs->baseAddr,
                                                        LIN_FLAG_MASK_RXRDY |
                                                        LIN_FLAG_MASK_BE    |
                                                        LIN_FLAG_MASK_FE    |
                                                        LIN_FLAG_MASK_PE    |
                                                        LIN_FLAG_MASK_OE    |
                                                        LIN_FLAG_MASK_CE    |
                                                        LIN_FLAG_MASK_NRE   |
                                                        LIN_FLAG_MASK_PBE   );

                if((intrStatus & LIN_FLAG_MASK_RXRDY) != 0U)
                {
                    LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr, readIndex, 1U);
                    readIndex++;
                    readCount--;
                    break;
                }
                else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_BIT_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_FRAMING_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_PARITY_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_OE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_OVERRUN_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_CE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_CHECKSUM_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_NRE) != 0U)
                {
                    status = SystemP_FAILURE;
                    frame->status = LIN_HLD_TXN_NO_RES_ERR;
                }
                else
                {
                    /* No Code */
                }

                /* Check Time */
                if ((ClockP_getTicks() - startTicks) >
                    (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                {
                    status = SystemP_TIMEOUT;
                    frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                }
            }
        }
    }

    if(status == SystemP_SUCCESS)
    {
        /* Set Object state to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
    }
    else
    {
        object->state = LIN_HLD_STATE_ERROR;
    }

    return status;
}

static int32_t LIN_ReadLinFrameCommanderInterrupt(LIN_Handle handle,
                                                  LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint8_t             id = 0x00U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

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

        /* Enable necessary Interrupts */
        LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_RX  |
                                                    LIN_INT_LVL_PBE |
                                                    LIN_INT_LVL_NRE |
                                                    LIN_INT_LVL_FE  |
                                                    LIN_INT_LVL_PE  |
                                                    LIN_INT_LVL_OE  |
                                                    LIN_INT_LVL_CE  |
                                                    LIN_INT_LVL_BE  );

        /* Set Message ID to initiate a header transmission */
        LIN_HLD_setIDByte(hwAttrs->baseAddr, id);

        /* Disable data Transmission */
        LIN_HLD_disableDataTx(hwAttrs->baseAddr);
    }
    else
    {
        /* Module Busy or in Reset State */
        status = SystemP_FAILURE;
        frame->status = LIN_HLD_TXN_STS_FAILURE;
    }

    return status;
}

static int32_t LIN_ReadLinFrameResponderPolling(LIN_Handle handle,
                                                LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            intrStatus = 0x00U;
    uint8_t             readCount = 0U;
    uint8_t             *readIndex = NULL;
    uint32_t            startTicks = 0U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    startTicks = ClockP_getTicks();

    /* Set Object State to BUSY */
    object->state = LIN_HLD_STATE_BUSY;

    if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
    {
        /* Set The Frame Length */
        LIN_HLD_setFrameLength(hwAttrs->baseAddr, frame->frameLen);
    }

    if(object->openParams->linConfigParams.maskFilteringType ==
                                                    LIN_HLD_MSG_FILTER_IDBYTE)
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

    if(object->openParams->multiBufferMode == true)
    {
        /* Wait for reception Completion */
        while(true)
        {
            /* Get Interrupt status */
            intrStatus = LIN_HLD_getInterruptStatusSCIFLR(  hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);

            if(( intrStatus & ( LIN_FLAG_MASK_RXRDY |
                                LIN_FLAG_MASK_BE    |
                                LIN_FLAG_MASK_FE    |
                                LIN_FLAG_MASK_OE    |
                                LIN_FLAG_MASK_PE    |
                                LIN_FLAG_MASK_CE    )) != 0U )
            {
                break;
            }

            /* Check Time */
            if ((ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
            {
                status = SystemP_TIMEOUT;
                frame->status = LIN_HLD_TXN_STS_TIMEOUT;
            }
        }

        if(status == SystemP_SUCCESS)
        {
            if((intrStatus & LIN_FLAG_MASK_RXRDY) == LIN_FLAG_MASK_RXRDY)
            {
                /* Store ID */
                if(object->openParams->enableParity == true)
                {
                    frame->id = (LIN_HLD_getReceivedID(hwAttrs->baseAddr) &
                                (LIN_FRAME_ID_BIT_MASK));
                    /* Ignore Parity bits */
                }
                else
                {
                    frame->id = LIN_HLD_getReceivedID(hwAttrs->baseAddr);
                }

                /* Store Data */
                if(object->openParams->commMode ==
                                                LIN_COMM_HLD_LIN_USELENGTHVAL)
                {
                    LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                            (uint8_t *)frame->dataBuf,
                                            frame->frameLen);
                }
                else
                {
                    if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
                    {
                        LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                                (uint8_t *)frame->dataBuf,
                                                LIN_FRAME_FIXED_LEN_2U);
                    }
                    else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
                    {
                        LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                                (uint8_t *)frame->dataBuf,
                                                LIN_FRAME_FIXED_LEN_4U);
                    }
                    else /* 0x30 - 0x3F */
                    {
                        LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr,
                                                (uint8_t *)frame->dataBuf,
                                                LIN_FRAME_FIXED_LEN_8U);
                    }
                }
            }
            else
            {
                status = SystemP_FAILURE;

                /* Set Transaction Status */
                if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_FRAMING_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_OE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_OVERRUN_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_PARITY_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_CE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_CHECKSUM_ERR;
                }
                else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                {
                    frame->status = LIN_HLD_TXN_BIT_ERR;
                }
                else
                {
                    frame->status = LIN_HLD_TXN_STS_FAILURE;
                }
            }
        }
        else
        {
            frame->status = LIN_HLD_TXN_STS_TIMEOUT;
        }
    }
    else
    {
        /* Wait for ID reception Post Compare Match */
        while(true)
        {
            intrStatus = LIN_HLD_getInterruptStatusSCIFLR(  hwAttrs->baseAddr,
                                                        LIN_INTR_STS_MASK_ALL);

            if(( intrStatus & ( LIN_FLAG_MASK_RXID  |
                                LIN_FLAG_MASK_BE    |
                                LIN_FLAG_MASK_FE    |
                                LIN_FLAG_MASK_OE    |
                                LIN_FLAG_MASK_PE    |
                                LIN_FLAG_MASK_CE    )) != 0U )
            {
                break;
            }

            /* Check Time */
            if ((ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
            {
                status = SystemP_TIMEOUT;
                frame->status = LIN_HLD_TXN_STS_TIMEOUT;
            }
        }

        if(status == SystemP_SUCCESS)
        {
            if( (intrStatus & LIN_FLAG_MASK_RXID) != 0U)
            {
                if(object->openParams->enableParity == true)
                {
                    frame->id = (LIN_HLD_getReceivedID(hwAttrs->baseAddr) &
                                (LIN_FRAME_ID_BIT_MASK));
                }
                else
                {
                    frame->id = LIN_HLD_getReceivedID(hwAttrs->baseAddr);
                }
            }

            /* Store read parameters */
            readIndex = (uint8_t *)frame->dataBuf;

            if(object->openParams->commMode == LIN_COMM_HLD_LIN_USELENGTHVAL)
            {
                readCount = frame->frameLen;
            }
            else
            {
                if(frame->id <= LIN_ID_DATA_LEN_2_RANGE_HIGH)
                {
                    readCount = LIN_FRAME_FIXED_LEN_2U;
                }
                else if(frame->id <= LIN_ID_DATA_LEN_4_RANGE_HIGH)
                {
                    readCount = LIN_FRAME_FIXED_LEN_4U;
                }
                else /* 0x30 - 0x3F */
                {
                    readCount = LIN_FRAME_FIXED_LEN_8U;
                }
            }

            while ((status == SystemP_SUCCESS) && (readCount != 0U))
            {
                while(status == SystemP_SUCCESS)
                {
                    intrStatus = LIN_HLD_getInterruptStatusSCIFLR (
                                                        hwAttrs->baseAddr,
                                                        LIN_FLAG_MASK_RXRDY |
                                                        LIN_FLAG_MASK_BE    |
                                                        LIN_FLAG_MASK_FE    |
                                                        LIN_FLAG_MASK_PE    |
                                                        LIN_FLAG_MASK_OE    |
                                                        LIN_FLAG_MASK_CE    |
                                                        LIN_FLAG_MASK_NRE   |
                                                        LIN_FLAG_MASK_PBE   );

                    if((intrStatus & LIN_FLAG_MASK_RXRDY) != 0U)
                    {
                        LIN_HLD_readLinRxBuffer(hwAttrs->baseAddr, readIndex,
                                                1U);
                        readIndex++;
                        readCount--;
                        break;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_BE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_BIT_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_FE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_FRAMING_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_PE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_PARITY_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_OE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_OVERRUN_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_CE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_CHECKSUM_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_PBE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_PHY_BUS_ERR;
                    }
                    else if((intrStatus & LIN_FLAG_MASK_NRE) != 0U)
                    {
                        status = SystemP_FAILURE;
                        frame->status = LIN_HLD_TXN_NO_RES_ERR;
                    }
                    else
                    {
                        /* No Code */
                    }

                    /* Check Time */
                    if ((ClockP_getTicks() - startTicks) >
                        (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                    {
                        status = SystemP_TIMEOUT;
                        frame->status = LIN_HLD_TXN_STS_TIMEOUT;
                    }
                }
            }
        }
    }

    if(status == SystemP_SUCCESS)
    {
        /* Set Object state to IDLE */
        object->state = LIN_HLD_STATE_IDLE;
    }
    else
    {
        object->state = LIN_HLD_STATE_ERROR;
    }

    return status;
}

static int32_t LIN_ReadLinFrameResponderInterrupt(LIN_Handle handle,
                                                  LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

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

        /* Enable necessary Interrupts */
        if(object->openParams->multiBufferMode == true)
        {
            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_RX  |
                                                        LIN_INT_LVL_PBE |
                                                        LIN_INT_LVL_FE  |
                                                        LIN_INT_LVL_PE  |
                                                        LIN_INT_LVL_OE  |
                                                        LIN_INT_LVL_CE  |
                                                        LIN_INT_LVL_BE  );
        }
        else
        {
            LIN_HLD_enableInterrupt(hwAttrs->baseAddr,  LIN_INT_LVL_ID  |
                                                        LIN_INT_LVL_RX  |
                                                        LIN_INT_LVL_PBE |
                                                        LIN_INT_LVL_FE  |
                                                        LIN_INT_LVL_PE  |
                                                        LIN_INT_LVL_OE  |
                                                        LIN_INT_LVL_CE  |
                                                        LIN_INT_LVL_BE  );
        }
    }

    return status;
}

static int32_t LIN_ReadSciFramePolling(LIN_Handle handle,
                                       LIN_SCI_Frame *frame)
{
    int32_t             status = SystemP_SUCCESS;
    LIN_Object          *object = NULL;
    LIN_HwAttrs const   *hwAttrs = NULL;
    uint32_t            startTicks = 0x00U;
    uint32_t            readCount = frame->frameLen;
    uint8_t             *readBufIdx = (uint8_t *)frame->dataBuf;
    uint8_t             count = 0U;
    uint32_t            error = 0U;

    object = handle->object;
    hwAttrs = handle->hwAttrs;

    startTicks = ClockP_getTicks();

    /* Wait for bus IDLE */
    while ( (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                              LIN_FLAG_MASK_IDLE) != 0U)
            && (status == SystemP_SUCCESS))
    {
        if  (   (ClockP_getTicks() - startTicks) >
                (ClockP_usecToTicks((uint64_t)(frame->timeout))))
        {
            status = SystemP_TIMEOUT;
        }
    }

    /* Clear All Interrrupts */
    LIN_HLD_clearInterrupt(hwAttrs->baseAddr,   LIN_FLAG_MASK_FE |
                                                LIN_FLAG_MASK_OE |
                                                LIN_FLAG_MASK_PE |
                                                LIN_FLAG_MASK_BREAK );

    while((readCount != 0U) && (status == SystemP_SUCCESS))
    {
        /* Check for Error */
        error = LIN_HLD_getInterruptStatusSCIFLR(   hwAttrs->baseAddr,
                                                    LIN_FLAG_MASK_FE |
                                                    LIN_FLAG_MASK_OE |
                                                    LIN_FLAG_MASK_PE );
        if(error != 0U)
        {
            status = SystemP_FAILURE;

            if((error & LIN_FLAG_MASK_PE) != 0U)
            {
                frame->status = LIN_HLD_TXN_PARITY_ERR;
            }
            else if((error & LIN_FLAG_MASK_OE) != 0U)
            {
                frame->status = LIN_HLD_TXN_OVERRUN_ERR;
            }
            else if((error & LIN_FLAG_MASK_FE) != 0U)
            {
                frame->status = LIN_HLD_TXN_FRAMING_ERR;
            }
            else
            {
                /* No Code */
            }
        }

        if(status == SystemP_SUCCESS)
        {
            /* Read Char or Frame */
            if(object->openParams->multiBufferMode == true)
            {
                if (readCount < 8U)
                {
                    /* Set the frame length */
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, (uint8_t)readCount);

                    /* Wait for Frame Reception */
                    while(  (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                                    LIN_FLAG_MASK_RXRDY) != 0U)
                            && (status == SystemP_SUCCESS))
                    {
                        if ((ClockP_getTicks() - startTicks) >
                            (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                        {
                            status = SystemP_TIMEOUT;
                        }
                    }

                    if(status == SystemP_SUCCESS)
                    {
                        while(readCount != 0U)
                        {
                            LIN_HLD_readSciRxReg(hwAttrs->baseAddr, readBufIdx,
                                object->openParams->sciConfigParams.dataBits);
                            readBufIdx++;
                            readCount--;
                        }
                    }
                }
                else
                {
                    /* Set the frame length */
                    LIN_HLD_setFrameLength(hwAttrs->baseAddr, 8U);

                    /* Wait for Frame Reception */
                    while(  (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                                    LIN_FLAG_MASK_RXRDY) != 0U)
                            && (status == SystemP_SUCCESS))
                    {
                        if  ((ClockP_getTicks() - startTicks) >
                             (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                        {
                            status = SystemP_TIMEOUT;
                        }
                    }

                    if(status == SystemP_SUCCESS)
                    {
                        for(count = 0; count < 8U; count++)
                        {
                            LIN_HLD_readSciRxReg(hwAttrs->baseAddr, readBufIdx,
                                object->openParams->sciConfigParams.dataBits);
                            readBufIdx++;
                            readCount--;
                        }
                    }
                }
            }
            else
            {
                /* Wait for RX Flag Set */
                while(  (LIN_HLD_getInterruptStatusSCIFLR(hwAttrs->baseAddr,
                                                    LIN_FLAG_MASK_RXRDY) != 0U)
                        && (status == SystemP_SUCCESS))
                {
                    if  (   (ClockP_getTicks() - startTicks) >
                            (ClockP_usecToTicks((uint64_t)(frame->timeout))))
                    {
                        status = SystemP_TIMEOUT;
                    }
                }

                /* Read Char */
                LIN_HLD_readSciRxReg(hwAttrs->baseAddr, readBufIdx,
                            object->openParams->sciConfigParams.dataBits);
                readBufIdx++;
                readCount--;
            }
        }
    }

    return status;
}

static int32_t LIN_ReadSciFrameInterrupt(LIN_Handle handle,
                                         LIN_SCI_Frame *frame)
{
    return SystemP_FAILURE;
}

static void LIN_HLD_completeCurrentTransfer(LIN_Handle handle)
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
