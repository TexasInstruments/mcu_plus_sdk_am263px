/*
 *  Copyright (C) 2024-26 Texas Instruments Incorporated
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
 * \file  sbl_enet.c
 *
 * \brief Ethernet transport layer for the Ethernet SBL.
 *
 * Exposes only four public functions (declared in sbl_enet.h):
 *   EnetSBL_setup, EnetSBL_destruct, EnetSBL_receiveAppimage, EnetSBL_txReply.
 *
 * All internal types, structs, and protocol constants are kept private to
 * this file and are NOT exported through sbl_enet.h.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include "sbl_enet.h"

#include <stdint.h>
#include <stdlib.h>

#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/CacheP.h>

#include <enet_apputils.h>
#include <enet_appmemutils.h>
#include <enet_appmemutils_cfg.h>

#include "ti_enet_open_close.h"
#include "ti_enet_config.h"

#include <drivers/bootloader/bootloader_uniflash/bootloader_uniflash.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define IPV4_HDR_VER_IHL                ((0x4 << 4) | 0x5)
#define IPV4_HDR_TOS                    (0x00)
#define IPV4_HDR_TOTAL_PKT_LEN          (36U)
#define IPV4_HDR_IPID                   (0x28)
#define IPV4_HDR_FLAGFRAFOFFSET         (0x0000)
#define IPV4_HDR_TTL                    (0xFF)
#define IPV4_HDR_UDPLITE                (0x88)
#define IPV4_HDR_UDP                    (0x11)
#define IPV4_ADDR_LEN                   (4U)
#define IPV4_ETHERTYPE                  (0x0800)

#define ENET_MTU_SIZE                   (1514U)
#define ETH_HDR_SIZE                    (14U)
#define IPV4_HDR_SIZE                   (20U)
#define UDP_HDR_SIZE                    (8U)
#define SEQ_NUM_SIZE                    (4U)
#define ACK_CODE_SIZE                   (4U)
#define MGC_NUM_SIZE                    (4U)
#define UDP_PKT_LEN                     (IPV4_HDR_TOTAL_PKT_LEN - IPV4_HDR_SIZE)

#define ENETSBL_PKT_HDR_SIZE            (ENET_UTILS_ALIGN((UDP_HDR_SIZE + IPV4_HDR_SIZE + ETH_HDR_SIZE), 128))
#define ENETSBL_PKT_MAX_SIZE            (ENET_UTILS_ALIGN((ENET_MTU_SIZE - UDP_HDR_SIZE - IPV4_HDR_SIZE - ETH_HDR_SIZE), 128))
#define ENETSBL_TX_PAYLOAD_SIZE         (ENET_UTILS_ALIGN((SEQ_NUM_SIZE + ACK_CODE_SIZE), 8))

#define ENETSBL_HEADER_MGC_NUMBER       (0x05B1C00D)
#define ENETSBL_HEADER_ACK              (0x05B10ACD)

#define ENETSBL_MAX_TX_WAIT_COUNT            (1000U)
#define ENETSBL_MAX_TX_WAIT_SLEEP_TIME_US   (10000U)
#define ENETSBL_MAX_RX_WAIT_COUNT          (100000U)
#define ENETSBL_MAX_RX_WAIT_SLEEP_TIME_US    (1000U)

#define ENETSBL_LINKUP_POLL_SLEEP         (1000000U)
#define ENETSBL_LINKUP_POLL_MAX_COUNT         (200U)
typedef struct EnetSBL_LLDObj_s
{
    /* Enet driver */
    Enet_Handle hEnet;
    Enet_Type enetType;
    uint32_t instId;
    uint32_t coreId;
    uint32_t coreKey;
    Enet_MacPort macPort[ENET_SYSCFG_MAX_MAC_PORTS];
    uint8_t numMacPorts;
    uint8_t hostMacAddr[ENET_MAC_ADDR_LEN];

    /* Tx, Rx Packet Queues */
    uint32_t rxChNum;
    EnetDma_RxChHandle hRxCh;
    EnetDma_PktQ rxFreeQ;
    EnetDma_PktQ rxReadyQ;
    
    EnetDma_TxChHandle hTxCh;
    EnetDma_PktQ txFreePktInfoQ;
    uint32_t txChNum;

} EnetSBL_LLDObj;

typedef struct
{
    uint8_t  verIHL;
    uint8_t  tos;
    uint16_t totalPktLen;
    uint16_t ipId;
    uint16_t flagFragOffset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t hdrChksum;
    uint8_t  srcIP[IPV4_ADDR_LEN];
    uint8_t  dstIP[IPV4_ADDR_LEN];
} __attribute__ ((packed)) EthAppIPv4Header;

typedef struct
{
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t length;
    uint16_t csum;
} __attribute__ ((packed)) EthAppUDPHeader;

typedef struct EnetSBL_AddrInfo_s
{
    uint8_t  dstMac[ENET_MAC_ADDR_LEN];
    uint8_t  srcIP[IPV4_ADDR_LEN];
    uint8_t  dstIP[IPV4_ADDR_LEN];
    uint16_t srcPortUDP;
    uint16_t dstPortUDP;
} EnetSBL_AddrInfo;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static bool EnetSBL_checkFrame(EthFrame *frame, uint32_t dataLen);

static uint32_t EnetSBL_retrieveFreeTxPkts(void);

static uint32_t EnetSBL_receivePkts(void);

static int32_t EnetSBL_setupCpswAle(void);

static void EnetSBL_closeEnet(void);

static int32_t EnetSBL_showAlivePhys(void);

static int32_t EnetSBL_waitForLinkUp(void);

static void EnetSBL_rxIsrFxn(void *appData);

static void EnetSBL_txIsrFxn(void *appData);

