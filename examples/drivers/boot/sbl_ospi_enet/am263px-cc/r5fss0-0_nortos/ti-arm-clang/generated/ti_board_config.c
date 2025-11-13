/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
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
/*
 * Auto generated file
 */
#include "ti_board_config.h"

/*
 * Auto generated file
 */




/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <enet.h>
#include "dp83869.h"

#include <enet_apputils.h>
#include <enet_appboardutils.h>

#include <drivers/hw_include/cslr_soc.h>
#include <networking/enet/core/src/phy/generic_phy.h>
#include <networking/enet/core/src/phy/enetphy_priv.h>
#include "ti_board_open_close.h"
#include <kernel/dpl/AddrTranslateP.h>

#include <ti_drivers_config.h>
#include <drivers/i2c.h>
#include <board/ioexp/ioexp_tca6424.h>
#include <board/ioexp/ioexp_tca6416.h>

static void EnetBoard_setMacPort2IOExpanderCfg(void);

/* PHY drivers */
extern Phy_DrvObj_t gEnetPhyDrvDp83869;
extern Phy_DrvObj_t gEnetPhyDrvGeneric;

/*! \brief All the registered PHY specific drivers. */
static const EthPhyDrv_If gEnetPhyDrvs[] =
{
    &gEnetPhyDrvDp83869,    /* DP83869 */
    &gEnetPhyDrvGeneric,   /* Generic PHY - must be last */
};

const EnetPhy_DrvInfoTbl gEnetPhyDrvTbl =
{
    .numHandles = ENET_ARRAYSIZE(gEnetPhyDrvs),
    .hPhyDrvList = gEnetPhyDrvs,
};

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/**
   TPR:MSS_CTRL:CPSW_CONTROL

   Address offset    0x0000016C
   Physical address  0x0212016C
   Instance          MSS_CTRL
   CPSW_CONTROL_RGMII1_ID_MODE     16  Writing 1'b1 would disable the internal clock delays. And those delays need to be handled on board.
   CPSW_CONTROL_RMII_REF_CLK_OE_N  8   To select the rmii_ref_clk from PAD or from MSS_RCM. 0: clock will be from mss_rcm through IO internal loopback 1: will be from
   CPSW_CONTROL_PORT1_MODE_SEL     2:0 Port 1 Interface
                                         00 = GMII/MII
                                         01 = RMII
                                         10 = RGMII
                                         11 = Not Supported
*/

#define MSS_CPSW_CONTROL_PORT_MODE_MII                                    (0x0U)
#define MSS_CPSW_CONTROL_PORT_MODE_RMII                                   (0x1U)
#define MSS_CPSW_CONTROL_PORT_MODE_RGMII                                  (0x2U)

#define EEPROM_MAGIC_NUMBER_VAL                                           (0xEE3355AAU)
#define EEPROM_MAGIC_NUMBER_OFFSET                                        (0x0U)
#define EEPROM_READ_PCB_REV_DATA_OFFSET                                   (0x0022)

#define I2C_EEPROM_MAC_DATA_OFFSET                                        (0x3D)
#define I2C_EEPROM_MAC_CTRL_OFFSET                                        (0x3B)

#define ENET_BOARD_NUM_MACADDR_MAX                                        (3U)
#define ENET_GET_NUM_MAC_ADDR(num)                                        ((num>>3)+1)
#define ENET_MAC_ADDR_VALIDATE_MASK                                       (0x01U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static const EnetBoard_PortCfg *EnetBoard_getPortCfg(const EnetBoard_EthPort *ethPort);

static const EnetBoard_PortCfg *EnetBoard_findPortCfg(const EnetBoard_EthPort *ethPort,
                                                      const EnetBoard_PortCfg *ethPortCfgs,
                                                      uint32_t numEthPorts);

static void EnetBoard_enableExternalMux();

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/*!
 * \brief Common Processor Board (CPB) board's DP83869 PHY configuration.
 */
