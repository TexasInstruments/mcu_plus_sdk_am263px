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
 * \file ti_enet_open_close.c
 *
 * \brief This file contains enet driver memory allocation related functionality.
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
#include <enet_board.h>
#include <enet_cfg.h>
#include <include/core/enet_per.h>
#include <include/core/enet_utils.h>
#include <include/core/enet_dma.h>
#include <include/common/enet_utils_dflt.h>
#include <include/per/cpsw.h>
#include <priv/per/cpsw_cpdma_priv.h>
#include <include/core/enet_soc.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/EventP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/QueueP.h>

#include "ti_enet_config.h"
#include "ti_drivers_config.h"
#include "ti_enet_open_close.h"
#include "ti_board_config.h"
#include <utils/include/enet_appsoc.h>

#if defined(SOC_AM275X)
#include <drivers/pinmux.h>
void EnetApp_fixPinmux();
#endif

#define ENETAPP_PHY_STATEHANDLER_TASK_PRIORITY        (7U)
#define ENETAPP_PHY_STATEHANDLER_TASK_STACK     (3 * 1024)
#define AppEventId_CPSW_PERIODIC_POLL    (1 << 3)

#define ENETAPP_NUM_IET_VERIFY_ATTEMPTS               (20U)



typedef struct EnetAppTxDmaSysCfg_Obj_s
{
    /* TX channel handle */
    EnetDma_TxChHandle hTxCh;
    /* TX channel number */
    uint32_t txChNum;
} EnetAppTxDmaSysCfg_Obj;

typedef struct EnetAppRxDmaSysCfg_Obj_s
{
    /* RX channel handle */
    EnetDma_RxChHandle hRxCh;
    /* RX channel number */
    uint32_t rxChNum;
    /* mac Address valid */
    uint32_t numValidMacAddress;
    /* MAC address. It's port/ports MAC address in Dual-MAC or
     * host port MAC address in Switch */
    uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN];
} EnetAppRxDmaSysCfg_Obj;


static void EnetAppUtils_openTxCh(Enet_Handle hEnet,
                                  uint32_t coreKey,
                                  uint32_t coreId,
                                  uint32_t *pTxChNum,
                                  EnetDma_TxChHandle *pTxChHandle,
                                  EnetCpdma_OpenTxChPrms *pCpswTxChCfg);

static void EnetAppUtils_openRxCh(Enet_Handle hEnet,
                                  uint32_t coreKey,
                                  uint32_t coreId,
                                  uint32_t *pRxChNum,
                                  EnetDma_RxChHandle *pRxChHandle,
                                  EnetCpdma_OpenRxChPrms *pCpswRxChCfg,
                                  uint32_t allocMacAddrCnt,
                                  uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN]);

static void EnetAppUtils_setCommonRxChPrms(EnetCpdma_OpenRxChPrms *pRxChPrms);

static void EnetAppUtils_setCommonTxChPrms(EnetCpdma_OpenTxChPrms *pTxChPrms);

static void EnetApp_openRxDma(EnetAppRxDmaSysCfg_Obj *rx,
                              uint32_t numRxPkts,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId,
                              uint32_t allocMacAddrCnt);

static void EnetApp_openTxDma(EnetAppTxDmaSysCfg_Obj *tx,
                              uint32_t numTxPkts,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId);

static void EnetAppUtils_closeTxCh(Enet_Handle hEnet,
                                   uint32_t coreKey,
                                   uint32_t coreId,
                                   EnetDma_PktQ *pFqPktInfoQ,
                                   EnetDma_PktQ *pCqPktInfoQ,
                                   EnetDma_TxChHandle hTxChHandle,
                                   uint32_t txChNum);

static void EnetAppUtils_closeRxCh(Enet_Handle hEnet,
                                   uint32_t coreKey,
                                   uint32_t coreId,
                                   EnetDma_PktQ *pFqPktInfoQ,
                                   EnetDma_PktQ *pCqPktInfoQ,
                                   EnetDma_RxChHandle hRxChHandle,
                                   uint32_t rxChNum,
                                   uint32_t freeHostMacAddrCount,
                                   uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN]);




typedef struct EnetAppDmaSysCfg_Obj_s
{
    EnetAppTxDmaSysCfg_Obj tx[ENET_SYSCFG_TX_CHANNELS_NUM];
    EnetAppRxDmaSysCfg_Obj rx[ENET_SYSCFG_RX_FLOWS_NUM];
} EnetAppDmaSysCfg_Obj;


typedef struct EnetAppSysCfg_Obj_s
{

    Enet_Handle hEnet;

    EnetAppDmaSysCfg_Obj dma;

    ClockP_Object timerObj;

    volatile bool timerTaskShutDownFlag;

    TaskP_Object task_phyStateHandlerObj;

    SemaphoreP_Object timerSemObj;

    volatile bool timerTaskShutDownDoneFlag;

    uint8_t appPhyStateHandlerTaskStack[ENETAPP_PHY_STATEHANDLER_TASK_STACK] __attribute__ ((aligned(32)));
}EnetAppSysCfg_Obj;

const EnetApp_IET_Config gEnetApp_IETCfg[] =
{

};

static EnetAppSysCfg_Obj gEnetAppSysCfgObj;

static void EnetApp_initLinkArgs(const Enet_Type enetType,
                          const uint32_t instId,
                          const Enet_MacPort macPort,
                          const bool isInNoPhyMode,
                          EnetPer_PortLinkCfg *linkArgs);

static int32_t EnetApp_enablePorts(Enet_Handle hEnet,
                                   Enet_Type enetType,
                                   uint32_t instId,
                                   uint32_t coreId,
                                   Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                   uint8_t numMacPorts);

static void EnetApp_getCpswInitCfg(Enet_Type enetType,
                                   uint32_t instId,
                                   Cpsw_Cfg *cpswCfg);

static void EnetApp_getMacPortInitConfig(CpswMacPort_Cfg *pMacPortCfg, const Enet_MacPort portIdx);

void EnetApp_getMacPortLinkCfg(EnetMacPort_LinkCfg *pMacPortLinkCfg, const Enet_MacPort portIdx);

static void EnetApp_openDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                    Enet_Type enetType,
                                    uint32_t instId,
                                    Enet_Handle hEnet,
                                    uint32_t coreKey,
                                    uint32_t coreId);

static void EnetApp_openAllRxDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                         Enet_Handle hEnet,
                                         uint32_t coreKey,
                                         uint32_t coreId);

static void EnetApp_openAllTxDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                         Enet_Handle hEnet,
                                         uint32_t coreKey,
                                         uint32_t coreId);

static void EnetApp_ConfigureDscpMapping(Enet_Type enetType,
                                         uint32_t instId,
                                         uint32_t coreId,
                                         Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                         uint8_t numMacPorts);

void EnetApp_phyStateHandler();


static Enet_Handle EnetApp_doCpswOpen(Enet_Type enetType, uint32_t instId, const Cpsw_Cfg *cpswCfg)
{
    void *perCfg = NULL_PTR;
    uint32_t cfgSize;
    Enet_Handle hEnet;

    EnetAppUtils_assert(true == Enet_isCpswFamily(enetType));

    perCfg = (void *)cpswCfg;
    cfgSize = sizeof(*cpswCfg);

    hEnet = Enet_open(enetType, instId, perCfg, cfgSize);
    if(hEnet == NULL_PTR)
    {
        EnetAppUtils_print("Enet_open failed\r\n");
        EnetAppUtils_assert(hEnet != NULL_PTR);
    }

    return hEnet;
}


static void EnetApp_timerCb(ClockP_Object *clkInst, void * arg)
{
    SemaphoreP_Object *pTimerSem = (SemaphoreP_Object *)arg;

    /* Tick! */
    SemaphoreP_post(pTimerSem);
}
void EnetApp_phyStateHandler()
{
    Enet_periodicTick(gEnetAppSysCfgObj.hEnet);
}