static void EnetSBL_initPkt(EnetDma_Pkt *pPktInfo, uint8_t *udpPayload);

static void EnetSBL_initTxFreePktQ(void);

static void EnetSBL_initRxReadyPktQ(void);

static int32_t EnetSBL_openDma(void);

static void EnetSBL_closeDma(void);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

const EnetSBL_AddrInfo enetAppAddrInfo =
{
    .dstMac     = ENET_HOST_PC_MAC_ADDRESS,
    .srcIP      = ENET_SOURCE_IP_ADDRESS,
    .dstIP      = ENET_DESTINATION_IP_ADDRESS,
    .srcPortUDP = ENET_PORT,
    .dstPortUDP = ENET_PORT,
};

const uint32_t EnetSBL_MagicNum = ENETSBL_HEADER_MGC_NUMBER;
const uint32_t EnetSBL_Ack      = ENETSBL_HEADER_ACK;

EnetSBL_LLDObj  gEnetSBL_LLDObj;
uint8_t gAappPktHeader[ENETSBL_PKT_HDR_SIZE] __attribute__ ((aligned(128)));
uint8_t gTxPayload[ENETSBL_TX_PAYLOAD_SIZE] __attribute__ ((aligned(8)));

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

int32_t EnetSBL_setup(void)
{
    int32_t status = 0;
    EnetApp_HandleInfo handleInfo;

    EnetApp_getEnetInstInfo(CONFIG_ENET_CPSW0, &gEnetSBL_LLDObj.enetType, &gEnetSBL_LLDObj.instId);
    EnetApp_getEnetInstMacInfo(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId, &gEnetSBL_LLDObj.macPort[0], &gEnetSBL_LLDObj.numMacPorts);
    DebugP_assert(gEnetSBL_LLDObj.numMacPorts == 1U);

    /* Enable peripheral clocks */
    EnetAppUtils_enableClocks(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId);
    
    /* Delay for phy to stabilize */
    ClockP_usleep(250000);
    /* Local core id */
    gEnetSBL_LLDObj.coreId = EnetSoc_getCoreId();
    EnetApp_driverInit();
    if (status == ENET_SOK)
    {
        status = EnetApp_driverOpen(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId);

        if (status != ENET_SOK)
        {
            EnetAppUtils_print("[ ENETSBL ] Failed to open Enet driver: %d\r\n", status);
        }
    }

    EnetApp_acquireHandleInfo(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId, &handleInfo);
    gEnetSBL_LLDObj.hEnet = handleInfo.hEnet;

    if (status == ENET_SOK)
    {
        EnetPer_AttachCoreOutArgs attachCoreOutArgs;

        EnetApp_coreAttach(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId, gEnetSBL_LLDObj.coreId, &attachCoreOutArgs);
        gEnetSBL_LLDObj.coreKey = attachCoreOutArgs.coreKey;
    }

    /* Open DMA driver */
    if (status == ENET_SOK)
    {
        status = EnetSBL_openDma();
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("[ ENETSBL ] Failed to open DMA: %d\r\n", status);
        }
    }

    /* Show alive PHYs */
    if (status == ENET_SOK)
    {
        status = EnetSBL_showAlivePhys();
    }

    EnetAppUtils_print("[ ENETSBL ] Please wait for Linkup ...\r\n");

    /* Wait for link up */
    if (status == ENET_SOK)
    {
        status = EnetSBL_waitForLinkUp();
    }

    if(status != ENET_ETIMEOUT)
        EnetAppUtils_print("[ ENETSBL ] Linkup Done!\r\n");

    return status;
}

void EnetSBL_destruct(void)
{
    Enet_IoctlPrms prms;
    int32_t status = 0;

    /* Disable host port */
    ENET_IOCTL_SET_NO_ARGS(&prms);
    ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, ENET_HOSTPORT_IOCTL_DISABLE, &prms,status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[ ENETSBL ] Failed to disable host port: %d\r\n", status);
    }

    /* Close Enet DMA driver */
    EnetSBL_closeDma();

    /* Close Enet driver */
    EnetSBL_closeEnet();

    /* Disable peripheral clocks */
    EnetAppUtils_disableClocks(gEnetSBL_LLDObj.enetType, gEnetSBL_LLDObj.instId);

    /* Deinit Enet driver */
    Enet_deinit();

}

