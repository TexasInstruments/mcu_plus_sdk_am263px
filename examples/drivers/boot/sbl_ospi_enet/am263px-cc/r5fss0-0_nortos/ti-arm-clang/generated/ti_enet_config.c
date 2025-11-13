/*
 *  Copyright (c) Texas Instruments Incorporated 2024
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
 * \file ti_enet_config.c
 *
 * \brief This file contains enet driver memory allocation related functionality.
 */

/*
 * Enet DMA memory allocation utility functions.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>

#include <enet.h>
#include <include/core/enet_utils.h>

#include <include/core/enet_dma.h>

#include "enet_appmemutils.h"
#include "enet_appmemutils_cfg.h"
#include "enet_apputils.h"

#include "ti_enet_config.h"

#include <drivers/hw_include/cslr_soc.h>
#include <networking/enet/hw_include/csl_cpswitch.h>
#include <networking/enet/core/src/dma/cpdma/enet_cpdma_priv.h>
#include <networking/enet/core/src/dma/cpdma/enet_cdma_memcfg.h>


#define ENET_MEM_LARGE_POOL_NUM_PKTS        (48U)
#define ENET_MEM_MEDIUM_POOL_NUM_PKTS       (0U)
#define ENET_MEM_SMALL_POOL_NUM_PKTS        (0U)
#define ENET_PKTINFOMEM_ONLY_NUM_PKTS       (16U)


/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */
/* Eth packet info memory Q - Large pool */
static EnetDma_Pkt gAppPktInfoMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS];

/* Eth packet Large pool memories */
static uint8_t gEthPktMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS][ENET_MEM_LARGE_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_LargePool[ENET_MEM_LARGE_POOL_NUM_PKTS];

/* Eth packet info memory Q - Medium pool */
static EnetDma_Pkt gAppPktInfoMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS];

/* Eth packet Medium pool memories */
static uint8_t gEthPktMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS][ENET_MEM_MEDIUM_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_MediumPool[ENET_MEM_MEDIUM_POOL_NUM_PKTS];

/* Eth packet info memory Q - Small pool */
static EnetDma_Pkt gAppPktInfoMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS];

/* Eth packet Small pool memories */
static uint8_t gEthPktMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS][ENET_MEM_SMALL_POOL_PKT_SIZE]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_DMA_PKT_MEMPOOL")));
static EnetMem_AppPktInfoMem gAppPktInfoContainerMem_SmallPool[ENET_MEM_SMALL_POOL_NUM_PKTS];



static const EnetMem_Cfg gEthMemCfg =
{
    .pktBufPool =
    {

        [ENET_MEM_POOLIDX_LARGE] =
        {
            .pktSize     = ENET_MEM_LARGE_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_LARGE_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_LargePool,
            .pktInfoSize = sizeof(gAppPktInfoMem_LargePool),
            .pktBufMem   = &gEthPktMem_LargePool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_LargePool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_LargePool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_LargePool),
        },

        [ENET_MEM_POOLIDX_MEDIUM] =
        {
            .pktSize     = ENET_MEM_MEDIUM_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_MEDIUM_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_MediumPool,
            .pktInfoSize = sizeof(gAppPktInfoMem_MediumPool),
            .pktBufMem   = &gEthPktMem_MediumPool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_MediumPool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_MediumPool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_MediumPool),
        },

        [ENET_MEM_POOLIDX_SMALL] =
        {
            .pktSize     = ENET_MEM_SMALL_POOL_PKT_SIZE,
            .numPkts     = (ENET_MEM_SMALL_POOL_NUM_PKTS),
            .pktInfoMem  = gAppPktInfoMem_SmallPool,
            .pktInfoSize = sizeof(gAppPktInfoMem_SmallPool),
            .pktBufMem   = &gEthPktMem_SmallPool[0][0],
            .pktBufSize  = sizeof(gEthPktMem_SmallPool),
            .pktInfoContainerMem = gAppPktInfoContainerMem_SmallPool,
            .pktInfoContainerSize = sizeof(gAppPktInfoContainerMem_SmallPool),
        },

    },
};

const EnetMem_Cfg * EnetMem_getCfg(void)
{
    return &gEthMemCfg;
}

