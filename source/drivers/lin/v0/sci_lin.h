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
 *  \defgroup DRV_LIN_HLD_MODULE APIs for LIN HLD
 *  \ingroup DRV_MODULE
 *
 *  @{
 *
 *  This module contains APIs to program and use the LIN.
 *
 */

/**
 *  \file v0/sci_lin.h
 *
 *  \brief This file contains the prototype of LIN driver APIs
 *
 *  @{
 */

#ifndef SCI_LIN_V0_H_
#define SCI_LIN_V0_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/dpl/SystemP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/HwiP.h>
#include <drivers/hw_include/cslr.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/hw_include/cslr_lin.h>
#include <drivers/hw_include/hw_types.h>
#include <kernel/dpl/CacheP.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                             Macros & Typedefs                              */
/* ========================================================================== */

/**
 *  \brief A handle that is returned from a LIN_open() call.
 */
typedef struct LIN_Config_ts        *LIN_Handle;

typedef void                        *LIN_DmaChConfig;

typedef void                        *LIN_DmaHandle;

/* ========================================================================== */
/*                                   Enums                                    */
/* ========================================================================== */

/**
 *  \anchor     LIN_OperationalMode
 *  \name       LIN/SCI Operational Mode
 *
 *  \brief      Enumeration defines the Operational Modes.
 *
 *  @{
 */
typedef enum
{
    LIN_OPER_MODE_POLLING           = 0x00U,
    /**< Module to carry out transfers in Polling Mode */
    LIN_OPER_MODE_INTERRUPT         = 0x01U,
    /**< Module to carry out transfers in Interrupt Mode */
    LIN_OPER_MODE_DMA               = 0x02U
    /**< Module to carry out transfers in DMA Mode */

} LIN_OperationalMode;

/** @} */

/**
 *  \anchor     LIN_TransferMode
 *  \name       LIN/SCI Transfer Mode
 *
 *  \brief      Enumeration defines the Transfer Modes.
 *
 *  @{
 */
typedef enum
{
    LIN_TRANSFER_MODE_BLOCKING      = 0x00U,
    /**< Module to transfers in Blocking Mode */
    LIN_TRANSFER_MODE_CALLBACK      = 0x01U
    /**< Module to transfers in Callback Mode */

} LIN_TransferMode;

/** @} */

/**
 *  \anchor     LIN_ModuleMode
 *  \name       LIN Module Operational Mode
 *
 *  \brief      Enumeration defines the Operational Modes of LIN
 *              [ LIN / SCI ]
 *  @{
 */
typedef enum
{
    LIN_MODULE_OP_MODE_LIN          = 0x01U,
    /**< Module to Operate in LIN Mode */
    LIN_MODULE_OP_MODE_SCI          = 0x00U
    /**< Module to Operate in SCI Compatible Mode */

} LIN_HLD_ModuleMode;

/** @} */

/**
 *  \anchor     LIN_LinMode
 *  \name       LIN Operational Mode
 *
 *  \brief      Enumeration defines the LIN Mode to operate in.
 *              [ COMMANDER / RESPONDER ]
 *  @{
 */
typedef enum
{
    LIN_MODE_HLD_LIN_COMMANDER      = 0x01U,
    /**< Module to Operate as LIN Commander */
    LIN_MODE_HLD_LIN_RESPONDER      = 0x00U
    /**< Module to Operate as LIN Responder */

} LIN_HLD_LinMode;

/** @} */

/**
 *  \anchor     LIN_CommMode
 *  \name       LIN COMM Mode
 *
 *  \brief      Enumeration defines the Comm Mode options for LIN & SCI Mode.
 *
 *  @{
 */
typedef enum
{
    LIN_COMM_HLD_LIN_ID4ID5LENCTL   = 0x01U,
    /**< Module to use ID4 & ID5 for length control in LIN Mode. */
    LIN_COMM_HLD_LIN_USELENGTHVAL   = 0x00U,
    /**< Module to use length value for length control in LIN Mode. */

    LIN_COMM_HLD_SCI_ADDRBITMODE    = 0x01U,
    /**< Module to use Address-bit mode in SCI Mode. */
    LIN_COMM_HLD_SCI_IDLELINEMODE   = 0x00U
    /**< Module to use Idle-line mode in SCI Mode. */

} LIN_HLD_CommMode;

/** @} */

/**
 *  \anchor     LIN_DebugMode
 *  \name       LIN Debug Mode
 *
 *  \brief      Enumeration defines the Debug Mode options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_DEBUG_FROZEN            = 0x00U,
    /**< Freeze module during debug. */
    LIN_HLD_DEBUG_COMPLETE          = 0x01U
    /**< Complete Tx/Rx before Freezing. */

} LIN_HLD_DebugMode;

