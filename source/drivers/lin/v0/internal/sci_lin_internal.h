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
 *  \defgroup DRV_SCI_LIN_MODULE APIs for LIN
 *  \ingroup DRV_MODULE
 *
 *  This module contains APIs to program and use the LIN.
 *
 *  @{
 */

/**
 *  \file v0/sci_lin_internal.h
 *
 *  \brief This file contains the prototype of LIN driver APIs
 */

#ifndef SCI_LIN_INTERNAL_V0_H_
#define SCI_LIN_INTERNAL_V0_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <drivers/lin/v0/sci_lin.h>
#include <drivers/lin/v0/lin.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/** \brief  Flags to redirect different interrupts to different Interrupt
 *  Lines */
#define LIN_INT_LVL_WAKEUP                      (0x00000002U) /* Wakeup */
#define LIN_INT_LVL_TO                          (0x00000010U) /* Time out */
#define LIN_INT_LVL_TOAWUS                      (0x00000040U) /* Time out after wakeup signal */
#define LIN_INT_LVL_TOA3WUS                     (0x00000080U) /* Time out after 3 wakeup signals */
#define LIN_INT_LVL_TX                          (0x00000100U) /* Transmit buffer ready */
#define LIN_INT_LVL_RX                          (0x00000200U) /* Receive buffer ready */
#define LIN_INT_LVL_ID                          (0x00002000U) /* Received matching identifier */
#define LIN_INT_LVL_PE                          (0x01000000U) /* Parity error */
#define LIN_INT_LVL_OE                          (0x02000000U) /* Overrun error */
#define LIN_INT_LVL_FE                          (0x04000000U) /* Framing error */
#define LIN_INT_LVL_NRE                         (0x08000000U) /* No response error */
#define LIN_INT_LVL_ISFE                        (0x10000000U) /* Inconsistent sync field error */
#define LIN_INT_LVL_CE                          (0x20000000U) /* Checksum error */
#define LIN_INT_LVL_PBE                         (0x40000000U) /* Physical bus error */
#define LIN_INT_LVL_BE                          (0x80000000U) /* Bit error */
#define LIN_INT_LVL_ALL                         (0xFF0023D2U) /* All interrupts */

/** \brief Flags to enable DMA Request */
#define LIN_INT_TX_DMA                          (0x00010000U)
#define LIN_INT_RX_DMA                          (0x00020000U)
#define LIN_INT_RX_DMA_ALL                      (0x00040000U)

/** \brief  Flags for all the different interrupts Lines */
#define LIN_FLAG_MASK_BREAK                     (CSL_LIN_SCIFLR_BRKDT_MASK)
#define LIN_FLAG_MASK_WAKEUP                    (CSL_LIN_SCIFLR_WAKEUP_MASK)
#define LIN_FLAG_MASK_IDLE                      (CSL_LIN_SCIFLR_IDLE_MASK)
#define LIN_FLAG_MASK_BUSY                      (CSL_LIN_SCIFLR_BUSY_MASK)
#define LIN_FLAG_MASK_TO                        (CSL_LIN_SCIFLR_TIMEOUT_MASK)
#define LIN_FLAG_MASK_TOAWUS                    (CSL_LIN_SCIFLR_TOAWUS_MASK)
#define LIN_FLAG_MASK_TOA3WUS                   (CSL_LIN_SCIFLR_TOA3WUS_MASK)
#define LIN_FLAG_MASK_TXRDY                     (CSL_LIN_SCIFLR_TXRDY_MASK)
#define LIN_FLAG_MASK_RXRDY                     (CSL_LIN_SCIFLR_RXRDY_MASK)
#define LIN_FLAG_MASK_TXWAKE                    (CSL_LIN_SCIFLR_TXWAKE_MASK)
#define LIN_FLAG_MASK_TXEMPTY                   (CSL_LIN_SCIFLR_TXEMPTY_MASK)
#define LIN_FLAG_MASK_RXWAKE                    (CSL_LIN_SCIFLR_RXWAKE_MASK)
#define LIN_FLAG_MASK_TXID                      (CSL_LIN_SCIFLR_IDTXFLAG_MASK)
#define LIN_FLAG_MASK_RXID                      (CSL_LIN_SCIFLR_IDRXFLAG_MASK)
#define LIN_FLAG_MASK_PE                        (CSL_LIN_SCIFLR_PE_MASK)
#define LIN_FLAG_MASK_OE                        (CSL_LIN_SCIFLR_OE_MASK)
#define LIN_FLAG_MASK_FE                        (CSL_LIN_SCIFLR_FE_MASK)
#define LIN_FLAG_MASK_NRE                       (CSL_LIN_SCIFLR_NRE_MASK)
#define LIN_FLAG_MASK_ISFE                      (CSL_LIN_SCIFLR_ISFE_MASK)
#define LIN_FLAG_MASK_CE                        (CSL_LIN_SCIFLR_CE_MASK)
#define LIN_FLAG_MASK_PBE                       (CSL_LIN_SCIFLR_PBE_MASK)
#define LIN_FLAG_MASK_BE                        (CSL_LIN_SCIFLR_BE_MASK)

/** \brief  LIN IO DFT Key which when written in IODFTENA
 *          enables the User and Previledge mode Writes */
#define LIN_IODFTENA_KEY                        (0x0AU)

/** \brief LIN ID bit Masks */
#define LIN_ID0_BIT_MASK                        (0x01U)
#define LIN_ID1_BIT_MASK                        (0x02U)
#define LIN_ID2_BIT_MASK                        (0x04U)
#define LIN_ID3_BIT_MASK                        (0x08U)
#define LIN_ID4_BIT_MASK                        (0x10U)
#define LIN_ID5_BIT_MASK                        (0x20U)

#define LIN_ID_DATA_LEN_2_RANGE_HIGH            (0x1FU)
#define LIN_ID_DATA_LEN_2_RANGE_LOW             (0x00U)

#define LIN_ID_DATA_LEN_4_RANGE_HIGH            (0x2FU)
#define LIN_ID_DATA_LEN_4_RANGE_LOW             (0x20U)

#define LIN_FRAME_ID_BIT_MASK                   (0x3FU)

#define LIN_INTR_STS_MASK_ALL                   (0xFFFFFFFFU)

#define LIN_FRAME_FIXED_LEN_2U                  (0x02U)
#define LIN_FRAME_FIXED_LEN_4U                  (0x04U)
#define LIN_FRAME_FIXED_LEN_8U                  (0x08U)

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

void LIN_HLD_EnableTxRxPin(uint32_t baseAddr);
void LIN_HLD_DisableTxRxPin(uint32_t baseAddr);

void LIN_HLD_ModuleResetEnter(uint32_t baseAddr);
void LIN_HLD_ModuleResetExit(uint32_t baseAddr);

void LIN_HLD_ModuleEnterSoftReset(uint32_t baseAddr);
void LIN_HLD_ModuleExitSoftReset(uint32_t baseAddr);

void LIN_HLD_setModuleMode(uint32_t baseAddr, LIN_HLD_ModuleMode mode);
void LIN_HLD_setLinOpMode(uint32_t baseAddr, LIN_HLD_LinMode mode);

void LIN_HLD_enableAutoBaud(uint32_t baseAddr);
void LIN_HLD_disableAutoBaud(uint32_t baseAddr);

void LIN_HLD_setCommMode(uint32_t baseAddr, LIN_HLD_CommMode mode);
void LIN_HLD_setDebugMode(uint32_t baseAddr, LIN_HLD_DebugMode mode);
void LIN_HLD_setChecksumType(uint32_t baseAddr, LIN_HLD_ChecksumType type);
void LIN_HLD_setMaskFilterType(uint32_t baseAddr, LIN_HLD_MaskFilterType type);

void LIN_HLD_enableInternalLoopback(uint32_t baseAddr);
void LIN_HLD_disableInternalLoopback(uint32_t baseAddr);

void LIN_HLD_enableMultiBufferMode(uint32_t baseAddr);
void LIN_HLD_disableMultiBufferMode(uint32_t baseAddr);

void LIN_HLD_enableParity(uint32_t baseAddr);
void LIN_HLD_disableParity(uint32_t baseAddr);

void LIN_HLD_setParityType(uint32_t baseAddr, LIN_HLD_SCIParityType type);
void LIN_HLD_setStopBits(uint32_t baseAddr, LIN_HLD_SCIStopBits bits);

void LIN_HLD_enableDataTx(uint32_t baseAddr);
void LIN_HLD_disableDataTx(uint32_t baseAddr);

void LIN_HLD_enableDataRx(uint32_t baseAddr);
void LIN_HLD_disableDataRx(uint32_t baseAddr);

void LIN_HLD_trigChecksumCompare(uint32_t baseAddr);

void LIN_HLD_setBaudParams(uint32_t baseAddr, LIN_BaudConfigParams *params);
void LIN_HLD_setMaxBaudRate(uint32_t baseAddr, uint32_t linClk,
                            uint32_t maxBaud);

void LIN_HLD_setFrameLength(uint32_t baseAddr, uint8_t length);
uint32_t LIN_HLD_getFrameLength(uint32_t baseAddr);

void LIN_HLD_setCharLength(uint32_t baseAddr, uint8_t length);
uint32_t LIN_HLD_getCharLength(uint32_t baseAddr);

void LIN_HLD_setSyncFields(uint32_t baseAddr, uint32_t syncBrk,
                           uint32_t syncDelimiter);

void LIN_HLD_setTxMask(uint32_t baseAddr, uint32_t mask);
void LIN_HLD_setRxMask(uint32_t baseAddr, uint32_t mask);

void LIN_HLD_disableExternalLoopback(uint32_t baseAddr);
void LIN_HLD_enableExternalLoopback(uint32_t baseAddr,
                                                  LIN_HLD_LoopbackType type,
                                                  LIN_HLD_LoopbackPath path);

void LIN_HLD_readLinRxBuffer(uint32_t baseAddr, uint8_t *data, uint8_t length);
void LIN_HLD_writeLinTxBuffer(uint32_t baseAddr, uint8_t *data, uint8_t length);

void LIN_HLD_writeSciTxReg(uint32_t baseAddr, uint8_t *data);
void LIN_HLD_readSciRxReg(uint32_t baseAddr, uint8_t *data, uint8_t charLength);

void LIN_HLD_setIDByte(uint32_t baseAddr, uint8_t identifier);
uint8_t LIN_HLD_getReceivedID(uint32_t baseAddr);
void LIN_HLD_setIDSlaveTaskByte(uint32_t baseAddr, uint8_t slaveTaskByteID);

/* Interrupt and Status Related Internal Functions */
void LIN_HLD_enableInterrupt(uint32_t baseAddr, uint32_t intrMask);
void LIN_HLD_disableInterrupt(uint32_t baseAddr, uint32_t intrMask);
void LIN_HLD_clearInterrupt(uint32_t baseAddr, uint32_t intrMask);
void LIN_HLD_setInterruptLevel0(uint32_t baseAddr, uint32_t intrMask);
void LIN_HLD_setInterruptLevel1(uint32_t baseAddr, uint32_t intrMask);
void LIN_HLD_enableGlobalInterrupt0(uint32_t baseAddr);
void LIN_HLD_disableGlobalInterrupt0(uint32_t baseAddr);
void LIN_HLD_enableGlobalInterrupt1(uint32_t baseAddr);
void LIN_HLD_disableGlobalInterrupt1(uint32_t baseAddr);
void LIN_HLD_clearGlobalInterrupt0(uint32_t baseAddr);
void LIN_HLD_clearGlobalInterrupt1(uint32_t baseAddr);
uint32_t LIN_HLD_getIntVect0ffset(uint32_t baseAddr);
uint32_t LIN_HLD_getIntVect1ffset(uint32_t baseAddr);
bool LIN_HLD_isTxBufferEmpty(uint32_t baseAddr);
bool LIN_HLD_isRxIdMatch(uint32_t baseAddr);
uint32_t LIN_HLD_getInterruptStatusSCIFLR(uint32_t baseAddr, uint32_t mask);
void LIN_HLD_clearInterruptStatusSCIFLR(uint32_t baseAddr, uint32_t mask);

uint8_t LIN_HLD_gen_ParityID(uint8_t identifier);

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef SCI_LIN_INTERNAL_V0_H_ */

/** @} */
