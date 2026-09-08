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
 * @file usb_ncm_benchmark.c
 * @brief USB NCM Network Interface Throughput Benchmark for AM261x
 *
 * Measures NCM (Network Control Model) throughput using lwIP stack with iperf.
 * Supports ping testing and throughput measurement.
 *
 * Device IP configured in ipaddr variable (see network parameters)
 * Host setup:
 *   - usb_ncm interface will get IP automatically (via arp/dhcp)
 *   - Or manually configure with device IP subnet
 *
 * Usage:
 *   # Test connectivity (ICMP echo)
 *   ping <device_ip>
 *
 *   # Measure throughput with iperf
 *   iperf -c <device_ip> -t 10
 *
 * Device prints network stats and iperf results to UART console.
 */

#include <ctype.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/EventP.h>
#include <kernel/dpl/QueueP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/TaskP.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ti_board_open_close.h"
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "tusb.h"

#include "dhserver.h"
#include "dnserver.h"
#include "httpd.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/ethip6.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"

/* ========================================================================== */
/*                           Task Definitions                                 */
/* ========================================================================== */

#define USB_TASK_PRIORITY       (TaskP_PRIORITY_HIGHEST - 2)
#define USB_TASK_STACK_SIZE     (2048U)
static uint8_t      gUsbTaskStack[USB_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gUsbTaskObj;

#define NETWORK_TASK_PRIORITY   (TaskP_PRIORITY_HIGHEST - 4)
#define NETWORK_TASK_STACK_SIZE (2048U)
static uint8_t      gNetworkTaskStack[NETWORK_TASK_STACK_SIZE] __attribute__((aligned(32)));
static TaskP_Object gNetworkTaskObj;

/* Event bits for network processing */
#define NETWORK_EVENT_TX_BIT        (1 << 0)  /* Bit 0: TX data ready */
#define NETWORK_EVENT_TX_RETRY_BIT  (1 << 1)  /* Bit 1: TX retry */

static EventP_Object gNetworkEventObj;

#define TX_QUEUE_SIZE 64

typedef struct
{
    QueueP_Elem elem;
    struct pbuf * pbuf;
}ncmQueueElem;

static QueueP_Object   freeQObjectTx;
static QueueP_Object   readyQObjectTx;
static QueueP_Handle   freeQHandleTx;
static QueueP_Handle   readyQHandleTx;

static SemaphoreP_Object txQueueMutex;
static SemaphoreP_Object initNetwork;

static ncmQueueElem ncmTxQueueBuffer[TX_QUEUE_SIZE] = {0};
static ncmQueueElem *pendingElem = NULL;

/* lwIP context */
static struct netif netif_data;

/* MAC address for virtual NCM interface - defined in ti_usb_descriptor.c */
extern uint8_t tud_network_mac_address[6];

/* Network parameters */
static const ip4_addr_t ipaddr = { .addr = PP_HTONL(LWIP_MAKEU32(192, 168, 7, 1)) };
static const ip4_addr_t netmask = { .addr = PP_HTONL(LWIP_MAKEU32(255, 255, 255, 0)) };
static const ip4_addr_t gateway = { .addr = PP_HTONL(LWIP_MAKEU32(0, 0, 0, 0)) };

/* DHCP entries */
static dhcp_entry_t entries[] = {
    {{0}, { .addr = PP_HTONL(LWIP_MAKEU32(192, 168, 7, 2)) }, 24 * 60 * 60},
    {{0}, { .addr = PP_HTONL(LWIP_MAKEU32(192, 168, 7, 3)) }, 24 * 60 * 60},
    {{0}, { .addr = PP_HTONL(LWIP_MAKEU32(192, 168, 7, 4)) }, 24 * 60 * 60},
};

static const dhcp_config_t dhcp_config = {
    .router = { .addr = PP_HTONL(LWIP_MAKEU32(0, 0, 0, 0)) },
    .port = 67,
    .dns = { .addr = PP_HTONL(LWIP_MAKEU32(192, 168, 7, 1)) },
    "usb",
    TU_ARRAY_SIZE(entries),
    entries
};

/* ========================================================================== */
/*                          lwIP Callbacks                                    */
/* ========================================================================== */

/**
 * iperf performance report callback - called when test completes
 */
static void lwiperf_report_callback(void *arg, enum lwiperf_report_type report_type,
                                   const ip_addr_t *local_addr, u16_t local_port,
                                   const ip_addr_t *remote_addr, u16_t remote_port,
                                   u32_t bytes_transferred, u32_t ms_duration,
                                   u32_t bandwidth_kbitpsec)
{
    (void)arg;
    (void)local_addr;
    (void)local_port;

    const char *test_type = "unknown";
    switch (report_type) {
        case LWIPERF_TCP_DONE_SERVER:
            test_type = "TCP server";
            break;
        case LWIPERF_TCP_DONE_CLIENT:
            test_type = "TCP client";
            break;
        case LWIPERF_TCP_ABORTED_LOCAL:
            test_type = "TCP aborted (local)";
            break;
        case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
            test_type = "TCP aborted (data error)";
            break;
        case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
            test_type = "TCP aborted (TX error)";
            break;
        case LWIPERF_TCP_ABORTED_REMOTE:
            test_type = "TCP aborted (remote)";
            break;
        default:
            break;
    }

    float throughput_mbps = (bandwidth_kbitpsec / 8.0f) / 1024.0f;
    float total_mb = (bytes_transferred / (1024.0f * 1024.0f));

    DebugP_log("\r\n[NCM BENCHMARK] %s test completed\r\n", test_type);
    DebugP_log("[NCM BENCHMARK] Remote: %s:%u\r\n",
               ipaddr_ntoa(remote_addr), remote_port);
    DebugP_log("[NCM BENCHMARK] Throughput : %.2f MB/s (%.2f MB in %.1f sec)\r\n",
               throughput_mbps, total_mb, ms_duration / 1000.0f);
}

/* Link output function for TX */
static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    if (!tud_ready())
        return ERR_USE;

    SemaphoreP_pend(&txQueueMutex, SystemP_WAIT_FOREVER);
    ncmQueueElem *elem = (ncmQueueElem*)QueueP_get(freeQHandleTx);
    if (elem != NULL)
    {
        pbuf_ref(p);
        elem->pbuf = p;
        QueueP_put(readyQHandleTx, elem);
        SemaphoreP_post(&txQueueMutex);
        EventP_setBits(&gNetworkEventObj, NETWORK_EVENT_TX_BIT);
        return ERR_OK;
    }
    SemaphoreP_post(&txQueueMutex);

    return ERR_MEM;
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p,
                           const ip4_addr_t *addr)
{
    return etharp_output(netif, p, addr);
}