void EnetApp_driverInit()
{
/* keep this implementation that is generic across enetType and instId.
 * Initialization should be done only once.
 */
    int32_t status = ENET_SOK;
    EnetUtils_Cfg utilsPrms;

    /* Initialize Enet driver with default utils */
    Enet_initUtilsCfg(&utilsPrms);
    Enet_init(&utilsPrms);

    status = EnetMem_init();
    EnetAppUtils_assert(ENET_SOK == status);
    #if defined(SOC_AM275X)
    EnetApp_fixPinmux();
    #endif
}

void EnetApp_driverDeInit()
{
/* keep this implementation that is generic across enetType and instId.
 * Denitialization should be done only once.
 */

    EnetMem_deInit();
    Enet_deinit();
}

int32_t EnetApp_driverOpen(Enet_Type enetType, uint32_t instId)
{
    int32_t status = ENET_SOK;
    Cpsw_Cfg cpswCfg;
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    uint8_t numMacPorts;
    uint32_t selfCoreId;
    EnetRm_ResCfg *resCfg = &cpswCfg.resCfg;
    EnetApp_HandleInfo handleInfo;
    EnetPer_AttachCoreOutArgs attachInfo;

    EnetDma_Cfg dmaCfg;

    EnetAppUtils_assert(Enet_isCpswFamily(enetType) == true);
    EnetApp_getEnetInstMacInfo(enetType, instId, macPortList, &numMacPorts);

    /* Set configuration parameters */
    selfCoreId   = EnetSoc_getCoreId();
    cpswCfg.dmaCfg = &dmaCfg;
    Enet_initCfg(enetType, instId, &cpswCfg, sizeof(cpswCfg));
    cpswCfg.dmaCfg = &dmaCfg;

    EnetApp_getCpswInitCfg(enetType, instId, &cpswCfg);

    resCfg = &cpswCfg.resCfg;
    EnetAppUtils_initResourceConfig(enetType, instId, selfCoreId, resCfg);

    dmaCfg.maxTxChannels = ENET_SYSCFG_TX_CHANNELS_NUM;
    dmaCfg.maxRxChannels = ENET_SYSCFG_RX_FLOWS_NUM;

    EnetApp_updateCpswInitCfg(enetType, instId, &cpswCfg);


    gEnetAppSysCfgObj.hEnet = EnetApp_doCpswOpen(enetType, instId, &cpswCfg);
    EnetAppUtils_assert(NULL != gEnetAppSysCfgObj.hEnet);

    EnetApp_enablePorts(gEnetAppSysCfgObj.hEnet, enetType, instId, selfCoreId, macPortList, numMacPorts);

    EnetApp_ConfigureDscpMapping(enetType, instId, selfCoreId, macPortList, numMacPorts);


    /* Open all DMA channels */
    EnetApp_acquireHandleInfo(enetType,
                              instId,
                              &handleInfo);

    (void)handleInfo; /* Handle info not used. Kill warning */
    EnetApp_coreAttach(enetType,
                       instId,
                       selfCoreId,
                       &attachInfo);
    EnetApp_openDmaChannels(&gEnetAppSysCfgObj.dma,
                            enetType,
                            instId,
                            gEnetAppSysCfgObj.hEnet,
                            attachInfo.coreKey,
                            selfCoreId);
    return status;
}

static void EnetApp_ConfigureDscpMapping(Enet_Type enetType,
                                         uint32_t instId,
                                         uint32_t coreId,
                                         Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                         uint8_t numMacPorts)
{
    Enet_IoctlPrms prms;
    int32_t status;
    EnetMacPort_SetIngressDscpPriorityMapInArgs setMacDscpInArgs;
    EnetPort_DscpPriorityMap setHostDscpInArgs;
    CpswAle_DfltThreadCfg dfltThreadCfg;
    uint32_t pri;
    uint8_t i;

    Enet_Handle hEnet = Enet_getHandle(enetType, instId);
    memset(&setMacDscpInArgs, 0, sizeof(setMacDscpInArgs));
    /* Each Port can have different dscp priority mapping values */
    for (i = 0U; i < numMacPorts; i++)
    {
        setMacDscpInArgs.macPort = macPortList[i];
        setMacDscpInArgs.dscpPriorityMap.dscpIPv4En = true;
        /* Example Mapping: 0 to 7  dscp values are mapped to 0 priority
         *                  8 to 15 dscp values are mapped to 1 priority
         *                  and so on
         *                  56 to 63 dscp values are mapped to 7 priority
         */
        for (pri = 0U; pri < 64U; pri++)
        {
            setMacDscpInArgs.dscpPriorityMap.tosMap[pri] = pri/8;
        }
        ENET_IOCTL_SET_IN_ARGS(&prms, &setMacDscpInArgs);
        ENET_IOCTL(hEnet, coreId, ENET_MACPORT_IOCTL_SET_INGRESS_DSCP_PRI_MAP, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("Failed to set dscp Priority map for Port %u - %d \r\n", macPortList[i], status);
        }
    }

    /* Fill the dscp priority mapping reg of host port */
    memset(&setHostDscpInArgs, 0, sizeof(setHostDscpInArgs));
    setHostDscpInArgs.dscpIPv4En = true;
    for (pri = 0U; pri < 64U; pri++)
    {
        setHostDscpInArgs.tosMap[pri] = pri/8;
    }
    ENET_IOCTL_SET_IN_ARGS(&prms, &setHostDscpInArgs);
    ENET_IOCTL(hEnet, coreId, ENET_HOSTPORT_IOCTL_SET_INGRESS_DSCP_PRI_MAP, &prms, status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("Failed to set dscp Priority map for Host Port - %d\r\n", status);
    }
    /* Enable the p0_rx_remap_dscp_ipv4 in CPPI_P0_Control for host port, This is done through syscfg */

    /* Enable Channel override (in EnetApp_updateCpswInitCfg)*/
    /* Configure the default threadId */
    memset(&dfltThreadCfg, 0, sizeof(dfltThreadCfg));
    dfltThreadCfg.dfltThreadEn = true;
    dfltThreadCfg.threadId = 0;
    ENET_IOCTL_SET_IN_ARGS(&prms, &dfltThreadCfg);
    ENET_IOCTL(hEnet,
               coreId,
               CPSW_ALE_IOCTL_SET_DEFAULT_THREADCFG,
               &prms,
               status);
}

void EnetApp_macMode2MacMii(emac_mode macMode, EnetMacPort_Interface *pMii)
{
    switch (macMode)
    {
        case MII:
        {
            pMii->layerType    = ENET_MAC_LAYER_MII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_STANDARD;
            pMii->variantType  = ENET_MAC_VARIANT_NONE;
            break;
        }
        case RMII:
        {
            pMii->layerType    = ENET_MAC_LAYER_MII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_REDUCED;
            pMii->variantType  = ENET_MAC_VARIANT_NONE;
            break;
        }
        case RGMII:
        {
            pMii->layerType    = ENET_MAC_LAYER_GMII;
            pMii->sublayerType = ENET_MAC_SUBLAYER_REDUCED;
            pMii->variantType  = ENET_MAC_VARIANT_FORCED;
            break;
        }
        default:
        {
            EnetAppUtils_print("Invalid MAC mode: %u\r\n", macMode);
            EnetAppUtils_assert(false);
        }
    }
}

