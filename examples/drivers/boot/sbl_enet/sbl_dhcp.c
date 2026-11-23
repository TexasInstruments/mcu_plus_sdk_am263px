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
 * \file  sbl_dhcp.c
 *
 * \brief DHCP client implementing the DORA exchange for the Ethernet SBL.
 *
 * State machine: INIT → SELECTING → REQUESTING → BOUND
 *
 * Packet flow (all broadcast, src-IP 0.0.0.0 until BOUND):
 *   TX DHCPDISCOVER  →  RX DHCPOFFER  →  TX DHCPREQUEST  →  RX DHCPACK
 *
 * Uses SblCpsw_txPacket for transmission (builds IP/UDP payload internally).
 * Receives via SblDhcp_handler which is called every main-loop iteration
 * regardless of whether packets arrived, enabling timeout-based retransmit.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stdio.h>
#include <kernel/dpl/ClockP.h>
#include <enet_apputils.h>
#include "sbl_ipudp.h"
#include "sbl_dhcp.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* ETH/IP/UDP constants moved to sbl_ipudp.h; use SBL_* names from there.
 * Local aliases kept only for ports and TTL which are DHCP-specific. */
/* #define DHCP_ETH_HDR_LEN  ... */  /* → SBL_ETH_HDR_LEN  */
/* #define DHCP_ETHERTYPE_IPV4 ... */ /* → SBL_ETHERTYPE_IPV4 */
/* #define DHCP_IP_HDR_MIN_LEN ... */ /* → SBL_IP_HDR_LEN    */
/* #define DHCP_IP_PROTO_UDP ...   */ /* → SBL_IP_PROTO_UDP  */
/* #define DHCP_UDP_HDR_LEN  ...   */ /* → SBL_UDP_HDR_LEN   */
/* #define DHCP_TTL          ...   */ /* → SBL_IP_TTL_DEFAULT */
#define DHCP_SERVER_PORT            (67U)
#define DHCP_CLIENT_PORT            (68U)

/* ---- DHCP fixed header field offsets (within DHCP payload) ---- */
#define DHCP_FIELD_OP               (0U)
#define DHCP_FIELD_HTYPE            (1U)
#define DHCP_FIELD_HLEN             (2U)
#define DHCP_FIELD_HOPS             (3U)
#define DHCP_FIELD_XID              (4U)    /* 4 bytes, big-endian              */
#define DHCP_FIELD_SECS             (8U)    /* 2 bytes                          */
#define DHCP_FIELD_FLAGS            (10U)   /* 2 bytes                          */
#define DHCP_FIELD_CIADDR           (12U)   /* 4 bytes                          */
#define DHCP_FIELD_YIADDR           (16U)   /* 4 bytes — offered/assigned IP    */
#define DHCP_FIELD_SIADDR           (20U)   /* 4 bytes                          */
#define DHCP_FIELD_GIADDR           (24U)   /* 4 bytes                          */
#define DHCP_FIELD_CHADDR           (28U)   /* 16 bytes (6-byte MAC + padding)  */
#define DHCP_FIXED_HDR_LEN          (236U)  /* op..file                         */
#define DHCP_MAGIC_COOKIE_LEN       (4U)
#define DHCP_FIELD_OPTIONS          (240U)  /* after fixed (236) + cookie (4)   */

/* ---- DHCP protocol constants ---- */
#define DHCP_MAGIC_COOKIE           (0x63825363UL)
#define DHCP_BOOT_REQUEST           (1U)
#define DHCP_BOOT_REPLY             (2U)
#define DHCP_HW_TYPE_ETHERNET       (1U)
#define DHCP_FLAG_BROADCAST         (0x8000U)

/* ---- DHCP message types (option 53) ---- */
#define DHCP_MSG_DISCOVER           (1U)
#define DHCP_MSG_OFFER              (2U)
#define DHCP_MSG_REQUEST            (3U)
#define DHCP_MSG_ACK                (5U)
#define DHCP_MSG_NAK                (6U)

