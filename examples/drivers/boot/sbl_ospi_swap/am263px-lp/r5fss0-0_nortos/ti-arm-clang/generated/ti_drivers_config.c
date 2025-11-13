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

#include "ti_drivers_config.h"

/*
 * OSPI
 */
#include <drivers/ospi/v0/ospi.h>


/* Regions restricted for DMA. We should use CPU memcpy in these cases */
static OSPI_AddrRegion gOspiDmaRestrictRegions[] =
{
    {
        .regionStartAddr = CSL_MSS_TCMA_RAM_BASE,
        .regionSize      = CSL_MSS_TCMA_RAM_SIZE,
    },
    {
        .regionStartAddr = CSL_HSM_RAM_U_BASE,
        .regionSize      = 0x2fffc,
    },
    {
        .regionStartAddr = 0xFFFFFFFFU,
        .regionSize      = 0U,
    }
};

/* OSPI attributes */
static OSPI_Attrs gOspiAttrs[CONFIG_OSPI_NUM_INSTANCES] =
{
    {
        .baseAddr             = CSL_FLASH_CONFIG_REG8_U_BASE,
        .dataBaseAddr         = CSL_FLASH_DATA_REG1_U_BASE,
        .protocol             = OSPI_PROTO_8D_8D_8D,
        .inputClkFreq         = 133333333U,
        .intrNum              = 54U,
        .intrEnable           = FALSE,
        .intrPriority         = 4U,
        .dmaEnable            = TRUE,
        .phyEnable            = TRUE,
        .dacEnable            = FALSE,
        .chipSelect           = OSPI_CS0,
        .frmFmt               = OSPI_FF_POL0_PHA0,
        .decChipSelect        = OSPI_DECODER_SELECT4,
        .baudRateDiv          = 8,
        .dmaRestrictedRegions = gOspiDmaRestrictRegions,
        .phyConfiguration     = {
            .phaseDelayElement    = 1,
            .phyControlMode = OSPI_FLASH_CFG_PHY_MASTER_CONTROL_REG_PHY_MASTER_MODE,
            .dllLockMode    = OSPI_PHY_DLL_HALF_CYCLE_LOCK,
            .tuningWindowParams = {
                .txDllLowWindowStart    = 0,
                .txDllLowWindowEnd      = 48,
                .txDllHighWindowStart   = 20,
                .txDllHighWindowEnd     = 96,
                .rxLowSearchStart       = 0,
                .rxLowSearchEnd         = 40,
                .rxHighSearchStart      = 10,
                .rxHighSearchEnd        = 127,
                .txLowSearchStart       = 0,
                .txLowSearchEnd         = 64,
                .txHighSearchStart      = 20,
                .txHighSearchEnd        = 127,
                .txDLLSearchOffset      = 8,
                .rxTxDLLSearchStep      = 4,
                .rdDelayMin             = 1,
                .rdDelayMax             = 3,
            }
        }
    },
};

/* OSPI objects - initialized by the driver */
static OSPI_Object gOspiObjects[CONFIG_OSPI_NUM_INSTANCES];
/* OSPI driver configuration */
OSPI_Config gOspiConfig[CONFIG_OSPI_NUM_INSTANCES] =
{
    {
        &gOspiAttrs[CONFIG_OSPI0],
        &gOspiObjects[CONFIG_OSPI0],
    },
};

uint32_t gOspiConfigNum = CONFIG_OSPI_NUM_INSTANCES;

#include <drivers/ospi/v0/lld/dma/ospi_lld_dma.h>
#include <drivers/ospi/v0/lld/dma/edma/ospi_edma_lld.h>

OspiDma_EdmaArgs gOspiEdmaArgs[1];

OSPI_DmaConfig gOspiDmaConfig[CONFIG_OSPI_NUM_DMA_INSTANCES] =
{
    {
        .fxns        = &gOspiDmaEdmaFxns,
        .ospiDmaArgs = &(gOspiEdmaArgs[0])
    }
};
uint32_t gOspiDmaConfigNum = CONFIG_OSPI_NUM_DMA_INSTANCES;


/*
 * BOOTLOADER
 */
/* Include ti_board_config.h for flash config macros */
#include "ti_board_config.h"

/* Bootloader boot media specific arguments */
Bootloader_FlashArgs gBootloader0Args =
{
    .flashIndex     = CONFIG_FLASH0,
    .curOffset      = 0,
    .appImageOffset = 0x00081000,
};