/** @} */

/**
 *  \anchor     LIN_ChecksumType
 *  \name       LIN Checksum Type
 *
 *  \brief      Enumeration defines the Checksum Type options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_CHECKSUM_CLASSIC        = 0x00U,
    /**< Module to use Classic Checksum. */
    LIN_HLD_CHECKSUM_ENHANCED       = 0x01U
    /**< Module to use Enhanced Checksum. */

} LIN_HLD_ChecksumType;

/** @} */

/**
 *  \anchor     LIN_MaskFilterType
 *  \name       LIN Mask Filtering Type
 *
 *  \brief      Enumeration defines the Mask filtering Type options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_MSG_FILTER_IDBYTE       = 0x00U,
    /**< IDBYTE fields in the LINID register are used for detecting a Match */
    LIN_HLD_MSG_FILTER_IDRESPONDER  = 0x01U
    /**< LAVETASKBYTE fields in the LINID register are used for detecting a
     * Match */

} LIN_HLD_MaskFilterType;

/** @} */

/**
 *  \anchor     LIN_SCIParityType
 *  \name       LIN SCI Parity Type
 *
 *  \brief      Enumeration defines the Parity Type options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_SCI_PARITY_ODD          = 0x00U,
    /**< Module to use Odd Parity. */
    LIN_HLD_SCI_PARITY_EVEN         = 0x01U
    /**< Module to use Even Parity. */

} LIN_HLD_SCIParityType;

/** @} */

/**
 *  \anchor     LIN_HLD_SCIStopBits
 *  \name       LIN SCI Stop Bits
 *
 *  \brief      Enumeration defines the Stop Bits options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_SCI_STOP_BITS_1         = 0x00U,
    /**< Module to use One Stop Bit. */
    LIN_HLD_SCI_STOP_BITS_2         = 0x01U
    /**< Module to use Two Stop Bits. */

} LIN_HLD_SCIStopBits;

/** @} */

/**
 *  \anchor     LIN_LoopbackMode
 *  \name       Module LoopBack Mode
 *
 *  \brief      Enumeration defines the Loopback Mode options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_LOOPBACK_INTERNAL       = 0x00U,
    /**< Module to use Internal Loopback. */
    LIN_HLD_LOOPBACK_EXTERNAL       = 0x01U
    /**< Module to use External Loopback. */

} LIN_HLD_LoopbackMode;

/** @} */

/**
 *  \anchor     LIN_HLD_LoopbackType
 *  \name       Module LoopBack type
 *
 *  \brief      Enumeration defines the Loopback Type options.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_LOOPBACK_DIGITAL        = 0x00U,
    /**< Module to use Digital Loopback. */
    LIN_HLD_LOOPBACK_ANALOG         = 0x01U
    /**< Module to use Analog Loopback. */

} LIN_HLD_LoopbackType;

/** @} */

/**
 *  \anchor     LIN_HLD_LoopbackPath
 *  \name       Module LoopBack Path in Analog loopback Mode
 *
 *  \brief      Enumeration defines the Loopback path Type options.
 *
 *  @{
 */
typedef enum
{

    LIN_HLD_ANALOG_LOOP_TX          = 0x00U,
    /**< Module to use Analog loopback through transmit Pin */
    LIN_HLD_ANALOG_LOOP_RX          = 0x01U
    /**< Module to use Analog loopback through receive Pin */

} LIN_HLD_LoopbackPath;

/** @} */