/* ---- DHCP option codes ---- */
#define DHCP_OPT_PAD                (0U)
#define DHCP_OPT_SUBNET_MASK        (1U)
#define DHCP_OPT_ROUTER             (3U)
#define DHCP_OPT_REQUESTED_IP       (50U)
#define DHCP_OPT_MSG_TYPE           (53U)
#define DHCP_OPT_SERVER_ID          (54U)
#define DHCP_OPT_PARAM_REQ_LIST     (55U)
#define DHCP_OPT_TFTP_SERVER        (66U)  /*!< TFTP server name (string, may be dotted-decimal IP) */
#define DHCP_OPT_BOOTFILE           (67U)  /*!< Boot file name (null-terminated string)             */
#define DHCP_OPT_END                (255U)

/*! Storage limit for the TFTP server name string from option 66. */
#define SBLDHCP_TFTP_SERVER_STR_MAX (64U)

/* ---- Retransmit timeouts ---- */
/*! Interval before retransmitting a DHCPDISCOVER (us) */
#define DHCP_DISCOVER_TIMEOUT_US    (3000000U)  /* 3 s */
/*! Interval before retransmitting a DHCPREQUEST (us) */
#define DHCP_REQUEST_TIMEOUT_US     (3000000U)  /* 3 s */

/*! Size of the static TX buffer — large enough for DISCOVER and REQUEST */
#define DHCP_TX_BUF_LEN             (300U)

/*! Set to 1 to enable verbose DHCP state-machine debug logs.
 *  Set to 0 to suppress all debug output (saves code / UART bandwidth). */
#define SBLDHCP_DEBUG_LOGS          (1U)

#if (SBLDHCP_DEBUG_LOGS == 1U)
#define SBLDHCP_LOG(...)            EnetAppUtils_print(__VA_ARGS__)
#else
#define SBLDHCP_LOG(...)            do { } while (0)
#endif

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

typedef enum SblDhcp_State_e
{
    SBLDHCP_STATE_INIT       = 0, /*!< Not started                           */
    SBLDHCP_STATE_SELECTING  = 1, /*!< DISCOVER sent, waiting for OFFER      */
    SBLDHCP_STATE_REQUESTING = 2, /*!< REQUEST sent, waiting for ACK         */
    SBLDHCP_STATE_BOUND      = 3, /*!< ACK received; IP assigned             */
    SBLDHCP_STATE_ERROR      = 4, /*!< Unrecoverable error                   */
} SblDhcp_State;

typedef struct SblDhcp_Obj_s
{
    SblDhcp_State state;
    uint8_t       macAddr[ENET_MAC_ADDR_LEN];
    uint32_t      xid;             /*!< Transaction ID, derived from MAC     */
    uint32_t      offeredIp;       /*!< yiaddr from OFFER                    */
    uint32_t      serverIp;        /*!< option 54 from OFFER/ACK             */
    uint32_t      assignedIp;      /*!< yiaddr from ACK                      */
    uint32_t      subnetMask;      /*!< option 1  from ACK                   */
    uint32_t      gatewayIp;       /*!< option 3  from ACK                   */
    uint32_t      tftpServerIp;    /*!< option 66 — 0 if absent/unparseable  */
    char          bootfileName[SBLDHCP_BOOTFILE_MAX_LEN]; /*!< option 67 — "" if absent */
    uint64_t      stateEntryTimeUs;/*!< Timestamp of last state transition   */
} SblDhcp_Obj;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static void     SblDhcp_buildDiscover(uint8_t *pkt, uint32_t *pktLen);
static void     SblDhcp_buildRequest(uint8_t *pkt, uint32_t *pktLen);
static void     SblDhcp_send(uint8_t *pkt, uint32_t pktLen);
static bool     SblDhcp_processFrame(const uint8_t *buf, uint32_t len);
static uint8_t  SblDhcp_parseMsgType(const uint8_t *opts, uint32_t optsLen);
static void     SblDhcp_parseOptions(const uint8_t *opts, uint32_t optsLen);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

static SblDhcp_Obj gSblDhcp;
static uint8_t     gSblDhcp_txBuf[DHCP_TX_BUF_LEN];

