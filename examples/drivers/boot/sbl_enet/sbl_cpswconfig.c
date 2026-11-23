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

/*!
 * \file  sbl_cpswconfig.c
 *
 * \brief CPSW peripheral open/close and DMA channel management for the
 *        Ethernet SBL.  NoRTOS: no semaphores, no tasks — polling only.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>

#include <kernel/dpl/ClockP.h>

#include <include/per/cpsw.h>
#include <enet_apputils.h>
#include <enet_appmemutils.h>
#include <enet_appmemutils_cfg.h>

#include "ti_enet_config.h"
#include "ti_enet_open_close.h"

#include "sbl_cpswconfig.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*! Poll interval between ENET_PER_IOCTL_IS_PORT_LINK_UP queries (us) */
#define SBLCPSW_LINK_POLL_INTERVAL_US       (50000U)   /* 50 ms */

/*! Sleep between TX free-descriptor reclaim retries (us) */
#define SBLCPSW_TX_RETRY_INTERVAL_US        (1000U)    /* 1 ms  */


/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static int32_t SblCpsw_openDma(void);
static void    SblCpsw_closeDma(void);
static void    SblCpsw_initTxFreePktQ(void);
static void    SblCpsw_initRxReadyPktQ(void);
static void    SblCpsw_retrieveFreeTxPkts(void);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

SblCpsw_Obj gSblCpsw;


/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/*!
 * Required callback invoked by EnetApp_driverOpen() (sysconfig-generated).
 * Configures the CPSW peripheral parameters before the driver opens.
 */
void EnetApp_updateCpswInitCfg(Enet_Type enetType, uint32_t instId, Cpsw_Cfg *cpswCfg)
{
    CpswHostPort_Cfg *hostPortCfg = &cpswCfg->hostPortCfg;
    CpswAle_Cfg      *aleCfg     = &cpswCfg->aleCfg;
    CpswCpts_Cfg     *cptsCfg    = &cpswCfg->cptsCfg;

    cpswCfg->vlanCfg.vlanAware = false;

    /* Host port: strip CRC from received frames, pad short TX frames */
    hostPortCfg->removeCrc      = true;
    hostPortCfg->padShortPacket = true;
    hostPortCfg->passCrcErrors  = true;

    /* ALE: enable module, auto-aging, flood unknown to all ports */
    aleCfg->modeFlags                          = CPSW_ALE_CFG_MODULE_EN;
    aleCfg->agingCfg.autoAgingEn               = true;
    aleCfg->agingCfg.agingPeriodInMs           = 1000U;
    aleCfg->nwSecCfg.vid0ModeEn                = true;
    aleCfg->vlanCfg.aleVlanAwareMode           = false;
    aleCfg->vlanCfg.cpswVlanAwareMode          = false;
    aleCfg->vlanCfg.unknownUnregMcastFloodMask = CPSW_ALE_ALL_PORTS_MASK;
    aleCfg->vlanCfg.unknownRegMcastFloodMask   = CPSW_ALE_ALL_PORTS_MASK;
    aleCfg->vlanCfg.unknownVlanMemberListMask  = CPSW_ALE_ALL_PORTS_MASK;

    /* Disable RX timestamping — not needed in SBL */
    cptsCfg->hostRxTsEn = false;
}

