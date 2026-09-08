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
 * @file usb_dfu_benchmark.c
 * @brief USB DFU Firmware Download/Upload Throughput Benchmark for AM261x
 *
 * Measures DFU firmware download and upload throughput using standard DFU protocol
 * with dfu-util tool. No custom host scripts or control transfers needed.
 *
 * Usage:
 *   # Download (host → device):
 *   dfu-util -D firmware.bin
 *
 *   # Upload (device → host):
 *   dfu-util -U firmware.bin
 *
 * Device prints throughput and CPU load to UART console.
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
#include "class/dfu/dfu.h"
#include "class/dfu/dfu_device.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

typedef enum {
    DFU_OP_IDLE = 0,
    DFU_OP_DOWNLOAD,  /* host → device */
    DFU_OP_UPLOAD,    /* device → host */
} dfu_op_t;

typedef enum {
    BENCH_STATE_WAIT_ENUM = 0,
    BENCH_STATE_IDLE,
    BENCH_STATE_MEASURING,
} bench_state_t;

/* ========================================================================== */
/*                             Global State                                   */
/* ========================================================================== */

static volatile bench_state_t gBenchState       = BENCH_STATE_WAIT_ENUM;
static volatile dfu_op_t      gCurrentOp        = DFU_OP_IDLE;
static volatile uint64_t      gBytesTransferred  = 0;
static volatile uint32_t      gStartTimeMs       = 0;
static volatile bool          gTestActive        = false;

static SemaphoreP_Object gDfuTaskSem;

