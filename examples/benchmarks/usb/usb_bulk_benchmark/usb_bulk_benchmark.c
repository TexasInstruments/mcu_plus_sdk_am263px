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
 * @file usb_bulk_benchmark.c
 * @brief USB Bulk Transfer Benchmark for AM261x
 * 
 * This benchmark measures USB High-Speed bulk transfer performance and provides
 * a standardized score for comparing USB implementations across different platforms.
 * 
 * Run the usb_bulk_benchmark_host.py script on the host PC to initiate tests and collect results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/TaskP.h>
#include <kernel/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/* USB includes */
#include <usb/synp/soc/usb_init.h>
#include "tusb.h"
#include "device/usbd_pvt.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* Vendor-specific control requests for commands (separate from bulk data) */
#define VENDOR_REQUEST_START_TEST   0x01
#define VENDOR_REQUEST_STOP_TEST    0x02
#define VENDOR_REQUEST_GET_STATUS   0x03

#define BULK_BUFFER_SIZE            (16384U)  /* 16KB buffers for optimal performance */

/* Test modes — values double as the wValue field in the START_TEST control request */
typedef enum {
    TEST_MODE_DEVICE_TO_HOST = 0x01,  /* IN transfers  (device → host) */
    TEST_MODE_HOST_TO_DEVICE = 0x02,  /* OUT transfers (host → device) */
    TEST_MODE_BIDIRECTIONAL  = 0x03,  /* Sequential TX then RX          */
} test_mode_t;

/* Benchmark state machine */
typedef enum {
    BENCH_STATE_WAIT_ENUM = 0,
    BENCH_STATE_IDLE,
    BENCH_STATE_RUNNING,
    BENCH_STATE_ERROR
} bench_state_t;

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

typedef struct {
    uint64_t bytes_transferred;
    uint32_t packets_transferred;
    uint32_t start_time_ms;
    float throughput_mbps;
} test_result_t;

typedef struct {
    bench_state_t state;
    test_mode_t current_mode;
    bool test_active;
} benchmark_context_t;

static uint8_t txBuffer[BULK_BUFFER_SIZE] __attribute__((aligned(64)));
static uint8_t rxBuffer[BULK_BUFFER_SIZE] __attribute__((aligned(64)));

/* Benchmark context */
static benchmark_context_t benchmark = {0};

/* Current test tracking */
static test_result_t currentTest = {0};

/* Semaphore to wake bulk_task on USB events (mount, test start) */
static SemaphoreP_Object gBulkTaskSem;

/* Task objects */
/* Task priorities - USB task must be higher to service stack continuously */
#define USB_TASK_PRIORITY      (TaskP_PRIORITY_HIGHEST-2)
#define USB_TASK_STACK_SIZE    (2048U)
uint8_t gUsbTaskStack[USB_TASK_STACK_SIZE] __attribute__((aligned(32)));
TaskP_Object gUsbTaskObj;

#define BULK_TASK_PRIORITY   (TaskP_PRIORITY_HIGHEST-4)
#define BULK_TASK_STACK_SIZE (2048U)
uint8_t gBulkTaskStack[BULK_TASK_STACK_SIZE] __attribute__((aligned(32)));
TaskP_Object gBulkTaskObj;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

static void init_test_buffers(void);
static void start_test(test_mode_t mode);
static void stop_test(void);
static void kick_bulk_in(void);
static void kick_bulk_out(void);
static void usb_task(void *args);
static void bulk_task(void *args);

/* ========================================================================== */
/*                        Custom Raw Bulk Class Driver                        */
/* ========================================================================== */

/* Claims the vendor-specific interface directly via usbd_app_driver_get_cb()
 * and drives 16KB DMA transfers with usbd_edpt_xfer(). One DWC3 completion
 * interrupt fires per 16KB chunk (zero-copy, no FIFO staging). */

/* USB endpoint addresses (0x01 = EP1 OUT, 0x81 = EP1 IN).
 * Populated from the USB descriptor by usbd_open_edpt_pair() in bulkbenchd_open(). */
static uint8_t gBulkEpOut = 0;
static uint8_t gBulkEpIn  = 0;

static void bulkbenchd_init(void)
{
}