static const uint8_t gSblDhcp_broadcastMac[ENET_MAC_ADDR_LEN] =
    { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU };

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void SblDhcp_init(const uint8_t *macAddr)
{
    memset(&gSblDhcp, 0, sizeof(gSblDhcp));
    memcpy(gSblDhcp.macAddr, macAddr, ENET_MAC_ADDR_LEN);

    /* Derive XID from MAC bytes 2-5 for a session-unique transaction ID */
    gSblDhcp.xid = ((uint32_t)macAddr[2] << 24U) |
                   ((uint32_t)macAddr[3] << 16U) |
                   ((uint32_t)macAddr[4] <<  8U) |
                    (uint32_t)macAddr[5];

    gSblDhcp.state = SBLDHCP_STATE_INIT;

    SBLDHCP_LOG("[DHCP] init: MAC=%02X:%02X:%02X:%02X:%02X:%02X  XID=0x%08X\r\n",
                macAddr[0], macAddr[1], macAddr[2],
                macAddr[3], macAddr[4], macAddr[5],
                gSblDhcp.xid);
}

void SblDhcp_handler(const SblCpsw_RxPktList *rxList)
{
    uint64_t nowUs;
    uint32_t i;
    uint32_t pktLen;

    /* State machine is terminal once BOUND or ERROR */
    if ((gSblDhcp.state == SBLDHCP_STATE_BOUND) ||
        (gSblDhcp.state == SBLDHCP_STATE_ERROR))
    {
        return;
    }

    nowUs = ClockP_getTimeUsec();

    switch (gSblDhcp.state)
    {
        case SBLDHCP_STATE_INIT:
        {
            pktLen = 0U;
            SblDhcp_buildDiscover(gSblDhcp_txBuf, &pktLen);
            SblDhcp_send(gSblDhcp_txBuf, pktLen);
            gSblDhcp.state            = SBLDHCP_STATE_SELECTING;
            gSblDhcp.stateEntryTimeUs = nowUs;
            SBLDHCP_LOG("[DHCP] DISCOVER sent (XID=0x%08X)\r\n", gSblDhcp.xid);
            break;
        }

        case SBLDHCP_STATE_SELECTING:
        {
            /* Scan for OFFER */
            for (i = 0U; i < rxList->count; i++)
            {
                if (SblDhcp_processFrame(rxList->pkts[i].buf, rxList->pkts[i].len))
                {
                    break; /* state advanced inside processFrame */
                }
            }

            /* Retransmit DISCOVER on timeout */
            if ((gSblDhcp.state == SBLDHCP_STATE_SELECTING) &&
                ((nowUs - gSblDhcp.stateEntryTimeUs) >= DHCP_DISCOVER_TIMEOUT_US))
            {
                SBLDHCP_LOG("[DHCP] DISCOVER timeout, retrying\r\n");
                pktLen = 0U;
                SblDhcp_buildDiscover(gSblDhcp_txBuf, &pktLen);
                SblDhcp_send(gSblDhcp_txBuf, pktLen);
                gSblDhcp.stateEntryTimeUs = nowUs;
            }
            break;
        }

        case SBLDHCP_STATE_REQUESTING:
        {
            /* Scan for ACK or NAK */
            for (i = 0U; i < rxList->count; i++)
            {
                if (SblDhcp_processFrame(rxList->pkts[i].buf, rxList->pkts[i].len))
                {
                    break;
                }
            }

            /* Retransmit REQUEST on timeout */
            if ((gSblDhcp.state == SBLDHCP_STATE_REQUESTING) &&
                ((nowUs - gSblDhcp.stateEntryTimeUs) >= DHCP_REQUEST_TIMEOUT_US))
            {
                SBLDHCP_LOG("[DHCP] REQUEST timeout, retrying\r\n");
                pktLen = 0U;
                SblDhcp_buildRequest(gSblDhcp_txBuf, &pktLen);
                SblDhcp_send(gSblDhcp_txBuf, pktLen);
                gSblDhcp.stateEntryTimeUs = nowUs;
            }
            break;
        }

        default:
            break;
    }
}

bool SblDhcp_isDone(void)
{
    return (gSblDhcp.state == SBLDHCP_STATE_BOUND);
}

int32_t SblDhcp_getNetworkParams(SblDhcp_Params *params)
{
    int32_t status = ENET_EFAIL;
    if (gSblDhcp.state == SBLDHCP_STATE_BOUND)
    {
        params->assignedIp   = gSblDhcp.assignedIp;
        params->subnetMask   = gSblDhcp.subnetMask;
        params->gatewayIp    = gSblDhcp.gatewayIp;
        params->tftpServerIp = gSblDhcp.tftpServerIp;
        strncpy(params->bootfileName, gSblDhcp.bootfileName,
                SBLDHCP_BOOTFILE_MAX_LEN - 1U);
        params->bootfileName[SBLDHCP_BOOTFILE_MAX_LEN - 1U] = '\0';
        status = ENET_SOK;
    }
    return status;
}