static const Dp83869_Cfg gEnetCpbBoard_ConfigEnetEthphy1PhyCfg =
{
	.txClkShiftEn         = true,
.rxClkShiftEn         = true,
.txDelayInPs          = 2000U,   /* Value in pecosec. Refer to DLL_RX_DELAY_CTRL_SL field in ANA_RGMII_DLL_CTRL register of DP83869 PHY datasheet */
.rxDelayInPs          = 2000U,   /* Value in pecosec. Refer to DLL_TX_DELAY_CTRL_SL field in ANA_RGMII_DLL_CTRL register of DP83869 PHY datasheet */
.txFifoDepth          = 4U,
.impedanceInMilliOhms = 35000,  /* 35 ohms */
.idleCntThresh        = 4U,     /* Improves short cable performance */
.gpio0Mode            = DP83869_GPIO0_LED3,
.gpio1Mode            = DP83869_GPIO1_COL, /* Unused */
.ledMode              =
{
	DP83869_LED_LINKED,         /* Unused */
	DP83869_LED_LINKED_100BTX,
	DP83869_LED_RXTXACT,
	DP83869_LED_LINKED_1000BT,
},
};

/*
 * am263px-cc board configuration.
 *
 * RMII/RGMII PHY connected to am263px-cc CPSW_3G MAC port.
 */
static const EnetBoard_PortCfg gEnetCpbBoard_am263px_cc_EthPort[] =
{
    {    /* "CPSW3G" */
        .enetType = ENET_CPSW_3G,
        .instId   = 0U,
        .macPort  = ENET_MAC_PORT_2,
        .mii      = {ENET_MAC_LAYER_GMII, ENET_MAC_SUBLAYER_REDUCED},
        .phyCfg   =
        {
            .phyAddr         = 0,
            .isStrapped      = false,
            .skipExtendedCfg = false,
			.extendedCfg     = &gEnetCpbBoard_ConfigEnetEthphy1PhyCfg,
			.extendedCfgSize = sizeof(gEnetCpbBoard_ConfigEnetEthphy1PhyCfg)
        },
        .flags    = 0U,
    },
};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

const EnetBoard_PhyCfg *EnetBoard_getPhyCfg(const EnetBoard_EthPort *ethPort)
{
    const EnetBoard_PortCfg *portCfg;

    portCfg = EnetBoard_getPortCfg(ethPort);

    return (portCfg != NULL) ? &portCfg->phyCfg : NULL;
}

static void EnetBoard_enableExternalMux()
{
    /* Enable external MUXes, if any, as per the board design */
}

static const EnetBoard_PortCfg *EnetBoard_getPortCfg(const EnetBoard_EthPort *ethPort)
{
    const EnetBoard_PortCfg *portCfg = NULL;

    if (ENET_NOT_ZERO(ethPort->boardId & ENETBOARD_CPB_ID) ||
        ((portCfg == NULL) && ENET_NOT_ZERO(ethPort->boardId & ENETBOARD_LOOPBACK_ID)))
    {
        portCfg = EnetBoard_findPortCfg(ethPort,
                                        gEnetCpbBoard_am263px_cc_EthPort,
                                        ENETPHY_ARRAYSIZE(gEnetCpbBoard_am263px_cc_EthPort));
    }

    return portCfg;
}

static const EnetBoard_PortCfg *EnetBoard_findPortCfg(const EnetBoard_EthPort *ethPort,
                                                      const EnetBoard_PortCfg *ethPortCfgs,
                                                      uint32_t numEthPorts)
{
    const EnetBoard_PortCfg *ethPortCfg = NULL;
    bool found = false;
    uint32_t i;

    for (i = 0U; i < numEthPorts; i++)
    {
        ethPortCfg = &ethPortCfgs[i];

        if ((ethPortCfg->enetType == ethPort->enetType) &&
            (ethPortCfg->instId == ethPort->instId) &&
            (ethPortCfg->macPort == ethPort->macPort) &&
            (ethPortCfg->mii.layerType == ethPort->mii.layerType) &&
            (ethPortCfg->mii.sublayerType == ethPort->mii.sublayerType))
        {
            found = true;
            break;
        }
    }

    return found ? ethPortCfg : NULL;
}

void EnetBoard_getMiiConfig(EnetMacPort_Interface *mii)
{
    mii->layerType      = ENET_MAC_LAYER_GMII;
    mii->variantType    = ENET_MAC_VARIANT_FORCED;
    mii->sublayerType   = ENET_MAC_SUBLAYER_REDUCED;
}

