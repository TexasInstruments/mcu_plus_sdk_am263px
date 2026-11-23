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
 * \file  sbl_main.c
 *
 * \brief Ethernet SBL main receive-and-boot loop.
 *
 * Owns the boot sequence:
 *   SblCpsw_open → protocol inits → SblCpsw_waitForLinkUp
 *   → receive loop (SblCpsw_getRxPackets → dispatch → handlers)
 *   → SblCpsw_close
 *
 * Per-frame dispatch (EtherType + UDP-port) lives here rather than inside
 * sbl_cpswconfig.c so the CPSW layer stays protocol-agnostic.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>

#include <enet_apputils.h>

#include "sbl_cpswconfig.h"
#include "sbl_arp.h"
#include "sbl_dhcp.h"
#include "sbl_tftp.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */


/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */



/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */


/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */


int32_t SBLEnet_reveiveAppimage(uint8_t    *buffer,
                        uint32_t    bufferLen)
{
    int32_t           status;
    SblCpsw_RxPktList rxList;
    uint8_t           macAddr[ENET_MAC_ADDR_LEN];

    EnetAppUtils_print("==========================================\r\n");
    EnetAppUtils_print("      Ethernet SBL APPLICATION            \r\n");
    EnetAppUtils_print("==========================================\r\n");

    EnetAppUtils_print("[ ENET-SBL ] : Openning Enet Driver ...\r\n");
    status = SblCpsw_open();
    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[ ENET-SBL ] : Enet Driver Open Failed\r\n");
    }
    else
    {
        EnetAppUtils_print("[ ENET-SBL ] : Enet Driver Open completed \r\n");
    }

    if(status == ENET_SOK)
    {
        status = SblCpsw_waitForLinkUp();
    }

    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[ ENET-SBL ] : Enet Link up failed\r\n");
    }
    else
    {
        EnetAppUtils_print("[ ENET-SBL ] : Link Up done \r\n");
    }

    if(status == ENET_SOK)
    {
        SblArp_init();
        
        SblCpsw_getMacAddr(macAddr);
        SblDhcp_init(macAddr);

        SblTftp_init(buffer, bufferLen);

        EnetAppUtils_print("[ ENET-SBL ] : Starting File Reception\r\n");

        while ((SblTftp_getState() != SBLTFTP_STATE_COMPLETE) && (SblTftp_getState() != SBLTFTP_STATE_ERROR))
        {
            /* Drain DMA: copies frames to scratch bufs, recycles descriptors.
            * rxList.count may be zero if no frames arrived this iteration. */
            SblCpsw_getRxPackets(&rxList);

            /* Call each protocol handler sequentially with the full received frame
            * list.  Each handler filters the packets it cares about internally
            * and drives its own state machine — including timeout-based retransmits
            * when rxList.count is zero. */
            
            SblArp_handler(&rxList);
            
            SblDhcp_handler(&rxList);

            SblTftp_handler(&rxList);

            /* Return DMA buffers to hardware after all handlers have read them.
            * No handler may access rxList.pkts[].buf after this call. */
            SblCpsw_submitRxPackets(&rxList);
        }
    }

    EnetAppUtils_print("[ ENET-SBL ] : Closing Enet Driver ...\r\n");

    status = SblCpsw_close();

    if (status != ENET_SOK)
    {
        EnetAppUtils_print("[ ENET-SBL ] : Enet Driver Close Failed %d\r\n",status);
    }
    else
    {
        EnetAppUtils_print("[ ENET-SBL ] : Enet Driver Closed\r\n");
    }

    if(SblTftp_getState() == SBLTFTP_STATE_ERROR)
    {
        EnetAppUtils_print("[ ENET-SBL ] : File  reception over TFTP is unsuccessful\r\n");
        status = ENET_EFAIL;
    }

    return status;
}

