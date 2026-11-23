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
 * \file  sbl_tftp.c
 *
 * \brief Minimal TFTP client (RFC 1350, read-only) for the Ethernet SBL.
 *
 * State machine: IDLE → ARP_WAIT → RRQ_SENT → RECEIVING → COMPLETE / ERROR
 *
 * Packet flow:
 *   TX RRQ to server:69  →  RX DATA(1) from server:TID  →  TX ACK(1)
 *   RX DATA(2)  →  TX ACK(2)  …  RX DATA(n, <512 B)  →  TX ACK(n)  → COMPLETE
 *
 * ARP and DHCP must be initialised before the first SblTftp_handler() call.
 * The IDLE→ARP_WAIT transition is deferred until DHCP is BOUND.
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
#include "sbl_tftp.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*! Set to 1 to enable verbose TFTP state-machine debug logs. */
#define SBLTFTP_DEBUG_LOGS          (1U)

#if (SBLTFTP_DEBUG_LOGS == 1U)
#define SBLTFTP_LOG(...)            EnetAppUtils_print(__VA_ARGS__)
#else
#define SBLTFTP_LOG(...)            do { } while (0)
#endif

/* ---- TFTP protocol constants ---- */
#define TFTP_OPCODE_RRQ             (1U)
#define TFTP_OPCODE_DATA            (3U)
#define TFTP_OPCODE_ACK             (4U)
#define TFTP_OPCODE_ERROR           (5U)

/*! Well-known TFTP server port. */
#define TFTP_SERVER_PORT            (69U)
/*! Fixed ephemeral TID used as the client UDP source port. */
#define TFTP_CLIENT_PORT            (10000U)

/*! Standard TFTP block payload size. */
#define TFTP_BLOCK_SIZE             (512U)
/*! Fixed overhead inside a DATA packet: opcode(2) + block-num(2). */
#define TFTP_DATA_HDR_LEN           (4U)
/*! ACK packet size: opcode(2) + block-num(2). */
#define TFTP_ACK_LEN                (4U)

/*! Interval before retransmitting RRQ or last ACK (us). */
#define TFTP_TIMEOUT_US             (3000000U)   /* 3 s */
/*! Maximum retransmit attempts before giving up. */
#define TFTP_MAX_RETRIES            (5U)

/*! TX buffer large enough for IP + UDP + RRQ (filename < ~270 bytes). */
#define TFTP_TX_BUF_LEN             (320U)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/*!
 * \brief  TFTP transfer configuration stored by SblTftp_init().
 */
typedef struct SblTftp_Cfg_s
{
    uint32_t    serverIp; /*!< TFTP server IPv4 address (host byte order) */
    const char *filename; /*!< Filename to request from the server        */
    uint8_t    *buf;      /*!< Destination buffer for the received image  */
    uint32_t    bufLen;   /*!< Size of \c buf in bytes                    */
} SblTftp_Cfg;


typedef struct SblTftp_Obj_s
{
    SblTftp_State state;
    SblTftp_Cfg   cfg;             /*!< Transfer parameters from init()      */
    uint16_t      nextBlock;       /*!< Next expected DATA block number      */
    uint16_t      lastAckedBlock;  /*!< Last block for which ACK was sent    */
    uint32_t      receivedBytes;   /*!< Total payload bytes written to buf   */
    uint16_t      serverTid;       /*!< Server TID port (from first DATA)    */
    uint8_t       serverMac[ENET_MAC_ADDR_LEN];
    uint64_t      stateEntryTimeUs;
    uint32_t      retryCount;
} SblTftp_Obj;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static void SblTftp_sendRrq(void);
static void SblTftp_sendAck(uint16_t blockNum, uint16_t dstPort);
static bool SblTftp_processFrame(const uint8_t *buf, uint32_t len);

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

static SblTftp_Obj gSblTftp;
static uint8_t     gSblTftp_txBuf[TFTP_TX_BUF_LEN];

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void SblTftp_init(uint8_t *buf, uint32_t bufLen)
{
    memset(&gSblTftp, 0, sizeof(gSblTftp));

    gSblTftp.cfg.buf      = buf;
    gSblTftp.cfg.bufLen   = bufLen;
    gSblTftp.nextBlock    = 1U;
    gSblTftp.state        = SBLTFTP_STATE_IDLE;
    /* serverIp and filename are resolved from DHCP options 66/67 in the IDLE state */
}