uint32_t SblDhcp_getTftpServerIp(void)
{
    return gSblDhcp.tftpServerIp;
}

const char *SblDhcp_getBootfileName(void)
{
    return gSblDhcp.bootfileName;
}


/* ========================================================================== */
/*                        Static Function Definitions                         */
/* ========================================================================== */

static void SblDhcp_fillDhcpFixedHdr(uint8_t *dhcp)
{
    dhcp[DHCP_FIELD_OP]    = DHCP_BOOT_REQUEST;
    dhcp[DHCP_FIELD_HTYPE] = DHCP_HW_TYPE_ETHERNET;
    dhcp[DHCP_FIELD_HLEN]  = ENET_MAC_ADDR_LEN;
    dhcp[DHCP_FIELD_HOPS]  = 0U;
    SblIpUdp_put32(dhcp, DHCP_FIELD_XID,   gSblDhcp.xid);
    SblIpUdp_put16(dhcp, DHCP_FIELD_SECS,  0U);
    SblIpUdp_put16(dhcp, DHCP_FIELD_FLAGS, DHCP_FLAG_BROADCAST);
    /* ciaddr, yiaddr, siaddr, giaddr remain 0 */
    memcpy(&dhcp[DHCP_FIELD_CHADDR], gSblDhcp.macAddr, ENET_MAC_ADDR_LEN);
    /* sname and file (bytes 44-235) remain 0 */

    /* Magic cookie */
    SblIpUdp_put32(dhcp, DHCP_FIXED_HDR_LEN, DHCP_MAGIC_COOKIE);
}

static void SblDhcp_buildDiscover(uint8_t *pkt, uint32_t *pktLen)
{
    uint8_t *dhcp       = pkt + SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN;
    uint8_t *opt        = dhcp + DHCP_FIELD_OPTIONS;
    uint32_t optLen;
    uint32_t dhcpPayLen;

    memset(pkt, 0, DHCP_TX_BUF_LEN);
    SblDhcp_fillDhcpFixedHdr(dhcp);

    /* Options */
    *opt++ = DHCP_OPT_MSG_TYPE;       *opt++ = 1U; *opt++ = DHCP_MSG_DISCOVER;
    *opt++ = DHCP_OPT_PARAM_REQ_LIST; *opt++ = 4U;  /* request 4 options */
    *opt++ = DHCP_OPT_SUBNET_MASK;
    *opt++ = DHCP_OPT_ROUTER;
    *opt++ = DHCP_OPT_TFTP_SERVER;    /* option 66: TFTP server IP/name */
    *opt++ = DHCP_OPT_BOOTFILE;       /* option 67: boot file name      */
    *opt++ = DHCP_OPT_END;

    optLen     = (uint32_t)(opt - (dhcp + DHCP_FIELD_OPTIONS));
    dhcpPayLen = DHCP_FIXED_HDR_LEN + DHCP_MAGIC_COOKIE_LEN + optLen;
    *pktLen    = SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + dhcpPayLen;

    SblIpUdp_fillTxHeaders(pkt,
                            SBL_IP_ADDR_NONE,
                            SBL_IP_ADDR_BROADCAST,
                            (uint16_t)DHCP_CLIENT_PORT,
                            (uint16_t)DHCP_SERVER_PORT,
                            dhcpPayLen);
}