#if LWIP_IPV6
static err_t ip6_output_fn(struct netif *netif, struct pbuf *p,
                           const ip6_addr_t *addr)
{
    return ethip6_output(netif, p, addr);
}
#endif

static err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = CFG_TUD_NET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP |
                   NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output = ip4_output_fn;
#if LWIP_IPV6
    netif->output_ip6 = ip6_output_fn;
#endif
    return ERR_OK;
}

/* DNS query processor */
bool dns_query_proc(const char *name, ip4_addr_t *addr)
{
    if (0 == strcmp(name, "tiny.usb")) {
        *addr = ipaddr;
        return true;
    }
    return false;
}

static void setup_netif_and_services(netif_input_fn input_fn)
{
    struct netif *netif = &netif_data;

    netif->hwaddr_len = sizeof(tud_network_mac_address);
    memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
    netif->hwaddr[5] ^= 0x01;

    netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, input_fn);
#if LWIP_IPV6
    netif_create_ip6_linklocal_address(netif, 1);
#endif
    netif_set_default(netif);

#if LWIP_LWIPERF_APP
    lwiperf_start_tcp_server_default(lwiperf_report_callback, NULL);
#endif
    while (!netif_is_up(&netif_data))
        ClockP_usleep(1000);
    while (dhserv_init(&dhcp_config) != ERR_OK)
        ClockP_usleep(1000);
    while (dnserv_init(IP_ADDR_ANY, 53, dns_query_proc) != ERR_OK)
        ClockP_usleep(1000);
    httpd_init();
}