SblTftp_State SblTftp_getState(void)
{
    return gSblTftp.state;
}

uint32_t SblTftp_getReceivedSize(void)
{
    return gSblTftp.receivedBytes;
}

void SblTftp_handler(const SblCpsw_RxPktList *rxList)
{
    uint64_t nowUs;
    uint32_t i;

    switch (gSblTftp.state)
    {
        case SBLTFTP_STATE_IDLE:
        {
            /* Defer until both DHCP is bound and a valid server IP is set */
            if (!SblDhcp_isDone()) { break; }

            gSblTftp.cfg.serverIp = SblDhcp_getTftpServerIp();
            gSblTftp.cfg.filename = SblDhcp_getBootfileName();

            SBLTFTP_LOG("[TFTP] init: server=%d.%d.%d.%d  file='%s' \r\n",
                (gSblTftp.cfg.serverIp >> 24U) & 0xFFU, (gSblTftp.cfg.serverIp >> 16U) & 0xFFU,
                (gSblTftp.cfg.serverIp >>  8U) & 0xFFU,  gSblTftp.cfg.serverIp         & 0xFFU,
                gSblTftp.cfg.filename);

            SblArp_resolve(gSblTftp.cfg.serverIp);
            gSblTftp.state            = SBLTFTP_STATE_ARP_WAIT;
            gSblTftp.stateEntryTimeUs = ClockP_getTimeUsec();
            SBLTFTP_LOG("[TFTP] DHCP bound, waiting for ARP of server\r\n");
            break;
        }

        case SBLTFTP_STATE_ARP_WAIT:
        {
            if (SblArp_lookup(gSblTftp.cfg.serverIp, gSblTftp.serverMac))
            {
                SBLTFTP_LOG("[TFTP] server MAC resolved, sending RRQ\r\n");
                SblTftp_sendRrq();
                gSblTftp.state            = SBLTFTP_STATE_RRQ_SENT;
                gSblTftp.stateEntryTimeUs = ClockP_getTimeUsec();
                gSblTftp.retryCount       = 0U;
            }
            break;
        }

        case SBLTFTP_STATE_RRQ_SENT:
        {
            /* Scan for DATA block 1 */
            for (i = 0U; i < rxList->count; i++)
            {
                if (SblTftp_processFrame(rxList->pkts[i].buf, rxList->pkts[i].len))
                {
                    break;
                }
            }

            /* Retransmit RRQ on timeout */
            if (gSblTftp.state == SBLTFTP_STATE_RRQ_SENT)
            {
                nowUs = ClockP_getTimeUsec();
                if ((nowUs - gSblTftp.stateEntryTimeUs) >= TFTP_TIMEOUT_US)
                {
                    if (gSblTftp.retryCount < TFTP_MAX_RETRIES)
                    {
                        gSblTftp.retryCount++;
                        SBLTFTP_LOG("[TFTP] RRQ timeout, retry %u/%u\r\n",
                                    gSblTftp.retryCount, TFTP_MAX_RETRIES);
                        SblTftp_sendRrq();
                        gSblTftp.stateEntryTimeUs = nowUs;
                    }
                    else
                    {
                        EnetAppUtils_print("[TFTP] no response from server, transfer failed\r\n");
                        gSblTftp.state = SBLTFTP_STATE_ERROR;
                    }
                }
            }
            break;
        }

        case SBLTFTP_STATE_RECEIVING:
        {
            /* Scan for next DATA block */
            for (i = 0U; i < rxList->count; i++)
            {
                if (SblTftp_processFrame(rxList->pkts[i].buf, rxList->pkts[i].len))
                {
                    break;
                }
            }

            /* Retransmit last ACK on timeout to unblock the server */
            if (gSblTftp.state == SBLTFTP_STATE_RECEIVING)
            {
                nowUs = ClockP_getTimeUsec();
                if ((nowUs - gSblTftp.stateEntryTimeUs) >= TFTP_TIMEOUT_US)
                {
                    if (gSblTftp.retryCount < TFTP_MAX_RETRIES)
                    {
                        gSblTftp.retryCount++;
                        SBLTFTP_LOG("[TFTP] DATA timeout, re-ACKing block %u (retry %u/%u)\r\n",
                                    (uint32_t)gSblTftp.lastAckedBlock,
                                    gSblTftp.retryCount, TFTP_MAX_RETRIES);
                        SblTftp_sendAck(gSblTftp.lastAckedBlock, gSblTftp.serverTid);
                        gSblTftp.stateEntryTimeUs = nowUs;
                    }
                    else
                    {
                        EnetAppUtils_print("[TFTP] transfer stalled, transfer failed\r\n");
                        gSblTftp.state = SBLTFTP_STATE_ERROR;
                    }
                }
            }
            break;
        }

        case SBLTFTP_STATE_COMPLETE:
        case SBLTFTP_STATE_ERROR:
            /* Terminal states: nothing to do */
            break;

        default:
            break;
    }
}


