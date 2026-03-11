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

#include <drivers/lin.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define I2C_TARGET_ADDRESS      (0x20U)
#define I2C_POLARITY_INV        (0x5U)

#define LIN_FRAME_ID_START      (0x11U)
#define LIN_FRAME_LENGTH_INC    (0x01U)
#define LIN_FRAME_TX_DATA0      (0xA1U)

/* ========================================================================== */
/*                          Example Description                               */
/* ==========================================================================  /
Description: Example demonstrates LIN Commander mode data write operation where
             the LIN Instance LIN is set as a Commander. The massage is received
             by PLIN-USB device connected to external PC.

The LIN instance initiates the transmission by sending ID followed by message
in Blocking Mode.

Note: Example Can be run in Polling, Interrupt and DMA mode, The Operating mode
      is configurable in SYSCONFIG.
/ =========================================================================== */

void lin_commander_write_main(void)
{
    int32_t                 status;
    I2C_Handle              i2cHandle;
    I2C_Transaction         i2cTransaction;
    LIN_Handle              handle;
    LIN_SCI_Frame           frame;
    uint32_t                i = 0U;
    uint8_t                 txBuffer[1];
    uint8_t                 txData[8] __attribute__((aligned(CacheP_CACHELINE_ALIGNMENT)));

    /* Fill Buffer */
    for (uint8_t i = 0; i < 8; i++)
    {
        txData[i] =  LIN_FRAME_TX_DATA0 + i;
    }

    Drivers_open();
    Board_driversOpen();

    i2cHandle = gI2cHandle[CONFIG_I2C0];
    handle = gLinHandle[CONFIG_LIN0];

    DebugP_log("[LIN] LIN Commander Write Application Started ...\r\n");

    I2C_Transaction_init(&i2cTransaction);
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1U;
    i2cTransaction.targetAddress = I2C_TARGET_ADDRESS;
    txBuffer[0] = I2C_POLARITY_INV;
    status = I2C_transfer(i2cHandle, &i2cTransaction);
    DebugP_assert(status == SystemP_SUCCESS);

    DebugP_log("[I2C] LIN Voltage Level Shifter started ...\r\n");

    DebugP_log("[LIN] Commander Write ... !!!\r\n");

    for (i = 0U; i < 8U; i++)
    {
        LIN_SCI_Frame_init(&frame);

        frame.id = LIN_FRAME_ID_START + i;
        frame.frameLen = i + LIN_FRAME_LENGTH_INC;
        frame.dataBuf = txData;

        status = LIN_SCI_transferFrame(handle, &frame);
        DebugP_assert(status == SystemP_SUCCESS);
    }

    DebugP_log("All tests have passed!!\r\n");

    Board_driversClose();
    Drivers_close();
}