int32_t EnetSBL_receiveAppimage(uint8_t *buf, uint32_t bufMaxSize, uint32_t *receivedSize)
{
    bool finished = false;
    uint32_t currSeqNum = 0U;
    uint32_t totalPktCnt = 0xFFFFFFFFU;
    int32_t status = ENET_SOK;
    uint32_t receivedApimageSize = 0U;
    uint32_t waitCount = 0;
    
    *receivedSize = 0U;
    
    while(!finished)
    {
        uint32_t rxReadyCnt = EnetSBL_receivePkts();
        while((rxReadyCnt == 0U) && (waitCount <= ENETSBL_MAX_RX_WAIT_COUNT))
        {
            waitCount++;
            ClockP_usleep(ENETSBL_MAX_RX_WAIT_SLEEP_TIME_US);
            rxReadyCnt = EnetSBL_receivePkts();   
        }

        if(waitCount >= ENETSBL_MAX_RX_WAIT_COUNT)
        {
            finished = true;
            status = ENET_EFAIL;
            break;
        }

        EnetDma_Pkt * pktInfoRx = (EnetDma_Pkt *)EnetQueue_deq(&gEnetSBL_LLDObj.rxReadyQ);
        while (NULL != pktInfoRx && !finished)
        {
            EnetDma_checkPktState(&pktInfoRx->pktState,
                                    ENET_PKTSTATE_MODULE_APP,
                                    ENET_PKTSTATE_APP_WITH_READYQ,
                                    ENET_PKTSTATE_APP_WITH_FREEQ);

            EthFrame * frame = (EthFrame *)pktInfoRx->sgList.list[0].bufPtr;
            uint32_t ethPayloadLen = pktInfoRx->sgList.list[0].segmentFilledLen;

            if(EnetSBL_checkFrame(frame, ethPayloadLen))
            {
                uint32_t receivedSeqNum = 0U; 
                uint8_t * appPktData = &(frame->payload[sizeof(EthAppIPv4Header)+sizeof(EthAppUDPHeader)]);
                memcpy(&receivedSeqNum, &appPktData[MGC_NUM_SIZE], SEQ_NUM_SIZE);

                if (receivedSeqNum == currSeqNum)
                {
                    if (currSeqNum == 0U)
                    {
                        /* Initialize total pkt count from pkt 0 */
                        /* It is stored in the rsv1 (reserved) field of the Uniflash header */
                        receivedApimageSize = 0U;
                        Bootloader_UniflashFileHeader* fileHeader = (Bootloader_UniflashFileHeader*) &appPktData[MGC_NUM_SIZE+SEQ_NUM_SIZE];
                        totalPktCnt = fileHeader->rsv1;
                        EnetAppUtils_print("[ ENETSBL ] Receiving file, please wait ...\r\n");
                    }

                    uint32_t dataChunkSize = ethPayloadLen  - (ETH_HDR_SIZE + IPV4_HDR_SIZE + UDP_HDR_SIZE + MGC_NUM_SIZE + SEQ_NUM_SIZE);

                    if ((receivedApimageSize + dataChunkSize) <= bufMaxSize)
                    {
                        memcpy(&buf[receivedApimageSize], &appPktData[MGC_NUM_SIZE+SEQ_NUM_SIZE], dataChunkSize);
                        receivedApimageSize += dataChunkSize;

                        /* Fill the TX payload with seq number and ACK code */
                        memcpy(&gTxPayload[0], &currSeqNum, sizeof(currSeqNum));
                        memcpy(&gTxPayload[sizeof(currSeqNum)], &EnetSBL_Ack, sizeof(EnetSBL_Ack));
                        
                        EnetSBL_txReply(&gTxPayload[0]);

                        currSeqNum++;

                        /* Check for finish */
                        if(currSeqNum >= totalPktCnt)
                        {
                            finished = true;
                        }
                    }
                    else
                    {
                        EnetAppUtils_print("[ ENETSBL ] Receive buffer overflow, aborting transfer\r\n");
                        status = ENET_EFAIL;
                        finished = true;
                    }   
                }
                else
                {
                    /* Check if the packet number received is for previous packet, indicating that
                    * the ACK (or this is second iteration of same packet) got lost in transit
                    * and hence the host re-transmitted it due to timeout
                    */
                    EnetAppUtils_print("[ ENETSBL ] Out-of-Order Packet reception\r\n");
                    DebugP_assert(receivedSeqNum == currSeqNum-1);

                    uint32_t tempCurrPktCnt = currSeqNum-1;

                    /* Fill the TX payload with prev seq number and ACK code */
                    memcpy(&gTxPayload[0], &tempCurrPktCnt, sizeof(tempCurrPktCnt));
                    memcpy(&gTxPayload[sizeof(tempCurrPktCnt)], &EnetSBL_Ack, sizeof(EnetSBL_Ack));

                    EnetSBL_txReply(&gTxPayload[0]);
                }
            }
            else
            {
                EnetAppUtils_print("[ ENETSBL ] Invalid packet received: Discarded\r\n");
            }

            /* Release the received packet */
            EnetQueue_enq(&gEnetSBL_LLDObj.rxFreeQ, &pktInfoRx->node);
            pktInfoRx = (EnetDma_Pkt *)EnetQueue_deq(&gEnetSBL_LLDObj.rxReadyQ);
        }

        /*Submit now processed buffers */
        EnetAppUtils_validatePacketState(&gEnetSBL_LLDObj.rxFreeQ,
                                                ENET_PKTSTATE_APP_WITH_FREEQ,
                                                ENET_PKTSTATE_APP_WITH_DRIVER);
        EnetDma_submitRxPktQ(gEnetSBL_LLDObj.hRxCh,
                                    &gEnetSBL_LLDObj.rxFreeQ);
        
    }
    
    *receivedSize = receivedApimageSize;

    EnetAppUtils_print("[ ENETSBL ] Receive status:%d, receivedSize:%d\r\n", status, *receivedSize);
    return status;
}

int32_t EnetSBL_txReply(uint8_t *payload)
{
    EnetDma_Pkt *pktInfoTx;
    uint32_t txFreeCnt;
    int32_t status = ENET_SOK;
    uint32_t wait_count = 0;

    pktInfoTx = (EnetDma_Pkt *)EnetQueue_deq(&gEnetSBL_LLDObj.txFreePktInfoQ);
    while (pktInfoTx == NULL && wait_count <= ENETSBL_MAX_TX_WAIT_COUNT)
    {
        txFreeCnt = EnetSBL_retrieveFreeTxPkts();
        if (txFreeCnt == 0)
        {
            ClockP_usleep(ENETSBL_MAX_TX_WAIT_SLEEP_TIME_US);
            wait_count++;
        }
        else
        {
            pktInfoTx = (EnetDma_Pkt *)EnetQueue_deq(&gEnetSBL_LLDObj.txFreePktInfoQ);
        }
    }

    if(wait_count >= ENETSBL_MAX_TX_WAIT_COUNT)
    {
        EnetAppUtils_print("[ ENETSBL ] Tx packet retrival timed out\r\n");
        DebugP_assert(false);
    }
    /* Init TX packet header */
    EnetSBL_initPkt(pktInfoTx, payload);

    EnetDma_checkPktState(&pktInfoTx->pktState,
                          ENET_PKTSTATE_MODULE_APP,
                          ENET_PKTSTATE_APP_WITH_FREEQ,
                          ENET_PKTSTATE_APP_WITH_DRIVER);

    /* Submit TX packet */
    status = EnetDma_submitTxPkt(gEnetSBL_LLDObj.hTxCh, pktInfoTx);
    return status;
}