/* TODO: can be passed as configuration parameters  */
/* Reserved Memory for CPPI descriptors */
#define CPSW_CPPI_DESC_NUM_DESC                      (32)

static  EnetCpdma_cppiDesc gCpswDescMem[CPSW_CPPI_DESC_NUM_DESC]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT),
                section(".bss:ENET_CPPI_DESC")));

void EnetSoc_getCppiDescInfo(Enet_Type enetType,
                             uint32_t instId,
                             uintptr_t *descStartAddr,
                             uint32_t *size)
{
    *descStartAddr = (uintptr_t)gCpswDescMem;
    *size = sizeof(gCpswDescMem);
}

/* RX flow object memories */
static EnetCpdma_RxChObjMem gCpswDmaRxChObjMem[ENET_SYSCFG_RX_FLOWS_NUM]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_CPDMA_OBJ_MEM")));

/* Tx channel object memories */
static EnetCpdma_TxChObjMem gCpswDmaTxChObjMem[ENET_SYSCFG_TX_CHANNELS_NUM]
__attribute__ ((aligned(ENETDMA_CACHELINE_ALIGNMENT), section(".bss:ENET_CPDMA_OBJ_MEM")));

static const EnetCpdma_MemCfg gEthCpdmaCfg =
{
    .rxChObjMemCfg =
    {
        .numRxCh = ENET_ARRAYSIZE(gCpswDmaRxChObjMem),
        .rxChObjMemContainerBase = gCpswDmaRxChObjMem,
        .rxChObjMemContainerSize = sizeof(gCpswDmaRxChObjMem),
    },
    .txChObjMemCfg =
    {
        .numTxCh = ENET_ARRAYSIZE(gCpswDmaTxChObjMem),
        .txChObjMemContainerBase = gCpswDmaTxChObjMem,
        .txChObjMemContainerSize = sizeof(gCpswDmaTxChObjMem),
    },
};

const EnetCpdma_MemCfg * EnetCpdmaMem_getCfg(void)
{
    return &gEthCpdmaCfg;
}

/*
 * Enet DMA memory allocation utility functions.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <enet.h>
#include "enet_appmemutils.h"
#include "enet_appmemutils_cfg.h"
#include "enet_apputils.h"
#include <enet_cfg.h>
#include <include/core/enet_per.h>
#include <include/core/enet_utils.h>
#include <include/core/enet_dma.h>

#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/ClockP.h>

#include "ti_enet_config.h"
#include "ti_drivers_config.h"
#include "ti_enet_open_close.h"
#include <enet_appboardutils.h>
#include <utils/include/enet_appsoc.h>

/* Maximum number of MAC address to be set per CPSW peripheral.
CPSW_3G can support upto two MACs. But for multicore applications,
each core needs one MAC address. So, this is set to 5 here.
2 addresses for MAC, and 3 addresses for remote cores */
#define MAC_ADDR_LIST_LEN_SHARED_PER_PER (5U)
#define ENET_MAX_NUM_MAC_ADDR_STORED (2U)

typedef struct EnetApp_MacAddrElem_s
{
    EnetQ_Node node;
    uint8_t macAddr[ENET_MAC_ADDR_LEN];
} EnetApp_MacAddrElem;

typedef struct EnetApp_MacAddrPool_s
{
    bool isInitialized;
    uint32_t macAddrContainerLen;
    EnetApp_MacAddrElem macAddrContainer[ENET_MAX_NUM_MAC_ADDR_STORED];
    EnetQ freeMacAddrQ;
} EnetApp_MacAddrPool;

EnetApp_MacAddrPool gEnetMacAddrPool = { .isInitialized = false };