int32_t SblCpsw_open(void)
{
    Enet_MacPort        macPortList[ENET_MAC_PORT_NUM];
    uint8_t             numMacPorts;
    EnetApp_HandleInfo  handleInfo;
    Enet_IoctlPrms      prms;
    int32_t             status = ENET_SOK;

    memset(&gSblCpsw, 0, sizeof(gSblCpsw));

    /* Discover enet instance and MAC port from sysconfig */
    EnetApp_getEnetInstInfo(CONFIG_ENET_CPSW0, &gSblCpsw.enetType, &gSblCpsw.instId);
    EnetApp_getEnetInstMacInfo(gSblCpsw.enetType, gSblCpsw.instId, macPortList, &numMacPorts);
    EnetAppUtils_assert(numMacPorts == 1U);
    gSblCpsw.macPort = macPortList[0];
    gSblCpsw.coreId  = EnetSoc_getCoreId();

    /* Initialise driver clocks and open the peripheral */
    EnetApp_driverInit();

    status = EnetApp_driverOpen(gSblCpsw.enetType, gSblCpsw.instId);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("SblCpsw_open: EnetApp_driverOpen failed: %d\r\n", status);
        return status;
    }

    /* Acquire peripheral and DMA handles */
    EnetApp_acquireHandleInfo(gSblCpsw.enetType, gSblCpsw.instId, &handleInfo);
    gSblCpsw.hEnet = handleInfo.hEnet;

    /* Attach this core to the resource manager */
    if (status == ENET_SOK)
    {
        EnetPer_AttachCoreOutArgs attachCoreOutArgs;

        EnetApp_coreAttach(gSblCpsw.enetType, gSblCpsw.instId,
                           gSblCpsw.coreId, &attachCoreOutArgs);
        gSblCpsw.coreKey = attachCoreOutArgs.coreKey;
    }

    /* Open TX and RX DMA channels; also populates gSblCpsw.hostMacAddr */
    if (status == ENET_SOK)
    {
        status = SblCpsw_openDma();
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("SblCpsw_open: SblCpsw_openDma failed: %d\r\n", status);
        }
    }

    /* Register host MAC address in the ALE so the switch forwards to host port */
    if (status == ENET_SOK)
    {
        CpswAle_SetUcastEntryInArgs setUcastInArgs;
        uint32_t entryIdx;

        setUcastInArgs.addr.vlanId  = 0U;
        setUcastInArgs.info.portNum = CPSW_ALE_HOST_PORT_NUM;
        setUcastInArgs.info.blocked = false;
        setUcastInArgs.info.secure  = false;
        setUcastInArgs.info.super   = false;
        setUcastInArgs.info.ageable = false;
        setUcastInArgs.info.trunk   = false;
        EnetUtils_copyMacAddr(&setUcastInArgs.addr.addr[0U], gSblCpsw.hostMacAddr);
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &setUcastInArgs, &entryIdx);

        ENET_IOCTL(gSblCpsw.hEnet, gSblCpsw.coreId,
                   CPSW_ALE_IOCTL_ADD_UCAST, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("SblCpsw_open: failed to add ALE ucast entry: %d\r\n", status);
        }
    }

    return status;
}

int32_t SblCpsw_waitForLinkUp(void)
{
    Enet_IoctlPrms prms;
    bool    linked = false;
    bool linkStable = false;
    int32_t status = ENET_SOK;
    uint16_t consecutivelinkupCount = 0;
    uint32_t waitCount = 0;

    ENET_IOCTL_SET_INOUT_ARGS(&prms, &gSblCpsw.macPort, &linked);

    while (!linkStable)
    {
        EnetApp_phyStateHandler();
        
        ENET_IOCTL(gSblCpsw.hEnet, gSblCpsw.coreId,
                   ENET_PER_IOCTL_IS_PORT_LINK_UP, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("SblCpsw_waitForLinkUp: IOCTL failed: %d\r\n", status);
            break;
        }
        if(waitCount%20 == 0)
        {
            EnetAppUtils_print("Waiting for Link-up .....\r\n");
        }

        if (!linked)
        {
            consecutivelinkupCount = 0; 
        }
        else
        {
            consecutivelinkupCount++;   
        }

        if(consecutivelinkupCount >= 5)
        {
            linkStable = true;   
        }
        else
        {
            ClockP_usleep(SBLCPSW_LINK_POLL_INTERVAL_US);
        }

        waitCount++;
    }

    return status;
}

int32_t SblCpsw_close(void)
{
    Enet_IoctlPrms prms;
    int32_t status;

    /* Close the MAC port link before tearing down DMA */
    ENET_IOCTL_SET_IN_ARGS(&prms, &gSblCpsw.macPort);
    ENET_IOCTL(gSblCpsw.hEnet, gSblCpsw.coreId,
               ENET_PER_IOCTL_CLOSE_PORT_LINK, &prms, status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("SblCpsw_close: failed to close port link: %d\r\n", status);
    }

    SblCpsw_closeDma();

    EnetApp_coreDetach(gSblCpsw.enetType, gSblCpsw.instId,
                       gSblCpsw.coreId, gSblCpsw.coreKey);

    EnetApp_releaseHandleInfo(gSblCpsw.enetType, gSblCpsw.instId);
    gSblCpsw.hEnet = NULL;

    EnetApp_driverDeInit();

    return status;
}

void SblCpsw_getMacAddr(uint8_t *macAddr)
{
    EnetUtils_copyMacAddr(macAddr, gSblCpsw.hostMacAddr);
}