/* ========================================================================== */
/*                   Static Function Definitions                              */
/* ========================================================================== */

static uint32_t EnetSBL_retrieveFreeTxPkts(void)
{
    EnetDma_PktQ txFreeQ;
    EnetDma_Pkt *pktInfo;
    int32_t status = 0;
    uint32_t txFreeQCnt = 0U;

    EnetQueue_initQ(&txFreeQ);

    /* Retrieve any CPSW packets that may be free now */
    status = EnetDma_retrieveTxPktQ(gEnetSBL_LLDObj.hTxCh, &txFreeQ);

    /* Push them into application object's free queue */
    if (status == ENET_SOK)
    {
        txFreeQCnt = EnetQueue_getQCount(&txFreeQ);

        pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
        while (NULL != pktInfo)
        {
            EnetDma_checkPktState(&pktInfo->pktState,
                                  ENET_PKTSTATE_MODULE_APP,
                                  ENET_PKTSTATE_APP_WITH_DRIVER,
                                  ENET_PKTSTATE_APP_WITH_FREEQ);
            EnetQueue_enq(&gEnetSBL_LLDObj.txFreePktInfoQ, &pktInfo->node);
            pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
        }
    }

    else
    {
        EnetAppUtils_print("[ ENETSBL ] retrieveFreeTxPkts() failed to retrieve pkts: %d\r\n",
                           status);
    }

    /* Return number of free packets retrieved */
    return txFreeQCnt;
}

static uint32_t EnetSBL_receivePkts(void)
{
    EnetDma_PktQ rxReadyQ;
    EnetDma_Pkt *pktInfo;
    int32_t status = 0;
    uint32_t rxReadyCnt = 0U;

    EnetQueue_initQ(&rxReadyQ);

    /* Retrieve any CPSW packets which are ready */
    status = EnetDma_retrieveRxPktQ(gEnetSBL_LLDObj.hRxCh, &rxReadyQ);
    if (status == ENET_SOK)
    {
        rxReadyCnt = EnetQueue_getQCount(&rxReadyQ);

        /* Queue the received packet to rxReadyQ and pass new ones from rxFreeQ */
        pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&rxReadyQ);
        while (pktInfo != NULL)
        {
            EnetDma_checkPktState(&pktInfo->pktState,
                                  ENET_PKTSTATE_MODULE_APP,
                                  ENET_PKTSTATE_APP_WITH_DRIVER,
                                  ENET_PKTSTATE_APP_WITH_READYQ);

            EnetQueue_enq(&gEnetSBL_LLDObj.rxReadyQ, &pktInfo->node);
            pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&rxReadyQ);
        }
    }
    else
    {
        EnetAppUtils_print("[ ENETSBL ] receivePkts() failed to retrieve pkts: %d\r\n", status);
    }

    return rxReadyCnt;
}

static int32_t EnetSBL_setupCpswAle(void)
{
    Enet_IoctlPrms prms;
    CpswAle_SetPortStateInArgs setPortStateInArgs;
    CpswAle_SetUcastEntryInArgs setUcastInArgs;
    uint32_t entryIdx;
    int32_t status = 0;

    /* ALE entry with "secure" bit cleared is required */
    setUcastInArgs.addr.vlanId  = 0U;
    setUcastInArgs.info.portNum = CPSW_ALE_HOST_PORT_NUM;
    setUcastInArgs.info.blocked = false;
    setUcastInArgs.info.secure  = false;
    setUcastInArgs.info.super   = false;
    setUcastInArgs.info.ageable = false;
    setUcastInArgs.info.trunk   = false;
    EnetUtils_copyMacAddr(&setUcastInArgs.addr.addr[0U], gEnetSBL_LLDObj.hostMacAddr);
    ENET_IOCTL_SET_INOUT_ARGS(&prms, &setUcastInArgs, &entryIdx);

    ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, CPSW_ALE_IOCTL_ADD_UCAST, &prms,status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[ ENETSBL ] Failed to add ucast entry: %d\r\n", status);
    }

    /* Set host port to 'forwarding' state */
    if (status == ENET_SOK)
    {
        setPortStateInArgs.portNum   = CPSW_ALE_HOST_PORT_NUM;
        setPortStateInArgs.portState = CPSW_ALE_PORTSTATE_FORWARD;
        ENET_IOCTL_SET_IN_ARGS(&prms, &setPortStateInArgs);

        ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, CPSW_ALE_IOCTL_SET_PORT_STATE, &prms,status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("[ ENETSBL ] Failed to set ALE port state: %d\r\n", status);
        }
    }
    return status;
}

static void EnetSBL_closeEnet(void)
{
    Enet_IoctlPrms prms;
    int32_t status = 0;

    for (uint32_t portIdx = 0; portIdx < gEnetSBL_LLDObj.numMacPorts; portIdx++)
    {
        /* Close port link */
        ENET_IOCTL_SET_IN_ARGS(&prms, &gEnetSBL_LLDObj.macPort[portIdx]);

        ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, ENET_PER_IOCTL_CLOSE_PORT_LINK, &prms,status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("[ ENETSBL ] Failed to close port %d link: %d\r\n", gEnetSBL_LLDObj.macPort[portIdx], status);
        }
    }

    /* Close Enet driver */
    Enet_close(gEnetSBL_LLDObj.hEnet);

    gEnetSBL_LLDObj.hEnet = NULL;
}