/* ========================================================================== */
/*                        Static Function Definitions                         */
/* ========================================================================== */

static void SblTftp_sendRrq(void)
{
    uint8_t        *tftp;
    uint8_t        *p;
    SblDhcp_Params  params;
    uint32_t        filenameLen;
    uint32_t        tftp_len;
    uint32_t        pktLen;
    int32_t         status;

    if (SblDhcp_getNetworkParams(&params) != ENET_SOK)
    { 
        return; 
    }

    filenameLen = (uint32_t)strlen(gSblTftp.cfg.filename);
    /* opcode(2) + filename + '\0' + "octet" + '\0' */
    tftp_len = 2U + filenameLen + 1U + 5U + 1U;

    if ((SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + tftp_len) > TFTP_TX_BUF_LEN)
    {
        EnetAppUtils_print("[TFTP] filename too long for TX buffer\r\n");
        return;
    }

    memset(gSblTftp_txBuf, 0, TFTP_TX_BUF_LEN);
    tftp = gSblTftp_txBuf + SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN;
    p    = tftp;

    SblIpUdp_put16(p, 0U, (uint16_t)TFTP_OPCODE_RRQ);
    p += 2U;
    memcpy(p, gSblTftp.cfg.filename, filenameLen + 1U); /* +1: null terminator */
    p += filenameLen + 1U;
    memcpy(p, "octet", 6U);                              /* 5 chars + null */

    pktLen = SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + tftp_len;

    SblIpUdp_fillTxHeaders(gSblTftp_txBuf,
                            params.assignedIp,
                            gSblTftp.cfg.serverIp,
                            (uint16_t)TFTP_CLIENT_PORT,
                            (uint16_t)TFTP_SERVER_PORT,
                            tftp_len);

    status = SblCpsw_txPacket(gSblTftp_txBuf, pktLen,
                               gSblTftp.serverMac,
                               (uint16_t)SBL_ETHERTYPE_IPV4);
    if (status == ENET_SOK)
    {
        SBLTFTP_LOG("[TFTP] RRQ sent: file='%s'\r\n", gSblTftp.cfg.filename);
    }
    else
    {
        EnetAppUtils_print("[TFTP] RRQ TX failed: %d\r\n", status);
    }
}

static void SblTftp_sendAck(uint16_t blockNum, uint16_t dstPort)
{
    uint8_t         txBuf[SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN + TFTP_ACK_LEN];
    uint8_t        *tftp;
    SblDhcp_Params  params;
    int32_t         status;

    if (SblDhcp_getNetworkParams(&params) != ENET_SOK)
    { 
        return; 
    }

    memset(txBuf, 0, sizeof(txBuf));
    tftp = txBuf + SBL_IP_HDR_LEN + SBL_UDP_HDR_LEN;
    SblIpUdp_put16(tftp, 0U, (uint16_t)TFTP_OPCODE_ACK);
    SblIpUdp_put16(tftp, 2U, blockNum);

    SblIpUdp_fillTxHeaders(txBuf,
                            params.assignedIp,
                            gSblTftp.cfg.serverIp,
                            (uint16_t)TFTP_CLIENT_PORT,
                            dstPort,
                            (uint32_t)TFTP_ACK_LEN);

    status = SblCpsw_txPacket(txBuf, (uint32_t)sizeof(txBuf),
                               gSblTftp.serverMac,
                               (uint16_t)SBL_ETHERTYPE_IPV4);
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[TFTP] ACK TX failed: %d\r\n", status);
    }
}