static void EnetApp_initLinkArgs(const Enet_Type enetType,
                          const uint32_t instId,
                          const Enet_MacPort macPort,
                          const bool isInNoPhyMode,
                          EnetPer_PortLinkCfg *pLinkArgs)
{
    EnetMacPort_LinkCfg *pLinkCfg = &pLinkArgs->linkCfg;
    int32_t status = ENET_SOK;
    EnetAppUtils_print("Open MAC port %u\r\n", ENET_MACPORT_ID(macPort));

    /* Set port link params */
    pLinkArgs->macPort = macPort;
    EnetBoard_getMiiConfig(&pLinkArgs->mii);
    EnetApp_getMacPortLinkCfg(pLinkCfg, macPort);

    /* Setup board for requested Ethernet port */
    EnetBoard_EthPort ethPort =
    {
       .enetType = enetType,
       .instId   = instId,
       .macPort  = macPort,
       .boardId  = EnetBoard_getId(),
       .mii      = pLinkArgs->mii,
    };
    status = EnetBoard_setupPorts(&ethPort, 1U);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("Failed to setup MAC port %u\r\n", ENET_MACPORT_ID(macPort));
        EnetAppUtils_assert(false);
    }

    EnetPhy_Cfg *pPhyCfg = &pLinkArgs->phyCfg;

    if (isInNoPhyMode)
    {
        /* In No-PHY MODE, speed and duplexity can not be in AUTO */

        /* In No-PHY mode, speed of 10M is not possible to operate,
         * as 10M needs in-band isignalling from PHY to set MAC port clock speed.
         * Hence, only 100M and 1G is supported in No-PHY mode */

        pPhyCfg->phyAddr    = ENETPHY_INVALID_PHYADDR;
        pLinkCfg->speed     = (pLinkArgs->mii.layerType == ENET_MAC_LAYER_GMII) ? ENET_SPEED_1GBIT : ENET_SPEED_100MBIT;
        pLinkCfg->duplexity = ENET_DUPLEX_FULL;
        EnetAppUtils_print("Setting in NO-PHY mode for MAC port %u\r\n", ENET_MACPORT_ID(macPort));
    }
    else
    {
        const EnetBoard_PhyCfg* pBoardPhyCfg = EnetBoard_getPhyCfg(&ethPort);
        if (pBoardPhyCfg != NULL)
        {
            pPhyCfg->phyAddr     = pBoardPhyCfg->phyAddr;
            pPhyCfg->isStrapped  = pBoardPhyCfg->isStrapped;
            pPhyCfg->loopbackEn  = false;
            pPhyCfg->skipExtendedCfg = pBoardPhyCfg->skipExtendedCfg;
            pPhyCfg->extendedCfgSize = pBoardPhyCfg->extendedCfgSize;
            memcpy(pPhyCfg->extendedCfg, pBoardPhyCfg->extendedCfg, pPhyCfg->extendedCfgSize);
        }
        else
        {
            EnetAppUtils_print("No PHY configuration found for MAC port %u\r\n", ENET_MACPORT_ID(macPort));
            EnetAppUtils_assert(false);
        }
    }

}

static int32_t EnetApp_enablePorts(Enet_Handle hEnet,
                                   Enet_Type enetType,
                                   uint32_t instId,
                                   uint32_t coreId,
                                   Enet_MacPort macPortList[ENET_MAC_PORT_NUM],
                                   uint8_t numMacPorts)
{
    int32_t status = ENET_SOK;
    Enet_IoctlPrms prms;
    uint8_t i;

    for (i = 0U; i < numMacPorts; i++)
    {
        EnetPer_PortLinkCfg linkArgs;
        CpswMacPort_Cfg cpswMacCfg;

        linkArgs.macCfg = &cpswMacCfg;
        CpswMacPort_initCfg(&cpswMacCfg);
        EnetPhy_initCfg(&linkArgs.phyCfg);

        linkArgs.macPort = macPortList[i];
        EnetApp_getMacPortInitConfig(linkArgs.macCfg, macPortList[i]);

        /* in MAC loopback mode, we should not configure PHYs. It shall be treated as NO-PHY system */
        EnetApp_initLinkArgs(enetType, instId, macPortList[i], cpswMacCfg.loopbackEn, &linkArgs);

        ENET_IOCTL_SET_IN_ARGS(&prms, &linkArgs);
        ENET_IOCTL(hEnet,
                   coreId,
                   ENET_PER_IOCTL_OPEN_PORT_LINK,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("EnetApp_enablePorts() failed to open MAC port: %d\r\n", status);
        }
    }

    if ((status == ENET_SOK) && (Enet_isCpswFamily(enetType)))
    {
        CpswAle_SetPortStateInArgs setPortStateInArgs;

        setPortStateInArgs.portNum   = CPSW_ALE_HOST_PORT_NUM;
        setPortStateInArgs.portState = CPSW_ALE_PORTSTATE_FORWARD;
        ENET_IOCTL_SET_IN_ARGS(&prms, &setPortStateInArgs);
        prms.outArgs = NULL_PTR;
        ENET_IOCTL(hEnet,
                   coreId,
                   CPSW_ALE_IOCTL_SET_PORT_STATE,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("EnetApp_enablePorts() failed CPSW_ALE_IOCTL_SET_PORT_STATE: %d\r\n", status);
        }

        if (status == ENET_SOK)
        {
            ENET_IOCTL_SET_NO_ARGS(&prms);
            ENET_IOCTL(hEnet,
                       coreId,
                       ENET_HOSTPORT_IOCTL_ENABLE,
                       &prms,
                       status);
            if (status != ENET_SOK)
            {
                EnetAppUtils_print("EnetApp_enablePorts() Failed to enable host port: %d\r\n", status);
            }
        }
    }

    /* Show alive PHYs */
    if (status == ENET_SOK)
    {
        Enet_IoctlPrms prms;
        bool alive;
        uint32_t i;

        for (i = 0U; i < ENET_MDIO_PHY_CNT_MAX; i++)
        {
            ENET_IOCTL_SET_INOUT_ARGS(&prms, &i, &alive);
            ENET_IOCTL(hEnet,
                       coreId,
                       ENET_MDIO_IOCTL_IS_ALIVE,
                       &prms,
                       status);

            if (status == ENET_SOK)
            {
                if (alive == true)
                {
                    EnetAppUtils_print("PHY %d is alive\r\n", i);
                }
            }
            else
            {
                EnetAppUtils_print("Failed to get PHY %d alive status: %d\r\n", i, status);
            }
        }
    }

    return status;
}

static void EnetApp_deleteClock(EnetAppSysCfg_Obj *hEnetAppObj)
{

    hEnetAppObj->timerTaskShutDownFlag = true;

    ClockP_stop(&hEnetAppObj->timerObj);

    /* Post Timer Sem once to get the Periodic Tick task terminated */
    SemaphoreP_post(&hEnetAppObj->timerSemObj);

    do
    {
        ClockP_usleep(ClockP_ticksToUsec(1));
    } while (hEnetAppObj->timerTaskShutDownDoneFlag != true);

    ClockP_destruct(&hEnetAppObj->timerObj);
    SemaphoreP_destruct(&hEnetAppObj->timerSemObj);

}
void EnetApp_driverClose(Enet_Type enetType, uint32_t instId)
{
    Enet_IoctlPrms prms;
    int32_t status;
    uint32_t selfCoreId;
    uint32_t i;
    Enet_MacPort macPortList[ENET_MAC_PORT_NUM];
    uint8_t numMacPorts;
    Enet_Handle hEnet = Enet_getHandle(enetType, instId);

    EnetAppUtils_assert(Enet_isCpswFamily(enetType) == true);
    selfCoreId   = EnetSoc_getCoreId();
    EnetApp_getEnetInstMacInfo(enetType, instId, macPortList, &numMacPorts);
    EnetApp_deleteClock(&gEnetAppSysCfgObj);
    /* Disable host port */
	ENET_IOCTL_SET_NO_ARGS(&prms);
	ENET_IOCTL(hEnet,
               selfCoreId,
               ENET_HOSTPORT_IOCTL_DISABLE,
               &prms,
               status);
	if (status != ENET_SOK)
	{
	    EnetAppUtils_print("Failed to disable host port: %d\r\n", status);
	}

    for (i = 0U; i < numMacPorts; i++)
    {
        Enet_MacPort macPort = macPortList[i];

        ENET_IOCTL_SET_IN_ARGS(&prms, &macPort);
        ENET_IOCTL(hEnet,
                   selfCoreId,
                   ENET_PER_IOCTL_CLOSE_PORT_LINK,
                   &prms,
                   status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("close() failed to close MAC port: %d\r\n", status);
        }
    }

    Enet_close(hEnet);
}