/**
 *  \anchor     LIN_HLD_SyncDelimiter
 *  \name       LIN Commander Mode Sync Delimiter
 *
 *  \brief      Enumeration defines LIN Commander Mode Sync Delimiter Length
 *              in T-Bits.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_SYNC_DELIMITER_LEN_1    = 0x0U,
    /**< The Sync delimiter is 1 Tbit Long */
    LIN_HLD_SYNC_DELIMITER_LEN_2    = 0x1U,
    /**< The Sync delimiter is 2 Tbit Long */
    LIN_HLD_SYNC_DELIMITER_LEN_3    = 0x2U,
    /**< The Sync delimiter is 3 Tbit Long */
    LIN_HLD_SYNC_DELIMITER_LEN_4    = 0x3U,
    /**< The Sync delimiter is 4 Tbit Long */

} LIN_HLD_Sync_Delimiter;

/** @} */

/**
 *  \anchor     LIN_HLD_SyncBreak
 *  \name       LIN Commander Mode Sync Delimiter
 *
 *  \brief      Enumeration defines LIN Commander Mode Sync Break field Length.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_SYNC_BREAK_LEN_13       = 0x0U,
    /**< The Sync Break Field is 13 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_14       = 0x1U,
    /**< The Sync Break Field is 14 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_15       = 0x2U,
    /**< The Sync Break Field is 15 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_16       = 0x3U,
    /**< The Sync Break Field is 16 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_17       = 0x4U,
    /**< The Sync Break Field is 17 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_18       = 0x5U,
    /**< The Sync Break Field is 18 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_19       = 0x6U,
    /**< The Sync Break Field is 19 Tbit Long */
    LIN_HLD_SYNC_BREAK_LEN_20       = 0x7U,
    /**< The Sync Break Field is 20 Tbit Long */

} LIN_HLD_Sync_Break;

/** @} */

/**
 *  \anchor     LIN_HLD_Txn_Status
 *  \name       LIN/SCI Transaction Status
 *
 *  \brief      Enumeration defines LIN/SCI Transaction Status.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_TXN_STS_SUCCESS         = 0x00U,
    /**< Transaction status Success Generic */
    LIN_HLD_TXN_STS_FAILURE         = 0x01U,
    /**< Transaction status Failure Generic */
    LIN_HLD_TXN_STS_TIMEOUT         = 0x02U,
    /**< Transaction status Timeout */
    LIN_HLD_TXN_PHY_BUS_ERR         = 0x03U,
    /**< Transaction status Physical Bus Error */
    LIN_HLD_TXN_FRAMING_ERR         = 0x04U,
    /**< Transaction status Frame Error */
    LIN_HLD_TXN_OVERRUN_ERR         = 0x05U,
    /**< Transaction status Overrun Error */
    LIN_HLD_TXN_PARITY_ERR          = 0x06U,
    /**< Transaction status Parity Error */
    LIN_HLD_TXN_CHECKSUM_ERR        = 0x07U,
    /**< Transaction status Checksum Error */
    LIN_HLD_TXN_NO_RES_ERR          = 0x08U,
    /**< Transaction status No Response Error */
    LIN_HLD_TXN_BIT_ERR             = 0x09U,
    /**< Transaction status Bit Error */

} LIN_HLD_Txn_Status;

/** @} */

/**
 *  \anchor     LIN_HLD_Txn_Type
 *  \name       LIN/SCI Transaction Type
 *
 *  \brief      Enumeration defines LIN/SCI Transaction Types.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_TXN_TYPE_WRITE          = 0x00U,
    /**< Transaction Type Write */
    LIN_HLD_TXN_TYPE_READ           = 0x01U,
    /**< Transaction Type Read */

} LIN_HLD_Txn_Type;

/** @} */

/**
 *  \anchor     LIN_HLD_State
 *  \name       LIN/SCI Driver State
 *
 *  \brief      Enumeration defines LIN/SCI Driver States.
 *
 *  @{
 */
typedef enum
{
    LIN_HLD_STATE_RESET             = (uint8_t)0U,
    /**< Driver State Reset */
    LIN_HLD_STATE_IDLE              = (uint8_t)1U,
    /**< Driver State Idle */
    LIN_HLD_STATE_BUSY              = (uint8_t)2U,
    /**< Driver State Busy */
    LIN_HLD_STATE_ERROR             = (uint8_t)3U,
    /**< Driver State Error */

} LIN_HLD_State;

/** @} */

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/**
 *  \anchor LIN_HwAttrs
 *  \name LIN Hardware Attributes
 *
 *  \brief LIN Hardware attributes Used during initialization.
 */
