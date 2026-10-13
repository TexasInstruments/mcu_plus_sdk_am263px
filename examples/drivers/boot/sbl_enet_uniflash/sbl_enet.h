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
 * \file  sbl_enet.h
 *
 * \brief Public API for the Ethernet SBL transport layer.
 *
 * This header exposes only the top-level functions used by main.c.
 * All internal types, structs, and protocol constants live in sbl_enet.c.
 */

#ifndef _SBL_ENET_H_
#define _SBL_ENET_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                        Network Configuration                               */
/* ========================================================================== */

/** Host PC MAC address */
#define ENET_HOST_PC_MAC_ADDRESS        {0x7C, 0xC2, 0xC6, 0x54, 0x57, 0x6C}
/** UDP port used for SBL communication (same for source and destination) */
#define ENET_PORT                       5001
/** EVM IP address (source) */
#define ENET_SOURCE_IP_ADDRESS          { 192U, 168U, 0U, 195U }
/** Host PC IP address (destination) */
#define ENET_DESTINATION_IP_ADDRESS     { 192U, 168U, 0U, 136U }

#define ENETSBL_HEADER_MGC_NUMBER       (0x05B1C00D)
#define ENETSBL_HEADER_ACK              (0x05B10ACD)

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 * \brief  Initialize the Ethernet peripheral, open DMA channels, and wait
 *         for link up.
 *
 * \return ENET_SOK on success, negative error code on failure.
 */
int32_t EnetSBL_setup(void);

/**
 * \brief  Tear down the Ethernet peripheral, close DMA channels, and free
 *         all associated resources.
 */
void EnetSBL_destruct(void);


/**
 * \brief  Receive a complete appimage/uniflash file over Ethernet into
 *         a caller-provided buffer.
 *
 * Blocks until all packets of one file have been received or a timeout
 * occurs.  Each received packet is ACK'd back to the sender individually.
 *
 * \param  buf           Caller-allocated buffer to write received data into.
 * \param  bufMaxSize    Size of \p buf in bytes.
 * \param  receivedSize  Set to the total bytes written to \p buf on success.
 *
 * \return ENET_SOK on success, ENET_ETIMEOUT if no transfer started within
 *         the wait window, ENET_EFAIL on buffer overflow, or another negative
 *         error code on failure.
 */
int32_t EnetSBL_receiveAppimage(uint8_t *buf, uint32_t bufMaxSize, uint32_t *receivedSize);

/**
 * \brief  Send a reply payload back to the host PC over Ethernet.
 *
 * \param  payload      Pointer to the bytes to transmit (e.g. a serialised
 *                      Bootloader_UniflashResponseHeader cast to uint8_t *).
 *
 * \return ENET_SOK on success, negative error code on failure.
 */
int32_t EnetSBL_txReply(uint8_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* _SBL_ENET_H_ */