static int32_t EnetSBL_showAlivePhys(void)
{
    Enet_IoctlPrms prms;
    bool alive = false;
    int32_t status;

    for (uint32_t phyAdd = 0U; phyAdd < ENET_MDIO_PHY_CNT_MAX; phyAdd++)
    {
        EnetApp_phyStateHandler();
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &phyAdd, &alive);

        ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, ENET_MDIO_IOCTL_IS_ALIVE, &prms,status);
        if (status == ENET_SOK)
        {
            if (alive == true)
            {
                EnetAppUtils_print("[ ENETSBL ] PHY %u is alive\r\n", phyAdd);
            }
        }
        else
        {
            EnetAppUtils_print("[ ENETSBL ] Failed to get PHY %u alive status: %d\r\n", phyAdd, status);
        }
    }

    return status;
}

static int32_t EnetSBL_waitForLinkUp(void)
{
    Enet_IoctlPrms prms;
    int32_t status = ENET_SOK;
    uint16_t linkUpWaitCount = 0;
    bool linked = false;

    while (!linked && linkUpWaitCount <= ENETSBL_LINKUP_POLL_MAX_COUNT)
    {
        EnetApp_phyStateHandler();
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &gEnetSBL_LLDObj.macPort[0], &linked);

        ENET_IOCTL(gEnetSBL_LLDObj.hEnet, gEnetSBL_LLDObj.coreId, ENET_PER_IOCTL_IS_PORT_LINK_UP, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("[ ENETSBL ]  Failed to get port link status: %d\r\n",  status);
            linked = false;
            break;
        }

        linkUpWaitCount++;
        if (!linked)
        {
            EnetAppUtils_print("[ ENETSBL ] %d Waiting for MAC Port %d to Linkup ....\r\n",linkUpWaitCount,(gEnetSBL_LLDObj.macPort[0]+1));
            ClockP_usleep(ENETSBL_LINKUP_POLL_SLEEP);
        }
    }

    if(linkUpWaitCount > ENETSBL_LINKUP_POLL_MAX_COUNT)
    {
        status = ENET_ETIMEOUT;
    }
    return status;
}

static uint32_t EnetApp_initEthFrameHdr(uint8_t *bufPtr)
{
    EthFrame *frame;
    uint32_t len = 0;

    frame = (EthFrame *)bufPtr;
    memcpy(frame->hdr.dstMac, enetAppAddrInfo.dstMac, ENET_MAC_ADDR_LEN);
    len += ENET_MAC_ADDR_LEN;
    memcpy(frame->hdr.srcMac, &gEnetSBL_LLDObj.hostMacAddr[0U], ENET_MAC_ADDR_LEN);
    len += ENET_MAC_ADDR_LEN;
    frame->hdr.etherType = Enet_htons(IPV4_ETHERTYPE);
    len += sizeof(frame->hdr.etherType);

    return len;
}

static bool EnetApp_verifyIPv4Checksum(uint16_t *header)
{
    uint32_t i;
    uint32_t sum = 0U;

    for(i = 0U; i < sizeof(EthAppIPv4Header)/2; i+=1)
    {
        sum += header[i];
    }

    /* Add the carries from top 16-bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (sum == 0xFFFFU);
}

static uint16_t EnetApp_calcIPv4Checksum(uint8_t src_ip[IPV4_ADDR_LEN], uint8_t dest_ip[IPV4_ADDR_LEN])
{
    uint32_t sum = (uint16_t)((IPV4_HDR_VER_IHL << 8) | IPV4_HDR_TOS) + (uint16_t)(IPV4_HDR_TOTAL_PKT_LEN) + (uint16_t)(IPV4_HDR_IPID) + (uint16_t)(IPV4_HDR_FLAGFRAFOFFSET) + (uint16_t)((IPV4_HDR_TTL << 8) | IPV4_HDR_UDP);
    uint32_t i;

    /* Add source IP addr*/
    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)src_ip[i] << 8) | (uint32_t)src_ip[i+1]);
    }

    /* Add destination IP addr*/
    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)dest_ip[i] << 8) | (uint32_t)dest_ip[i+1]);
    }

    /* Add the carries from top 16-bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    /* Return the one's complement of sum */
    return (uint16_t)((~sum) & 0xFFFF);
}

static uint32_t EnetApp_initIPv4Hdr(uint8_t *bufPtr)
{
    EthAppIPv4Header *ipv4Hdr;

    ipv4Hdr = (EthAppIPv4Header *)bufPtr;
    ipv4Hdr->verIHL = IPV4_HDR_VER_IHL;
    ipv4Hdr->tos    = IPV4_HDR_TOS;
    ipv4Hdr->totalPktLen = Enet_htons(IPV4_HDR_TOTAL_PKT_LEN);
    ipv4Hdr->ipId = Enet_htons(IPV4_HDR_IPID);
    ipv4Hdr->flagFragOffset = Enet_htons(IPV4_HDR_FLAGFRAFOFFSET);
    ipv4Hdr->ttl = IPV4_HDR_TTL;
    ipv4Hdr->protocol = IPV4_HDR_UDP;
    ipv4Hdr->hdrChksum = Enet_htons(EnetApp_calcIPv4Checksum((uint8_t*)enetAppAddrInfo.srcIP, (uint8_t*)enetAppAddrInfo.dstIP));
    memcpy(&ipv4Hdr->srcIP,enetAppAddrInfo.srcIP,sizeof(ipv4Hdr->srcIP));
    memcpy(&ipv4Hdr->dstIP,enetAppAddrInfo.dstIP,sizeof(ipv4Hdr->dstIP));
    return sizeof(EthAppIPv4Header);
}