static bool bulkbenchd_deinit(void)
{
    return true;
}

static void bulkbenchd_reset(uint8_t rhport)
{
    (void)rhport;
    gBulkEpOut = 0;
    gBulkEpIn = 0;
}

static uint16_t bulkbenchd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
    TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass, 0);

    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) + 2U * sizeof(tusb_desc_endpoint_t));
    TU_VERIFY(max_len >= drv_len, 0);

    TU_ASSERT(usbd_open_edpt_pair(rhport, tu_desc_next(itf_desc), 2, TUSB_XFER_BULK,
                                   &gBulkEpOut, &gBulkEpIn), 0);

    return drv_len;
}

/* Standard (non-vendor-type) control requests targeting this interface (e.g.
 * GET_INTERFACE/SET_INTERFACE) are forwarded here unconditionally by usbd - return
 * false so usbd falls back to its generic handling. Vendor-type requests never reach
 * this callback; they are routed directly to tud_vendor_control_xfer_cb() by usbd. */
static bool bulkbenchd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    (void)rhport; (void)stage; (void)request;
    return false;
}

static bool bulkbenchd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)rhport;

    if (result != XFER_RESULT_SUCCESS) {
        DebugP_log("[BULK BENCHMARK] Transfer error: ep=0x%02X result=%d\r\n", ep_addr, result);
        return true;
    }

    if (ep_addr == gBulkEpIn) {
        currentTest.bytes_transferred += xferred_bytes;
        currentTest.packets_transferred++;

        if (benchmark.test_active) {
            if (benchmark.current_mode == TEST_MODE_DEVICE_TO_HOST) {
                kick_bulk_in();
            } else if (benchmark.current_mode == TEST_MODE_BIDIRECTIONAL) {
                kick_bulk_out();
            }
        }
    } else if (ep_addr == gBulkEpOut) {
        currentTest.bytes_transferred += xferred_bytes;
        currentTest.packets_transferred++;

        if (benchmark.test_active) {
            if (benchmark.current_mode == TEST_MODE_HOST_TO_DEVICE) {
                kick_bulk_out();
            } else if (benchmark.current_mode == TEST_MODE_BIDIRECTIONAL) {
                kick_bulk_in();
            }
        }
    }

    return true;
}

static usbd_class_driver_t const gBulkBenchDriver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "BULKBENCH",
#endif
    .init            = bulkbenchd_init,
    .deinit          = bulkbenchd_deinit,
    .reset           = bulkbenchd_reset,
    .open            = bulkbenchd_open,
    .control_xfer_cb = bulkbenchd_control_xfer_cb,
    .xfer_cb         = bulkbenchd_xfer_cb,
    .xfer_isr        = NULL,
    .sof             = NULL,
};

/* Invoked by TinyUSB to collect application class drivers. Registered before
 * built-in classes so this driver claims the vendor interface first. */
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &gBulkBenchDriver;
}

/* Invoked by TinyUSB when the host sets the device configuration (enumeration complete) */
void tud_mount_cb(void)
{
    SemaphoreP_post(&gBulkTaskSem);
}

/**
 * @brief Handle vendor-specific control requests
 *
 * This callback handles custom control requests for test commands.
 * Using control transfers keeps commands separate from bulk data,
 * ensuring precise timing for benchmark measurements.
 */
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
    (void)rhport;
    
    /* Handle only SETUP stage */
    if (stage != CONTROL_STAGE_SETUP) return true;
    
    /* Only handle vendor-specific requests */
    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR) return false;
    
    switch (request->bRequest) {
        case VENDOR_REQUEST_START_TEST:
        {
            /* wValue contains test mode */
            test_mode_t mode = (test_mode_t)request->wValue;
            if (mode < TEST_MODE_DEVICE_TO_HOST || mode > TEST_MODE_BIDIRECTIONAL) {
                return false;  /* Invalid mode */
            }
            
            /* Start the test */
            if (!benchmark.test_active) {
                start_test(mode);
            }
            
            /* Send zero-length status packet */
            return tud_control_status(rhport, request);
        }
        
        case VENDOR_REQUEST_STOP_TEST:
        {
            /* Stop the test immediately for precise timing */
            if (benchmark.test_active) {
                stop_test();
            }
            
            /* Send zero-length status packet */
            return tud_control_status(rhport, request);
        }
        
        case VENDOR_REQUEST_GET_STATUS:
        {
            /* Return current test status */
            uint8_t status = benchmark.test_active ? 1 : 0;
            return tud_control_xfer(rhport, request, &status, 1);
        }
        
        default:
            /* Unsupported request */
            return false;
    }
}

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

