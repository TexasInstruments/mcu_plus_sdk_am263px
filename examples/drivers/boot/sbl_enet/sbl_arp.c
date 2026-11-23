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
 * \file  sbl_arp.c
 *
 * \brief Minimal ARP client/responder for the Ethernet SBL.
 *
 * Supports:
 *  - Replying to ARP requests directed at the SBL's own IP (once DHCP is BOUND).
 *  - Resolving one target IP (the TFTP server) to its MAC address via
 *    SblArp_resolve() / SblArp_lookup(), with timeout-driven retransmits in
 *    SblArp_handler().
 *
 * Protocols: Ethernet (HTYPE=1) and IPv4 (PTYPE=0x0800) only.
 * ARP table size: one entry (sufficient for TFTP server resolution).
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <kernel/dpl/ClockP.h>
#include <enet_apputils.h>
#include "sbl_ipudp.h"
#include "sbl_dhcp.h"
#include "sbl_arp.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*! Set to 1 to enable verbose ARP debug logs.
 *  Set to 0 to suppress all debug output. */
#define SBLARP_DEBUG_LOGS           (1U)

#if (SBLARP_DEBUG_LOGS == 1U)
#define SBLARP_LOG(...)             EnetAppUtils_print(__VA_ARGS__)
#else
#define SBLARP_LOG(...)             do { } while (0)
#endif

/* ---- ARP packet field offsets (relative to ARP payload, after ETH header) ----
 *
 *  [0:1]  HTYPE  Hardware type (2 bytes)
 *  [2:3]  PTYPE  Protocol type (2 bytes)
 *  [4]    HLEN   Hardware address length (1 byte)
 *  [5]    PLEN   Protocol address length (1 byte)
 *  [6:7]  OPER   Operation (2 bytes)
 *  [8:13] SHA    Sender hardware address (6 bytes)
 *  [14:17]SPA    Sender protocol address (4 bytes)
 *  [18:23]THA    Target hardware address (6 bytes)
 *  [24:27]TPA    Target protocol address (4 bytes)
 *  Total: 28 bytes
 */
#define ARP_OFF_HTYPE               (0U)
#define ARP_OFF_PTYPE               (2U)
#define ARP_OFF_HLEN                (4U)
#define ARP_OFF_PLEN                (5U)
#define ARP_OFF_OPER                (6U)
#define ARP_OFF_SHA                 (8U)
#define ARP_OFF_SPA                 (14U)
#define ARP_OFF_THA                 (18U)
#define ARP_OFF_TPA                 (24U)
#define ARP_PKT_LEN                 (28U)

#define ARP_HTYPE_ETHERNET          (1U)
#define ARP_PTYPE_IPV4              (0x0800U)
#define ARP_OPER_REQUEST            (1U)
#define ARP_OPER_REPLY              (2U)

/*! Minimum Ethernet frame length that can carry a complete ARP payload. */
#define ARP_MIN_FRAME_LEN           (SBL_ETH_HDR_LEN + ARP_PKT_LEN)

/*! Interval before retransmitting an unanswered ARP REQUEST (us). */
#define ARP_REQUEST_TIMEOUT_US      (1000000U)   /* 1 s */

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/*! Single-entry ARP cache sufficient for TFTP server resolution. */
typedef struct SblArp_CacheEntry_s
{
    uint32_t ip;                        /*!< Cached IPv4 address (host byte order) */
    uint8_t  mac[ENET_MAC_ADDR_LEN];    /*!< Resolved MAC address                 */
    bool     valid;                     /*!< true once a REPLY has been received   */
} SblArp_CacheEntry;

typedef struct SblArp_Obj_s
{
    SblArp_CacheEntry cache;      /*!< Single-entry cache for the TFTP server   */
    uint32_t          pendingIp;  /*!< IP currently being resolved              */
    bool              resolving;  /*!< true while awaiting a REPLY              */
    uint64_t          reqTimeUs;  /*!< Timestamp of last REQUEST transmission   */
} SblArp_Obj;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static void SblArp_sendRequest(uint32_t targetIp);
static void SblArp_sendReply(uint32_t tpa, const uint8_t *tha);
static void SblArp_processFrame(const uint8_t *buf, uint32_t len);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

static SblArp_Obj gSblArp;