static uint32_t EnetApp_retrieveFreeTxPkts(EnetDma_TxChHandle hTxCh, EnetDma_PktQ *txPktInfoQ)
{
    EnetDma_PktQ txFreeQ;
    EnetDma_Pkt *pktInfo;
    uint32_t txFreeQCnt = 0U;
    int32_t status;

    EnetQueue_initQ(&txFreeQ);

    /* Retrieve any packets that may be free now */
    status = EnetDma_retrieveTxPktQ(hTxCh, &txFreeQ);
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

            EnetQueue_enq(txPktInfoQ, &pktInfo->node);
            pktInfo = (EnetDma_Pkt *)EnetQueue_deq(&txFreeQ);
        }
    }
    else
    {
        EnetAppUtils_print("retrieveFreeTxPkts() failed to retrieve pkts: %d\r\n", status);
    }

    return txFreeQCnt;
}



static void EnetApp_openDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                    Enet_Type enetType,
                                    uint32_t instId,
                                    Enet_Handle hEnet,
                                    uint32_t coreKey,
                                    uint32_t coreId)
{
    EnetApp_openAllTxDmaChannels(dma,
                                 hEnet,
                                 coreKey,
                                 coreId);
    EnetApp_openAllRxDmaChannels(dma,
                                 hEnet,
                                 coreKey,
                                 coreId);
}



static void EnetApp_setPortTsEventPrms(CpswMacPort_TsEventCfg *tsPortEventCfg)
{
    memset(tsPortEventCfg, 0, sizeof(CpswMacPort_TsEventCfg));

    tsPortEventCfg->txAnnexFEn = true;
    tsPortEventCfg->rxAnnexFEn = true;
    tsPortEventCfg->txHostTsEn = true;
    /* Enable ts for SYNC, PDELAY_REQUEST, PDELAY_RESPONSE */
    tsPortEventCfg->messageType = 13;
    tsPortEventCfg->seqIdOffset = 30;
    tsPortEventCfg->domainOffset = 4;
}

int32_t EnetApp_enablePortTsEvent(Enet_Handle hEnet, uint32_t coreId, uint32_t macPort[], uint32_t numPorts)
{
    int32_t status = ENET_SOK;
    Enet_IoctlPrms prms;
    CpswMacPort_EnableTsEventInArgs enableTsEventInArgs;
    uint8_t i = 0U;

    EnetApp_setPortTsEventPrms(&(enableTsEventInArgs.tsEventCfg));
    for (i = 0U; i < numPorts; i++)
    {
        enableTsEventInArgs.macPort = (Enet_MacPort)macPort[i];
        ENET_IOCTL_SET_IN_ARGS(&prms, &enableTsEventInArgs);
        ENET_IOCTL(hEnet, coreId, CPSW_MACPORT_IOCTL_ENABLE_CPTS_EVENT, &prms, status);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("Enet_ioctl ENABLE_CPTS_EVENT failed %d port %d\n", status, macPort[i]);
        }
    }

    return status;
}

int32_t EnetApp_getRxTimeStamp(Enet_Handle hEnet, uint32_t coreId, EnetTimeSync_GetEthTimestampInArgs* inArgs, uint64_t *ts)
{
    int32_t status = ENET_SOK;
    Enet_IoctlPrms prms;

    ENET_IOCTL_SET_INOUT_ARGS(&prms, inArgs, ts);
    ENET_IOCTL(hEnet, coreId, ENET_TIMESYNC_IOCTL_GET_ETH_RX_TIMESTAMP, &prms, status);

    return status;
}

int32_t EnetApp_setTimeStampComplete(Enet_Handle hEnet, uint32_t coreId)
{
    int32_t status = ENET_SOK;
    /* Not supported for CPSW, the completion ioctl is only vaild for ICSSG */
    /* Need this funtion definition to align CPSW and ICSSG for TSN libs */
    return status;
}

static void EnetApp_postPollEvent(ClockP_Object *clkInst, void *pArg)
{
    EventP_Object* pEvent = (EventP_Object *) pArg;
    const int32_t status = EventP_setBits(pEvent, AppEventId_CPSW_PERIODIC_POLL);

        if (status != SystemP_SUCCESS)
    {
        EnetAppUtils_print("Failed to post Poll Event handle\r\n");
        EnetAppUtils_assert(false);
    }
}

void EnetApp_initPhyStateHandlerTask(EventP_Object *pEvent)
{
    int32_t status;
    ClockP_Params clkParams;
    uint32_t timPeriodTicks = ClockP_usecToTicks((ENETPHY_FSM_TICK_PERIOD_MS)*1000U);  // Set timer expiry time in OS ticks

    ClockP_Params_init(&clkParams);
    clkParams.start     = TRUE;
    clkParams.timeout   = timPeriodTicks;
    clkParams.period    = timPeriodTicks;
    clkParams.args      = pEvent;
    clkParams.callback  = &EnetApp_postPollEvent;

    /* Creating timer and setting timer callback function*/
    status = ClockP_construct(&gEnetAppSysCfgObj.timerObj ,
                              &clkParams);

    if (status == SystemP_SUCCESS)
    {
        gEnetAppSysCfgObj.timerTaskShutDownFlag = false;
    }
    else
    {
        EnetAppUtils_print("Failed to construct clock handle\r\n");
        EnetAppUtils_assert(false);
    }
}




#define ENET_SYSCFG_DEFAULT_NUM_TX_PKT                                     (16U)
#define ENET_SYSCFG_DEFAULT_NUM_RX_PKT                                     (32U)



static void EnetAppUtils_setCommonRxChPrms(EnetCpdma_OpenRxChPrms *pRxChPrms)
{
    pRxChPrms->numRxPkts           = ENET_SYSCFG_DEFAULT_NUM_RX_PKT;
}

static void EnetAppUtils_setCommonTxChPrms(EnetCpdma_OpenTxChPrms *pTxChPrms)
{
    pTxChPrms->numTxPkts           = ENET_SYSCFG_DEFAULT_NUM_TX_PKT;
}

static void EnetAppUtils_openTxCh(Enet_Handle hEnet,
                                  uint32_t coreKey,
                                  uint32_t coreId,
                                  uint32_t *pTxChNum,
                                  EnetDma_TxChHandle *pTxChHandle,
                                  EnetCpdma_OpenTxChPrms *pCpswTxChCfg)
{
    EnetDma_Handle hDma = Enet_getDmaHandle(hEnet);
    int32_t status;

    EnetAppUtils_assert(hDma != NULL);

    pCpswTxChCfg->hEnet = hEnet;
    status = EnetAppUtils_allocTxCh(hEnet,
                                    coreKey,
                                    coreId,
                                    pTxChNum);
    EnetAppUtils_assert(ENET_SOK == status);

    pCpswTxChCfg->chNum = *pTxChNum;

    *pTxChHandle = EnetCpdma_openTxCh(hDma, pCpswTxChCfg);
    EnetAppUtils_assert(NULL != *pTxChHandle);
}

static void EnetAppUtils_closeTxCh(Enet_Handle hEnet,
                                   uint32_t coreKey,
                                   uint32_t coreId,
                                   EnetDma_PktQ *pFqPktInfoQ,
                                   EnetDma_PktQ *pCqPktInfoQ,
                                   EnetDma_TxChHandle hTxChHandle,
                                   uint32_t txChNum)
{
    int32_t status;

    EnetQueue_initQ(pFqPktInfoQ);
    EnetQueue_initQ(pCqPktInfoQ);

    EnetDma_disableTxEvent(hTxChHandle);
    status = EnetCpdma_closeTxCh(hTxChHandle, pFqPktInfoQ, pCqPktInfoQ);
    EnetAppUtils_assert(ENET_SOK == status);

    status = EnetAppUtils_freeTxCh(hEnet,
                                   coreKey,
                                   coreId,
                                   txChNum);
    EnetAppUtils_assert(ENET_SOK == status);
}