void EnetApp_getEnetInstInfo(uint32_t enetInstanceId, Enet_Type *enetType, uint32_t *instId)
{
    uint32_t idx = 0;
    const uint32_t instInfoTable[ENET_SYSCFG_MAX_ENET_INSTANCES][3] = { {0, ENET_CPSW_3G, 0}, }; 

    EnetAppUtils_assert(enetInstanceId < ENET_SYSCFG_MAX_ENET_INSTANCES);
    for (idx = 0; idx < ENET_SYSCFG_MAX_ENET_INSTANCES; idx++)
    {
        if (instInfoTable[idx][0] ==  enetInstanceId)
        {
            *enetType = (Enet_Type)instInfoTable[idx][1];
            *instId   = instInfoTable[idx][2];
            break;
        }
    }

    /* assert if enetInstanceId is not found in the table. Wrong enetInstanceId parameter */
    EnetAppUtils_assert(idx < ENET_SYSCFG_MAX_ENET_INSTANCES);
}

void EnetApp_getEnetInstMacInfo(Enet_Type enetType,
                             uint32_t instId,
                             Enet_MacPort macPortList[],
                             uint8_t *numMacPorts)
{
    *numMacPorts = 1;
    macPortList[0] = ENET_MAC_PORT_2;
}

void EnetApp_acquireHandleInfo(Enet_Type enetType, uint32_t instId,
                                   EnetApp_HandleInfo *handleInfo)
{
    handleInfo->hEnet = Enet_getHandle(enetType, instId);
}

void EnetApp_coreAttach(Enet_Type enetType, uint32_t instId,
                            uint32_t coreId,
                            EnetPer_AttachCoreOutArgs *attachInfo)
{
    Enet_IoctlPrms prms;
    int32_t status;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    if (NULL_PTR != attachInfo)
    {
        ENET_IOCTL_SET_INOUT_ARGS(&prms, &coreId, attachInfo);
        ENET_IOCTL(hEnet,
                   coreId,
                   ENET_PER_IOCTL_ATTACH_CORE,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("EnetApp_coreAttach failed ENET_PER_IOCTL_ATTACH_CORE: %d\r\n", status);
            EnetAppUtils_assert(false);
        }
    }
    else
    {
        EnetAppUtils_assert(false);
    }
}

void EnetApp_coreDetach(Enet_Type enetType, uint32_t instId,
                            uint32_t coreId,
                            uint32_t coreKey)
{
    Enet_IoctlPrms prms;
    int32_t status;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    ENET_IOCTL_SET_IN_ARGS(&prms, &coreKey);
    ENET_IOCTL(hEnet,
               coreId,
               ENET_PER_IOCTL_DETACH_CORE,
               &prms,
               status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("close() failed ENET_PER_IOCTL_DETACH_CORE: %d\r\n", status);
        EnetAppUtils_assert(false);
    }
}

void EnetApp_releaseHandleInfo(Enet_Type enetType, uint32_t instId)
{
    EnetApp_driverClose(enetType, instId);
}

bool EnetApp_isPortLinked(Enet_Handle hEnet)
{
    uint32_t coreId = EnetSoc_getCoreId();
    uint32_t linkUpMask = 0U;
    bool linkUp;
    linkUpMask |= (EnetAppUtils_isPortLinkUp(hEnet, coreId, ENET_MAC_PORT_2)) << 0;

    linkUp =  (linkUpMask != 0) ? true : false;
    return linkUp;
}

int32_t EnetAppSoc_fillMacAddrList(uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                   uint32_t maxMacEntries,
                                   uint32_t *pAvailMacEntries)
{
    uint32_t numEfusedMacAddrs;
    uint32_t numBoardMacAddrs;
    int32_t status = ENET_SOK;

    if (maxMacEntries >= 1)
    {
        numEfusedMacAddrs = maxMacEntries;
        numBoardMacAddrs = 0;
        EnetSoc_getEFusedMacAddrs(&macAddr[0], &numEfusedMacAddrs);

        if (maxMacEntries > numEfusedMacAddrs)
        {
            EnetBoard_getMacAddrList(&macAddr[numEfusedMacAddrs], (maxMacEntries - numEfusedMacAddrs),&numBoardMacAddrs);
        }
        EnetAppUtils_assert(pAvailMacEntries != NULL);
        *pAvailMacEntries = numEfusedMacAddrs + numBoardMacAddrs;
    }
    else
    {
        status = ENET_EINVALIDPARAMS;
    }

    return status;
}

