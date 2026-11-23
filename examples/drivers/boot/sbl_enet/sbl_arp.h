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
 * \file  sbl_arp.h
 *
 * \brief ARP (Address Resolution Protocol) request/reply handling for the
 *        Ethernet SBL.
 *
 * Provides a minimal ARP implementation: sending gratuitous ARP announcements,
 * resolving a target IP to its MAC address, and replying to inbound ARP
 * requests directed at the SBL's own IP.
 */

#ifndef SBL_ARP_H_
#define SBL_ARP_H_

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
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  ARP state-machine handler — called every main-loop iteration.
 *
 * Receives the full list of raw Ethernet frames captured since the last call.
 * The handler is responsible for identifying ARP frames (EtherType 0x0806),
 * replying to ARP requests directed at the SBL's IP, and updating the ARP
 * cache on receipt of ARP replies.
 *
 * Also handles timeout-driven retransmits (e.g. gratuitous ARP) because it is
 * called even when \p rxList->count is zero.
 *
 * \param  rxList  Received frame list from SblCpsw_getRxPackets(); may be empty.
 */
void SblArp_handler(const SblCpsw_RxPktList *rxList);

/*!
 * \brief  Initialise the ARP module state.
 *
 * Must be called once before any other SblArp_* function.
 */
void SblArp_init(void);

/*!
 * \brief  Request resolution of \p targetIp to its MAC address.
 *
 * Records the target and immediately sends an ARP REQUEST if DHCP is already
 * bound (so the SBL's own IP is known).  If DHCP is not yet bound the first
 * request is deferred to the first SblArp_handler() call after it completes.
 * Subsequent retransmits are driven by SblArp_handler() on timeout.
 *
 * Calling this function when \p targetIp is already cached is a no-op.
 *
 * \param  targetIp  IPv4 address to resolve (host byte order).
 */
void SblArp_resolve(uint32_t targetIp);

/*!
 * \brief  Look up a resolved MAC address in the ARP cache.
 *
 * \param  ip      IPv4 address to look up (host byte order).
 * \param  macOut  Output buffer of at least ENET_MAC_ADDR_LEN bytes;
 *                 written only on a cache hit.
 *
 * \return \c true if \p ip is in the cache (and \p macOut was written),
 *         \c false if resolution is still pending.
 */
bool SblArp_lookup(uint32_t ip, uint8_t *macOut);


#ifdef __cplusplus
}
#endif

#endif /* SBL_ARP_H_ */
