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
 * \file  sbl_dhcp.h
 *
 * \brief Minimal DHCP client for the Ethernet SBL.
 *
 * Implements the DISCOVER → OFFER → REQUEST → ACK exchange to obtain an IP
 * address from a DHCP server.  Only the subset of DHCP options needed for
 * basic network connectivity (assigned IP, subnet mask, default gateway) is
 * parsed.  The state machine stops automatically after reaching BOUND.
 */

#ifndef SBL_DHCP_H_
#define SBL_DHCP_H_

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
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*! Maximum length (including null terminator) for the boot-file name string
 *  received in DHCP option 67. */
#define SBLDHCP_BOOTFILE_MAX_LEN    (128U)

/* ========================================================================== */
/*                          Structures / Types                                */
/* ========================================================================== */

/*!
 * \brief  Network parameters obtained from a successful DHCP exchange.
 */
typedef struct SblDhcp_Params_s
{
    uint32_t assignedIp;                        /*!< Assigned IPv4 address (host byte order)             */
    uint32_t subnetMask;                        /*!< Subnet mask (host byte order)                       */
    uint32_t gatewayIp;                         /*!< Default gateway IP (host byte order)                */
    uint32_t tftpServerIp;                      /*!< TFTP server IP from option 66, or 0 if not provided */
    char     bootfileName[SBLDHCP_BOOTFILE_MAX_LEN]; /*!< Boot-file name from option 67, or "" if absent */
} SblDhcp_Params;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  DHCP state-machine handler — called every main-loop iteration.
 *
 * On the first call after \c INIT the handler sends a DHCPDISCOVER.
 * It then scans \p rxList for OFFER / ACK / NAK replies and advances the
 * state machine accordingly.  Retransmits are triggered by timeout because
 * this function is invoked even when \p rxList->count is zero.
 * Once the state reaches \c BOUND the handler is a no-op.
 *
 * \param  rxList  Received frame list from SblCpsw_getRxPackets(); may be empty.
 */
void SblDhcp_handler(const SblCpsw_RxPktList *rxList);

/*!
 * \brief  Initialise the DHCP client with the host MAC address.
 *
 * Resets the state machine to INIT and stores \p macAddr for use in DHCP
 * packets.  Must be called once before \c SblDhcp_handler.
 *
 * \param  macAddr  6-byte host MAC address (from SblCpsw_getMacAddr()).
 */
void SblDhcp_init(const uint8_t *macAddr);

/*!
 * \brief  Return true once the DORA exchange has completed successfully.
 */
bool SblDhcp_isDone(void);

/*!
 * \brief  Copy the network parameters obtained during the DORA exchange.
 *
 * \param  params  Output structure populated on success.
 *
 * \return ENET_SOK when bound, ENET_EFAIL if the exchange is not yet complete.
 */
int32_t SblDhcp_getNetworkParams(SblDhcp_Params *params);

/*!
 * \brief  Return the TFTP server IP address received in DHCP option 66.
 *
 * Option 66 carries the TFTP server name as a dotted-decimal string
 * (e.g. "192.168.1.1") or a raw 4-byte address.  Returns 0 if the option
 * was not present in the ACK or DHCP is not yet BOUND.
 */
uint32_t SblDhcp_getTftpServerIp(void);

/*!
 * \brief  Return the boot-file name received in DHCP option 67.
 *
 * Returns a pointer to a null-terminated string inside the DHCP module's
 * internal state.  The pointer is valid as long as the DHCP object is alive.
 * Returns an empty string ("") if option 67 was not present or DHCP is not
 * yet BOUND.
 */
const char *SblDhcp_getBootfileName(void);

#ifdef __cplusplus
}
#endif

#endif /* SBL_DHCP_H_ */