static void SblDhcp_buildRequest(uint8_t *pkt, uint32_t *pktLen)
{
    uint8_t *dhcp       = pkt + SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN;
    uint8_t *opt        = dhcp + DHCP_FIELD_OPTIONS;
    uint32_t optLen;
    uint32_t dhcpPayLen;

    memset(pkt, 0, DHCP_TX_BUF_LEN);
    SblDhcp_fillDhcpFixedHdr(dhcp);

    /* Options */
    *opt++ = DHCP_OPT_MSG_TYPE;    *opt++ = 1U; *opt++ = DHCP_MSG_REQUEST;
    *opt++ = DHCP_OPT_REQUESTED_IP; *opt++ = 4U;
    *opt++ = (uint8_t)(gSblDhcp.offeredIp >> 24U);
    *opt++ = (uint8_t)(gSblDhcp.offeredIp >> 16U);
    *opt++ = (uint8_t)(gSblDhcp.offeredIp >>  8U);
    *opt++ = (uint8_t)(gSblDhcp.offeredIp);
    *opt++ = DHCP_OPT_SERVER_ID;   *opt++ = 4U;
    *opt++ = (uint8_t)(gSblDhcp.serverIp >> 24U);
    *opt++ = (uint8_t)(gSblDhcp.serverIp >> 16U);
    *opt++ = (uint8_t)(gSblDhcp.serverIp >>  8U);
    *opt++ = (uint8_t)(gSblDhcp.serverIp);
    *opt++ = DHCP_OPT_END;

    optLen     = (uint32_t)(opt - (dhcp + DHCP_FIELD_OPTIONS));
    dhcpPayLen = DHCP_FIXED_HDR_LEN + DHCP_MAGIC_COOKIE_LEN + optLen;
    *pktLen    = SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + dhcpPayLen;

    SblIpUdp_fillTxHeaders(pkt,
                            SBL_IP_ADDR_NONE,
                            SBL_IP_ADDR_BROADCAST,
                            (uint16_t)DHCP_CLIENT_PORT,
                            (uint16_t)DHCP_SERVER_PORT,
                            dhcpPayLen);
}

static void SblDhcp_send(uint8_t *pkt, uint32_t pktLen)
{
    int32_t status;
    status = SblCpsw_txPacket(pkt, pktLen, gSblDhcp_broadcastMac,
                               (uint16_t)SBL_ETHERTYPE_IPV4);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[DHCP] TX failed: %d\r\n", status);
    }
}

static uint8_t SblDhcp_parseMsgType(const uint8_t *opts, uint32_t optsLen)
{
    uint32_t i   = 0U;
    uint8_t  len;
    uint8_t retVal = 0;

    while (i < optsLen)
    {
        if (opts[i] == DHCP_OPT_END) { break; }
        if (opts[i] == DHCP_OPT_PAD) { i++; continue; }
        if ((i + 1U) >= optsLen)     { break; }

        len = opts[i + 1U];
        if ((opts[i] == DHCP_OPT_MSG_TYPE) && (len >= 1U) && ((i + 2U) < optsLen))
        {
            retVal = opts[i + 2U];
            break;
        }
        i += 2U + (uint32_t)len;
    }
    return retVal;
}