int32_t SblCpsw_txPacket(const uint8_t *payloadBuf,
                          uint32_t       payloadLen,
                          const uint8_t *dstMacAddr,
                          uint16_t       etherType)
{
    EnetDma_PktQ  txSubmitQ;
    EnetDma_Pkt  *pktInfo;
    EthFrame     *frame;
    uint32_t      retries;
    int32_t       status = ENET_SOK;

    if (payloadLen > (ENET_MEM_LARGE_POOL_PKT_SIZE - sizeof(EthFrameHeader)))
    {
        EnetAppUtils_print("SblCpsw_txPacket: payloadLen %u exceeds buffer limit\r\n",
                           payloadLen);
        return ENET_EINVALIDPARAMS;
    }

    /* Try to get a free TX descriptor; if the pool is empty, reclaim from DMA */
    pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&gSblCpsw.txFreePktInfoQ);
    if (pktInfo == NULL)
    {
        for (retries = 0U; retries < SBLCPSW_TX_FREE_PKT_MAX_RETRIES; retries++)
        {
            SblCpsw_retrieveFreeTxPkts();
            pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&gSblCpsw.txFreePktInfoQ);
            if (pktInfo != NULL)
            {
                break;
            }
            ClockP_usleep(SBLCPSW_TX_RETRY_INTERVAL_US);
        }
    }

    if (pktInfo == NULL)
    {
        EnetAppUtils_print("SblCpsw_txPacket: no free TX descriptor after %u retries\r\n",
                           SBLCPSW_TX_FREE_PKT_MAX_RETRIES);
        return ENET_EFAIL;
    }

    /* Build the Ethernet frame in the descriptor's pre-allocated buffer */
    frame = (EthFrame *)pktInfo->sgList.list[0].bufPtr;
    memcpy(frame->hdr.dstMac,  dstMacAddr,          ENET_MAC_ADDR_LEN);
    memcpy(frame->hdr.srcMac,  gSblCpsw.hostMacAddr, ENET_MAC_ADDR_LEN);
    frame->hdr.etherType = Enet_htons(etherType);
    memcpy(frame->payload, payloadBuf, payloadLen);

    pktInfo->sgList.list[0].segmentFilledLen = (uint32_t)sizeof(EthFrameHeader) + payloadLen;
    pktInfo->sgList.numScatterSegments       = 1U;
    pktInfo->chkSumInfo                      = 0U;
    pktInfo->appPriv                         = &gSblCpsw;

    EnetDma_checkPktState(&pktInfo->pktState,
                          ENET_PKTSTATE_MODULE_APP,
                          ENET_PKTSTATE_APP_WITH_FREEQ,
                          ENET_PKTSTATE_APP_WITH_DRIVER);

    /* Submit the single packet to the TX DMA channel */
    EnetQueue_initQ(&txSubmitQ);
    EnetQueue_enq(&txSubmitQ, &pktInfo->node);

    status = EnetDma_submitTxPktQ(gSblCpsw.hTxCh, &txSubmitQ);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("SblCpsw_txPacket: EnetDma_submitTxPktQ failed: %d\r\n", status);
        /* DMA rejected the packet — transition state back and return to free pool */
        EnetDma_checkPktState(&pktInfo->pktState,
                              ENET_PKTSTATE_MODULE_APP,
                              ENET_PKTSTATE_APP_WITH_DRIVER,
                              ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&gSblCpsw.txFreePktInfoQ, &pktInfo->node);
    }

    return status;
}

/* ========================================================================== */
/*                        Static Function Definitions                         */
/* ========================================================================== */

void SblCpsw_getRxPackets(SblCpsw_RxPktList *list)
{
    EnetDma_PktQ rxReadyQ;
    EnetDma_Pkt *pktInfo;
    uint32_t     slot   = 0U;
    int32_t      status;

    list->count = 0U;

    EnetQueue_initQ(&rxReadyQ);

    status = EnetDma_retrieveRxPktQ(gSblCpsw.hRxCh, &rxReadyQ);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("SblCpsw_getRxPackets: DMA retrieve failed: %d\r\n", status);
        return;
    }

    pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&rxReadyQ);
    while ((pktInfo != NULL) && (slot < SBLCPSW_RX_MAX_PKTS_PER_CALL))
    {
        EnetDma_checkPktState(&pktInfo->pktState,
                              ENET_PKTSTATE_MODULE_APP,
                              ENET_PKTSTATE_APP_WITH_DRIVER,
                              ENET_PKTSTATE_APP_WITH_READYQ);

        /* Zero-copy: store a direct pointer into the DMA buffer */
        list->pkts[slot].buf     = (const uint8_t *)pktInfo->sgList.list[0].bufPtr;
        list->pkts[slot].len     = pktInfo->sgList.list[0].segmentFilledLen;
        list->pkts[slot].pktInfo = pktInfo;
        slot++;

        pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&rxReadyQ);
    }

    list->count = slot;
}