static void EnetAppUtils_openRxCh(Enet_Handle hEnet,
                                  uint32_t coreKey,
                                  uint32_t coreId,
                                  uint32_t *pRxChNum,
                                  EnetDma_RxChHandle *pRxChHandle,
                                  EnetCpdma_OpenRxChPrms *pCpswRxChCfg,
                                  uint32_t allocMacAddrCnt,
                                  uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN])
{
    EnetDma_Handle hDma = Enet_getDmaHandle(hEnet);
    int32_t status;
    uint32_t rxFlowStartIdx;
    uint32_t rxFlowIdx;

    EnetAppUtils_assert(hDma != NULL);

    pCpswRxChCfg->hEnet = hEnet;
    status = EnetAppUtils_allocRxFlow(hEnet,
                                      coreKey,
                                      coreId,
                                      &rxFlowStartIdx,
                                      &rxFlowIdx);
    EnetAppUtils_assert(status == ENET_SOK);
    pCpswRxChCfg->chNum = rxFlowIdx;
	*pRxChNum = rxFlowIdx;

    *pRxChHandle = EnetCpdma_openRxCh(hDma, pCpswRxChCfg);
    EnetAppUtils_assert(NULL != *pRxChHandle);

    for (uint32_t i = 0; i < allocMacAddrCnt; i++)
    {
        status = EnetAppUtils_allocMac(hEnet,
                                       coreKey,
                                       coreId,
                                       macAddr[i]);
        EnetAppUtils_assert(ENET_SOK == status);
        EnetAppUtils_addHostPortEntry(hEnet, coreId, macAddr[i]);
    }
}

static void EnetAppUtils_closeRxCh(Enet_Handle hEnet,
                                   uint32_t coreKey,
                                   uint32_t coreId,
                                   EnetDma_PktQ *pFqPktInfoQ,
                                   EnetDma_PktQ *pCqPktInfoQ,
                                   EnetDma_RxChHandle hRxChHandle,
                                   uint32_t rxChNum,
                                   uint32_t freeHostMacAddrCount,
                                   uint8_t macAddr[ENET_MAX_NUM_MAC_PER_PHER][ENET_MAC_ADDR_LEN])
{
    int32_t status;

    EnetQueue_initQ(pFqPktInfoQ);
    EnetQueue_initQ(pCqPktInfoQ);

    status = EnetCpdma_closeRxCh(hRxChHandle, pFqPktInfoQ, pCqPktInfoQ);
    EnetAppUtils_assert(ENET_SOK == status);
    for (uint32_t macAdrIdx = 0;  macAdrIdx < freeHostMacAddrCount; macAdrIdx++)
    {
        EnetAppUtils_delAddrEntry(hEnet, coreId, macAddr[macAdrIdx]);
        EnetAppUtils_freeMac(hEnet,
                             coreKey,
                             coreId,
                             macAddr[macAdrIdx]);
    }
}


static void EnetApp_openTxDma(EnetAppTxDmaSysCfg_Obj *tx,
                              uint32_t numTxPkts,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId)
{
    EnetCpdma_OpenTxChPrms txChCfg;

    memset(tx, 0, sizeof(*tx));
    /* Open the TX channel for Regular traffic */
    EnetCpdma_initTxChParams(&txChCfg);

    txChCfg.cbArg    = NULL;
    txChCfg.notifyCb = NULL;

    EnetAppUtils_setCommonTxChPrms(&txChCfg);

    txChCfg.numTxPkts = numTxPkts;
    EnetAppUtils_openTxCh(hEnet,
                          coreKey,
                          coreId,
                          &tx->txChNum,
                          &tx->hTxCh,
                          &txChCfg);
    return;
}

static void EnetApp_openRxDma(EnetAppRxDmaSysCfg_Obj *rx,
                              uint32_t numRxPkts,
                              Enet_Handle hEnet, 
                              uint32_t coreKey,
                              uint32_t coreId,
                              uint32_t allocMacAddrCnt)
{
    EnetCpdma_OpenRxChPrms rxChCfg;

    memset(rx, 0, sizeof(*rx));
    /* Open the TX channel for Regular traffic */
    EnetCpdma_initRxChParams(&rxChCfg);

    rxChCfg.cbArg    = NULL;
    rxChCfg.notifyCb = NULL;

    EnetAppUtils_setCommonRxChPrms(&rxChCfg);

    rxChCfg.numRxPkts = numRxPkts;
    EnetAppUtils_openRxCh(hEnet,
                          coreKey,
                          coreId,
                          &rx->rxChNum,
                          &rx->hRxCh,
                          &rxChCfg,
                          allocMacAddrCnt,
                          rx->macAddr);

    rx->numValidMacAddress = allocMacAddrCnt;
    return;
}

static void EnetApp_openAllTxDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                         Enet_Handle hEnet, 
                                         uint32_t coreKey,
                                         uint32_t coreId)
{
    uint32_t txNumPkts[ENET_SYSCFG_TX_CHANNELS_NUM] = 
                           {
                            8   
                           };
    uint32_t i;
    
    for (i = 0; i < ENET_SYSCFG_TX_CHANNELS_NUM;i++)
    {
        EnetApp_openTxDma(&dma->tx[i], txNumPkts[i], hEnet, coreKey, coreId);
    }
}

static void EnetApp_openAllRxDmaChannels(EnetAppDmaSysCfg_Obj *dma,
                                         Enet_Handle hEnet, 
                                         uint32_t coreKey,
                                         uint32_t coreId)
{
    const uint32_t rxNumPkts[ENET_SYSCFG_RX_FLOWS_NUM] =
                           {
                            8
                           };
    const uint32_t allocMacAddrCnt[ENET_SYSCFG_RX_FLOWS_NUM] = 
                           {
                            1
                           };


    for (uint32_t i = 0; i < ENET_SYSCFG_RX_FLOWS_NUM;i++)
    {
        EnetApp_openRxDma(&dma->rx[i], rxNumPkts[i], hEnet, coreKey, coreId, allocMacAddrCnt[i]);
    }
}

void EnetApp_closeTxDma(uint32_t enetTxDmaChId,
                        Enet_Handle hEnet, 
                        uint32_t coreKey,
                        uint32_t coreId,
                        EnetDma_PktQ *fqPktInfoQ,
                        EnetDma_PktQ *cqPktInfoQ)
{
    EnetAppTxDmaSysCfg_Obj *tx;

    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma.tx));
    tx = &gEnetAppSysCfgObj.dma.tx[enetTxDmaChId];
    EnetQueue_initQ(fqPktInfoQ);
    EnetQueue_initQ(cqPktInfoQ);
    EnetApp_retrieveFreeTxPkts(tx->hTxCh, cqPktInfoQ);
    EnetAppUtils_closeTxCh(hEnet,
                           coreKey,
                           coreId,
                           fqPktInfoQ,
                           cqPktInfoQ,
                           tx->hTxCh,
                           tx->txChNum);

    memset(tx, 0, sizeof(*tx));
}

void EnetApp_closeRxDma(uint32_t enetRxDmaChId,
                        Enet_Handle hEnet, 
                        uint32_t coreKey,
                        uint32_t coreId,
                        EnetDma_PktQ *fqPktInfoQ,
                        EnetDma_PktQ *cqPktInfoQ)
{
    EnetAppRxDmaSysCfg_Obj *rx;

    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma.rx));
    rx = &gEnetAppSysCfgObj.dma.rx[enetRxDmaChId];
    /* Close RX channel */
    EnetQueue_initQ(fqPktInfoQ);
    EnetQueue_initQ(cqPktInfoQ);

	EnetAppUtils_closeRxCh(hEnet,
                           coreKey,
                           coreId,
                           fqPktInfoQ,
                           cqPktInfoQ,
                           rx->hRxCh,
                           rx->rxChNum,
                           rx->numValidMacAddress,
                           rx->macAddr);

    EnetAppSoc_releaseMacAddrList(rx->macAddr, rx->numValidMacAddress);
    memset(rx, 0, sizeof(*rx));
}