#if !NO_SYS
void lwIP_tcpipCallback(void *pvArg)
{
    setup_netif_and_services(tcpip_input);
    SemaphoreP_post(&initNetwork);
}
#endif

static void init_lwip(void)
{
#if NO_SYS
    lwip_init();
    setup_netif_and_services(netif_input);
#else
    tcpip_init(lwIP_tcpipCallback, 0);
    SemaphoreP_pend(&initNetwork, SystemP_WAIT_FOREVER);
#endif
}

/* Receive callback from TinyUSB NCM */
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (size == 0) {
        return true;
    }
    if (netif_data.input == NULL) {
        return false;
    }
    struct pbuf *pbuf = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (pbuf == NULL) {
        tud_network_recv_renew();
        return false;
    }
    pbuf_take(pbuf, src, size);
    if (ERR_OK != netif_data.input(pbuf, &netif_data)) {
        pbuf_free(pbuf);
    }
    tud_network_recv_renew();
    return true;
}

/* Transmit callback from TinyUSB NCM */
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = (struct pbuf *)ref;
    (void)arg;
    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

/* Network initialization callback */
void tud_network_init_cb(void)
{
    ncmQueueElem *elem;

    SemaphoreP_pend(&txQueueMutex, SystemP_WAIT_FOREVER);

    if (pendingElem != NULL) {
        pbuf_free(pendingElem->pbuf);
        QueueP_put(freeQHandleTx, pendingElem);
        pendingElem = NULL;
    }

    while (QueueP_isEmpty(readyQHandleTx) == QueueP_NOTEMPTY) {
        elem = QueueP_get(readyQHandleTx);
        if (elem != NULL) {
            pbuf_free(elem->pbuf);
            QueueP_put(freeQHandleTx, elem);
        }
    }
    SemaphoreP_post(&txQueueMutex);
}

/* Initialize queues and synchronization primitives */
static void queue_init(void)
{
    int32_t status;

    status = EventP_construct(&gNetworkEventObj);
    DebugP_assert(status == SystemP_SUCCESS);

    status = SemaphoreP_constructMutex(&txQueueMutex);
    DebugP_assert(status == SystemP_SUCCESS);

    status = SemaphoreP_constructBinary(&initNetwork, 0);
    DebugP_assert(status == SystemP_SUCCESS);

    freeQHandleTx = QueueP_create(&freeQObjectTx);
    readyQHandleTx = QueueP_create(&readyQObjectTx);

    for (uint8_t i = 0; i < TX_QUEUE_SIZE; i++) {
        QueueP_put(freeQHandleTx, &ncmTxQueueBuffer[i]);
    }
}