static void init_test_buffers(void)
{
    for (uint32_t i = 0; i < BULK_BUFFER_SIZE; i++) {
        txBuffer[i] = (uint8_t)(i & 0xFF);
    }
    memset(rxBuffer, 0, BULK_BUFFER_SIZE);
}

/* Set by start_test() (called from the EP0 SETUP callback) and consumed by
 * bulk_task(). The initial usbd_edpt_xfer() kicks must NOT be issued from
 * within that callback - submitting bulk transfers while EP0 hasn't reached
 * its STATUS phase yet wedges the controller. Deferring to bulk_task avoids
 * this. */
static volatile bool gPendingKickOff = false;

static void start_test(test_mode_t mode)
{
    static const char* const mode_names[] = {
        [TEST_MODE_DEVICE_TO_HOST] = "Device-to-Host (IN)",
        [TEST_MODE_HOST_TO_DEVICE] = "Host-to-Device (OUT)",
        [TEST_MODE_BIDIRECTIONAL]  = "Bidirectional",
    };

    /* Validate mode is within bounds */
    if (mode < TEST_MODE_DEVICE_TO_HOST || mode > TEST_MODE_BIDIRECTIONAL) {
        DebugP_log("\r\n[BULK BENCHMARK] ERROR: Invalid test mode %u\r\n", mode);
        return;
    }

    DebugP_log("\r\n[BULK BENCHMARK] Starting Test - Mode: %s\r\n", mode_names[mode]);

    /* Reset current test tracking */
    memset(&currentTest, 0, sizeof(currentTest));
    currentTest.start_time_ms = ClockP_getTimeUsec() / 1000;

    /* Update benchmark state */
    benchmark.test_active = true;
    benchmark.current_mode = mode;
    benchmark.state = BENCH_STATE_RUNNING;
    gPendingKickOff = true;

    /* Reset CPU load counters so measurement covers only this test window */
    TaskP_loadResetAll();

    /* Wake bulk_task to issue the initial kick */
    SemaphoreP_post(&gBulkTaskSem);
}

static void stop_test(void)
{
    if (!benchmark.test_active) return;
    
    benchmark.test_active = false;
    benchmark.state = BENCH_STATE_IDLE;
    
    /* Calculate final duration */
    uint32_t end_time_ms = ClockP_getTimeUsec() / 1000;
    uint32_t duration_ms = end_time_ms - currentTest.start_time_ms;
    
    /* Calculate final throughput */
    float duration_sec = duration_ms / 1000.0f;
    if (duration_sec > 0.0f) {
        currentTest.throughput_mbps = (float)currentTest.bytes_transferred /
                                         duration_sec / (1024.0f * 1024.0f);
    }
    
    /* Read CPU load for the test window */
    uint32_t totalCpuLoad = TaskP_loadGetTotalCpuLoad();
    TaskP_Load usbLoad, bulkLoad;
    TaskP_loadGet(&gUsbTaskObj, &usbLoad);
    TaskP_loadGet(&gBulkTaskObj, &bulkLoad);

    DebugP_log("[BULK BENCHMARK] Throughput : %.2f MB/s (%.2f MB in %.1f sec)\r\n",
               currentTest.throughput_mbps,
               (float)currentTest.bytes_transferred / (1024.0f * 1024.0f),
               duration_sec);
    DebugP_log("[BULK BENCHMARK] CPU Load   : total %u.%02u%%  usb_task %u.%02u%%  bulk_task %u.%02u%%\r\n",
               totalCpuLoad / 100U, totalCpuLoad % 100U,
               usbLoad.cpuLoad / 100U, usbLoad.cpuLoad % 100U,
               bulkLoad.cpuLoad / 100U, bulkLoad.cpuLoad % 100U);
}