static bool EnetApp_verifyUDPChecksum(EthAppIPv4Header *ipv4Hdr, EthAppUDPHeader *udpHdr, uint8_t *data_payload)
{
    uint8_t *payload = data_payload;
    uint32_t sum = 0U;
    uint32_t i;
    uint32_t length = (uint32_t)Enet_ntohs(udpHdr->length);

    /* Add the pseudo-header */
    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)ipv4Hdr->srcIP[i] << 8) | (uint32_t)ipv4Hdr->srcIP[i+1]);
    }

    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)ipv4Hdr->dstIP[i] << 8) | (uint32_t)ipv4Hdr->dstIP[i+1]);
    }

    sum += ipv4Hdr->protocol;
    sum += Enet_ntohs(udpHdr->length);
    sum += Enet_ntohs(udpHdr->srcPort);
    sum += Enet_ntohs(udpHdr->dstPort);
    sum += Enet_ntohs(udpHdr->length);
    sum += Enet_ntohs(udpHdr->csum);

    /* Add payload */
    while (length > sizeof(EthAppUDPHeader)+1)
    {
        sum += ((uint32_t)*(payload) << 8) | (uint32_t)*(payload + 1);
        payload += 2;

        /* If 32-bit number going to overflow, add accumulated carries from top 16-bits */
        if (sum & 0x80000000)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        length -= 2;
    }

    if (Enet_ntohs(udpHdr->length) & 1)
    {
        /* Add padding if the packet length is odd */
        sum += ((uint32_t)*(payload) << 8);
    }

    /* Add the carries from top 16-bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (sum == 0xFFFFU);
}

static uint16_t EnetApp_calcUDPChecksum(uint8_t src_ip[IPV4_ADDR_LEN], uint16_t src_port, uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t dest_port, uint16_t ip_protocol, uint16_t len, uint8_t *data_payload)
{
    uint8_t *payload = data_payload;
    uint32_t sum = 0U;
    uint32_t i;
    uint32_t length = (uint32_t)len;

    /* Add the pseudo-header */
    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)src_ip[i] << 8) | (uint32_t)src_ip[i+1]);
    }

    for(i = 0U; i < IPV4_ADDR_LEN; i+=2)
    {
        sum += (((uint32_t)dest_ip[i] << 8) | (uint32_t)dest_ip[i+1]);
    }

    sum += ip_protocol;
    sum += len;
    sum += src_port;
    sum += dest_port;
    sum += len;

    /* Add payload */
    while (length > sizeof(EthAppUDPHeader)+1)
    {
        sum += ((uint32_t)*(payload) << 8) | (uint32_t)*(payload + 1);
        payload += 2;

        /* If 32-bit number going to overflow, add accumulated carries from top 16-bits */
        if (sum & 0x80000000)
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        length -= 2;
    }

    if (len & 1)
    {
        /* Add padding if the packet length is odd */
        sum += ((uint32_t)*(payload) << 8);
    }

    /* Add the carries from top 16-bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    /* Return the one's complement of sum */
    return (uint16_t)((~sum) & 0xFFFF);
}

static uint32_t EnetApp_initUDPHdr(uint8_t *bufPtr, uint8_t *payload)
{
    EthAppUDPHeader *udpHdr;

    udpHdr = (EthAppUDPHeader *)bufPtr;
    udpHdr->srcPort = Enet_htons(enetAppAddrInfo.srcPortUDP);
    udpHdr->dstPort = Enet_htons(enetAppAddrInfo.dstPortUDP);
    udpHdr->length = Enet_htons(UDP_PKT_LEN);
    udpHdr->csum = Enet_htons(EnetApp_calcUDPChecksum((uint8_t*)enetAppAddrInfo.srcIP,
                                                      enetAppAddrInfo.srcPortUDP,
                                                      (uint8_t*)enetAppAddrInfo.dstIP,
                                                      enetAppAddrInfo.dstPortUDP,
                                                      (uint16_t)IPV4_HDR_UDP,
                                                      (uint16_t)UDP_PKT_LEN,
                                                      payload));
    return sizeof(EthAppUDPHeader);
}

static void EnetSBL_initPkt(EnetDma_Pkt *pPktInfo, uint8_t *udpPayload)
{
    uint32_t len = 0;

    memset(&gAappPktHeader[0],0,sizeof(gAappPktHeader));

    len += EnetApp_initEthFrameHdr(&gAappPktHeader[len]);
    len += EnetApp_initIPv4Hdr(&gAappPktHeader[len]);
    len += EnetApp_initUDPHdr(&gAappPktHeader[len], &udpPayload[0]);

    pPktInfo->chkSumInfo = 0U;
    pPktInfo->sgList.list[0].segmentFilledLen = len;
    pPktInfo->appPriv    = &gEnetSBL_LLDObj;
    pPktInfo->sgList.numScatterSegments = 2;
    pPktInfo->sgList.list[1].bufPtr = udpPayload;
    pPktInfo->sgList.list[1].segmentFilledLen = IPV4_HDR_TOTAL_PKT_LEN - (sizeof(EthAppUDPHeader) + sizeof(EthAppIPv4Header));
    pPktInfo->sgList.list[0].bufPtr = &gAappPktHeader[0];
    pPktInfo->sgList.list[0].disableCacheOps = true;

    CacheP_wbInv(pPktInfo->sgList.list[0].bufPtr, len, CacheP_TYPE_ALLD);
    pPktInfo->sgList.list[1].disableCacheOps = true;
    CacheP_wbInv(pPktInfo->sgList.list[1].bufPtr, (IPV4_HDR_TOTAL_PKT_LEN - (sizeof(EthAppUDPHeader) + sizeof(EthAppIPv4Header))), CacheP_TYPE_ALLD);
}