void EnetApp_getTxDmaHandle(uint32_t enetTxDmaChId,
                            const EnetApp_GetDmaHandleInArgs *inArgs,
                            EnetApp_GetTxDmaHandleOutArgs *outArgs)
{
    int32_t status;
    EnetAppTxDmaSysCfg_Obj *tx;
    uint32_t txNumPkts[ENET_SYSCFG_TX_CHANNELS_NUM] = 
                           {
                            8   
                           };

    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma.tx));
    tx = &gEnetAppSysCfgObj.dma.tx[enetTxDmaChId];

    EnetAppUtils_assert(tx->hTxCh != NULL);
    status = EnetDma_registerTxEventCb(tx->hTxCh, inArgs->notifyCb, inArgs->cbArg);
    EnetAppUtils_assert(status == ENET_SOK);
    
    outArgs->hTxCh = tx->hTxCh;
    outArgs->txChNum = tx->txChNum;
    EnetAppUtils_assert(enetTxDmaChId < ENET_ARRAYSIZE(txNumPkts));
    outArgs->maxNumTxPkts = txNumPkts[enetTxDmaChId];
    return;
}

void EnetApp_getMacAddress(uint32_t enetRxDmaChId,
                            EnetApp_GetMacAddrOutArgs *outArgs)
{

    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma.rx));
    EnetAppRxDmaSysCfg_Obj* rx = &gEnetAppSysCfgObj.dma.rx[enetRxDmaChId];

    outArgs->macAddressCnt = rx->numValidMacAddress;
    EnetAppUtils_assert(outArgs->macAddressCnt <= ENET_MAX_NUM_MAC_PER_PHER);
    for (uint32_t i = 0; i < outArgs->macAddressCnt; i++)
    {
        EnetUtils_copyMacAddr(outArgs->macAddr[i], rx->macAddr[i]);
    }

}

void EnetApp_getRxDmaHandle(uint32_t enetRxDmaChId,
                            const EnetApp_GetDmaHandleInArgs *inArgs,
                            EnetApp_GetRxDmaHandleOutArgs *outArgs)
{
    int32_t status;
    EnetAppRxDmaSysCfg_Obj *rx;
    uint32_t rxNumPkts[ENET_SYSCFG_RX_FLOWS_NUM] = 
                           {
                            8   
                           };

    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(gEnetAppSysCfgObj.dma.rx));
    rx = &gEnetAppSysCfgObj.dma.rx[enetRxDmaChId];

    EnetAppUtils_assert(rx->hRxCh != NULL);
    status = EnetDma_registerRxEventCb(rx->hRxCh, inArgs->notifyCb, inArgs->cbArg);
    EnetAppUtils_assert(status == ENET_SOK);
    
    outArgs->hRxCh = rx->hRxCh;
    outArgs->rxChNum = rx->rxChNum;
    outArgs->numValidMacAddress = rx->numValidMacAddress;
    for (uint32_t i = 0; i < outArgs->numValidMacAddress; i++)
    {
        EnetUtils_copyMacAddr(outArgs->macAddr[i], rx->macAddr[i]);
    }

    EnetAppUtils_assert(enetRxDmaChId < ENET_ARRAYSIZE(rxNumPkts));
    outArgs->maxNumRxPkts = rxNumPkts[enetRxDmaChId];
    return;
}