void SblCpsw_submitRxPackets(SblCpsw_RxPktList *list)
{
    EnetDma_PktQ rxFreeQ;
    uint32_t     i;

    if (list->count == 0U)
    {
        return;
    }

    EnetQueue_initQ(&rxFreeQ);

    for (i = 0U; i < list->count; i++)
    {
        EnetDma_checkPktState(&list->pkts[i].pktInfo->pktState,
                              ENET_PKTSTATE_MODULE_APP,
                              ENET_PKTSTATE_APP_WITH_READYQ,
                              ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&rxFreeQ, &list->pkts[i].pktInfo->node);
    }

    /* Transition all buffers to driver ownership and submit in one DMA call */
    EnetAppUtils_validatePacketState(&rxFreeQ,
                                     ENET_PKTSTATE_APP_WITH_FREEQ,
                                     ENET_PKTSTATE_APP_WITH_DRIVER);
    EnetDma_submitRxPktQ(gSblCpsw.hRxCh, &rxFreeQ);

    list->count = 0U;
}


/*!
 * Drain completed TX descriptors from the DMA engine back into txFreePktInfoQ.
 * Called when txFreePktInfoQ is empty.
 */
static void SblCpsw_retrieveFreeTxPkts(void)
{
    EnetDma_PktQ txFreeQ;
    EnetDma_Pkt *pktInfo;
    int32_t      status;

    EnetQueue_initQ(&txFreeQ);

    status = EnetDma_retrieveTxPktQ(gSblCpsw.hTxCh, &txFreeQ);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("SblCpsw_retrieveFreeTxPkts: retrieve failed: %d\r\n", status);
        return;
    }

    pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
    while (pktInfo != NULL)
    {
        EnetDma_checkPktState(&pktInfo->pktState,
                              ENET_PKTSTATE_MODULE_APP,
                              ENET_PKTSTATE_APP_WITH_DRIVER,
                              ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&gSblCpsw.txFreePktInfoQ, &pktInfo->node);
        pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
    }
}

static int32_t SblCpsw_openDma(void)
{
    EnetApp_GetDmaHandleInArgs    txInArgs;
    EnetApp_GetTxDmaHandleOutArgs txChInfo;
    EnetApp_GetDmaHandleInArgs    rxInArgs;
    EnetApp_GetRxDmaHandleOutArgs rxChInfo;
    int32_t status = ENET_SOK;

    /* ---- TX channel ---- */
    memset(&txInArgs, 0, sizeof(txInArgs));
    /* NoRTOS: no ISR notification — TX completions are polled directly */
    txInArgs.notifyCb = NULL;
    txInArgs.cbArg    = NULL;

    EnetApp_getTxDmaHandle(ENET_DMA_TX_CH0, &txInArgs, &txChInfo);
    gSblCpsw.hTxCh = txChInfo.hTxCh;

    if (gSblCpsw.hTxCh == NULL)
    {
        EnetAppUtils_print("SblCpsw_openDma: EnetApp_getTxDmaHandle failed\r\n");
        return ENET_EFAIL;
    }

    SblCpsw_initTxFreePktQ();

    /* ---- RX channel ---- */
    memset(&rxInArgs, 0, sizeof(rxInArgs));
    /* NoRTOS: no ISR notification — RX arrivals are polled directly */
    rxInArgs.notifyCb = NULL;
    rxInArgs.cbArg    = NULL;

    EnetApp_getRxDmaHandle(ENET_DMA_RX_CH0, &rxInArgs, &rxChInfo);
    gSblCpsw.hRxCh = rxChInfo.hRxCh;

    if (gSblCpsw.hRxCh == NULL)
    {
        EnetAppUtils_print("SblCpsw_openDma: EnetApp_getRxDmaHandle failed\r\n");
        return ENET_EFAIL;
    }

    /* MAC address is assigned by the driver when the RX channel is opened */
    EnetAppUtils_assert(rxChInfo.numValidMacAddress == 1U);
    EnetUtils_copyMacAddr(gSblCpsw.hostMacAddr,
                          &rxChInfo.macAddr[rxChInfo.numValidMacAddress - 1U][0U]);

    /* Submit all pre-allocated RX buffers to DMA */
    SblCpsw_initRxReadyPktQ();

    return status;
}

