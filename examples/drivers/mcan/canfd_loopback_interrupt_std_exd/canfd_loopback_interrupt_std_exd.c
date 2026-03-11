/*
 *  Copyright (C) 2025 Texas Instruments Incorporated
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

/* This example demonstrates the CAN message transmission and reception in
 * digital loop back mode for standard and extended CAN with the following 
 * configuration.
 *
 * CAN FD Message Format.
 * Message ID Type is Standard, Msg Id 0x29E.
 * MCAN is configured in Interrupt Mode.
 * Arbitration Bit Rate 1Mbps.
 * Data Bit Rate 5Mbps.
 * Buffer mode is used for Tx and RX to store message in message RAM.
 *
 * Message is transmitted and received back internally using internal loopback
 * mode. When the received message id and the data matches with the transmitted
 * one, then the example is completed.
 *
 */

#include <string.h>
#include <kernel/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/** \brief Number of messages sent */
#define MCAN_APP_TEST_MESSAGE_COUNT        (1U)
/** \brief Number of messages sent */
#define MCAN_APP_EXT_TEST_DATA_SIZE        (64U)
/** \brief Number of messages sent */
#define MCAN_APP_STD_TEST_DATA_SIZE        (8U)
/* Message object number */
#define MCAN_NUM_OF_MSG_OBJ                (2U)

static SemaphoreP_Object    gMcanTxDoneSem, gMcanRxDoneSem;
uint8_t                     rxData[MCAN_APP_EXT_TEST_DATA_SIZE] = {0};
uint8_t                     txData[128U] =
                               {0xA1, 0x1A, 0xFF, 0xFF, 0xC1, 0x1C, 0xB1, 0x1B,
                                0xA2, 0x2A, 0xFF, 0xFF, 0xC2, 0x2C, 0xB2, 0x2B,
                                0xA3, 0x3A, 0xFF, 0xFF, 0xC3, 0x3C, 0xB3, 0x3B,
                                0xA4, 0x4A, 0xFF, 0xFF, 0xC4, 0x4C, 0xB4, 0x4B,
                                0xA5, 0x5A, 0xFF, 0xFF, 0xC5, 0x5C, 0xB5, 0x5B,
                                0xA6, 0x6A, 0xFF, 0xFF, 0xC6, 0x6C, 0xB6, 0x6B,
                                0xA7, 0x7A, 0xFF, 0xFF, 0xC7, 0x7C, 0xB7, 0x7B,
                                0xA8, 0x8A, 0xFF, 0xFF, 0xC8, 0x8C, 0xB8, 0x8B,
                                0xA1, 0x1A, 0xFF, 0xFF, 0xC1, 0x1C, 0xB1, 0x1B,
                                0xA2, 0x2A, 0xFF, 0xFF, 0xC2, 0x2C, 0xB2, 0x2B,
                                0xA3, 0x3A, 0xFF, 0xFF, 0xC3, 0x3C, 0xB3, 0x3B,
                                0xA4, 0x4A, 0xFF, 0xFF, 0xC4, 0x4C, 0xB4, 0x4B,+
                                0xA5, 0x5A, 0xFF, 0xFF, 0xC5, 0x5C, 0xB5, 0x5B,
                                0xA6, 0x6A, 0xFF, 0xFF, 0xC6, 0x6C, 0xB6, 0x6B,
                                0xA7, 0x7A, 0xFF, 0xFF, 0xC7, 0x7C, 0xB7, 0x7B,
                                0xA8, 0x8A, 0xFF, 0xFF, 0xC8, 0x8C, 0xB8, 0x8B
                                };

