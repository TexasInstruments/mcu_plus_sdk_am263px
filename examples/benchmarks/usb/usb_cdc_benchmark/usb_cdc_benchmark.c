/*
 * Copyright (C) 2026 Texas Instruments Incorporated
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file usb_cdc_benchmark.c
 * @brief USB CDC-ACM Throughput Benchmark for AM261x
 *
 * Measures CDC-ACM serial throughput using event-driven callbacks to wake
 * the cdc_task, which performs the actual I/O. This avoids interrupt-context
 * I/O contention while maintaining event-driven pipelining.
 *
 * The host controls the test via low-level USB control transfers:
 *   Vendor OUT 0x01 (wValue=0x01) -> TX mode: device continuously sends
 *   Vendor OUT 0x01 (wValue=0x02) -> RX mode: device reads and discards
 *   Vendor OUT 0x02               -> idle / stop current test
 *
 * Run usb_cdc_benchmark_host.py on the host PC to initiate tests.
 */

#include <string.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <usb/synp/soc/usb_init.h>
#include "tusb.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* TX/RX chunk size matched to CFG_TUD_CDC_EP_BUFSIZE for USB HS */
#define CDC_CHUNK_SIZE  (4096U)

/* Vendor control request codes (must match usb_cdc_benchmark_host.py) */
#define VENDOR_REQUEST_START_TEST  0x01U
#define VENDOR_REQUEST_STOP_TEST   0x02U

/* wValue for VENDOR_REQUEST_START_TEST */
#define TEST_MODE_TX  0x01U   /* device → host */
#define TEST_MODE_RX  0x02U   /* host  → device */

typedef enum {
    CDC_MODE_IDLE = 0,
    CDC_MODE_TX,   /* device → host */
    CDC_MODE_RX,   /* host  → device */
} cdc_mode_t;

typedef enum {
    BENCH_STATE_WAIT_ENUM = 0,
    BENCH_STATE_IDLE,
    BENCH_STATE_RUNNING,
} bench_state_t;

/* ========================================================================== */
/*                             Global State                                   */
/* ========================================================================== */

static uint8_t gTxBuf[CDC_CHUNK_SIZE];
static uint8_t gRxBuf[CDC_CHUNK_SIZE];

static volatile cdc_mode_t    gCurrentMode      = CDC_MODE_IDLE;
static volatile bench_state_t gBenchState       = BENCH_STATE_WAIT_ENUM;
static volatile uint64_t      gBytesTransferred  = 0;
static          uint32_t      gStartTimeMs       = 0;

static SemaphoreP_Object gCdcTaskSem;