int32_t EnetBoard_setupPorts(EnetBoard_EthPort *ethPorts,
                             uint32_t numEthPorts)
{
    CSL_mss_ctrlRegs *mssCtrlRegs = (CSL_mss_ctrlRegs *)CSL_MSS_CTRL_U_BASE;

    DebugP_assert(numEthPorts == 1);

    EnetBoard_enableExternalMux();

    switch(ethPorts->macPort)
    {
        case ENET_MAC_PORT_1:
            CSL_FINS( mssCtrlRegs->CPSW_CONTROL,MSS_CTRL_CPSW_CONTROL_RGMII1_ID_MODE, 0U);
            CSL_FINS( mssCtrlRegs->CPSW_CONTROL,MSS_CTRL_CPSW_CONTROL_PORT1_MODE_SEL, MSS_CPSW_CONTROL_PORT_MODE_RGMII);
            break;
        case ENET_MAC_PORT_2:
            CSL_FINS( mssCtrlRegs->CPSW_CONTROL,MSS_CTRL_CPSW_CONTROL_RGMII2_ID_MODE, 0U);
            CSL_FINS( mssCtrlRegs->CPSW_CONTROL,MSS_CTRL_CPSW_CONTROL_PORT2_MODE_SEL, MSS_CPSW_CONTROL_PORT_MODE_RGMII);
            EnetBoard_setMacPort2IOExpanderCfg();
            break;
        default:
            DebugP_assert(false);
    }

    /* Nothing else to do */
    return ENET_SOK;
}