typedef struct LIN_HwAttrs_ts
{
/** LIN Peripheral Base Address */
    uint32_t                        baseAddr;
/** LIN Peripheral Interrupt vector 0 */
    uint32_t                        intrNum0;
/** LIN Peripheral Interrupt vector 1 */
    uint32_t                        intrNum1;
/** Operational Mode
 *  Refer Enumeration \ref LIN_OperationalMode */
    LIN_OperationalMode             opMode;
/** LIN interrupt Priority */
    uint8_t                         intrPriority;
/** Debug Mode Configuration of Module
 *  Refer Enumeration \ref LIN_HLD_DebugMode */
    LIN_HLD_DebugMode               debugMode;
/** LIN Clock Speed */
    uint32_t                        linClk;
/** Enable Loopback */
    bool                            enableLoopback;
/** Loopback Mode
 *  Refer Enumeration \ref LIN_HLD_LoopbackMode */
    LIN_HLD_LoopbackMode            loopBackMode;
/** Loopback Type
 *  Refer Enumeration \ref LIN_HLD_LoopbackType */
    LIN_HLD_LoopbackType            loopBackType;

} LIN_HwAttrs;

/**
 *  \anchor LIN_BaudConfigParams
 *  \name   LIN Baud Configuration Parameters
 *
 *  \brief   Data structure defines the Baud Rate configuration parameters.
 */
typedef struct LIN_BaudConfigParams_ts
{
/** 24-bit integer Pre-Scaler */
    uint32_t                        preScaler;
/** 4-bit Fractional Divider Selection
 *  These bits are effective in LIN or SCI asynchronous mode.
 *  These bits are used to select a baud rate for the SCI/LIN
 *  module, and they are a fractional part for the baud rate
 *  specification. The M divider allows fine-tuning of the baud rate
 *  over the P pre-scaler with 15 additional intermediate values for
 *  each of the P integer values. */
    uint8_t                         fracDivSel_M;
/** Super fractional Divider Selection
 *  These bits are an additional fractional part for the baudrate
 *  specification. These bits allow a super fine tuning of the
 *  fractional baudrate with 7 more intermediate values for each
 *  of the M fractional divider values. See the Super fractional
 *  Divider section for more details. */
    uint8_t                         supFracDivSel_U;

} LIN_BaudConfigParams;

/**
 *  \brief LIN Frame Structure
 */
typedef struct LIN_SCI_Frame_ts
{
/** Frame ID
 *  LIN Mode: ID is Used to write data to the Bus or read data from slave.
 *  SCI Mode: ID is ignored. */
    uint8_t                         id;
/** Frame Length, Length of the Frame to write/read */
    uint8_t                         frameLen;
/** Pointer to Data Buffer */
    void                            *dataBuf;
/** Frame Transfer Type Write/Read */
    LIN_HLD_Txn_Type                txnType;
/** Timeout(Micro Seconds) of Transaction in Polling mode */
    uint32_t                        timeout;
/** Transaction Status */
    LIN_HLD_Txn_Status              status;
/** Argument to be passed to the callback function in callback Mode */
    void                            *args;

} LIN_SCI_Frame;

/* ========================================================================== */
/*                      Function Pointers Declarations                        */
/* ========================================================================== */

/**
 *  \brief  The definition of a callback function used by the LIN driver
 *  when used in #LIN_TRANSFER_MODE_CALLBACK.
 *
 *  \param handle           LIN_Handle
 *  \param frame            Pointer to a #LIN_SCI_Frame
 */
typedef void (*LIN_IdMatchCallbackFxn) (LIN_Handle handle,
                                        LIN_SCI_Frame *frame);

/**
 *  \brief  The definition of a callback function used by the LIN driver
 *  when used in #LIN_TRANSFER_MODE_CALLBACK
 *
 *  \param handle           LIN_Handle
 *  \param frame            Pointer to a #LIN_SCI_Frame
 */
typedef void (*LIN_TransferCompleteCallbackFxn) (LIN_Handle handle,
                                                 LIN_SCI_Frame *frame);

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/**
 *  \anchor LIN_SciConfigParams
 *  \name   LIN SCI Configuration Parameters
 *
 *  \brief   Data structure defines the SCI configuration parameters.
 */
