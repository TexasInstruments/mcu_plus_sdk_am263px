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
 *  This example performs EDMA Errror interrupt functionality.
 *
 * A Count, B Count and C count are set to 0 to emulate a NULL transfer.
 * After starting the transfer, EDMA detects a NULL transfer and signals
 * TPCC error. Application error callback gets invoked which signals an 
 * error.
 *
 */

#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/edma.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/* Value for A count*/
#define EDMA_TEST_A_COUNT           (0U)
/* Value for B count */
#define EDMA_TEST_B_COUNT           (0U)
/* Value for C count */
#define EDMA_TEST_C_COUNT           (0U)
/* Event queue to be used  */
#define EDMA_TEST_EVT_QUEUE_NO      (0U)

#define EDMA_TEST_BUFFER_SIZE             (EDMA_TEST_A_COUNT * EDMA_TEST_B_COUNT * EDMA_TEST_C_COUNT)

/* The source buffer used for transfer */
static uint8_t gEdmaTestSrcBuff[EDMA_TEST_BUFFER_SIZE] __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));
/* The destination buffer used for transfer */
static uint8_t gEdmaTestDstBuff[EDMA_TEST_BUFFER_SIZE] __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));

/* Semaphore to indicate transfer completion */
static SemaphoreP_Object gEdmaTestDoneSem;
/* Semaphore to indicate error */
static SemaphoreP_Object gEdmaErrorSem;
/* Structure to store the error info from EDMA */
static EDMA_ErrorInfo  gEdmaErrorInfo;

/* EDMA error Callback function */
static void EDMA_errorCallback(EDMA_ErrorInfo *errorInfo, void *args);
/* EDMA transfer Callback function */
static void EDMA_regionIsrFxn(Edma_IntrHandle intrHandle, void *args);

