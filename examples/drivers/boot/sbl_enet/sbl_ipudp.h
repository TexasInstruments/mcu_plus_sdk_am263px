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
 * \file  sbl_ipudp.h
 *
 * \brief Ethernet / IPv4 / UDP packet utilities for the Ethernet SBL.
 *
 * Provides:
 *  - Shared frame-level constants (EtherType values, header lengths).
 *  - Big-endian byte helpers as \c static \c inline functions so every
 *    protocol module can use them without a function-call overhead.
 *  - SblIpUdp_fillTxHeaders() to build a complete IPv4 + UDP header pair
 *    for outgoing packets.
 *  - SblIpUdp_parseRxFrame() to validate and decompose an incoming Ethernet
 *    frame into its IPv4 and UDP fields.
 *  - SblIpUdp_calcIpChecksum() for use whenever a header checksum must be
 *    recomputed (e.g. after updating the source IP).
 */

#ifndef SBL_IPUDP_H_
#define SBL_IPUDP_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* ---- Ethernet frame ---- */

/*! Total length of a standard (non-VLAN) Ethernet header in bytes. */
#define SBL_ETH_HDR_LEN             (14U)
/*! Byte offset of the EtherType field inside the Ethernet header. */
#define SBL_ETH_ETHERTYPE_OFFSET    (12U)
/*! EtherType value for ARP frames. */
#define SBL_ETHERTYPE_ARP           (0x0806U)
/*! EtherType value for IPv4 frames. */
#define SBL_ETHERTYPE_IPV4          (0x0800U)

/* ---- IPv4 header ---- */

/*! Minimum IPv4 header length in bytes (no options). */
#define SBL_IP_HDR_LEN              (20U)
/*! IP protocol number for UDP. */
#define SBL_IP_PROTO_UDP            (17U)
/*! Default TTL used in transmitted packets. */
#define SBL_IP_TTL_DEFAULT          (64U)
/*! Unspecified / all-zeros source address (used before DHCP completes). */
#define SBL_IP_ADDR_NONE            (0x00000000UL)
/*! Limited broadcast address. */
#define SBL_IP_ADDR_BROADCAST       (0xFFFFFFFFUL)

/* ---- UDP header ---- */

/*! UDP header length in bytes. */
#define SBL_UDP_HDR_LEN             (8U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/*!
 * \brief  Parsed fields of an IPv4/UDP frame returned by SblIpUdp_parseRxFrame().
 *
 * All multi-byte fields are in host byte order.
 * \c payload points directly into the original frame buffer and is valid only
 * as long as that buffer is valid.
 */
typedef struct SblIpUdp_RxInfo_s
{
    uint32_t       srcIp;       /*!< IPv4 source address (host byte order)      */
    uint32_t       dstIp;       /*!< IPv4 destination address (host byte order) */
    uint16_t       srcPort;     /*!< UDP source port                            */
    uint16_t       dstPort;     /*!< UDP destination port                       */
    const uint8_t *payload;     /*!< Pointer to UDP payload (after UDP header)  */
    uint32_t       payloadLen;  /*!< UDP payload length in bytes                */
} SblIpUdp_RxInfo;

/* ========================================================================== */
/*                   Inline Big-Endian Byte Helpers                           */
/* ========================================================================== */

/*!
 * \brief  Read a big-endian 16-bit value from \p buf at byte offset \p off.
 */
static inline uint16_t SblIpUdp_get16(const uint8_t *buf, uint32_t off)
{
    return (uint16_t)(((uint16_t)buf[off] << 8U) | buf[off + 1U]);
}

/*!
 * \brief  Read a big-endian 32-bit value from \p buf at byte offset \p off.
 */
static inline uint32_t SblIpUdp_get32(const uint8_t *buf, uint32_t off)
{
    return ((uint32_t)buf[off]      << 24U) |
           ((uint32_t)buf[off + 1U] << 16U) |
           ((uint32_t)buf[off + 2U] <<  8U) |
            (uint32_t)buf[off + 3U];
}