typedef struct LIN_SciConfigParams_ts
{
/** Parity selection
 *  true: Enable Parity.
 *  false: Disable Parity. */
    LIN_HLD_SCIParityType           parityType;
/** Stop bits
 *  true: Two Stop Bits.
 *  false: One Stop Bit. */
    LIN_HLD_SCIStopBits             stopBits;
/** Data bits [1-8] */
    uint8_t                         dataBits;
/** Transfer Complete Callback Function Pointer(Both TX & RX) */
    LIN_TransferCompleteCallbackFxn transferCompleteCallbackFxn;

} LIN_SciConfigParams;

/**
 *  \anchor LIN_LinConfigParams
 *  \name   LIN Configuration Parameters
 *
 *  \brief   Data structure defines the SCI configuration parameters.
 */
typedef struct LIN_LinConfigParams_ts
{
/** LIN Operational Mode
 *  Refer Enumeration \ref LIN_HLD_LinMode */
    LIN_HLD_LinMode                 linMode;
/** Mask filtering Type
 *  Refer Enumeration \ref LIN_HLD_MaskFilterType */
    LIN_HLD_MaskFilterType          maskFilteringType;
/** LIN TX Mask */
    uint8_t                         linTxMask;
/** LIN RX Mask */
    uint8_t                         linRxMask;
/** Checksum Type
 *  Refer Enumeration \ref LIN_HLD_ChecksumType */
    LIN_HLD_ChecksumType            checksumType;
/** Auto baud adjustment ADAPT (Enable/Disable)
 *  false: Automatic Baud Rate adjustment is Disabled.
 *  true: Automatic Baud Rate adjustment is Enabled. */
    bool                            adaptModeEnable;
/** Maximum Possible Baud Rate */
    uint32_t                        maxBaudRate;
/** Sync Delimiter Length */
    LIN_HLD_Sync_Delimiter          syncDelimiter;
/** Sync Break Length */
    LIN_HLD_Sync_Break              syncBreak;
/** ID Match Callback Function Pointer(only when Slave/Responder) */
    LIN_IdMatchCallbackFxn          idMatchCallbackFxn;
/** Transfer Complete Callback Function Pointer(Both Commander & Responder) */
    LIN_TransferCompleteCallbackFxn transferCompleteCallbackFxn;

} LIN_LinConfigParams;

/**
 *  \anchor LIN_OpenParams
 *  \name   LIN Open Params
 *
 *  \brief LIN Parameters. Data structure defines the LIN initialization
 *  parameters.
 *  LIN Open Parameters are used with the #LIN_open() call.
 */
typedef struct LIN_OpenParams_ts
{
/** Mode of Transfer
 *  Refer Enumeration \ref LIN_TransferMode */
    LIN_TransferMode                transferMode;
/** LIN Mode Enable
 *  Refer Enumeration \ref LIN_HLD_ModuleMode */
    LIN_HLD_ModuleMode              moduleMode;
/** Parity Enable
 *  true: Enable Parity.
 *  false: Disable Parity. */
    bool                            enableParity;
/** Communication Mode
 *  Refer Enumeration \ref LIN_HLD_CommMode */
    LIN_HLD_CommMode                commMode;
/** Multi Buffer Mode Enable
 *  true: The multi-buffer mode is enabled.
 *  false: The multi-buffer mode is disabled. */
    bool                            multiBufferMode;
/** Baud Config Parameters
 *  Refer Structure \ref LIN_BaudConfigParams */
    LIN_BaudConfigParams            baudConfigParams;
/** SCI frame Configuration Parameters
 *  Refer Structure \ref LIN_SciConfigParams */
    LIN_SciConfigParams             sciConfigParams;
/** LIN frame Configuration Parameters
 *  Refer Structure \ref LIN_LinConfigParams */
    LIN_LinConfigParams             linConfigParams;

} LIN_OpenParams;

/**
 *  \anchor LIN_Object
 *  \name LIN Object
 *
 *  \brief SCI/LIN object Used by the Driver.
 *  The application must not access any member variables of this Structure.
 */