#if defined(SOC_AM275X) /* This is a workaround, will be removed. */
void EnetApp_fixPinmux()
{
    Pinmux_PerCfg_t gPinMuxRGMII[] = {
        /* MDIO0 pin config */
        /* MDIO0_MDC -> MDIO0_MDC (V11) */
        {
            PIN_MDIO0_MDC,
            ( PIN_MODE(0) | PIN_PULL_DISABLE )
        },
        /* MDIO0_MDIO -> MDIO0_MDIO (U11) */
        {
            PIN_MDIO0_MDIO,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2 pin config */
        /* RGMII2_RD0 -> RGMII2_RD0 (U15) */
        {
            PIN_RGMII2_RD0,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_RD1 -> RGMII2_RD1 (V15) */
        {
            PIN_RGMII2_RD1,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_RD2 -> RGMII2_RD2 (W14) */
        {
            PIN_RGMII2_RD2,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_RD3 -> RGMII2_RD3 (T15) */
        {
            PIN_RGMII2_RD3,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_RXC -> RGMII2_RXC (W15) */
        {
            PIN_RGMII2_RXC,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_RX_CTL -> RGMII2_RX_CTL (V14) */
        {
            PIN_RGMII2_RX_CTL,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TD0 -> RGMII2_TD0 (V16) */
        {
            PIN_RGMII2_TD0,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TD1 -> RGMII2_TD1 (W16) */
        {
            PIN_RGMII2_TD1,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TD2 -> RGMII2_TD2 (V17) */
        {
            PIN_RGMII2_TD2,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TD3 -> RGMII2_TD3 (W18) */
        {
            PIN_RGMII2_TD3,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TXC -> RGMII2_TXC (W17) */
        {
            PIN_RGMII2_TXC,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII2_TX_CTL -> RGMII2_TX_CTL (U16) */
        {
            PIN_RGMII2_TX_CTL,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1 pin config */
        /* RGMII1_RD0 -> RGMII1_RD0 (W11) */
        {
            PIN_RGMII1_RD0,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_RD1 -> RGMII1_RD1 (T11) */
        {
            PIN_RGMII1_RD1,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_RD2 -> RGMII1_RD2 (T12) */
        {
            PIN_RGMII1_RD2,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_RD3 -> RGMII1_RD3 (U12) */
        {
            PIN_RGMII1_RD3,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_RXC -> RGMII1_RXC (W12) */
        {
            PIN_RGMII1_RXC,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_RX_CTL -> RGMII1_RX_CTL (V12) */
        {
            PIN_RGMII1_RX_CTL,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TD0 -> RGMII1_TD0 (U13) */
        {
            PIN_RGMII1_TD0,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TD1 -> RGMII1_TD1 (W13) */
        {
            PIN_RGMII1_TD1,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TD2 -> RGMII1_TD2 (T14) */
        {
            PIN_RGMII1_TD2,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TD3 -> RGMII1_TD3 (U14) */
        {
            PIN_RGMII1_TD3,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TXC -> RGMII1_TXC (V13) */
        {
            PIN_RGMII1_TXC,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },
        /* RGMII1_TX_CTL -> RGMII1_TX_CTL (T13) */
        {
            PIN_RGMII1_TX_CTL,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DISABLE )
        },

        {PINMUX_END, 0U}
    };

    Pinmux_PerCfg_t i2cPinmuxConfig[] =
    {
        /* I2C0 pin config */
        /* I2C0_SCL -> I2C0_SCL (M3) */
        {
            PIN_I2C0_SCL,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION  )
        },
                /* I2C0_SDA -> I2C0_SDA (N3) */
        {
            PIN_I2C0_SDA,
            ( PIN_MODE(0) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION  )
        },


        {PINMUX_END, 0U}
    };
    Pinmux_config(gPinMuxRGMII, PINMUX_DOMAIN_ID_MAIN);
    Pinmux_config(i2cPinmuxConfig, PINMUX_DOMAIN_ID_MAIN);
}
#endif



#include <string.h>

#include <enet.h>
#include <include/core/enet_utils.h>

#include <include/core/enet_dma.h>
#include <include/per/cpsw.h>

#include "enet_appmemutils.h"
#include "enet_appmemutils_cfg.h"
#include "enet_apputils.h"




static void EnetApp_initEnetLinkCbPrms(Cpsw_Cfg *cpswCfg)
{
    cpswCfg->mdioLinkStateChangeCb     = NULL;
    cpswCfg->mdioLinkStateChangeCbArg  = NULL;

    cpswCfg->portLinkStatusChangeCb    = NULL;
    cpswCfg->portLinkStatusChangeCbArg = NULL;
}

static const CpswAle_Cfg enetAppCpswAleCfg =
{
    .modeFlags = (CPSW_ALE_CFG_MODULE_EN),
    .policerGlobalCfg =
    {
        .policingEn         = false,
        .yellowDropEn       = false,
        .redDropEn          = false,
        .yellowThresh       = CPSW_ALE_POLICER_YELLOWTHRESH_DROP_PERCENT_100,
        .policerNoMatchMode = CPSW_ALE_POLICER_NOMATCH_MODE_GREEN,
        .noMatchPolicer     = {
                                  .peakRateInBitsPerSec   = 0,
                                  .commitRateInBitsPerSec = 0,
                              }
    },
    .agingCfg =
    {
        .autoAgingEn        = true,
        .agingPeriodInMs    = 1000,
    },
    .vlanCfg =
    {
        .aleVlanAwareMode   = true,
        .cpswVlanAwareMode  = false,
        .autoLearnWithVlan  = false,
        .unknownVlanNoLearn = false,
        .unknownForceUntaggedEgressMask = (0),
        .unknownRegMcastFloodMask       = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
        .unknownUnregMcastFloodMask     = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
        .unknownVlanMemberListMask      = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
    },
    .nwSecCfg =
    {
        .hostOuiNoMatchDeny  = false,
        .vid0ModeEn          = true,
        .malformedPktCfg     = {
                                   .srcMcastDropDis = false,
                                   .badLenPktDropEn = false,
                               },
        .ipPktCfg            = {
                                   .dfltNoFragEn          = false,
                                   .dfltNxtHdrWhitelistEn = false,
                                   .ipNxtHdrWhitelistCnt  =  0,
                                   .ipNxtHdrWhitelist     = {
                                                              
                                                            },
                               },


        .macAuthCfg          = {
                                   .authModeEn           = false,
                                   .macAuthDisMask       = (0 | CPSW_ALE_HOST_PORT_MASK),
                               },
    },
    .portCfg =
    {
        [CPSW_ALE_HOST_PORT_NUM] =
        {
            .learningCfg =
            {
                .noLearn         = false,
                .noSaUpdateEn    = false,
            },
            .vlanCfg =
            {
                .vidIngressCheck = false,
                .dropUntagged    = false,
                .dropDualVlan    = false,
                .dropDoubleVlan  = false,
            },
            .macModeCfg =
            {
                .macOnlyCafEn    = false,
                .macOnlyEn       = false,
            },
            .pvidCfg =
            {
                .vlanIdInfo      =
                {
                    .vlanId   = 0,
                    .tagType  = ENET_VLAN_TAG_TYPE_INNER,
                },
                .vlanMemberList          = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .unregMcastFloodMask     = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .regMcastFloodMask       = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .forceUntaggedEgressMask = (0),
                .noLearnMask             = (0),
                .vidIngressCheck         = false,
                .limitIPNxtHdr           = false,
                .disallowIPFrag          = false,
            },
        },
        [CPSW_ALE_MACPORT_TO_ALEPORT(ENET_MAC_PORT_1)] =
        {
            .learningCfg =
            {
                .noLearn         = false,
                .noSaUpdateEn    = false,
            },
            .vlanCfg =
            {
                .vidIngressCheck = false,
                .dropUntagged    = false,
                .dropDualVlan    = false,
                .dropDoubleVlan  = false,
            },
            .macModeCfg =
            {
                .macOnlyCafEn    = false,
                .macOnlyEn       = false,
            },
            .pvidCfg =
            {
                .vlanIdInfo      =
                {
                    .vlanId   = 0,
                    .tagType  = ENET_VLAN_TAG_TYPE_INNER,
                },
                .vlanMemberList          = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .unregMcastFloodMask     = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .regMcastFloodMask       = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .forceUntaggedEgressMask = (0),
                .noLearnMask             = (0),
                .vidIngressCheck         = false,
                .limitIPNxtHdr           = false,
                .disallowIPFrag          = false,
            },
        },
        [CPSW_ALE_MACPORT_TO_ALEPORT(ENET_MAC_PORT_2)] =
        {
            .learningCfg =
            {
                .noLearn         = false,
                .noSaUpdateEn    = false,
            },
            .vlanCfg =
            {
                .vidIngressCheck = false,
                .dropUntagged    = false,
                .dropDualVlan    = false,
                .dropDoubleVlan  = false,
            },
            .macModeCfg =
            {
                .macOnlyCafEn    = false,
                .macOnlyEn       = false,
            },
            .pvidCfg =
            {
                .vlanIdInfo      =
                {
                    .vlanId   = 0,
                    .tagType  = ENET_VLAN_TAG_TYPE_INNER,
                },
                .vlanMemberList          = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .unregMcastFloodMask     = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .regMcastFloodMask       = (0 | CPSW_ALE_HOST_PORT_MASK | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_1) | CPSW_ALE_MACPORT_TO_PORTMASK(ENET_MAC_PORT_2)),
                .forceUntaggedEgressMask = (0),
                .noLearnMask             = (0),
                .vidIngressCheck         = false,
                .limitIPNxtHdr           = false,
                .disallowIPFrag          = false,
            },
        },




    }

};

static const Mdio_Cfg enetAppCpswMdioCfg =
{
    .mode               = MDIO_MODE_STATE_CHANGE_MON,
    .mdioBusFreqHz      = 2200000,
    .phyStatePollFreqHz = 22000,
    .pollEnMask         = -1,
    .c45EnMask          = 0,
    .isMaster           = true,
    .disableStateMachineOnInit = false,
};


static const CpswCpts_Cfg enetAppCpswCptsCfg =
{
    .hostRxTsEn     = false,
    .tsCompPolarity = true,
    .tsRxEventsDis  = false,
    .tsGenfClrEn    = true,
    .cptsRftClkFreq = CPSW_CPTS_RFTCLK_FREQ_200MHZ,
};

static const CpswHostPort_Cfg enetAppCpswHostPortCfg =
{
    .crcType           = ENET_CRC_ETHERNET,
    .removeCrc         = false,
    .padShortPacket    = true,
    .passCrcErrors     = false,
    .rxMtu             = 1518,
    .passPriorityTaggedUnchanged = false,
    .rxCsumOffloadEn   = true,
    .txCsumOffloadEn   = true,
    .rxVlanRemapEn     = false,
    .rxDscpIPv4RemapEn = true,
    .rxDscpIPv6RemapEn = false,
    .vlanCfg           =
    {
        .portPri = 0,
        .portCfi = false,
        .portVID = 0,
    },
    .rxPriorityType    = ENET_INGRESS_PRI_TYPE_FIXED,
    .txPriorityType    = ENET_EGRESS_PRI_TYPE_FIXED,
};

static const EnetMacPort_LinkCfg enetAppMacPortLinkCfg[] =
{
    [ENET_MAC_PORT_1] =
    {
        ENET_SPEED_AUTO,
        ENET_DUPLEX_AUTO,
    },
    [ENET_MAC_PORT_2] =
    {
        ENET_SPEED_AUTO,
        ENET_DUPLEX_AUTO,
    },
};

static const CpswMacPort_Cfg enetAppCpswMacPortCfg[] =
{
    [ENET_MAC_PORT_1] =
    {
        .loopbackEn = false,
        .crcType    = ENET_CRC_ETHERNET,
        .rxMtu      = 1518,
        .passPriorityTaggedUnchanged = false,
        .vlanCfg =
        {
            .portPri = 0,
            .portCfi = false,
            .portVID = 0,
        },
        .txPriorityType = ENET_EGRESS_PRI_TYPE_FIXED,
        .sgmiiMode      = ENET_MAC_SGMIIMODE_INVALID, // INVALID as SGMII is not supported in MCU+ devices

    },
    [ENET_MAC_PORT_2] =
    {
        .loopbackEn = false,
        .crcType    = ENET_CRC_ETHERNET,
        .rxMtu      = 1518,
        .passPriorityTaggedUnchanged = false,
        .vlanCfg =
        {
            .portPri = 0,
            .portCfi = false,
            .portVID = 0,
        },
        .txPriorityType = ENET_EGRESS_PRI_TYPE_FIXED,
        .sgmiiMode      = ENET_MAC_SGMIIMODE_INVALID, // INVALID as SGMII is not supported in MCU+ devices

    },
};

static void EnetApp_initAleConfig(CpswAle_Cfg *pAleCfg)
{
    *pAleCfg = enetAppCpswAleCfg;
}

static void EnetApp_initMdioConfig(Mdio_Cfg *pMdioCfg)
{
    *pMdioCfg = enetAppCpswMdioCfg;
}

static void EnetApp_initCptsConfig(CpswCpts_Cfg *pCptsCfg)
{
    *pCptsCfg = enetAppCpswCptsCfg;
}

static void EnetApp_initHostPortConfig(CpswHostPort_Cfg *pHostPortCfg)
{
    *pHostPortCfg = enetAppCpswHostPortCfg;
}

static void EnetApp_initCpdmaConfig(EnetDma_Cfg *pCpdmaCfg)
{
    pCpdmaCfg->enHostRxTsFlag = false;

}

static void EnetApp_getCpswInitCfg(Enet_Type enetType,
                                   uint32_t instId,
                                   Cpsw_Cfg *cpswCfg)
{
    cpswCfg->vlanCfg.vlanAware          = false;
    cpswCfg->hostPortCfg.removeCrc      = true;
    cpswCfg->hostPortCfg.padShortPacket = true;
    cpswCfg->hostPortCfg.passCrcErrors  = true;
    EnetApp_initEnetLinkCbPrms(cpswCfg);
    EnetApp_initAleConfig(&cpswCfg->aleCfg);
    EnetApp_initMdioConfig(&cpswCfg->mdioCfg);
    EnetApp_initCptsConfig(&cpswCfg->cptsCfg);
    EnetApp_initHostPortConfig(&cpswCfg->hostPortCfg);
    EnetApp_initCpdmaConfig((EnetDma_Cfg *)cpswCfg->dmaCfg);
}

static void EnetApp_getMacPortInitConfig(CpswMacPort_Cfg *pMacPortCfg, const Enet_MacPort portIdx)
{
    EnetAppUtils_assert(portIdx < ENET_ARRAYSIZE(enetAppCpswMacPortCfg));
    *pMacPortCfg = enetAppCpswMacPortCfg[portIdx];
}

void EnetApp_getMacPortLinkCfg(EnetMacPort_LinkCfg *pMacPortLinkCfg, const Enet_MacPort portIdx)
{
    EnetAppUtils_assert(portIdx < ENET_ARRAYSIZE(enetAppMacPortLinkCfg));
    *pMacPortLinkCfg = enetAppMacPortLinkCfg[portIdx];
}

static bool IsMacAddrSet(uint8_t *mac)
{
    return ((mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]) != 0);
}

static int AddVlan(Enet_Handle hEnet, uint32_t coreId, uint32_t vlanId)
{
    CpswAle_VlanEntryInfo inArgs;
    uint32_t outArgs;
    Enet_IoctlPrms prms;
    int32_t status = ENET_SOK;

    inArgs.vlanIdInfo.vlanId        = vlanId;
    inArgs.vlanIdInfo.tagType       = ENET_VLAN_TAG_TYPE_INNER;
    inArgs.vlanMemberList           = CPSW_ALE_ALL_PORTS_MASK;
    inArgs.unregMcastFloodMask      = CPSW_ALE_ALL_PORTS_MASK;
    inArgs.regMcastFloodMask        = CPSW_ALE_ALL_PORTS_MASK;
    inArgs.forceUntaggedEgressMask  = 0U;
    inArgs.noLearnMask              = 0U;
    inArgs.vidIngressCheck          = false;
    inArgs.limitIPNxtHdr            = false;
    inArgs.disallowIPFrag           = false;

    ENET_IOCTL_SET_INOUT_ARGS(&prms, &inArgs, &outArgs);
    ENET_IOCTL(hEnet, coreId, CPSW_ALE_IOCTL_ADD_VLAN, &prms, status);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("%s():CPSW_ALE_IOCTL_ADD_VLAN failed: %d\r\n",
                           __func__, status);
    }
    else
    {
        EnetAppUtils_print("CPSW_ALE_IOCTL_ADD_VLAN: %d\r\n", vlanId);
    }

    return status;
}

int32_t EnetApp_applyClassifier(Enet_Handle hEnet, uint32_t coreId, uint8_t *dstMacAddr,
                                uint32_t vlanId, uint32_t ethType, uint32_t rxFlowIdx)
{
    Enet_IoctlPrms prms;
    CpswAle_SetPolicerEntryOutArgs setPolicerEntryOutArgs;
    CpswAle_SetPolicerEntryInArgs setPolicerEntryInArgs;
    int32_t status;

    if (IsMacAddrSet(dstMacAddr) == true)
    {
        status = EnetAppUtils_addAllPortMcastMembership(hEnet, dstMacAddr);
        if (status != ENET_SOK)
        {
            EnetAppUtils_print("%s:EnetAppUtils_addAllPortMcastMembership failed: %d\r\n",
                               "sitara-cpsw", status);
        }
    }
    memset(&setPolicerEntryInArgs, 0, sizeof (setPolicerEntryInArgs));

    if (ethType > 0)
    {
        setPolicerEntryInArgs.policerMatch.policerMatchEnMask |=
            CPSW_ALE_POLICER_MATCH_ETHERTYPE;
        setPolicerEntryInArgs.policerMatch.etherType = ethType;
    }
    setPolicerEntryInArgs.policerMatch.portIsTrunk = false;
    setPolicerEntryInArgs.threadIdEn = true;
    setPolicerEntryInArgs.threadId = rxFlowIdx;

    ENET_IOCTL_SET_INOUT_ARGS(&prms, &setPolicerEntryInArgs, &setPolicerEntryOutArgs);
    ENET_IOCTL(hEnet, coreId,
            CPSW_ALE_IOCTL_SET_POLICER, &prms, status);

    if (status != ENET_SOK)
    {
        EnetAppUtils_print("%s():CPSW_ALE_IOCTL_ADD_VLAN failed: %d\r\n",
                           __func__, status);
    }
    else
    {
        if (vlanId > 0)
        {
            status = AddVlan(hEnet, coreId, vlanId);
        }
    }
    return status;
}

int32_t EnetApp_filterPriorityPacketsCfg(Enet_Handle hEnet, uint32_t coreId)
{
    EnetMacPort_SetPriorityRegenMapInArgs params;
    Enet_IoctlPrms prms;
    int32_t retVal = ENET_SOK;

    params.macPort = ENET_MAC_PORT_1;

    params.priorityRegenMap.priorityMap[0] =0U;
    for (uint32_t i = 1; i < 8U; i++)
    {
        params.priorityRegenMap.priorityMap[i] =1U;  // Map all priorities from (1 to 7) to priority 1, these packets will be received on DMA channel 1.
    }

    ENET_IOCTL_SET_IN_ARGS(&prms, &params);

    ENET_IOCTL(hEnet, coreId, ENET_MACPORT_IOCTL_SET_PRI_REGEN_MAP, &prms, retVal);

    return retVal;
}



