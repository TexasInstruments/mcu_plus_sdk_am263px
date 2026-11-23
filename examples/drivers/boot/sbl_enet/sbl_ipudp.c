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
 * \file  sbl_ipudp.c
 *
 * \brief Ethernet / IPv4 / UDP packet utilities for the Ethernet SBL.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include "sbl_ipudp.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* Byte offsets within the IPv4 header (relative to the first byte of the
 * IPv4 header, i.e. frame[SBL_ETH_HDR_LEN]). */
#define IP_OFF_VERSION_IHL      (0U)
#define IP_OFF_TOTAL_LEN        (2U)
#define IP_OFF_IDENTIFICATION   (4U)
#define IP_OFF_FLAGS_FRAG       (6U)
#define IP_OFF_TTL              (8U)
#define IP_OFF_PROTOCOL         (9U)
#define IP_OFF_CHECKSUM         (10U)
#define IP_OFF_SRC              (12U)
#define IP_OFF_DST              (16U)

/* Byte offsets within the UDP header (relative to the first byte of the
 * UDP header, i.e. frame[SBL_ETH_HDR_LEN + ihl]). */
#define UDP_OFF_SRC_PORT        (0U)
#define UDP_OFF_DST_PORT        (2U)
#define UDP_OFF_LENGTH          (4U)
#define UDP_OFF_CHECKSUM        (6U)

/* Fixed identification field used in outgoing IPv4 headers.
 * No fragmentation is performed so the value is arbitrary. */
#define IP_IDENTIFICATION       (0x0001U)

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

uint16_t SblIpUdp_calcIpChecksum(const uint8_t *hdr, uint32_t hdrLen)
{
    uint32_t sum = 0U;
    uint32_t i;

    for (i = 0U; (i + 1U) < hdrLen; i += 2U)
    {
        sum += (uint32_t)SblIpUdp_get16(hdr, i);
    }
    /* If hdrLen is odd, pad the last byte with a zero byte */
    if ((hdrLen & 1U) != 0U)
    {
        sum += (uint32_t)hdr[hdrLen - 1U] << 8U;
    }
    /* Fold 32-bit accumulator into 16 bits */
    while ((sum >> 16U) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return (uint16_t)(~sum);
}

void SblIpUdp_fillTxHeaders(uint8_t  *pkt,
                              uint32_t  srcIp,
                              uint32_t  dstIp,
                              uint16_t  srcPort,
                              uint16_t  dstPort,
                              uint32_t  udpPayloadLen)
{
    uint16_t ipTotalLen = (uint16_t)(SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + udpPayloadLen);
    uint16_t udpLen     = (uint16_t)(SBL_UDP_HDR_LEN + udpPayloadLen);
    uint16_t checksum;

    /* ---- IPv4 header (20 bytes, no options) ---- */
    pkt[IP_OFF_VERSION_IHL] = 0x45U;                        /* version=4, IHL=5 */
    pkt[1]                  = 0x00U;                        /* DSCP/ECN         */
    SblIpUdp_put16(pkt, IP_OFF_TOTAL_LEN,    ipTotalLen);
    SblIpUdp_put16(pkt, IP_OFF_IDENTIFICATION, IP_IDENTIFICATION);
    SblIpUdp_put16(pkt, IP_OFF_FLAGS_FRAG,   0x0000U);     /* DF=0, MF=0, offset=0 */
    pkt[IP_OFF_TTL]      = SBL_IP_TTL_DEFAULT;
    pkt[IP_OFF_PROTOCOL] = SBL_IP_PROTO_UDP;
    SblIpUdp_put16(pkt, IP_OFF_CHECKSUM, 0x0000U);         /* placeholder      */
    SblIpUdp_put32(pkt, IP_OFF_SRC, srcIp);
    SblIpUdp_put32(pkt, IP_OFF_DST, dstIp);

    /* Fill in the IPv4 header checksum */
    checksum = SblIpUdp_calcIpChecksum(pkt, SBL_IP_HDR_LEN);
    SblIpUdp_put16(pkt, IP_OFF_CHECKSUM, checksum);

    /* ---- UDP header (8 bytes) immediately after IP header ---- */
    SblIpUdp_put16(pkt, SBL_IP_HDR_LEN + UDP_OFF_SRC_PORT, srcPort);
    SblIpUdp_put16(pkt, SBL_IP_HDR_LEN + UDP_OFF_DST_PORT, dstPort);
    SblIpUdp_put16(pkt, SBL_IP_HDR_LEN + UDP_OFF_LENGTH,   udpLen);
    SblIpUdp_put16(pkt, SBL_IP_HDR_LEN + UDP_OFF_CHECKSUM, 0x0000U); /* optional */
}

bool SblIpUdp_parseRxFrame(const uint8_t   *frame,
                             uint32_t         frameLen,
                             SblIpUdp_RxInfo *info)
{
    uint32_t ihl;
    uint32_t ipBase;
    uint32_t udpBase;
    uint16_t etherType;
    uint16_t udpLen;
    uint16_t ipTotalLen;

    /* ---- Ethernet header ---- */
    if (frameLen < (SBL_ETH_HDR_LEN + SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN))
    {
        return false;
    }

    etherType = SblIpUdp_get16(frame, SBL_ETH_ETHERTYPE_OFFSET);
    if (etherType != SBL_ETHERTYPE_IPV4)
    {
        return false;
    }

    /* ---- IPv4 header ---- */
    ipBase = SBL_ETH_HDR_LEN;

    if ((frame[ipBase + IP_OFF_VERSION_IHL] >> 4U) != 4U)
    {
        return false; /* not IPv4 */
    }

    ihl = (uint32_t)(frame[ipBase + IP_OFF_VERSION_IHL] & 0x0FU) * 4U;
    if (ihl < SBL_IP_HDR_LEN)
    {
        return false; /* IHL < 20 is invalid */
    }

    if (frame[ipBase + IP_OFF_PROTOCOL] != SBL_IP_PROTO_UDP)
    {
        return false;
    }

    /* Use the IP total-length field to bound how far we read; this also
     * handles Ethernet padding that extends the frame beyond the IP datagram. */
    ipTotalLen = SblIpUdp_get16(frame, ipBase + IP_OFF_TOTAL_LEN);
    if (((uint32_t)ipTotalLen < (ihl + SBL_UDP_HDR_LEN)) ||
        ((SBL_ETH_HDR_LEN + (uint32_t)ipTotalLen) > frameLen))
    {
        return false;
    }

    info->srcIp = SblIpUdp_get32(frame, ipBase + IP_OFF_SRC);
    info->dstIp = SblIpUdp_get32(frame, ipBase + IP_OFF_DST);

    /* ---- UDP header ---- */
    udpBase = ipBase + ihl;

    info->srcPort = SblIpUdp_get16(frame, udpBase + UDP_OFF_SRC_PORT);
    info->dstPort = SblIpUdp_get16(frame, udpBase + UDP_OFF_DST_PORT);

    /* UDP length field is authoritative for payload size */
    udpLen = SblIpUdp_get16(frame, udpBase + UDP_OFF_LENGTH);
    if (udpLen < SBL_UDP_HDR_LEN)
    {
        return false;
    }

    info->payload    = frame + udpBase + SBL_UDP_HDR_LEN;
    info->payloadLen = (uint32_t)(udpLen - (uint16_t)SBL_UDP_HDR_LEN);

    return true;
}