#define USB_TASK_PRIORITY    (TaskP_PRIORITY_HIGHEST - 2)
#define USB_TASK_STACK_SIZE  (2048U)
static uint8_t      gUsbTaskStack[USB_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gUsbTaskObj;

#define CDC_TASK_PRIORITY    (TaskP_PRIORITY_HIGHEST - 4)
#define CDC_TASK_STACK_SIZE  (2048U)
static uint8_t      gCdcTaskStack[CDC_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gCdcTaskObj;

/* ========================================================================== */
/*                     Benchmark State Transitions                             */
/* ========================================================================== */

static void start_test(cdc_mode_t mode)
{
    static const char * const mode_names[] = {
        [CDC_MODE_TX] = "Device-to-Host (TX)",
        [CDC_MODE_RX] = "Host-to-Device (RX)",
    };

    DebugP_log("\r\n[CDC BENCHMARK] Starting Test - Mode: %s\r\n", mode_names[mode]);

    gBytesTransferred = 0;
    gStartTimeMs      = ClockP_getTimeUsec() / 1000;
    gCurrentMode      = mode;
    gBenchState       = BENCH_STATE_RUNNING;

    /* Reset CPU load counters so measurement covers only this test window */
    TaskP_loadResetAll();

    /* Wake cdc_task to start the transfer loop */
    SemaphoreP_post(&gCdcTaskSem);
}

static void stop_test(void)
{
    if (gBenchState != BENCH_STATE_RUNNING) return;
    gBenchState = BENCH_STATE_IDLE;

    uint32_t endTimeMs   = ClockP_getTimeUsec() / 1000;
    uint32_t durationMs  = endTimeMs - gStartTimeMs;
    float    durationSec = durationMs / 1000.0f;
    float    throughput  = 0.0f;

    if (durationSec > 0.0f)
        throughput = (float)gBytesTransferred / durationSec / (1024.0f * 1024.0f);

    uint32_t   totalLoad = TaskP_loadGetTotalCpuLoad();
    TaskP_Load usbLoad, cdcLoad;
    TaskP_loadGet(&gUsbTaskObj, &usbLoad);
    TaskP_loadGet(&gCdcTaskObj, &cdcLoad);

    DebugP_log("[CDC BENCHMARK] Throughput : %.2f MB/s (%.2f MB in %.1f sec)\r\n",
               throughput,
               (float)gBytesTransferred / (1024.0f * 1024.0f),
               durationSec);
    DebugP_log("[CDC BENCHMARK] CPU Load   : total %u.%02u%%  usb_task %u.%02u%%  cdc_task %u.%02u%%\r\n",
               totalLoad / 100U,        totalLoad % 100U,
               usbLoad.cpuLoad / 100U,  usbLoad.cpuLoad % 100U,
               cdcLoad.cpuLoad / 100U,  cdcLoad.cpuLoad % 100U);
}

/* ========================================================================== */
/*                          TinyUSB Callbacks                                 */
/* ========================================================================== */

/* Invoked when the device is mounted (USB enumeration complete) */
void tud_mount_cb(void)
{
    SemaphoreP_post(&gCdcTaskSem);
}

/*
 * Invoked when TX completes (previous write has been sent).
 * Signal cdc_task to queue the next write.
 * Runs in interrupt context — only post semaphore, no I/O here.
 */
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    if (itf == 0 && gCurrentMode == CDC_MODE_TX)
        SemaphoreP_post(&gCdcTaskSem);
}

/*
 * Invoked when RX data arrives.
 * Signal cdc_task to read the available data.
 * Runs in interrupt context — only post semaphore, no I/O here.
 */
void tud_cdc_rx_cb(uint8_t itf)
{
    if (itf == 0 && gCurrentMode == CDC_MODE_RX)
        SemaphoreP_post(&gCdcTaskSem);
}

/*
 * Invoked for vendor-class control requests on EP0.
 * The host sends START_TEST / STOP_TEST via pyusb ctrl_transfer() to keep
 * test control on a dedicated channel, free of serial line state glitches.
 */
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                  tusb_control_request_t const *request)
{
    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR)
        return false;

    if (stage == CONTROL_STAGE_SETUP)
        return tud_control_status(rhport, request);

    if (stage == CONTROL_STAGE_ACK) {
        switch (request->bRequest) {
            case VENDOR_REQUEST_START_TEST:
                if      (request->wValue == TEST_MODE_TX) start_test(CDC_MODE_TX);
                else if (request->wValue == TEST_MODE_RX) start_test(CDC_MODE_RX);
                break;
            case VENDOR_REQUEST_STOP_TEST:
                gCurrentMode = CDC_MODE_IDLE;
                break;
            default:
                break;
        }
    }
    return true;
}

/* ========================================================================== */
/*                               Task Loops                                   */
/* ========================================================================== */

static void usb_task(void *args)
{
    (void)args;
    while (1) {
        USB_dwcTask();           /* Synopsys DWC3 driver task - blocks until USB interrupt */
        tud_task_ext(0, false);  /* TinyUSB device task - process pending USB events */
    }
}