/* Configuration option for lockstep or standalone */
Bootloader_socCoreOpModeConfig operatingMode=
{
    BOOTLOADER_OPMODE_LOCKSTEP,
    BOOTLOADER_OPMODE_LOCKSTEP,
};


/* Bootloader driver configuration */
Bootloader_Config gBootloaderConfig[CONFIG_BOOTLOADER_NUM_INSTANCES] =
{
    {
        &gBootloaderFlashFxns,
        &gBootloader0Args,
        BOOTLOADER_MEDIA_FLASH,
        0,
        0,
        NULL,
        .socCoreOpMode= (void *)&operatingMode,
        .isAppimageSigned = TRUE,
        .disableAppImageAuth = FALSE,
        .initICSSCores = FALSE,
        0,
    },
};

uint32_t gBootloaderConfigNum = CONFIG_BOOTLOADER_NUM_INSTANCES;

SecureBoot_Stream_t gSecureBootStreamArray[MAX_SECURE_BOOT_STREAM_LENGTH];

/*
 * HSM Client
 */

/* memory assigned for each R5x <-> HSM channel */
uint8_t gQueue_HsmToSecureHost[SIPC_NUM_R5_CORES][SIPC_QUEUE_LENGTH*SIPC_MSG_SIZE] __attribute__((aligned(8),section(".bss.sipc_hsm_queue_mem")));
uint8_t gQueue_SecureHostToHsm[SIPC_NUM_R5_CORES][SIPC_QUEUE_LENGTH*SIPC_MSG_SIZE] __attribute__((aligned(8),section(".bss.sipc_secure_host_queue_mem")));
HsmClient_t gHSMClient ;

/* Queue used to store HSM client messages that need to be dispatched via SIPC */
HsmMsg_t gHsmClientMsgQueue[HSM_CLIENT_MSG_QUEUE_SIZE];

void HsmClient_config(void)
{
    SIPC_Params sipcParams;
    int32_t status;

    /* initialize parameters to default */
    SIPC_Params_init(&sipcParams);

    sipcParams.ipcQueue_eleSize_inBytes = SIPC_MSG_SIZE;
    sipcParams.ipcQueue_length = SIPC_QUEUE_LENGTH ;
    /* list the cores that will do SIPC communication with this core
    * Make sure to NOT list 'self' core in the list below
    */
    sipcParams.numCores = 1;
    sipcParams.coreIdList[0] = CORE_INDEX_HSM;

    /* specify the priority of SIPC Notify interrupt */
    sipcParams.intrPriority = 7U;


    /* This is HSM -> R5F queue */
    sipcParams.tx_SipcQueues[CORE_INDEX_HSM] = (uintptr_t)gQueue_SecureHostToHsm[0] ;
    sipcParams.rx_SipcQueues[CORE_INDEX_HSM] = (uintptr_t)gQueue_HsmToSecureHost[0] ;
    sipcParams.secHostCoreId[CORE_INDEX_SEC_MASTER_0] = CORE_ID_R5FSS0_0;

    /* initialize the HsmClient module */
    status = HsmClient_init(&sipcParams);
    HsmClient_SecureBootQueueInit(HSM_CLIENT_MSG_QUEUE_SIZE);
    DebugP_assert(status==SystemP_SUCCESS);

    /* register a hsm client to detect bootnotify message and keyring import from HSM */
    status = HsmClient_register(&gHSMClient, HSM_BOOT_NOTIFY_CLIENT_ID);
    DebugP_assert(status==SystemP_SUCCESS);
}

void HsmClient_unRegister(void)
{
     /* Unregister bootnotify client */
    HsmClient_unregister(&gHSMClient, HSM_BOOT_NOTIFY_CLIENT_ID);
}

/*
 * EDMA
 */