static void kick_bulk_out(void)
{
    usbd_edpt_xfer(0, gBulkEpOut, rxBuffer, BULK_BUFFER_SIZE);
}

static void kick_bulk_in(void)
{
    usbd_edpt_xfer(0, gBulkEpIn, txBuffer, BULK_BUFFER_SIZE);
}

static void usb_task(void *args)
{
    (void)args;
        
    while (1) {
        USB_dwcTask();           /* Synopsys DWC3 driver task - MUST run continuously */
        tud_task_ext(0, false);  /* TinyUSB device task - MUST run continuously */
        /* NO TaskP_yield() here - USB stack needs continuous servicing */
    }
}

static void bulk_task(void *args)
{
    (void)args;

    while (1) {
        /* Block until tud_mount_cb() or start_test() signals an action is needed */
        SemaphoreP_pend(&gBulkTaskSem, SystemP_WAIT_FOREVER);

        switch (benchmark.state) {
            case BENCH_STATE_WAIT_ENUM:
                DebugP_log("[BULK BENCHMARK] USB enumeration complete\r\n");
                DebugP_log("[BULK BENCHMARK] Waiting for host to initiate tests...\r\n");
                DebugP_log("[BULK BENCHMARK]   python3 tools/usb_test_scripts/usb_bulk_benchmark_host.py --mode in --duration 10\r\n");
                DebugP_log("[BULK BENCHMARK]   python3 tools/usb_test_scripts/usb_bulk_benchmark_host.py --mode out --duration 10\r\n");
                DebugP_log("[BULK BENCHMARK]   python3 tools/usb_test_scripts/usb_bulk_benchmark_host.py --mode bidir --duration 10\r\n");
                benchmark.state = BENCH_STATE_IDLE;
                break;

            case BENCH_STATE_RUNNING:
                /* One-shot initial kick; all subsequent re-arming is done in bulkbenchd_xfer_cb() */
                if (gPendingKickOff) {
                    gPendingKickOff = false;
                    if (benchmark.current_mode == TEST_MODE_DEVICE_TO_HOST) {
                        kick_bulk_in();
                    } else if (benchmark.current_mode == TEST_MODE_HOST_TO_DEVICE) {
                        kick_bulk_out();
                    } else if (benchmark.current_mode == TEST_MODE_BIDIRECTIONAL) {
                        kick_bulk_in();
                    }
                }
                break;

            case BENCH_STATE_ERROR:
                DebugP_log("[BULK BENCHMARK] Error state - halting\r\n");
                while(1) {
                    TaskP_yield();
                }
                break;

            default:
                break;
        }
    }
}

void usb_bulk_benchmark_main(void *args)
{
    (void)args;
    int32_t status;
    
    /* Initialize drivers */
    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n[BULK BENCHMARK] USB Bulk Transfer Benchmark - AM261x\r\n\r\n");
        
    /* Initialize test buffers */
    init_test_buffers();
    benchmark.state = BENCH_STATE_WAIT_ENUM;
    SemaphoreP_constructBinary(&gBulkTaskSem, 0);

    /* Create USB task */
    TaskP_Params usbTaskParams;
    TaskP_Params_init(&usbTaskParams);
    usbTaskParams.name = "usb_task";
    usbTaskParams.stackSize = USB_TASK_STACK_SIZE;
    usbTaskParams.stack = gUsbTaskStack;
    usbTaskParams.priority = USB_TASK_PRIORITY;
    usbTaskParams.args = NULL;
    usbTaskParams.taskMain = usb_task;

    status = TaskP_construct(&gUsbTaskObj, &usbTaskParams);
    DebugP_assert(status == SystemP_SUCCESS);

    /* Create bulk task */
    TaskP_Params bulkTaskParams;
    TaskP_Params_init(&bulkTaskParams);
    bulkTaskParams.name = "bulk_task";
    bulkTaskParams.stackSize = BULK_TASK_STACK_SIZE;
    bulkTaskParams.stack = gBulkTaskStack;
    bulkTaskParams.priority = BULK_TASK_PRIORITY;
    bulkTaskParams.args = NULL;
    bulkTaskParams.taskMain = bulk_task;

    status = TaskP_construct(&gBulkTaskObj, &bulkTaskParams);
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
