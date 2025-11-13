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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#define APP_OSPI_DATA_SIZE (1024*4)   /* Data transfer buffer size */
#define APP_OSPI_OFFSET    (0)        /* PSRAM Write offset*/

uint8_t gOspiTxBuf[APP_OSPI_DATA_SIZE];
/* read buffer MUST be cache line aligned when using DMA, we aligned to 128B though 32B is enough */
uint8_t gOspiRxBuf[APP_OSPI_DATA_SIZE] __attribute__((aligned(128U)));

static void ospi_psram_io_fill_buffers(void);

void ospi_psram_io_main(void *args)
{
    /* Open drivers to open the UART driver for console */
    int32_t status = SystemP_SUCCESS;
    uint32_t offset = APP_OSPI_OFFSET;
    uint64_t startTime, endTime, duration;

    /* Write & read speed in Mbps(Megabits per second)*/
    float writeSpeed = 0;
    float readSpeed = 0;
    
    Drivers_open();
    status  = Board_driversOpen();
    DebugP_assert(status == SystemP_SUCCESS);

    DebugP_log("OSPI Psram W/R test\r\n");

    ospi_psram_io_fill_buffers();

    startTime = ClockP_getTimeUsec();
    status = Ram_write(gRamHandle[CONFIG_RAM0], offset, gOspiTxBuf, APP_OSPI_DATA_SIZE);
    endTime = ClockP_getTimeUsec();
    DebugP_assert(status == SystemP_SUCCESS);

    duration = endTime - startTime;
    writeSpeed = ((float)APP_OSPI_DATA_SIZE * 8U)/(duration);

    startTime = ClockP_getTimeUsec();
    status = Ram_read(gRamHandle[CONFIG_RAM0], offset, gOspiRxBuf, APP_OSPI_DATA_SIZE);
    endTime = ClockP_getTimeUsec();
    DebugP_assert(status == SystemP_SUCCESS);

    duration = endTime - startTime;
    readSpeed = ((float)APP_OSPI_DATA_SIZE * 8U )/(duration);

    DebugP_assert(status == SystemP_SUCCESS);

    status = memcmp(gOspiTxBuf,gOspiRxBuf,APP_OSPI_DATA_SIZE);
    DebugP_assert(status == SystemP_SUCCESS);

    DebugP_log("Write Speed: %f Mbps\r\nRead Speed: %f Mbps\r\n",writeSpeed,readSpeed);
    DebugP_log("All Tests Passed !!!!\r\n");

    Board_driversClose();
    Drivers_close();
}

static void ospi_psram_io_fill_buffers(void)
{
    srand(time(0));

    for(uint32_t i = 0U; i < APP_OSPI_DATA_SIZE; i++)
    {
        uint8_t val = rand()%(APP_OSPI_DATA_SIZE - i + 1);
        gOspiTxBuf[i] = val%256;
        gOspiRxBuf[i] = 0U;
    }
}