void canfd_loopback_interrupt_std_exd_main(void *args)
{
    int32_t               status = SystemP_SUCCESS;
    uint32_t              i = 0U;
    CANFD_MessageObject   txMsgObject[MCAN_NUM_OF_MSG_OBJ];
    CANFD_MessageObject   rxMsgObject[MCAN_NUM_OF_MSG_OBJ];
    uint32_t dataSize[]  = {MCAN_APP_EXT_TEST_DATA_SIZE, 
                            MCAN_APP_STD_TEST_DATA_SIZE};
    uint32_t frameType[] = {CANFD_MCANFrameType_FD, 
                            CANFD_MCANFrameType_CLASSIC};

    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();

    status = SemaphoreP_constructBinary(&gMcanTxDoneSem, 0);
    DebugP_assert(SystemP_SUCCESS == status);
    status = SemaphoreP_constructBinary(&gMcanRxDoneSem, 0);
    DebugP_assert(SystemP_SUCCESS == status);

    DebugP_log("[MCAN] Loopback Interrupt mode for Standard and Extended CAN test application started ...\r\n");

    /* Setup the transmit message object for Extended CAN */
    txMsgObject[0].direction  = CANFD_Direction_TX;
    txMsgObject[0].msgIdType  = CANFD_MCANXidType_29_BIT;
    txMsgObject[0].startMsgId = 0x29E;
    txMsgObject[0].endMsgId   = 0x29E;
    txMsgObject[0].txMemType  = MCAN_MEM_TYPE_BUF;
    txMsgObject[0].dataLength = dataSize[0];
    txMsgObject[0].args       = NULL;
    status = CANFD_createMsgObject (gCanfdHandle[CONFIG_MCAN0], &txMsgObject[0]);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log ("Error: CANFD create Tx message object failed\r\n");
        return;
    }

    /* Setup the transmit message object for Standard CAN */
    txMsgObject[1].direction  = CANFD_Direction_TX;
    txMsgObject[1].msgIdType  = CANFD_MCANXidType_11_BIT;
    txMsgObject[1].startMsgId = 0x1C;
    txMsgObject[1].endMsgId   = 0x1C;
    txMsgObject[1].txMemType  = MCAN_MEM_TYPE_BUF;
    txMsgObject[1].dataLength = dataSize[1];
    txMsgObject[1].args       = NULL;
    status = CANFD_createMsgObject (gCanfdHandle[CONFIG_MCAN0], &txMsgObject[1]);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log ("Error: CANFD create Tx message object failed\r\n");
        return;
    }

    /* Setup the receive message object for Extended CAN */
    rxMsgObject[0].direction  = CANFD_Direction_RX;
    rxMsgObject[0].msgIdType  = CANFD_MCANXidType_29_BIT;
    rxMsgObject[0].startMsgId = 0x29E;
    rxMsgObject[0].endMsgId   = 0x29E;
    rxMsgObject[0].args       = (uint8_t*) rxData;
    rxMsgObject[0].rxMemType  = MCAN_MEM_TYPE_BUF;
    rxMsgObject[0].dataLength = dataSize[0];
    status = CANFD_createMsgObject (gCanfdHandle[CONFIG_MCAN0], &rxMsgObject[0]);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log ("Error: CANFD create Rx message object failed\r\n");
        return;
    }

    /* Setup the receive message object for Standard CAN */
    rxMsgObject[1].direction  = CANFD_Direction_RX;
    rxMsgObject[1].msgIdType  = CANFD_MCANXidType_11_BIT;
    rxMsgObject[1].startMsgId = 0x1C;
    rxMsgObject[1].endMsgId   = 0x1C;
    rxMsgObject[1].args       = (uint8_t*) rxData;
    rxMsgObject[1].rxMemType  = MCAN_MEM_TYPE_BUF;
    rxMsgObject[1].dataLength = dataSize[1];
    status = CANFD_createMsgObject (gCanfdHandle[CONFIG_MCAN0], &rxMsgObject[1]);
    if (status != SystemP_SUCCESS)
    {
        DebugP_log ("Error: CANFD create Rx message object failed\r\n");
        return;
    }

    for(i = 0U; i < MCAN_NUM_OF_MSG_OBJ; i++)
    {
        if(txMsgObject[i].msgIdType== CANFD_MCANXidType_11_BIT)
        {
            DebugP_log("MCAN Standard mode test started..\r\n");
        }
        else
        {
            DebugP_log("MCAN Extended mode test started..\r\n");
        }

        status += CANFD_read(&rxMsgObject[i], MCAN_APP_TEST_MESSAGE_COUNT, &rxData[0]);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log ("Error: CANFD read in interrupt mode failed\r\n");
            return;
        }

        /* Send data over Tx message object */
        status += CANFD_write (&txMsgObject[i],
                              txMsgObject[i].startMsgId,
                              frameType[i],
                              0,
                              &txData[0]);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log ("Error: CANFD write in interrupt mode failed\r\n");
            return;
        }

        /* Wait for Tx completion */
        SemaphoreP_pend(&gMcanTxDoneSem, SystemP_WAIT_FOREVER);
        /* Wait for Rx completion */
        SemaphoreP_pend(&gMcanRxDoneSem, SystemP_WAIT_FOREVER);

        /* Compare data */
        for(int32_t j = 0U; j < dataSize[i]; j++)
        {
            if(txData[j] != rxData[j])
            {
                status += SystemP_FAILURE;   /* Data mismatch */
                DebugP_log("Data Mismatch at offset %d\r\n", j);
                break;
            }
        }

        if(txMsgObject[i].msgIdType== CANFD_MCANXidType_11_BIT)
        {
            DebugP_log("MCAN Standard mode test passed.\r\n");
        }
        else
        {
            DebugP_log("MCAN Extended mode test passed.\r\n");
        }

        (void)memset (rxData, 0, sizeof(rxData));
    }

    for(uint32_t i = 0U; i < MCAN_NUM_OF_MSG_OBJ; i++)
    {
        status += CANFD_deleteMsgObject(&txMsgObject[i]);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log ("Error: CANFD delete Tx message object failed\r\n");
            return;
        }

        status += CANFD_deleteMsgObject(&rxMsgObject[i]);
        if (status != SystemP_SUCCESS)
        {
            DebugP_log ("Error: CANFD delete Rx message object failed\r\n");
            return;
        }
    }

    SemaphoreP_destruct(&gMcanTxDoneSem);
    SemaphoreP_destruct(&gMcanRxDoneSem);

    if (status == SystemP_SUCCESS)
    {
        DebugP_log("All tests have passed!!\r\n");
    }
    else
    {
        DebugP_log("Some tests have failed!!\r\n");
    }

    Board_driversClose();
    Drivers_close();

    return;
}

void App_CANFDTransferCallback(void *args, CANFD_Reason reason)
{
    if (reason == CANFD_Reason_TX_COMPLETION)
    {
        SemaphoreP_post((SemaphoreP_Object *)&gMcanTxDoneSem);
    }
    if (reason == CANFD_Reason_RX)
    {
        SemaphoreP_post((SemaphoreP_Object *)&gMcanRxDoneSem);
    }
}

void App_CANFDErrorCallback(void *args, CANFD_Reason reason)
{
    /* Do nothing. */
}