typedef struct LIN_Object_ts
{
/** Grants exclusive access to LIN Instance */
    SemaphoreP_Object               mutex;
/** Hwi objects 0 */
    HwiP_Object                     hwiObj0;
/** Hwi objects 1 */
    HwiP_Object                     hwiObj1;
/** Stores the LIN module instance state */
    LIN_HLD_State                   state;
/** Flag to indicate whether module instance is open or not */
    bool                            isOpen;
/** Pointer to the LIN_OpenParams Typedef Structure used to open this instance */
    LIN_OpenParams                  *openParams;
/** Dma driver handle */
    LIN_DmaHandle                   linDmaHandle;
/** Pointer to DMA Configuration for this instance */
    LIN_DmaChConfig                 dmaChCfg;
/** Pointer to the current Transaction Frame */
    LIN_SCI_Frame                   *currentTxnFrame;
/** Internal inc. writeBuf index */
    uint8_t                         *writeBufIdx;
/** Internal dec. writeCounter */
    uint32_t                        writeCountIdx;
/** Internal inc. readBuf index */
    uint8_t                         *readBufIdx __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));
/** Internal dec. readCounter */
    uint32_t                        readCountIdx __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));
/** Temporary buffer used by the driver internally */
    uint8_t                         tempBuffer[8] __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));
/** Write Frame Sync Semaphore - to sync between transfer completion ISR
 *  and task */
    SemaphoreP_Object               writeFrmCompSemObj;
/** Read Frame Sync Semaphore - to sync between transfer completion ISR
 *  and task */
    SemaphoreP_Object               readFrmCompSemObj;

} LIN_Object;

/**
 *  \brief LIN Global Configuration
 *
 *  The LIN_Config structure contains a set of pointers used to characterize
 *  the LIN driver implementation.
 *
 *  This structure needs to be defined before calling LIN_init() and it must
 *  not be changed thereafter.
 *
 */
typedef struct LIN_Config_ts
{
/** Pointer to a driver specific data object
 *  Refer Structure \ref LIN_Object */
    LIN_Object                      *object;
/** Pointer to a driver specific hardware attributes structure
 *  Refer Structure \ref LIN_HwAttrs */
    LIN_HwAttrs const               *hwAttrs;

} LIN_Config;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 *  \brief  Initialize each driver instance object and create driver lock.
 */
void LIN_init(void);

/**
 *  \brief  De-initialize each driver instance object and delete driver lock.
 */
void LIN_deinit(void);

/**
 *  \brief  API to initialize the #LIN_OpenParams struct to its defaults.
 *
 *  \param  openParams  [IN] Pointer to #LIN_OpenParams structure to be
 *                           initialized
 */
void LIN_Params_init(LIN_OpenParams *openParams);

/**
 *  \brief  API to Open a given LIN Instance
 *
 *  \pre    LIN has been initialized using #LIN_init()
 *
 *  \param  index       [IN] Index of config to be used in the #LIN_Config array
 *  \param  openParams  [IN] Pointer to open parameters. If NULL is passed, then
 *                           default values will be used.
 *
 *  \return #LIN_Handle on Success or NULL in case of an error or already open.
 *
 *  \sa #LIN_init()
 */
LIN_Handle LIN_open(uint32_t index, LIN_OpenParams *openParams);

/**
 *  \brief API to Close the LIN Instance specified by the handle
 *
 *  \pre #LIN_open() has to be called first
 *
 *  \param handle       [IN] #LIN_Handle returned from LIN_open()
 *
 *  \sa #LIN_open()
 */
void LIN_close(LIN_Handle handle);

/**
 *  \brief API to set default values of LIN_SCI_Frame in frame
 *
 *  \param frame        [IN] pointer to the structure to be initialized
 */
void LIN_SCI_Frame_init(LIN_SCI_Frame *frame);

/**
 *  \brief API to initiate a LIN/SCI frame transfer.
 *
 *  \pre #LIN_open() has to be called first
 *
 *  \param handle       [IN] #LIN_Handle returned from LIN_open()
 *  \param frame        [IN] Pointer to the #LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 *
 *  \sa #LIN_open()
 */
int32_t LIN_SCI_transferFrame(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 *  \brief  API to get the handle of an open LIN instance from the
 *  instance index
 *
 *  \param  index       [IN] Index of config to use in the *LIN_Config* array
 *
 *  \return A #LIN_Handle on success or a NULL on an error or if the instance
 *            index has NOT been opened yet.
 */
LIN_Handle LIN_getHandle(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif  /* SCI_LIN_V0_H_ */

/** @} */
/** @} */