static void EnetBoard_setMacPort2IOExpanderCfg(void)
{
    EEPROM_Handle  eepromHandle = gEepromHandle[CONFIG_EEPROM0];
    uint8_t    readBuffer[10] = {0};
    uint8_t* pEepromRdPtr = (uint8_t *) &readBuffer;
    int32_t status    = SystemP_SUCCESS;
    uint32_t magicNum = 0;

    status = EEPROM_read(eepromHandle, EEPROM_MAGIC_NUMBER_OFFSET,  pEepromRdPtr, 4);
    memcpy(&magicNum, pEepromRdPtr, sizeof(uint32_t));

    /* Check magic number header */
    if (magicNum == EEPROM_MAGIC_NUMBER_VAL)
    {
        /* Read from Am263Px PCB revision */
        status = EEPROM_read(eepromHandle, EEPROM_READ_PCB_REV_DATA_OFFSET, pEepromRdPtr, 2);
        
        if ((pEepromRdPtr[0] == 'E') && (pEepromRdPtr[1] == '1')) /* Checking PCB revision E1 */
        {
            DebugP_logInfo("AM263Px CC E1 Board found\r\n");

            const uint32_t mdioMuxSelLine = 8*1 + 4; // PORT 1, PIN 4
            const uint32_t rgmii1MuxSelLine = 8*0 + 2; // PORT 0, PIN 2
            const uint32_t rgmii2MuxSelLine = 8*0 + 3; // PORT 0, PIN 3
            static TCA6416_Config  gTCA6416_Config;

            TCA6416_Params      TCA6416Params;
            TCA6416_Params_init(&TCA6416Params);
            status = TCA6416_open(&gTCA6416_Config, &TCA6416Params);

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6416_config(&gTCA6416_Config,
                            mdioMuxSelLine,
                            TCA6416_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6416_setOutput(&gTCA6416_Config, mdioMuxSelLine, TCA6416_OUT_STATE_HIGH);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6416_config(&gTCA6416_Config,
                            rgmii1MuxSelLine,
                            TCA6416_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6416_setOutput(&gTCA6416_Config, rgmii1MuxSelLine, TCA6416_OUT_STATE_LOW);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6416_config(&gTCA6416_Config,
                            rgmii2MuxSelLine,
                            TCA6416_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6416_setOutput(&gTCA6416_Config, rgmii2MuxSelLine, TCA6416_OUT_STATE_LOW);
            }
        }
        else if (((pEepromRdPtr[0] == 'E') && (pEepromRdPtr[1] == '2')) || (pEepromRdPtr[0] == 'A') || (pEepromRdPtr[0] == 'B')) /* Checking PCB revision Rev E2/Rev A/Rev B*/
        {
            DebugP_logInfo("AM263Px CC E2/A/B Board found\r\n");
            const uint32_t mdioMuxSel1Line  = 8*2 + 2; // PORT 2, PIN 2
            const uint32_t mdioMuxSel2Line  = 8*2 + 3; // PORT 2, PIN 3
            const uint32_t rgmii0MuxSelLine = 8*2 + 4; // PORT 2, PIN 4
            const uint32_t rgmii1MuxSelLine = 8*0 + 2; // PORT 0, PIN 2
            const uint32_t rgmii2MuxSelLine = 8*0 + 3; // PORT 0, PIN 3
            static TCA6424_Config  gTCA6424_Config;

            TCA6424_Params      TCA6424Params;
            TCA6424_Params_init(&TCA6424Params);

            status = TCA6424_open(&gTCA6424_Config, &TCA6424Params);

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6424_config(&gTCA6424_Config,
                            mdioMuxSel1Line,
                            TCA6424_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6424_setOutput(&gTCA6424_Config, mdioMuxSel1Line, TCA6424_OUT_STATE_HIGH);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6424_config(&gTCA6424_Config,
                            mdioMuxSel2Line,
                            TCA6424_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6424_setOutput(&gTCA6424_Config, mdioMuxSel2Line, TCA6424_OUT_STATE_HIGH);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6424_config(&gTCA6424_Config,
                            rgmii0MuxSelLine,
                            TCA6424_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6424_setOutput(&gTCA6424_Config, rgmii0MuxSelLine, TCA6424_OUT_STATE_LOW);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6424_config(&gTCA6424_Config,
                            rgmii1MuxSelLine,
                            TCA6424_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6424_setOutput(&gTCA6424_Config, rgmii1MuxSelLine, TCA6424_OUT_STATE_HIGH);
            }

            if (status == SystemP_SUCCESS)
            {
                /* Configure as output  */
                status = TCA6424_config(&gTCA6424_Config,
                            rgmii2MuxSelLine,
                            TCA6424_MODE_OUTPUT);
            }
            if (status == SystemP_SUCCESS)
            {
                status =  TCA6424_setOutput(&gTCA6424_Config, rgmii2MuxSelLine, TCA6424_OUT_STATE_LOW);
            }
        }
            else
            {
                DebugP_logInfo("AM263Px CC Unable to get version\r\n");
                status = SystemP_FAILURE;
            }
    }
    else
    {
        DebugP_logInfo("AM263Px CC EEPROM NOT FOUND or CURRUPTED\r\n");
        status = SystemP_FAILURE;
    }

}

void EnetBoard_getMacAddrList(uint8_t macAddr[][ENET_MAC_ADDR_LEN],
                              uint32_t maxMacEntries,
                              uint32_t *pAvailMacEntries)
{
    int32_t status = ENET_SOK;
    uint32_t macAddrCnt;
    uint32_t i;
    uint8_t numMacMax;
    uint8_t macAddrBuf[ENET_BOARD_NUM_MACADDR_MAX * ENET_MAC_ADDR_LEN];
    uint8_t validNumMac = 0U;

    status = EEPROM_read(gEepromHandle[CONFIG_EEPROM0],  I2C_EEPROM_MAC_CTRL_OFFSET, &numMacMax, sizeof(uint8_t));
    EnetAppUtils_assert(status == ENET_SOK);
    EnetAppUtils_assert(ENET_GET_NUM_MAC_ADDR(numMacMax) <= ENET_BOARD_NUM_MACADDR_MAX);
    EnetAppUtils_assert(pAvailMacEntries != NULL);

    macAddrCnt = EnetUtils_min(ENET_GET_NUM_MAC_ADDR(numMacMax), maxMacEntries);

    status = EEPROM_read(gEepromHandle[CONFIG_EEPROM0], I2C_EEPROM_MAC_DATA_OFFSET, macAddrBuf, (macAddrCnt * ENET_MAC_ADDR_LEN) );
    EnetAppUtils_assert(status == ENET_SOK);

    /* Save only those required to meet the max number of MAC entries */
    /* Validating that the MAC addresses from the EEPROM are not MULTICAST addresses */
    for (i = 0U; i < macAddrCnt; i++)
    {
        if(!(macAddrBuf[i * ENET_MAC_ADDR_LEN] & ENET_MAC_ADDR_VALIDATE_MASK)){
            memcpy(macAddr[validNumMac], &macAddrBuf[i * ENET_MAC_ADDR_LEN], ENET_MAC_ADDR_LEN);
            validNumMac++;
        }
    }

    *pAvailMacEntries = validNumMac;

    if (macAddrCnt == 0U)
    {
        EnetAppUtils_print("EnetBoard_getMacAddrList Failed - IDK not present\n");
        EnetAppUtils_assert(false);
    }
}

/*
 * Get ethernet board id
 */
uint32_t EnetBoard_getId(void)
{
    return ENETBOARD_AM263PX_EVM;
}



void Board_init(void)
{

}

void Board_deinit(void)
{


}