void edma_error_interrupt(void *args)
{
    uint32_t            baseAddr, regionId;
    int32_t             status = SystemP_SUCCESS;
    EDMACCPaRAMEntry    edmaParam;
    Edma_IntrObject     intrObj;
    uint32_t            dmaCh, tcc, param, channel, eventMissStatusLow, eventMissStatusHigh;

    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();

    DebugP_log("[EDMA] EDMA Error Interrupt Test Started...\r\n");

    status = EDMA_registerErrorCallback(gEdmaHandle[0], EDMA_errorCallback, (void *) &gEdmaErrorSem);

    baseAddr = EDMA_getBaseAddr(gEdmaHandle[0]);
    DebugP_assert(baseAddr != 0);

    regionId = EDMA_getRegionId(gEdmaHandle[0]);
    DebugP_assert(regionId < SOC_EDMA_NUM_REGIONS);

    dmaCh = EDMA_RESOURCE_ALLOC_ANY;
    status = EDMA_allocDmaChannel(gEdmaHandle[0], &dmaCh);
    DebugP_assert(status == SystemP_SUCCESS);

    tcc = EDMA_RESOURCE_ALLOC_ANY;
    status = EDMA_allocTcc(gEdmaHandle[0], &tcc);
    DebugP_assert(status == SystemP_SUCCESS);

    param = EDMA_RESOURCE_ALLOC_ANY;
    status = EDMA_allocParam(gEdmaHandle[0], &param);
    DebugP_assert(status == SystemP_SUCCESS);

    /* Request channel */
    EDMA_configureChannelRegion(baseAddr, regionId, EDMA_CHANNEL_TYPE_DMA,
         dmaCh, tcc, param, EDMA_TEST_EVT_QUEUE_NO);

    /* Program Param Set */
    EDMA_ccPaRAMEntry_init(&edmaParam);
    edmaParam.srcAddr       = (uint32_t) SOC_virtToPhy((uint8_t *) gEdmaTestSrcBuff);
    edmaParam.destAddr      = (uint32_t) SOC_virtToPhy((uint8_t *) gEdmaTestDstBuff);
    edmaParam.aCnt          = (uint16_t) EDMA_TEST_A_COUNT;
    edmaParam.bCnt          = (uint16_t) EDMA_TEST_B_COUNT;
    edmaParam.cCnt          = (uint16_t) EDMA_TEST_C_COUNT;
    edmaParam.bCntReload    = (uint16_t) EDMA_TEST_B_COUNT;
    edmaParam.srcBIdx       = (int16_t) EDMA_PARAM_BIDX(EDMA_TEST_A_COUNT);
    edmaParam.destBIdx      = (int16_t) EDMA_PARAM_BIDX(EDMA_TEST_A_COUNT);
    edmaParam.srcCIdx       = (int16_t) EDMA_TEST_A_COUNT;
    edmaParam.destCIdx      = (int16_t) EDMA_TEST_A_COUNT;
    edmaParam.linkAddr      = 0xFFFFU;
    edmaParam.srcBIdxExt    = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_TEST_A_COUNT);
    edmaParam.destBIdxExt   = (int8_t) EDMA_PARAM_BIDX_EXT(EDMA_TEST_A_COUNT);
    edmaParam.opt          |=
        (EDMA_OPT_TCINTEN_MASK | EDMA_OPT_ITCINTEN_MASK |
         ((((uint32_t)tcc) << EDMA_OPT_TCC_SHIFT) & EDMA_OPT_TCC_MASK));
    EDMA_setPaRAM(baseAddr, param, &edmaParam);

    status = SemaphoreP_constructBinary(&gEdmaTestDoneSem, 0);
    DebugP_assert(SystemP_SUCCESS == status);
    status = SemaphoreP_constructBinary(&gEdmaErrorSem, 0);
    DebugP_assert(SystemP_SUCCESS == status);

    /* Register interrupt */
    intrObj.tccNum = tcc;
    intrObj.cbFxn  = &EDMA_regionIsrFxn;
    intrObj.appData = (void *) &gEdmaTestDoneSem;
    status = EDMA_registerIntr(gEdmaHandle[0], &intrObj);
    DebugP_assert(status == SystemP_SUCCESS);

    EDMA_enableTransferRegion(baseAddr, regionId, dmaCh, EDMA_TRIG_MODE_MANUAL);

    /* Wait for transfer completion signal */
    SemaphoreP_pend(&gEdmaTestDoneSem, SystemP_WAIT_FOREVER);
    /* Wait for transfer error signal*/
    SemaphoreP_pend(&gEdmaErrorSem, SystemP_WAIT_FOREVER);

    if((gEdmaErrorInfo.errorStatus & EDMA_TPCC_A_ERRINT) == EDMA_TPCC_A_ERRINT)
    {
        DebugP_log("[EDMA] TPCC Error detected due to NULL transfer on below EDMA channels -\r\n");
        
        for(channel = 0U; channel < 32U; channel++)
        {
            eventMissStatusLow = gEdmaErrorInfo.ccErrorInfo.dmaEventMissStatusLow;

            if((eventMissStatusLow & (1U << channel)) == (1U << channel))
            {
                DebugP_log("[EDMA] Channel: %d\r\n", channel);
            }
        }
        for(channel = 32U; channel < SOC_EDMA_NUM_DMACH; channel++)
        {
            eventMissStatusHigh = gEdmaErrorInfo.ccErrorInfo.dmaEventMissStatusHigh;

            if((eventMissStatusHigh & (1U << (channel - 32U))) == (1U << (channel - 32U)))
            {
                DebugP_log("[EDMA] Channel: %d\r\n", channel);
            }
        }
    }

    status = EDMA_unregisterIntr(gEdmaHandle[0], &intrObj);
    SemaphoreP_destruct(&gEdmaTestDoneSem);
    DebugP_assert(status == SystemP_SUCCESS);
    SemaphoreP_destruct(&gEdmaErrorSem);
    DebugP_assert(status == SystemP_SUCCESS);

    /* Free channel */
    EDMA_freeChannelRegion(baseAddr, regionId, EDMA_CHANNEL_TYPE_DMA,
        dmaCh, EDMA_TRIG_MODE_MANUAL, tcc, EDMA_TEST_EVT_QUEUE_NO);

    /* Free the EDMA resources managed by driver. */
    status = EDMA_freeDmaChannel(gEdmaHandle[0], &dmaCh);
    DebugP_assert(status == SystemP_SUCCESS);
    status = EDMA_freeTcc(gEdmaHandle[0], &tcc);
    DebugP_assert(status == SystemP_SUCCESS);
    status = EDMA_freeParam(gEdmaHandle[0], &param);
    DebugP_assert(status == SystemP_SUCCESS);

    status = EDMA_unregisterErrorCallback(gEdmaHandle[0]);

    if(status == SystemP_SUCCESS)
    {
        DebugP_log("[EDMA] Error Transfer Test Completed. Null tranfer error detected !!\r\n");
        DebugP_log("All tests have passed!!\r\n");
    }
    else
    {
        DebugP_log("Some tests have failed!!\r\n");
    }

    Board_driversClose();
    Drivers_close();
    return;
}

static void EDMA_errorCallback(EDMA_ErrorInfo *errorInfo, void *args)
{
    SemaphoreP_Object *semObjPtr = (SemaphoreP_Object *)args;
    DebugP_assert(semObjPtr != NULL);

    if ((errorInfo->errorStatus & EDMA_TPCC_A_ERRINT) == EDMA_TPCC_A_ERRINT)
    {
        /* Copy the Error info from EDMA to application */
        memcpy(&gEdmaErrorInfo, errorInfo, sizeof(EDMA_ErrorInfo));

        SemaphoreP_post(semObjPtr);
    }
}

static void EDMA_regionIsrFxn(Edma_IntrHandle intrHandle, void *args)
{
    SemaphoreP_Object *semObjPtr = (SemaphoreP_Object *)args;
    DebugP_assert(semObjPtr != NULL);
    SemaphoreP_post(semObjPtr);
}
