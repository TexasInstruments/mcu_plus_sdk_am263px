/*
 *  Copyright (C) 2025-2026 Texas Instruments Incorporated
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
 * \file  sbl_tftp.h
 *
 * \brief TFTP client for the Ethernet SBL.
 *
 * Implements the client side of TFTP (RFC 1350) in octet mode to fetch the
 * appimage from a TFTP server.  Only the Read Request (RRQ) path is needed;
 * write support is not implemented.
 */

#ifndef SBL_TFTP_H_
#define SBL_TFTP_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include "sbl_cpswconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

typedef enum SblTftp_State_e
{
    SBLTFTP_STATE_IDLE      = 0, /*!< Waiting for DHCP to complete          */
    SBLTFTP_STATE_ARP_WAIT  = 1, /*!< ARP request sent, waiting for reply   */
    SBLTFTP_STATE_RRQ_SENT  = 2, /*!< RRQ sent, waiting for DATA block 1    */
    SBLTFTP_STATE_RECEIVING = 3, /*!< Receiving DATA blocks                 */
    SBLTFTP_STATE_COMPLETE  = 4, /*!< Transfer finished successfully        */
    SBLTFTP_STATE_ERROR     = 5, /*!< Transfer failed                       */
} SblTftp_State;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  TFTP state-machine handler — called every main-loop iteration.
 *
 * Identifies TFTP DATA / ERROR frames (non-DHCP UDP) in \p rxList and
 * advances the RRQ_SENT → RECEIVING → COMPLETE state machine.
 * Handles retransmit of the last ACK (or the initial RRQ) on timeout because
 * it is called even when \p rxList->count is zero.
 *
 * \param  rxList  Received frame list from SblCpsw_getRxPackets(); may be empty.
 */
void SblTftp_handler(const SblCpsw_RxPktList *rxList);

/*!
 * \brief  Initialise the TFTP client with the transfer parameters.
 *
 * Stores all transfer context so the state machine driven by
 * SblTftp_handler() knows where to fetch from and where to write.
 * Must be called once before any other SblTftp_* function.
 *
 * \param  buf       Caller-allocated buffer to write the received image into.
 * \param  bufLen    Size of \p buf in bytes.
 */
void SblTftp_init(uint8_t    *buf,
                  uint32_t    bufLen);



/*!
 * \brief  Return true once the transfer has finished — either successfully or
 *         with an error.  The main loop uses this to exit the receive loop.
 */
SblTftp_State SblTftp_getState(void);

/*!
 * \brief  Return the number of bytes written to the appimage buffer so far.
 *
 * Valid at any time; meaningful once SblTftp_isComplete() returns true.
 */
uint32_t SblTftp_getReceivedSize(void);

#ifdef __cplusplus
}
#endif

#endif /* SBL_TFTP_H_ */