static void EnetSBL_initTxFreePktQ(void)
{
    EnetDma_Pkt *pPktInfo;
    uint32_t i;
    uint32_t scatterSegments[] = { ENET_MEM_LARGE_POOL_PKT_SIZE };

    /* Initialize all queues */
    EnetQueue_initQ(&gEnetSBL_LLDObj.txFreePktInfoQ);

    /* Initialize TX EthPkts and queue them to txFreePktInfoQ */
    for (i = 0U; i < ENET_SYSCFG_TOTAL_NUM_TX_PKT; i++)
    {
        pPktInfo = EnetMem_allocEthPkt(&gEnetSBL_LLDObj,
                                       ENETDMA_CACHELINE_ALIGNMENT,
                                       ENET_ARRAYSIZE(scatterSegments),
                                       scatterSegments);
        EnetAppUtils_assert(pPktInfo != NULL);
        ENET_UTILS_SET_PKT_APP_STATE(&pPktInfo->pktState, ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetDma_initPktInfo(pPktInfo);

        EnetQueue_enq(&gEnetSBL_LLDObj.txFreePktInfoQ, &pPktInfo->node);
    }

    EnetAppUtils_print("[ ENETSBL ] initQs() txFreePktInfoQ initialized with %d pkts\r\n",
                       EnetQueue_getQCount(&gEnetSBL_LLDObj.txFreePktInfoQ));
}

static void EnetSBL_initRxReadyPktQ(void)
{
    EnetDma_PktQ rxReadyQ;
    EnetDma_Pkt *pPktInfo;
    int32_t status;
    uint32_t i;
    uint32_t scatterSegments[] = { ENET_MEM_LARGE_POOL_PKT_SIZE };

    EnetQueue_initQ(&gEnetSBL_LLDObj.rxFreeQ);
    EnetQueue_initQ(&gEnetSBL_LLDObj.rxReadyQ);
    EnetQueue_initQ(&rxReadyQ);

    for (i = 0U; i < ENET_SYSCFG_TOTAL_NUM_RX_PKT; i++)
    {
        pPktInfo = EnetMem_allocEthPkt(&gEnetSBL_LLDObj,
                                       ENETDMA_CACHELINE_ALIGNMENT,
                                       ENET_ARRAYSIZE(scatterSegments),
                                       scatterSegments);
        EnetAppUtils_assert(pPktInfo != NULL);
        ENET_UTILS_SET_PKT_APP_STATE(&pPktInfo->pktState, ENET_PKTSTATE_APP_WITH_FREEQ);
        EnetQueue_enq(&gEnetSBL_LLDObj.rxFreeQ, &pPktInfo->node);
    }

    /* Retrieve any CPSW packets which are ready */
    status = EnetDma_retrieveRxPktQ(gEnetSBL_LLDObj.hRxCh, &rxReadyQ);
    EnetAppUtils_assert(status == ENET_SOK);
    /* There should not be any packet with DMA during init */
    EnetAppUtils_assert(EnetQueue_getQCount(&rxReadyQ) == 0U);

    EnetAppUtils_validatePacketState(&gEnetSBL_LLDObj.rxFreeQ,
                                     ENET_PKTSTATE_APP_WITH_FREEQ,
                                     ENET_PKTSTATE_APP_WITH_DRIVER);

    EnetDma_submitRxPktQ(gEnetSBL_LLDObj.hRxCh,
                         &gEnetSBL_LLDObj.rxFreeQ);

    /* Assert here as during init no. of DMA descriptors should be equal to
     * no. of free Ethernet buffers available with app */

    EnetAppUtils_assert(0U == EnetQueue_getQCount(&gEnetSBL_LLDObj.rxFreeQ));
}

static int32_t EnetSBL_openDma(void)
{
    int32_t status = ENET_SOK;

    /* Open the CPSW TX channel  */
    if (status == ENET_SOK)
    {
        EnetApp_GetDmaHandleInArgs     txInArgs;
        EnetApp_GetTxDmaHandleOutArgs  txChInfo;

        txInArgs.notifyCb = NULL;
        txInArgs.cbArg    = NULL;

        EnetApp_getTxDmaHandle(ENET_DMA_TX_CH0,
                               &txInArgs,
                               &txChInfo);

        gEnetSBL_LLDObj.txChNum = txChInfo.txChNum;
        gEnetSBL_LLDObj.hTxCh   = txChInfo.hTxCh;

        EnetSBL_initTxFreePktQ();

        if (NULL != gEnetSBL_LLDObj.hTxCh)
        {
            status = ENET_SOK;
            if (ENET_SOK != status)
            {
                EnetAppUtils_print("[ ENETSBL ] EnetUdma_startTxCh() failed: %d\r\n", status);
                status = ENET_EFAIL;
            }
        }
        else
        {
            EnetAppUtils_print("[ ENETSBL ] EnetDma_openTxCh() failed to open: %d\r\n",
                               status);
            status = ENET_EFAIL;
        }
    }

    /* Open the CPSW RX flow  */
    if (status == ENET_SOK)
    {
        EnetApp_GetRxDmaHandleOutArgs  rxChInfo;
        EnetApp_GetDmaHandleInArgs     rxInArgs;

        rxInArgs.cbArg    = NULL;
        rxInArgs.notifyCb = NULL;

        EnetApp_getRxDmaHandle(ENET_DMA_RX_CH0,
                              &rxInArgs,
                              &rxChInfo);
        gEnetSBL_LLDObj.hRxCh  = rxChInfo.hRxCh;
        EnetAppUtils_assert(rxChInfo.numValidMacAddress == 1);

        if (NULL == gEnetSBL_LLDObj.hRxCh)
        {
            EnetAppUtils_print("[ ENETSBL ] EnetDma_openRxCh() failed to open: %d\r\n",
                               status);
            EnetAppUtils_assert(NULL != gEnetSBL_LLDObj.hRxCh);
        }
        else
        {
            EnetAppUtils_assert(rxChInfo.numValidMacAddress > 0);
            EnetUtils_copyMacAddr(gEnetSBL_LLDObj.hostMacAddr, rxChInfo.macAddr[rxChInfo.numValidMacAddress - 1]);
            EnetAppUtils_print("[ ENETSBL ] EVM MAC address: ");
            EnetAppUtils_printMacAddr(gEnetSBL_LLDObj.hostMacAddr);
            /* Submit all ready RX buffers to DMA.*/
            EnetSBL_initRxReadyPktQ();
        }
    }

    return status;
}

static void EnetSBL_closeDma(void)
{
    EnetDma_PktQ fqPktInfoQ;
    EnetDma_PktQ cqPktInfoQ;

    EnetQueue_initQ(&fqPktInfoQ);
    EnetQueue_initQ(&cqPktInfoQ);


    /* There should not be any ready packet */
    EnetAppUtils_assert(0U == EnetQueue_getQCount(&gEnetSBL_LLDObj.rxReadyQ));

    /* Close RX channel */
    EnetApp_closeRxDma(ENET_DMA_RX_CH0,
                       gEnetSBL_LLDObj.hEnet,
                       gEnetSBL_LLDObj.coreKey,
                       gEnetSBL_LLDObj.coreId,
                       &fqPktInfoQ,
                       &cqPktInfoQ);

    EnetAppUtils_freePktInfoQ(&fqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&cqPktInfoQ);

    /* Close TX channel */
    EnetQueue_initQ(&fqPktInfoQ);
    EnetQueue_initQ(&cqPktInfoQ);

    EnetApp_closeTxDma(ENET_DMA_TX_CH0,
                       gEnetSBL_LLDObj.hEnet,
                       gEnetSBL_LLDObj.coreKey,
                       gEnetSBL_LLDObj.coreId,
                       &fqPktInfoQ,
                       &cqPktInfoQ);

    EnetAppUtils_freePktInfoQ(&fqPktInfoQ);
    EnetAppUtils_freePktInfoQ(&cqPktInfoQ);

    EnetAppUtils_freePktInfoQ(&gEnetSBL_LLDObj.rxFreeQ);
    EnetAppUtils_freePktInfoQ(&gEnetSBL_LLDObj.txFreePktInfoQ);

}

static bool EnetSBL_checkFrame(EthFrame *frame, uint32_t dataLen)
{
    bool matchingPkt = true;
    uint8_t* payload = frame->payload;
    EthAppIPv4Header *ipv4Frame = (EthAppIPv4Header *) payload;
    EthAppUDPHeader *udpFrame = (EthAppUDPHeader *) &payload[sizeof(EthAppIPv4Header)];

    if (dataLen < (ETH_HDR_SIZE + IPV4_HDR_SIZE + UDP_HDR_SIZE))
    {
        matchingPkt = false;
    }

    matchingPkt = matchingPkt && (memcmp(&ipv4Frame->dstIP[0], &enetAppAddrInfo.srcIP[0], sizeof(ipv4Frame->dstIP)) == 0);
    matchingPkt = matchingPkt && (frame->hdr.etherType == Enet_htons(IPV4_ETHERTYPE));
    matchingPkt = matchingPkt && (ipv4Frame->protocol == IPV4_HDR_UDP);
    matchingPkt = matchingPkt && (udpFrame->srcPort == Enet_htons(enetAppAddrInfo.srcPortUDP));
    matchingPkt = matchingPkt && EnetApp_verifyIPv4Checksum((uint16_t*)ipv4Frame);
    matchingPkt = matchingPkt && EnetApp_verifyUDPChecksum(ipv4Frame, udpFrame, &payload[sizeof(EthAppIPv4Header)+sizeof(EthAppUDPHeader)]);
    matchingPkt = matchingPkt && (memcmp(&payload[sizeof(EthAppIPv4Header)+sizeof(EthAppUDPHeader)], &EnetSBL_MagicNum, MGC_NUM_SIZE) == 0);

    return matchingPkt;
}

void EnetApp_updateCpswInitCfg(Enet_Type enetType,  uint32_t instId, Cpsw_Cfg *cpswCfg)
{
    CpswHostPort_Cfg *hostPortCfg = &cpswCfg->hostPortCfg;
    CpswAle_Cfg *aleCfg = &cpswCfg->aleCfg;
    CpswCpts_Cfg *cptsCfg = &cpswCfg->cptsCfg;

    cpswCfg->vlanCfg.vlanAware = false;

    /* Host port config */
    hostPortCfg->removeCrc      = true;
    hostPortCfg->padShortPacket = true;
    hostPortCfg->passCrcErrors  = true;

    /* ALE config */
    aleCfg->modeFlags                          = CPSW_ALE_CFG_MODULE_EN;
    aleCfg->agingCfg.autoAgingEn               = true;
    aleCfg->agingCfg.agingPeriodInMs           = 1000;
    aleCfg->nwSecCfg.vid0ModeEn                = true;
    aleCfg->vlanCfg.aleVlanAwareMode           = false;
    aleCfg->vlanCfg.cpswVlanAwareMode          = false;
    aleCfg->vlanCfg.unknownUnregMcastFloodMask = CPSW_ALE_ALL_PORTS_MASK;
    aleCfg->vlanCfg.unknownRegMcastFloodMask   = CPSW_ALE_ALL_PORTS_MASK;
    aleCfg->vlanCfg.unknownVlanMemberListMask  = CPSW_ALE_ALL_PORTS_MASK;

    /* CPTS config */
    /* Note: Timestamping and MAC are not supported together because of
     * IP limitation, so disabling timestamping for this application */
    cptsCfg->hostRxTsEn = false;

}