static void SblDhcp_parseOptions(const uint8_t *opts, uint32_t optsLen)
{
    uint32_t i   = 0U;
    uint8_t  len;

    while (i < optsLen)
    {
        if (opts[i] == DHCP_OPT_END) { break; }
        if (opts[i] == DHCP_OPT_PAD) { i++; continue; }
        if ((i + 1U) >= optsLen)     { break; }

        len = opts[i + 1U];
        if ((i + 2U + (uint32_t)len) > optsLen) { break; } /* truncated option */

        switch (opts[i])
        {
            case DHCP_OPT_SUBNET_MASK:
                if (len >= 4U)
                {
                    gSblDhcp.subnetMask = SblIpUdp_get32(opts, i + 2U);
                    SBLDHCP_LOG("[DHCP] opt subnet=%d.%d.%d.%d\r\n",
                                (gSblDhcp.subnetMask >> 24U) & 0xFFU,
                                (gSblDhcp.subnetMask >> 16U) & 0xFFU,
                                (gSblDhcp.subnetMask >>  8U) & 0xFFU,
                                 gSblDhcp.subnetMask         & 0xFFU);
                }
                break;
            case DHCP_OPT_ROUTER:
                if (len >= 4U)
                {
                    gSblDhcp.gatewayIp = SblIpUdp_get32(opts, i + 2U);
                    SBLDHCP_LOG("[DHCP] opt gateway=%d.%d.%d.%d\r\n",
                                (gSblDhcp.gatewayIp >> 24U) & 0xFFU,
                                (gSblDhcp.gatewayIp >> 16U) & 0xFFU,
                                (gSblDhcp.gatewayIp >>  8U) & 0xFFU,
                                 gSblDhcp.gatewayIp         & 0xFFU);
                }
                break;
            case DHCP_OPT_SERVER_ID:
                if (len >= 4U)
                {
                    gSblDhcp.serverIp = SblIpUdp_get32(opts, i + 2U);
                    SBLDHCP_LOG("[DHCP] opt serverID=%d.%d.%d.%d\r\n",
                                (gSblDhcp.serverIp >> 24U) & 0xFFU,
                                (gSblDhcp.serverIp >> 16U) & 0xFFU,
                                (gSblDhcp.serverIp >>  8U) & 0xFFU,
                                 gSblDhcp.serverIp         & 0xFFU);
                }
                break;
            case DHCP_OPT_TFTP_SERVER:
            {
                /* Option 66: TFTP server name — a dotted-decimal string
                 * (e.g. "192.168.1.1") or a raw 4-byte big-endian address. */
                if (len == 4U)
                {
                    /* Raw 4-byte IP address */
                    gSblDhcp.tftpServerIp = SblIpUdp_get32(opts, i + 2U);
                }
                else if (len > 0U)
                {
                    /* Null-terminate and try to parse as dotted-decimal */
                    char tmp[SBLDHCP_TFTP_SERVER_STR_MAX];
                    uint32_t copyLen = (len < (SBLDHCP_TFTP_SERVER_STR_MAX - 1U)) ?
                                        len : (SBLDHCP_TFTP_SERVER_STR_MAX - 1U);
                    unsigned int a = 0U, b = 0U, c = 0U, d = 0U;

                    memcpy(tmp, &opts[i + 2U], copyLen);
                    tmp[copyLen] = '\0';

                    if (sscanf(tmp, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
                    {
                        gSblDhcp.tftpServerIp = ((uint32_t)a << 24U) |
                                                 ((uint32_t)b << 16U) |
                                                 ((uint32_t)c <<  8U) |
                                                  (uint32_t)d;
                    }
                }
                SBLDHCP_LOG("[DHCP] opt tftpServer=%d.%d.%d.%d\r\n",
                            (gSblDhcp.tftpServerIp >> 24U) & 0xFFU,
                            (gSblDhcp.tftpServerIp >> 16U) & 0xFFU,
                            (gSblDhcp.tftpServerIp >>  8U) & 0xFFU,
                             gSblDhcp.tftpServerIp         & 0xFFU);
                break;
            }
            case DHCP_OPT_BOOTFILE:
            {
                /* Option 67: boot file name — null-terminated string */
                if (len > 0U)
                {
                    uint32_t copyLen = (len < (SBLDHCP_BOOTFILE_MAX_LEN - 1U)) ?
                                        len : (SBLDHCP_BOOTFILE_MAX_LEN - 1U);
                    memcpy(gSblDhcp.bootfileName, &opts[i + 2U], copyLen);
                    gSblDhcp.bootfileName[copyLen] = '\0';
                    SBLDHCP_LOG("[DHCP] opt bootfile='%s'\r\n", gSblDhcp.bootfileName);
                }
                break;
            }
            default:
                break;
        }
        i += 2U + (uint32_t)len;
    }
}

static bool SblDhcp_processFrame(const uint8_t *buf, uint32_t len)
{
    SblIpUdp_RxInfo rxInfo;
    uint32_t        optsLen;
    uint32_t        rcvXid;
    uint32_t        cookie;
    uint8_t         msgType;
    const uint8_t  *dhcp;
    const uint8_t  *opts;
    uint32_t        pktLen;
    bool retVal = false;

    /* ---- Validate and extract IPv4/UDP headers ---- */
    if (!SblIpUdp_parseRxFrame(buf, len, &rxInfo))
    { 
        return false; 
    }
    
    if (rxInfo.dstPort != (uint16_t)DHCP_CLIENT_PORT) 
    { 
        return false; 
    }

    /* rxInfo.payload points to the DHCP payload (immediately after UDP hdr) */
    dhcp = rxInfo.payload;
    if (rxInfo.payloadLen < (DHCP_FIXED_HDR_LEN + DHCP_MAGIC_COOKIE_LEN + 1U))
    {
        return false;
    }

    /* ---- DHCP fixed header ---- */
    if (dhcp[DHCP_FIELD_OP] != DHCP_BOOT_REPLY) 
    { 
        return false; 
    }

    rcvXid = SblIpUdp_get32(dhcp, DHCP_FIELD_XID);
    if (rcvXid != gSblDhcp.xid)
    {
        SBLDHCP_LOG("[DHCP] XID mismatch: got 0x%08X expected 0x%08X\r\n",
                    rcvXid, gSblDhcp.xid);
        return false;
    }

    cookie = SblIpUdp_get32(dhcp, DHCP_FIXED_HDR_LEN);
    if (cookie != DHCP_MAGIC_COOKIE)
    { 
        return false; 
    }

    opts    = dhcp + DHCP_FIELD_OPTIONS;
    optsLen = rxInfo.payloadLen - DHCP_FIELD_OPTIONS;
    msgType = SblDhcp_parseMsgType(opts, optsLen);

    switch (msgType)
    {
        case DHCP_MSG_OFFER:
        {
            if (gSblDhcp.state != SBLDHCP_STATE_SELECTING) { break; }

            gSblDhcp.offeredIp = SblIpUdp_get32(dhcp, DHCP_FIELD_YIADDR);
            SblDhcp_parseOptions(opts, optsLen); /* captures serverIp */

            SBLDHCP_LOG("[DHCP] OFFER: IP=%d.%d.%d.%d  server=%d.%d.%d.%d\r\n",
                        (gSblDhcp.offeredIp >> 24U) & 0xFFU,
                        (gSblDhcp.offeredIp >> 16U) & 0xFFU,
                        (gSblDhcp.offeredIp >>  8U) & 0xFFU,
                         gSblDhcp.offeredIp         & 0xFFU,
                        (gSblDhcp.serverIp  >> 24U) & 0xFFU,
                        (gSblDhcp.serverIp  >> 16U) & 0xFFU,
                        (gSblDhcp.serverIp  >>  8U) & 0xFFU,
                         gSblDhcp.serverIp          & 0xFFU);

            pktLen = 0U;
            SblDhcp_buildRequest(gSblDhcp_txBuf, &pktLen);
            SblDhcp_send(gSblDhcp_txBuf, pktLen);

            gSblDhcp.state            = SBLDHCP_STATE_REQUESTING;
            gSblDhcp.stateEntryTimeUs = ClockP_getTimeUsec();
            SBLDHCP_LOG("[DHCP] REQUEST sent\r\n");
            retVal = true;
            break;
        }

        case DHCP_MSG_ACK:
        {
            if (gSblDhcp.state != SBLDHCP_STATE_REQUESTING) { break; }

            gSblDhcp.assignedIp = SblIpUdp_get32(dhcp, DHCP_FIELD_YIADDR);
            SblDhcp_parseOptions(opts, optsLen); /* captures subnet, gateway */

            gSblDhcp.state = SBLDHCP_STATE_BOUND;
            SBLDHCP_LOG("[DHCP] BOUND: IP=%d.%d.%d.%d  mask=%d.%d.%d.%d  GW=%d.%d.%d.%d\r\n",
                        (gSblDhcp.assignedIp >> 24U) & 0xFFU,
                        (gSblDhcp.assignedIp >> 16U) & 0xFFU,
                        (gSblDhcp.assignedIp >>  8U) & 0xFFU,
                         gSblDhcp.assignedIp         & 0xFFU,
                        (gSblDhcp.subnetMask >> 24U) & 0xFFU,
                        (gSblDhcp.subnetMask >> 16U) & 0xFFU,
                        (gSblDhcp.subnetMask >>  8U) & 0xFFU,
                         gSblDhcp.subnetMask         & 0xFFU,
                        (gSblDhcp.gatewayIp  >> 24U) & 0xFFU,
                        (gSblDhcp.gatewayIp  >> 16U) & 0xFFU,
                        (gSblDhcp.gatewayIp  >>  8U) & 0xFFU,
                         gSblDhcp.gatewayIp          & 0xFFU);
            retVal = true;
            break;
        }

        case DHCP_MSG_NAK:
        {
            SBLDHCP_LOG("[DHCP] NAK received, restarting DISCOVER\r\n");
            gSblDhcp.offeredIp = 0U;
            gSblDhcp.serverIp  = 0U;
            gSblDhcp.state     = SBLDHCP_STATE_INIT;
            retVal = true;
            break;
        }

        default:
            SBLDHCP_LOG("[DHCP] unexpected message type %u, ignoring\r\n",
                        (uint32_t)msgType);
            break;
    }
    return retVal;
}