#define USB_TASK_PRIORITY    (TaskP_PRIORITY_HIGHEST - 2)
#define USB_TASK_STACK_SIZE  (2048U)
static uint8_t      gUsbTaskStack[USB_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gUsbTaskObj;

#define DFU_TASK_PRIORITY    (TaskP_PRIORITY_HIGHEST - 4)
#define DFU_TASK_STACK_SIZE  (2048U)
static uint8_t      gDfuTaskStack[DFU_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gDfuTaskObj;

/* ========================================================================== */
/*                           Benchmark Utilities                              */
/* ========================================================================== */

static void report_throughput(dfu_op_t op)
{
    uint32_t endTimeMs   = ClockP_getTimeUsec() / 1000;
    uint32_t durationMs  = endTimeMs - gStartTimeMs;
    float    durationSec = durationMs / 1000.0f;
    float    throughput  = 0.0f;

    if (durationSec > 0.0f)
        throughput = (float)gBytesTransferred / durationSec / (1024.0f * 1024.0f);

    uint32_t   totalLoad = TaskP_loadGetTotalCpuLoad();
    TaskP_Load usbLoad, dfuLoad;
    TaskP_loadGet(&gUsbTaskObj, &usbLoad);
    TaskP_loadGet(&gDfuTaskObj, &dfuLoad);

    const char *op_name = (op == DFU_OP_DOWNLOAD) ? "Download (Host to Device)" : "Upload (Device to Host)";

    DebugP_log("[DFU BENCHMARK] %s completed\r\n", op_name);
    DebugP_log("[DFU BENCHMARK] Throughput : %.2f MB/s (%.2f MB in %.1f sec)\r\n",
               throughput,
               (float)gBytesTransferred / (1024.0f * 1024.0f),
               durationSec);
    DebugP_log("[DFU BENCHMARK] CPU Load   : total %u.%02u%%  usb_task %u.%02u%%  dfu_task %u.%02u%%\r\n",
               totalLoad / 100U,        totalLoad % 100U,
               usbLoad.cpuLoad / 100U,  usbLoad.cpuLoad % 100U,
               dfuLoad.cpuLoad / 100U,  dfuLoad.cpuLoad % 100U);
}

/* ========================================================================== */
/*                          TinyUSB DFU Callbacks                             */
/* ========================================================================== */

/* Invoked when the device is mounted (USB enumeration complete) */
void tud_mount_cb(void)
{
    SemaphoreP_post(&gDfuTaskSem);
}

/*
 * Invoked when DFU download (DNLOAD) begins.
 * dfu-util sends firmware blocks here. Measure throughput.
 */
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    if (block_num == 0) {
        /* First block: (re)start measurement, even if a prior session was aborted */
        gTestActive = true;
        gBytesTransferred = 0;
        gStartTimeMs = ClockP_getTimeUsec() / 1000;
        gCurrentOp = DFU_OP_DOWNLOAD;
        gBenchState = BENCH_STATE_MEASURING;
        TaskP_loadResetAll();
        DebugP_log("\r\n[DFU BENCHMARK] Download started, block size: %u bytes\r\n", length);
    }

    gBytesTransferred += length;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

/*
 * Invoked during DFU manifest phase (after all DNLOAD blocks received).
 * Only triggered for DOWNLOAD operations. UPLOAD reporting happens in upload_cb.
 */
void tud_dfu_manifest_cb(uint8_t alt)
{
    /* Report download throughput (upload reports in callback, not here) */
    if (gTestActive && gCurrentOp == DFU_OP_DOWNLOAD) {
        gTestActive = false;
        report_throughput(DFU_OP_DOWNLOAD);
    }

    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

/*
 * Invoked when DFU upload (UPLOAD) begins.
 * dfu-util reads firmware blocks from here. Measure throughput.
 * For benchmark, return dummy data (repeating pattern).
 */
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
    static uint32_t upload_total = 0;

    uint32_t total_to_send = 16 * 1024 * 1024;  /* 16 MB */

    if (block_num == 0) {
        /* First block: reset state and start measurement */
        upload_total = 0;
        gTestActive = true;
        gBytesTransferred = 0;
        gStartTimeMs = ClockP_getTimeUsec() / 1000;
        gCurrentOp = DFU_OP_UPLOAD;
        gBenchState = BENCH_STATE_MEASURING;
        TaskP_loadResetAll();
        DebugP_log("\r\n[DFU BENCHMARK] Upload started (sending firmware blocks)...\r\n");
    }

    uint32_t to_send = total_to_send - upload_total;
    if (to_send >= length) {
        to_send = length;
    } else if ((to_send % 512) == 0 && (upload_total + to_send) == total_to_send) {
        /* Only force short packet on the true final block (when all bytes are accounted for) */
        to_send = to_send + 1;  /* Send extra 1 dummy byte as short packet (never 0) */
    }

    /* Generate dummy firmware data (walking byte pattern) */
    for (uint16_t i = 0; i < to_send; i++) {
        data[i] = (uint8_t)((upload_total + i) & 0xFF);
    }

    if (to_send < length)
    {
        gTestActive = false;
        report_throughput(DFU_OP_UPLOAD);
    }

    upload_total += to_send;
    gBytesTransferred += to_send;

    return to_send;
}

/*
 * Invoked to get the timeout for DFU operations.
 * Return 0 for immediate processing (benchmark doesn't need delays).
 */
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    return 0;
}

/* ========================================================================== */
/*                               Task Loops                                   */
/* ========================================================================== */

static void usb_task(void *args)
{
    (void)args;
    while (1) {
        USB_dwcTask();
        tud_task_ext(0, false);
    }
}

static void dfu_task(void *args)
{
    (void)args;
    while (1) {
        SemaphoreP_pend(&gDfuTaskSem, SystemP_WAIT_FOREVER);

        switch (gBenchState) {
            case BENCH_STATE_WAIT_ENUM:
                DebugP_log("[DFU BENCHMARK] USB enumeration complete\r\n");
                DebugP_log("[DFU BENCHMARK] Ready to receive DFU commands from host\r\n");
                DebugP_log("[DFU BENCHMARK] Example commands:\r\n");
                DebugP_log("[DFU BENCHMARK]   dfu-util -D firmware.bin    (download)\r\n");
                DebugP_log("[DFU BENCHMARK]   dfu-util -U firmware.bin    (upload)\r\n");
                gBenchState = BENCH_STATE_IDLE;
                break;

            case BENCH_STATE_IDLE:
                /* Just wait for DFU operations via callbacks */
                break;

            case BENCH_STATE_MEASURING:
                /* Measurement in progress via callbacks, just yield */
                TaskP_yield();
                break;

            default:
                break;
        }
    }
}

/* ========================================================================== */
/*                              Entry Point                                   */
/* ========================================================================== */

void usb_dfu_benchmark_main(void *args)
{
    (void)args;
    int32_t status;

    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n[DFU BENCHMARK] USB DFU Firmware Transfer Benchmark - AM261x\r\n\r\n");

    gBenchState = BENCH_STATE_WAIT_ENUM;
    SemaphoreP_constructBinary(&gDfuTaskSem, 0);

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

    TaskP_Params dfuTaskParams;
    TaskP_Params_init(&dfuTaskParams);
    dfuTaskParams.name      = "dfu_task";
    dfuTaskParams.stackSize = DFU_TASK_STACK_SIZE;
    dfuTaskParams.stack     = gDfuTaskStack;
    dfuTaskParams.priority  = DFU_TASK_PRIORITY;
    dfuTaskParams.args      = NULL;
    dfuTaskParams.taskMain  = dfu_task;
    status = TaskP_construct(&gDfuTaskObj, &dfuTaskParams);
    DebugP_assert(status == SystemP_SUCCESS);
}

/* ========================================================================== */
/*                 NCM Stubs (required by benchmark library)            */
/* ========================================================================== */

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    (void)src;
    (void)size;
    return false;
}