static void SblCpsw_closeDma(void)
{
    EnetDma_PktQ fqPktInfoQ;
    EnetDma_PktQ cqPktInfoQ;

    /**
     * As Tx path is fire and forget, we need to retrevie Tx packets from DMA before closing
     */
    SblCpsw_retrieveFreeTxPkts();
    EnetQueue_initQ(&fqPktInfoQ);
    EnetQueue_initQ(&cqPktInfoQ);

    EnetApp_closeRxDma(ENET_DMA_RX_CH0,
                       gSblCpsw.hEnet, gSblCpsw.coreKey, gSblCpsw.coreId,
                       &fqPktInfoQ, &cqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&fqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&cqPktInfoQ);

    EnetQueue_initQ(&fqPktInfoQ);
    EnetQueue_initQ(&cqPktInfoQ);

    EnetApp_closeTxDma(ENET_DMA_TX_CH0,
                       gSblCpsw.hEnet, gSblCpsw.coreKey, gSblCpsw.coreId,
                       &fqPktInfoQ, &cqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&fqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&cqPktInfoQ);

    EnetAppUtils_freePktInfoQ(&gSblCpsw.rxFreeQ);
    EnetAppUtils_freePktInfoQ(&gSblCpsw.txFreePktInfoQ);
}

static void SblCpsw_initTxFreePktQ(void)
{
    EnetDma_Pkt *pPktInfo;
    uint32_t i;
    uint32_t scatterSegments[] = { ENET_MEM_LARGE_POOL_PKT_SIZE };

    EnetQueue_initQ(&gSblCpsw.txFreePktInfoQ);

    for (i = 0U; i < ENET_SYSCFG_TOTAL_NUM_TX_PKT; i++)
    {
        pPktInfo = EnetMem_allocEthPkt(&gSblCpsw,
                                       ENETDMA_CACHELINE_ALIGNMENT,
                                       ENET_ARRAYSIZE(scatterSegments),
                                       scatterSegments);
        EnetAppUtils_assert(pPktInfo != NULL);
        ENET_UTILS_SET_PKT_APP_STATE(&pPktInfo->pktState, ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&gSblCpsw.txFreePktInfoQ, &pPktInfo->node);
    }
}

static void SblCpsw_initRxReadyPktQ(void)
{
    EnetDma_PktQ rxReadyQ;
    EnetDma_Pkt *pPktInfo;
    int32_t status;
    uint32_t i;
    uint32_t scatterSegments[] = { ENET_MEM_LARGE_POOL_PKT_SIZE };

    EnetQueue_initQ(&gSblCpsw.rxFreeQ);
    EnetQueue_initQ(&gSblCpsw.rxReadyQ);
    EnetQueue_initQ(&rxReadyQ);

    for (i = 0U; i < ENET_SYSCFG_TOTAL_NUM_RX_PKT; i++)
    {
        pPktInfo = EnetMem_allocEthPkt(&gSblCpsw,
                                       ENETDMA_CACHELINE_ALIGNMENT,
                                       ENET_ARRAYSIZE(scatterSegments),
                                       scatterSegments);
        EnetAppUtils_assert(pPktInfo != NULL);
        ENET_UTILS_SET_PKT_APP_STATE(&pPktInfo->pktState, ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&gSblCpsw.rxFreeQ, &pPktInfo->node);
    }

    /* Confirm DMA has no stale descriptors at startup */
    status = EnetDma_retrieveRxPktQ(gSblCpsw.hRxCh, &rxReadyQ);
    EnetAppUtils_assert(status == ENET_SOK);
    EnetAppUtils_assert(EnetQueue_getQCount(&rxReadyQ) == 0U);

    /* Hand all free buffers to the DMA engine */
    EnetAppUtils_validatePacketState(&gSblCpsw.rxFreeQ,
                                     ENET_PKTSTATE_APP_WITH_FREEQ,
                                     ENET_PKTSTATE_APP_WITH_DRIVER);
    EnetDma_submitRxPktQ(gSblCpsw.hRxCh, &gSblCpsw.rxFreeQ);

    EnetAppUtils_assert(EnetQueue_getQCount(&gSblCpsw.rxFreeQ) == 0U);
}