/* Service TX traffic */
static void service_traffic(void)
{
    ncmQueueElem *elem;
    uint32_t eventBits = 0;

    EventP_waitBits(&gNetworkEventObj,
        NETWORK_EVENT_TX_BIT | NETWORK_EVENT_TX_RETRY_BIT,
        0,
        0,
        SystemP_WAIT_FOREVER,
        &eventBits);

    if (eventBits & (NETWORK_EVENT_TX_BIT | NETWORK_EVENT_TX_RETRY_BIT))
    {
        if (eventBits & NETWORK_EVENT_TX_RETRY_BIT) {
            EventP_clearBits(&gNetworkEventObj, NETWORK_EVENT_TX_RETRY_BIT);
        }

        if (SystemP_SUCCESS == SemaphoreP_pend(&txQueueMutex, SystemP_WAIT_FOREVER))
        {
            if (pendingElem != NULL)
            {
                if (tud_ready() && tud_network_can_xmit(pendingElem->pbuf->tot_len)) {
                    tud_network_xmit(pendingElem->pbuf, 0);
                    pbuf_free(pendingElem->pbuf);
                    QueueP_put(freeQHandleTx, pendingElem);
                    pendingElem = NULL;
                } else {
                    EventP_setBits(&gNetworkEventObj, NETWORK_EVENT_TX_RETRY_BIT);
                    SemaphoreP_post(&txQueueMutex);
                    return;
                }
            }

            while (QueueP_isEmpty(readyQHandleTx) == QueueP_NOTEMPTY) {
                elem = QueueP_get(readyQHandleTx);
                if (elem != NULL)
                {
                    if (tud_ready() && tud_network_can_xmit(elem->pbuf->tot_len)) {
                        tud_network_xmit(elem->pbuf, 0);
                        pbuf_free(elem->pbuf);
                        QueueP_put(freeQHandleTx, elem);
                    }
                    else {
                        pendingElem = elem;
                        EventP_setBits(&gNetworkEventObj, NETWORK_EVENT_TX_RETRY_BIT);
                        SemaphoreP_post(&txQueueMutex);
                        return;
                    }
                }
            }

            if (QueueP_isEmpty(readyQHandleTx) == QueueP_EMPTY && pendingElem == NULL)
            {
                EventP_clearBits(&gNetworkEventObj, NETWORK_EVENT_TX_BIT);
            }

            SemaphoreP_post(&txQueueMutex);
        }
    }
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

static void network_service_task(void *args)
{
    (void)args;

    init_lwip();

    DebugP_log("[NCM BENCHMARK] Network interface configured\r\n");
    DebugP_log("[NCM BENCHMARK] Device IP: %u.%u.%u.%u\r\n",
               ip4_addr1(&ipaddr), ip4_addr2(&ipaddr), ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));
    DebugP_log("[NCM BENCHMARK] DHCP server ready\r\n");
    DebugP_log("[NCM BENCHMARK] DNS server ready\r\n");
    DebugP_log("[NCM BENCHMARK] iperf TCP server ready on port 5001\r\n");

    DebugP_log("\r\n[NCM BENCHMARK] Ready for network tests:\r\n");
    DebugP_log("[NCM BENCHMARK]   ping %u.%u.%u.%u\r\n",
               ip4_addr1(&ipaddr), ip4_addr2(&ipaddr), ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));
    DebugP_log("[NCM BENCHMARK]   iperf -c %u.%u.%u.%u -t 10 -r\r\n",
               ip4_addr1(&ipaddr), ip4_addr2(&ipaddr), ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));

    while (1) {
        service_traffic();
    }
}

void usb_ncm_benchmark_main(void *args)
{
    (void)args;
    int32_t status;

    Drivers_open();
    Board_driversOpen();

    DebugP_log("\r\n[NCM BENCHMARK] USB NCM Network Throughput Benchmark - AM261x\r\n\r\n");

    queue_init();

    /* Create USB task */
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

    /* Create network service task */
    TaskP_Params networkTaskParams;
    TaskP_Params_init(&networkTaskParams);
    networkTaskParams.name      = "network_task";
    networkTaskParams.stackSize = NETWORK_TASK_STACK_SIZE;
    networkTaskParams.stack     = gNetworkTaskStack;
    networkTaskParams.priority  = NETWORK_TASK_PRIORITY;
    networkTaskParams.args      = NULL;
    networkTaskParams.taskMain  = network_service_task;
    status = TaskP_construct(&gNetworkTaskObj, &networkTaskParams);
    DebugP_assert(status == SystemP_SUCCESS);
}

/* ========================================================================== */
/*                 DFU Stubs (required by benchmark library)                  */
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