static void EnetApp_initializeMacAddrPool()
{
    uint8_t macAddrList[ENET_MAX_NUM_MAC_ADDR_STORED][ENET_MAC_ADDR_LEN];
    uint32_t filledMacEntries = 0;

    if (!gEnetMacAddrPool.isInitialized)
    {
        EnetQueue_initQ(&gEnetMacAddrPool.freeMacAddrQ);
       macAddrList[filledMacEntries][0] = 0x70;
       macAddrList[filledMacEntries][1] = 0xff;
       macAddrList[filledMacEntries][2] = 0x76;
       macAddrList[filledMacEntries][3] = 0x1d;
       macAddrList[filledMacEntries][4] = 0xec;
       macAddrList[filledMacEntries][5] = 0xf2;
       filledMacEntries++;

       macAddrList[filledMacEntries][0] = 0x70;
       macAddrList[filledMacEntries][1] = 0xff;
       macAddrList[filledMacEntries][2] = 0x76;
       macAddrList[filledMacEntries][3] = 0x1d;
       macAddrList[filledMacEntries][4] = 0xec;
       macAddrList[filledMacEntries][5] = 0xe3;
       filledMacEntries++;

        gEnetMacAddrPool.macAddrContainerLen =  filledMacEntries;

        for (uint32_t entryIdx = 0; entryIdx < gEnetMacAddrPool.macAddrContainerLen; entryIdx++)
        {
            EnetApp_MacAddrElem* pElem = &gEnetMacAddrPool.macAddrContainer[entryIdx];
            memcpy(pElem->macAddr, &macAddrList[entryIdx][0], ENET_MAC_ADDR_LEN);
            EnetQueue_enq(&gEnetMacAddrPool.freeMacAddrQ, &pElem->node);
        }
        gEnetMacAddrPool.isInitialized = true;
    }
}

static int32_t EnetApp_getMacAddrFromPool(uint8_t macAddr[ENET_MAC_ADDR_LEN])
{
    int32_t status = ENET_EALLOC;
    EnetApp_MacAddrElem* pElem = (EnetApp_MacAddrElem*)EnetQueue_deq(&gEnetMacAddrPool.freeMacAddrQ);
    if (pElem != NULL)
    {
        memcpy(macAddr, pElem->macAddr, ENET_MAC_ADDR_LEN);
        status = ENET_SOK;
    }
    return status;
}

static int32_t EnetApp_releaseMacAddrToPool(uint8_t macAddr[ENET_MAC_ADDR_LEN])
{
    int32_t status = ENET_EALLOC;
    
    for (uint32_t entryIdx = 0; entryIdx < gEnetMacAddrPool.macAddrContainerLen; entryIdx++)
    {
        EnetApp_MacAddrElem* pElem = &gEnetMacAddrPool.macAddrContainer[entryIdx];
        if (0 == memcmp(pElem->macAddr, &macAddr[0], ENET_MAC_ADDR_LEN))
        {
            EnetQueue_enq(&gEnetMacAddrPool.freeMacAddrQ, &pElem->node);
            status = ENET_SOK;
            break;
        }
    }
    return status;
}

int32_t EnetAppSoc_getMacAddrList(Enet_Type enetType,
                                  uint32_t instId,
                                  uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                  uint32_t *pAvailMacEntries)
{
    const uint32_t sharedListLen = MAC_ADDR_LIST_LEN_SHARED_PER_PER;

    EnetApp_initializeMacAddrPool();
    *pAvailMacEntries = 0;
    for (uint32_t idx = 0; idx < sharedListLen; idx++)
    {
        if (EnetApp_getMacAddrFromPool(macAddr[idx]) == ENET_SOK)
        {
            (*pAvailMacEntries)++;
        }
        else
        {
            break;
        }
    }

    return ((*pAvailMacEntries) == 0) ? ENET_EINVALIDPARAMS : ENET_SOK;
}

int32_t EnetAppSoc_releaseMacAddrList(uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                                        uint32_t maxMacEntries)
{
    int32_t status = ENET_SOK;
    for (uint32_t idx = 0; idx < maxMacEntries; idx++)
    {
        status = EnetApp_releaseMacAddrToPool(macAddr[idx]);
        if (status != ENET_SOK)
        {
            break;
        }
    }
    return status;
}

