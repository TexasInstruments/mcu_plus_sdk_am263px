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
 * \file  sbl_cpswconfig.h
 *
 * \brief CPSW peripheral open/close and DMA channel management for the
 *        Ethernet SBL.
 */

#ifndef SBL_CPSWCONFIG_H_
#define SBL_CPSWCONFIG_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <stdbool.h>
#include <enet.h>
#include <include/core/enet_dma.h>

#include "ti_enet_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*! Maximum number of times SblCpsw_txPacket will attempt to reclaim a free
 *  TX descriptor from DMA before giving up. */
#define SBLCPSW_TX_FREE_PKT_MAX_RETRIES     (10U)

/*! Maximum packets returned per SblCpsw_getRxPackets() call.
 *  Must not exceed the RX DMA ring depth (ENET_SYSCFG_TOTAL_NUM_RX_PKT). */
#define SBLCPSW_RX_MAX_PKTS_PER_CALL        (ENET_SYSCFG_TOTAL_NUM_RX_PKT)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/*!
 * \brief  SBL CPSW state object.
 *
 * Holds all handles and queues needed for the lifetime of the CPSW peripheral.
 * Intentionally excludes semaphores and task handles — this is a NoRTOS
 * polling-based implementation.
 */
typedef struct SblCpsw_Obj_s
{
    Enet_Handle        hEnet;              /*!< Enet peripheral handle            */
    Enet_Type          enetType;           /*!< Peripheral type (ENET_CPSW_2G)    */
    uint32_t           instId;             /*!< Peripheral instance ID             */
    uint32_t           coreId;             /*!< Core ID of this R5F               */
    uint32_t           coreKey;            /*!< Key returned by core attach        */
    Enet_MacPort       macPort;            /*!< MAC port in use                    */
    EnetDma_TxChHandle hTxCh;             /*!< TX DMA channel handle              */
    EnetDma_RxChHandle hRxCh;             /*!< RX DMA channel handle              */
    EnetDma_PktQ       txFreePktInfoQ;    /*!< Pool of free TX packet descriptors */
    EnetDma_PktQ       rxFreeQ;           /*!< Pool of free RX buffers            */
    EnetDma_PktQ       rxReadyQ;          /*!< Received packets ready for app     */
    uint8_t            hostMacAddr[ENET_MAC_ADDR_LEN]; /*!< Host-port MAC address */

} SblCpsw_Obj;

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/*! \brief  Single global CPSW state object — directly accessible by other
 *          SBL modules (ARP, DHCP, TFTP) for DMA handle and MAC address. */
extern SblCpsw_Obj gSblCpsw;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  Open the CPSW peripheral: initialise driver, acquire handle, attach
 *         core, open TX/RX DMA channels, and add the host MAC address to ALE.
 *
 * Does NOT wait for PHY link-up; call SblCpsw_waitForLinkUp() after this
 * returns to block until the link is established.
 *
 * \return ENET_SOK on success, negative error code on failure.
 */
int32_t SblCpsw_open(void);

/*!
 * \brief  Poll until the MAC port reports link-up.
 *
 * Loops with a 50 ms sleep between IOCTL polls.  Suitable for NoRTOS because
 * it does not block on a semaphore.
 *
 * \return ENET_SOK when link is up, ENET_EFAIL if the IOCTL returns an error.
 */
int32_t SblCpsw_waitForLinkUp(void);

/*!
 * \brief  Close DMA channels, detach core, release handle, and deinitialise
 *         the CPSW driver.
 */
int32_t SblCpsw_close(void);

/*!
 * \brief  One received Ethernet frame.
 *
 * \c buf points directly into the DMA-owned buffer — zero copy.
 * The pointer is valid only until SblCpsw_submitRxPackets() is called.
 * Handlers must NOT store \c buf beyond that point.
 */
typedef struct SblCpsw_RxPkt_s
{
    const uint8_t *buf;     /*!< DMA buffer — raw frame at the Ethernet header */
    uint32_t       len;     /*!< Frame length in bytes                         */
    EnetDma_Pkt   *pktInfo; /*!< Opaque DMA handle used by submitRxPackets     */
} SblCpsw_RxPkt;

/*!
 * \brief  List of received packets returned by SblCpsw_getRxPackets().
 */
typedef struct SblCpsw_RxPktList_s
{
    SblCpsw_RxPkt pkts[SBLCPSW_RX_MAX_PKTS_PER_CALL];
    uint32_t      count; /*!< Number of valid entries in \c pkts */
} SblCpsw_RxPktList;

/*!
 * \brief  Drain the RX DMA channel into \p list (zero-copy).
 *
 * Retrieves all available frames from the DMA engine and stores direct
 * pointers into the DMA buffers in \p list.  No data is copied.
 * Descriptors are held in app ownership until SblCpsw_submitRxPackets()
 * is called — the caller must call that after all handlers have finished.
 *
 * \param  list  Output list populated with up to SBLCPSW_RX_MAX_PKTS_PER_CALL
 *               packets.  \c list->count is set to 0 when no frames are ready.
 */
void SblCpsw_getRxPackets(SblCpsw_RxPktList *list);

/*!
 * \brief  Return all packets in \p list back to the RX DMA engine.
 *
 * Must be called once per loop iteration after all protocol handlers have
 * finished reading the frame data.  Invalidates all \c buf pointers in
 * \p list and resets \c list->count to zero.
 *
 * \param  list  List previously populated by SblCpsw_getRxPackets().
 */
void SblCpsw_submitRxPackets(SblCpsw_RxPktList *list);

/*!
 * \brief  Copy the host-port MAC address obtained during DMA open.
 *
 * \param  macAddr  Output buffer of at least ENET_MAC_ADDR_LEN (6) bytes.
 */
void SblCpsw_getMacAddr(uint8_t *macAddr);

/*!
 * \brief  Transmit a single Ethernet frame.
 *
 * Dequeues one free TX descriptor from \c txFreePktInfoQ, fills the Ethernet
 * header (src = host MAC, dst = \p dstMacAddr, type = \p etherType), copies
 * \p payloadBuf into the frame body, then submits the packet to the TX DMA
 * channel.
 *
 * If \c txFreePktInfoQ is empty the function polls \c EnetDma_retrieveTxPktQ
 * in a loop (up to \c SBLCPSW_TX_FREE_PKT_MAX_RETRIES times with a 1 ms
 * sleep between attempts) to reclaim completed descriptors back from the DMA
 * engine before retrying the dequeue.
 *
 * \param  payloadBuf   Pointer to the payload bytes to transmit.
 * \param  payloadLen   Length of \p payloadBuf in bytes.
 * \param  dstMacAddr   6-byte destination MAC address.
 * \param  etherType    EtherType field (host byte order; will be byte-swapped).
 *
 * \return ENET_SOK on success, ENET_EINVALIDPARAMS if \p payloadLen exceeds
 *         the pool buffer size, ENET_EFAIL if no descriptor became available.
 */
int32_t SblCpsw_txPacket(const uint8_t *payloadBuf,
                          uint32_t       payloadLen,
                          const uint8_t *dstMacAddr,
                          uint16_t       etherType);

#ifdef __cplusplus
}
#endif

#endif /* SBL_CPSWCONFIG_H_ */