static const uint8_t gSblArp_broadcastMac[ENET_MAC_ADDR_LEN] =
    { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void SblArp_init(void)
{
    memset(&gSblArp, 0, sizeof(gSblArp));
    SBLARP_LOG("[ARP] init\r\n");
}

void SblArp_resolve(uint32_t targetIp)
{
    /* No-op if already cached */
    if (gSblArp.cache.valid && (gSblArp.cache.ip == targetIp))
    {
        return;
    }

    gSblArp.pendingIp = targetIp;
    gSblArp.resolving = true;

    SBLARP_LOG("[ARP] resolving %d.%d.%d.%d\r\n",
               (targetIp >> 24U) & 0xFFU, (targetIp >> 16U) & 0xFFU,
               (targetIp >>  8U) & 0xFFU,  targetIp         & 0xFFU);

    if (SblDhcp_isDone())
    {
        /* DHCP is bound: send first request immediately */
        SblArp_sendRequest(targetIp);
        gSblArp.reqTimeUs = ClockP_getTimeUsec();
    }
    else
    {
        /* Defer until DHCP completes; set timestamp to 0 so the handler
         * triggers an immediate send on the first call after DHCP is bound. */
        gSblArp.reqTimeUs = 0U;
    }
}

bool SblArp_lookup(uint32_t ip, uint8_t *macOut)
{
    bool lookupStatus = false;
    if (gSblArp.cache.valid && (gSblArp.cache.ip == ip))
    {
        memcpy(macOut, gSblArp.cache.mac, ENET_MAC_ADDR_LEN);
        lookupStatus = true;
    }
    return lookupStatus;
}

void SblArp_handler(const SblCpsw_RxPktList *rxList)
{
    uint64_t nowUs;
    uint32_t i;

    /* ---- Retransmit pending REQUEST on timeout ---- */
    if (gSblArp.resolving && !gSblArp.cache.valid && SblDhcp_isDone())
    {
        nowUs = ClockP_getTimeUsec();
        if ((nowUs - gSblArp.reqTimeUs) >= ARP_REQUEST_TIMEOUT_US)
        {
            SBLARP_LOG("[ARP] REQUEST timeout, retrying %d.%d.%d.%d\r\n",
                       (gSblArp.pendingIp >> 24U) & 0xFFU,
                       (gSblArp.pendingIp >> 16U) & 0xFFU,
                       (gSblArp.pendingIp >>  8U) & 0xFFU,
                        gSblArp.pendingIp         & 0xFFU);
            SblArp_sendRequest(gSblArp.pendingIp);
            gSblArp.reqTimeUs = nowUs;
        }
    }

    /* ---- Process received frames ---- */
    for (i = 0U; i < rxList->count; i++)
    {
        SblArp_processFrame(rxList->pkts[i].buf, rxList->pkts[i].len);
    }
}

/* ========================================================================== */
/*                        Static Function Definitions                         */
/* ========================================================================== */

/*!
 * Build and transmit an ARP REQUEST for \p targetIp.
 * No-op if DHCP is not yet BOUND (own IP unknown).
 */
static void SblArp_sendRequest(uint32_t targetIp)
{
    uint8_t         pkt[ARP_PKT_LEN];
    SblDhcp_Params  params;
    int32_t         status;

    if (SblDhcp_getNetworkParams(&params) != ENET_SOK)
    {
        return; /* own IP not yet known */
    }

    memset(pkt, 0, ARP_PKT_LEN);
    SblIpUdp_put16(pkt, ARP_OFF_HTYPE, (uint16_t)ARP_HTYPE_ETHERNET);
    SblIpUdp_put16(pkt, ARP_OFF_PTYPE, (uint16_t)ARP_PTYPE_IPV4);
    pkt[ARP_OFF_HLEN] = ENET_MAC_ADDR_LEN;
    pkt[ARP_OFF_PLEN] = 4U;
    SblIpUdp_put16(pkt, ARP_OFF_OPER, (uint16_t)ARP_OPER_REQUEST);
    memcpy(&pkt[ARP_OFF_SHA], gSblCpsw.hostMacAddr, ENET_MAC_ADDR_LEN);
    SblIpUdp_put32(pkt, ARP_OFF_SPA, params.assignedIp);
    /* THA left as 0 — target MAC is unknown */
    SblIpUdp_put32(pkt, ARP_OFF_TPA, targetIp);

    status = SblCpsw_txPacket(pkt, ARP_PKT_LEN,
                               gSblArp_broadcastMac,
                               (uint16_t)SBL_ETHERTYPE_ARP);
    if (status == ENET_SOK)
    {
        SBLARP_LOG("[ARP] REQUEST sent for %d.%d.%d.%d\r\n",
                   (targetIp >> 24U) & 0xFFU, (targetIp >> 16U) & 0xFFU,
                   (targetIp >>  8U) & 0xFFU,  targetIp         & 0xFFU);
    }
    else
    {
        EnetAppUtils_print("[ARP] TX failed: %d\r\n", status);
    }
}

/*!
 * Build and transmit an ARP REPLY to a received REQUEST.
 *
 * \param  tpa  Requester's IP (becomes our TARGET protocol address).
 * \param  tha  Requester's MAC (becomes our TARGET hardware address);
 *              also used as unicast destination.
 */
static void SblArp_sendReply(uint32_t tpa, const uint8_t *tha)
{
    uint8_t         pkt[ARP_PKT_LEN];
    SblDhcp_Params  params;
    int32_t         status;

    if (SblDhcp_getNetworkParams(&params) != ENET_SOK)
    {
        return;
    }

    memset(pkt, 0, ARP_PKT_LEN);
    SblIpUdp_put16(pkt, ARP_OFF_HTYPE, (uint16_t)ARP_HTYPE_ETHERNET);
    SblIpUdp_put16(pkt, ARP_OFF_PTYPE, (uint16_t)ARP_PTYPE_IPV4);
    pkt[ARP_OFF_HLEN] = ENET_MAC_ADDR_LEN;
    pkt[ARP_OFF_PLEN] = 4U;
    SblIpUdp_put16(pkt, ARP_OFF_OPER, (uint16_t)ARP_OPER_REPLY);
    memcpy(&pkt[ARP_OFF_SHA], gSblCpsw.hostMacAddr, ENET_MAC_ADDR_LEN);
    SblIpUdp_put32(pkt, ARP_OFF_SPA, params.assignedIp);
    memcpy(&pkt[ARP_OFF_THA], tha, ENET_MAC_ADDR_LEN);
    SblIpUdp_put32(pkt, ARP_OFF_TPA, tpa);

    /* Send unicast directly back to the requester */
    status = SblCpsw_txPacket(pkt, ARP_PKT_LEN,
                               tha,
                               (uint16_t)SBL_ETHERTYPE_ARP);
    if (status == ENET_SOK)
    {
        SBLARP_LOG("[ARP] REPLY sent to %d.%d.%d.%d\r\n",
                   (tpa >> 24U) & 0xFFU, (tpa >> 16U) & 0xFFU,
                   (tpa >>  8U) & 0xFFU,  tpa         & 0xFFU);
    }
    else
    {
        EnetAppUtils_print("[ARP] TX failed: %d\r\n", status);
    }
}

/*!
 * Parse a single raw Ethernet frame and handle any ARP packet it contains.
 */
static void SblArp_processFrame(const uint8_t *buf, uint32_t len)
{
    const uint8_t  *arp;
    uint16_t        htype;
    uint16_t        ptype;
    uint16_t        oper;
    uint32_t        spa;
    uint32_t        tpa;
    const uint8_t  *sha;
    SblDhcp_Params  params;

    /* ---- Ethernet header validation ---- */
    if (len < ARP_MIN_FRAME_LEN) { return; }

    if (SblIpUdp_get16(buf, SBL_ETH_ETHERTYPE_OFFSET) != SBL_ETHERTYPE_ARP)
    {
        return;
    }

    /* ---- ARP header validation: Ethernet/IPv4 only ---- */
    arp   = buf + SBL_ETH_HDR_LEN;
    htype = SblIpUdp_get16(arp, ARP_OFF_HTYPE);
    ptype = SblIpUdp_get16(arp, ARP_OFF_PTYPE);

    if ((htype != (uint16_t)ARP_HTYPE_ETHERNET) ||
        (ptype != (uint16_t)ARP_PTYPE_IPV4))
    {
        SBLARP_LOG("[ARP] dropping: unsupported HTYPE=%u PTYPE=0x%04X\r\n",
                   (uint32_t)htype, (uint32_t)ptype);
        return;
    }

    oper = SblIpUdp_get16(arp, ARP_OFF_OPER);
    sha  = arp + ARP_OFF_SHA;
    spa  = SblIpUdp_get32(arp, ARP_OFF_SPA);
    tpa  = SblIpUdp_get32(arp, ARP_OFF_TPA);

    switch (oper)
    {
        case ARP_OPER_REQUEST:
        {
            /* Reply only if we know our own IP and the request targets us */
            if (SblDhcp_getNetworkParams(&params) != ENET_SOK) { break; }

            if (tpa != params.assignedIp) { break; }

            SBLARP_LOG("[ARP] REQUEST from %d.%d.%d.%d "
                       "(%02X:%02X:%02X:%02X:%02X:%02X) for our IP\r\n",
                       (spa >> 24U) & 0xFFU, (spa >> 16U) & 0xFFU,
                       (spa >>  8U) & 0xFFU,  spa         & 0xFFU,
                       sha[0], sha[1], sha[2], sha[3], sha[4], sha[5]);

            SblArp_sendReply(spa, sha);
            break;
        }

        case ARP_OPER_REPLY:
        {
            /* Accept only if this answers our outstanding request */
            if (!gSblArp.resolving || (spa != gSblArp.pendingIp)) { break; }

            memcpy(gSblArp.cache.mac, sha, ENET_MAC_ADDR_LEN);
            gSblArp.cache.ip    = spa;
            gSblArp.cache.valid = true;
            gSblArp.resolving   = false;

            SBLARP_LOG("[ARP] REPLY: %d.%d.%d.%d → "
                       "%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                       (spa >> 24U) & 0xFFU, (spa >> 16U) & 0xFFU,
                       (spa >>  8U) & 0xFFU,  spa         & 0xFFU,
                       sha[0], sha[1], sha[2], sha[3], sha[4], sha[5]);
            break;
        }

        default:
            SBLARP_LOG("[ARP] unexpected opcode %u, ignoring\r\n",
                       (uint32_t)oper);
            break;
    }
}