/* EDMA atrributes */
static EDMA_Attrs gEdmaAttrs[CONFIG_EDMA_NUM_INSTANCES] =
{
    {

        .baseAddr           = CSL_TPCC0_U_BASE,
        .compIntrNumber     = CSLR_R5FSS0_CORE0_INTR_TPCC0_INT_0,
        .intrPriority       = 15U,
        .errIntrNumber      = CSLR_R5FSS0_CORE0_INTR_TPCC0_ERRAGGR,
        .errIntrPriority    = 15U,
        .intrAggEnableAddr  = CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_TPCC0_INTAGG_MASK,
        .intrAggEnableMask  = 0x1FF & (~(2U << 0)),
        .intrAggStatusAddr  = CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_TPCC0_INTAGG_STATUS,
        .intrAggClearMask   = (2U << 0),
        .errIntrAggEnableAddr  = CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_TPCC0_ERRAGG_MASK,
        .errIntrAggEnableMask  = 0xFFFFFFFF & (~(0x707001F)),
        .errIntrAggStatusAddr  = CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_TPCC0_ERRAGG_STATUS,
        .errIntrAggRawStatusAddr  = CSL_MSS_CTRL_U_BASE + CSL_MSS_CTRL_TPCC0_ERRAGG_STATUS_RAW,
        .initPrms           =
        {
            .regionId     = 0,
            .queNum       = 0,
            .initParamSet = FALSE,
            .ownResource    =
            {
                .qdmaCh      = 0x03U,
                .dmaCh[0]    = 0xFFFFFFFFU,
                .dmaCh[1]    = 0x000000FFU,
                .tcc[0]      = 0xFFFFFFFFU,
                .tcc[1]      = 0x000000FFU,
                .paramSet[0] = 0xFFFFFFFFU,
                .paramSet[1] = 0xFFFFFFFFU,
                .paramSet[2] = 0xFFFFFFFFU,
                .paramSet[3] = 0xFFFFFFFFU,
                .paramSet[4] = 0xFFFFFFFFU,
                .paramSet[5] = 0xFFFFFFFFU,
                .paramSet[6] = 0xFFFFFFFFU,
                .paramSet[7] = 0x000007FFU,
            },
            .reservedDmaCh[0]    = 0x00000000U,
            .reservedDmaCh[1]    = 0x00000000U,
        },
    },
};

/* EDMA objects - initialized by the driver */
static EDMA_Object gEdmaObjects[CONFIG_EDMA_NUM_INSTANCES];
/* EDMA driver configuration */
EDMA_Config gEdmaConfig[CONFIG_EDMA_NUM_INSTANCES] =
{
    {
        &gEdmaAttrs[CONFIG_EDMA0],
        &gEdmaObjects[CONFIG_EDMA0],
    },
};

uint32_t gEdmaConfigNum = CONFIG_EDMA_NUM_INSTANCES;

/*
 * UART
 */

/* UART atrributes */
static UART_Attrs gUartAttrs[CONFIG_UART_NUM_INSTANCES] =
{
        {
            .baseAddr           = CSL_UART0_U_BASE,
            .inputClkFreq       = 48000000U,
        },
};
/* UART objects - initialized by the driver */
static UART_Object gUartObjects[CONFIG_UART_NUM_INSTANCES];
/* UART driver configuration */
UART_Config gUartConfig[CONFIG_UART_NUM_INSTANCES] =
{
        {
            &gUartAttrs[CONFIG_UART0],
            &gUartObjects[CONFIG_UART0],
        },
};

uint32_t gUartConfigNum = CONFIG_UART_NUM_INSTANCES;

#include <drivers/uart/v0/lld/dma/uart_dma.h>
UART_DmaHandle gUartDmaHandle[] =
{
};

uint32_t gUartDmaConfigNum = CONFIG_UART_NUM_DMA_INSTANCES;

void Drivers_uartInit(void)
{
    UART_init();
}

/*
 * MCU_LBIST
 */

uint32_t gMcuLbistTestStatus = 0U;

void SDL_lbist_selftest(void)
{
}

void Pinmux_init(void);
void PowerClock_init(void);
void PowerClock_deinit(void);
/*
 * Common Functions
 */
void System_init(void)
{
    /* DPL init sets up address transalation unit, on some CPUs this is needed
     * to access SCICLIENT services, hence this needs to happen first
     */
    Dpl_init();

    
    PowerClock_init();
    /* Now we can do pinmux */
    Pinmux_init();
    /* finally we initialize all peripheral drivers */
    OSPI_init();
    HsmClient_config();

    EDMA_init();
    Drivers_uartInit();
}

void System_deinit(void)
{
    OSPI_deinit();
    HsmClient_unRegister();

    EDMA_deinit();
    UART_deinit();
    PowerClock_deinit();

    Dpl_deinit();
}