/*!
 * \brief  Write a big-endian 16-bit value \p val into \p buf at byte offset \p off.
 */
static inline void SblIpUdp_put16(uint8_t *buf, uint32_t off, uint16_t val)
{
    buf[off]      = (uint8_t)(val >> 8U);
    buf[off + 1U] = (uint8_t)(val & 0xFFU);
}

/*!
 * \brief  Write a big-endian 32-bit value \p val into \p buf at byte offset \p off.
 */
static inline void SblIpUdp_put32(uint8_t *buf, uint32_t off, uint32_t val)
{
    buf[off]      = (uint8_t)(val >> 24U);
    buf[off + 1U] = (uint8_t)(val >> 16U);
    buf[off + 2U] = (uint8_t)(val >>  8U);
    buf[off + 3U] = (uint8_t)(val & 0xFFU);
}

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  Compute the RFC 791 one's-complement checksum over an IPv4 header.
 *
 * The checksum field in the header should be zeroed before calling this
 * function.  Write the returned value back into bytes 10-11 of the header.
 *
 * \param  hdr     Pointer to the start of the IPv4 header.
 * \param  hdrLen  Header length in bytes (typically SBL_IP_HDR_LEN = 20).
 *
 * \return Computed 16-bit checksum in host byte order.
 */
uint16_t SblIpUdp_calcIpChecksum(const uint8_t *hdr, uint32_t hdrLen);

/*!
 * \brief  Fill the IPv4 and UDP headers at the start of \p pkt.
 *
 * Writes \c SBL_IP_HDR_LEN + \c SBL_UDP_HDR_LEN = 28 bytes into
 * \p pkt[0..27].  The caller is responsible for placing the UDP payload
 * at \p pkt[28] onwards before transmitting.
 *
 * The IPv4 checksum is computed and inserted automatically.
 * The UDP checksum is set to zero (optional for IPv4, RFC 768).
 *
 * \param  pkt            Output buffer (must be at least 28 bytes).
 * \param  srcIp          Source IPv4 address (host byte order).
 * \param  dstIp          Destination IPv4 address (host byte order).
 * \param  srcPort        UDP source port.
 * \param  dstPort        UDP destination port.
 * \param  udpPayloadLen  Length of the UDP payload in bytes.
 */
void SblIpUdp_fillTxHeaders(uint8_t  *pkt,
                              uint32_t  srcIp,
                              uint32_t  dstIp,
                              uint16_t  srcPort,
                              uint16_t  dstPort,
                              uint32_t  udpPayloadLen);

/*!
 * \brief  Validate an inbound Ethernet frame as IPv4/UDP and extract fields.
 *
 * Checks in order:
 *  1. Minimum frame length.
 *  2. EtherType == 0x0800 (IPv4).
 *  3. IP version == 4 and IHL >= 20.
 *  4. IP protocol == 17 (UDP).
 *  5. Frame long enough to contain the IP and UDP headers.
 *  6. UDP length field is self-consistent with the frame.
 *
 * UDP checksum is not validated (legal under RFC 768 when set to zero, and
 * too costly for a minimal SBL).
 *
 * \param  frame     Pointer to the start of the Ethernet frame (Ethernet hdr).
 * \param  frameLen  Total byte length of \p frame.
 * \param  info      Output: populated with src/dst IP, src/dst port, and a
 *                   direct pointer into \p frame for the UDP payload.
 *
 * \return \c true if the frame is a valid IPv4/UDP datagram; \c false otherwise.
 */
bool SblIpUdp_parseRxFrame(const uint8_t    *frame,
                             uint32_t          frameLen,
                             SblIpUdp_RxInfo  *info);

#ifdef __cplusplus
}
#endif

#endif /* SBL_IPUDP_H_ */