static void cdc_task(void *args)
{
    (void)args;
    while (1) {
        /* Block until tud_mount_cb(), callback, or start_test() signals an action */
        SemaphoreP_pend(&gCdcTaskSem, SystemP_WAIT_FOREVER);

        switch (gBenchState) {
            case BENCH_STATE_WAIT_ENUM:
                DebugP_log("[CDC BENCHMARK] USB enumeration complete\r\n");
                DebugP_log("[CDC BENCHMARK] Waiting for host to initiate test...\r\n");
                DebugP_log("[CDC BENCHMARK]   python3 usb_cdc_benchmark_host.py --mode tx\r\n");
                DebugP_log("[CDC BENCHMARK]   python3 usb_cdc_benchmark_host.py --mode rx\r\n");
                gBenchState = BENCH_STATE_IDLE;
                break;

            case BENCH_STATE_RUNNING:
                /* Tight loop: callbacks post semaphore to wake us for I/O.
                 * No polling, no yield — just process each event as callbacks signal. */
                while (gCurrentMode != CDC_MODE_IDLE) {
                    if (gCurrentMode == CDC_MODE_TX) {
                        uint32_t avail = tud_cdc_n_write_available(0);
                        if (avail > 0) {
                            uint32_t n       = (avail < CDC_CHUNK_SIZE) ? avail : CDC_CHUNK_SIZE;
                            uint32_t written = tud_cdc_n_write(0, gTxBuf, n);
                            tud_cdc_n_write_flush(0);
                            gBytesTransferred += written;
                        } else {
                            /* FIFO full, wait for tx_complete_cb to post semaphore */
                            SemaphoreP_pend(&gCdcTaskSem, SystemP_WAIT_FOREVER);
                        }
                    } else if (gCurrentMode == CDC_MODE_RX) {
                        uint32_t avail = tud_cdc_n_available(0);
                        if (avail > 0) {
                            uint32_t rd = tud_cdc_n_read(0, gRxBuf,
                                          (avail < CDC_CHUNK_SIZE) ? avail : CDC_CHUNK_SIZE);
                            gBytesTransferred += rd;
                        } else {
                            /* No data, wait for rx_cb to post semaphore */
                            SemaphoreP_pend(&gCdcTaskSem, SystemP_WAIT_FOREVER);
                        }
                    }
                }
                stop_test();
                break;

            default:
                break;
        }
    }
}

/* ========================================================================== */
/*                              Entry Point                                   */
/* ========================================================================== */

void usb_cdc_benchmark_main(void *args)
{
    (void)args;
    int32_t status;

    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n[CDC BENCHMARK] USB CDC-ACM Throughput Benchmark - AM261x\r\n\r\n");

    /* Pre-fill TX buffer with a walking byte pattern */
    for (uint32_t i = 0; i < CDC_CHUNK_SIZE; i++)
        gTxBuf[i] = (uint8_t)(i & 0xFFU);

    gBenchState = BENCH_STATE_WAIT_ENUM;
    SemaphoreP_constructBinary(&gCdcTaskSem, 0);

    TaskP_Params usbTaskParams;
    TaskP_Params_init(&usbTaskParams);
    usbTaskParams.name      = "usb_task";
    usbTaskParams.stackSize = USB_TASK_STACK_SIZE;
    usbTaskParams.stack     = gUsbTaskStack;
    usbTaskParams.priority  = USB_TASK_PRIORITY;
    usbTaskParams.args      = NULL;
    usbTaskParams.taskMain  = usb_task;
    status = TaskP_construct(&gUsbTaskObj, &usbTaskParams);
    DebugP_assert(status == SystemP_SUCCESS);

    TaskP_Params cdcTaskParams;
    TaskP_Params_init(&cdcTaskParams);
    cdcTaskParams.name      = "cdc_task";
    cdcTaskParams.stackSize = CDC_TASK_STACK_SIZE;
    cdcTaskParams.stack     = gCdcTaskStack;
    cdcTaskParams.priority  = CDC_TASK_PRIORITY;
    cdcTaskParams.args      = NULL;
    cdcTaskParams.taskMain  = cdc_task;
    status = TaskP_construct(&gCdcTaskObj, &cdcTaskParams);
    DebugP_assert(status == SystemP_SUCCESS);
}


/* ========================================================================== */
/*                 DFU & NCM Stubs (required by benchmark library)            */
/* ========================================================================== */

/* These callbacks are required by the benchmark library but not used in NCM */
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    (void)alt;
    (void)block_num;
    (void)data;
    (void)length;
}

void tud_dfu_manifest_cb(uint8_t alt)
{
    (void)alt;
}

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    (void)alt;
    (void)state;
    return 0;
}

uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
    (void)alt;
    (void)block_num;
    (void)data;
    (void)length;
    return 0;
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    (void)src;
    (void)size;
    return false;
}