static bool SblTftp_processFrame(const uint8_t *buf, uint32_t len)
{
    SblIpUdp_RxInfo rxInfo;
    uint16_t        opcode;
    uint16_t        blockNum;
    const uint8_t  *tftp;
    uint32_t        dataLen;
    const uint8_t  *data;
    bool retVal = false;

    if (!SblIpUdp_parseRxFrame(buf, len, &rxInfo))
    { 
        return false; 
    }

    /* Only accept packets from the configured TFTP server */
    if (rxInfo.srcIp != gSblTftp.cfg.serverIp) 
    { 
        return false; 
    }    

    /* Only accept packets destined to our client TID */
    if (rxInfo.dstPort != (uint16_t)TFTP_CLIENT_PORT) 
    { 
        return false; 
    }

    if (rxInfo.payloadLen < 2U) 
    { 
        return false; 
    }

    tftp   = rxInfo.payload;
    opcode = SblIpUdp_get16(tftp, 0U);

    switch (opcode)
    {
        case TFTP_OPCODE_DATA:
        {
            if (rxInfo.payloadLen < TFTP_DATA_HDR_LEN) 
            { 
                retVal = false;
                break; 
            }

            blockNum = SblIpUdp_get16(tftp, 2U);
            data     = tftp + TFTP_DATA_HDR_LEN;
            dataLen  = rxInfo.payloadLen - TFTP_DATA_HDR_LEN;

            /* On the first DATA we learn the server's TID */
            if (gSblTftp.state == SBLTFTP_STATE_RRQ_SENT)
            {
                if (blockNum != 1U) 
                { 
                    retVal = false;
                    break; 
                }
                gSblTftp.serverTid = rxInfo.srcPort;
                SBLTFTP_LOG("[TFTP] server TID=%u\r\n", (uint32_t)gSblTftp.serverTid);
            }
            else
            {
                /* Subsequent blocks must come from the same TID */
                if (rxInfo.srcPort != gSblTftp.serverTid) 
                { 
                   retVal = false;
                    break; 
                }
            }

            if (blockNum == gSblTftp.nextBlock)
            {
                /* Bounds check */
                if ((gSblTftp.receivedBytes + dataLen) > gSblTftp.cfg.bufLen)
                {
                    EnetAppUtils_print("[TFTP] buffer overflow (%u+%u > %u), aborting\r\n",
                                       gSblTftp.receivedBytes, dataLen,
                                       gSblTftp.cfg.bufLen);
                    gSblTftp.state = SBLTFTP_STATE_ERROR;
                    retVal = true;
                    break;
                }

                memcpy(gSblTftp.cfg.buf + gSblTftp.receivedBytes, data, dataLen);
                gSblTftp.receivedBytes  += dataLen;
                gSblTftp.lastAckedBlock  = blockNum;
                gSblTftp.nextBlock++;
                gSblTftp.retryCount       = 0U;
                gSblTftp.stateEntryTimeUs = ClockP_getTimeUsec();

                SblTftp_sendAck(blockNum, gSblTftp.serverTid);

                if (dataLen < TFTP_BLOCK_SIZE)
                {
                    /* Last block: transfer complete */
                    gSblTftp.state = SBLTFTP_STATE_COMPLETE;
                    EnetAppUtils_print("[TFTP] transfer complete: %u bytes received\r\n",
                                       gSblTftp.receivedBytes);
                }
                else
                {
                    gSblTftp.state = SBLTFTP_STATE_RECEIVING;
                }
            }
            else if (blockNum == gSblTftp.lastAckedBlock)
            {
                /* Duplicate — re-send ACK to unblock the server */
                SBLTFTP_LOG("[TFTP] duplicate block %u, re-ACKing\r\n",
                            (uint32_t)blockNum);
                SblTftp_sendAck(blockNum, gSblTftp.serverTid);
            }
            retVal = true;
            break;
        }

        case TFTP_OPCODE_ERROR:
        {
            uint16_t errCode = (rxInfo.payloadLen >= 4U) ?
                                SblIpUdp_get16(tftp, 2U) : 0U;
            EnetAppUtils_print("[TFTP] server error code=%u, transfer failed\r\n",
                               (uint32_t)errCode);
            gSblTftp.state = SBLTFTP_STATE_ERROR;
           retVal = true;
        }

        default:
            break;
    }
    return retVal;
}
