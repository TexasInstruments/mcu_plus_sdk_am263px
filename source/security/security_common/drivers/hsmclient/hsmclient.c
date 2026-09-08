/*
 *  Copyright (C) 2022-24 Texas Instruments Incorporated
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

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <security_common/drivers/secure_ipc_notify/sipc_notify.h>
#include <kernel/dpl/SemaphoreP.h>
#include <kernel/dpl/SystemP.h>
#include <kernel/dpl/CacheP.h>
#include <drivers/hw_include/cslr_soc.h>
#include <kernel/dpl/HwiP.h>
#include <security_common/drivers/hsmclient/hsmclient.h>
#include <security_common/drivers/hsmclient/hsmclient_msg.h>
#include <drivers/soc.h>
#include <string.h>
#include <kernel/dpl/DebugP.h>

void Hsmclient_updateBootNotificationRegister(void);

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/**
 * @brief
 *        Macro calculates the cache-aligned size for a given buffer or data structure
 */
#define GET_CACHE_ALIGNED_SIZE(x) (((x) + (CacheP_CACHELINE_ALIGNMENT - 1U)) & ~(CacheP_CACHELINE_ALIGNMENT - 1U))

/**
 * @brief
 *        Conditional cache writeback-and-invalidate wrapper.
 *        When HSMCLIENT_CACHE_ENABLE is 0 (buffers in non-cacheable memory)
 *        this expands to nothing, avoiding unnecessary cache maintenance.
 */
#if (HSMCLIENT_CACHE_ENABLE == 1U)
#define HSMCLIENT_CACHE_WB_INV(addr, size, type)  CacheP_wbInv((addr), (size), (type))
#define HSMCLIENT_CACHE_INV(addr, size, type)      CacheP_inv((addr), (size), (type))
#else
#define HSMCLIENT_CACHE_WB_INV(addr, size, type)  /* cache disabled */
#define HSMCLIENT_CACHE_INV(addr, size, type)      /* cache disabled */
#endif

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* This variable indicates HSM Runtime load status */
volatile uint32_t gHsmrtLoadStatus = HSMRT_LOAD_NOT_REQUESTED;

/* This variable indicates Secure Boot status */
static volatile int32_t gSecureBootStatus = SystemP_SUCCESS;

/* This variable indicates whether Boot notification is received or not */
static volatile int32_t gBootNotificationReceived = SystemP_FAILURE;

static int32_t gLastSentIndex = -1;
static int32_t gLastEnqueuedIndex = -1;
static int32_t gNum_HsmRequestSent = 0;
static volatile int32_t gNum_HsmResponseReceived = 0;
static uint32_t hsm_client_msg_queue_size = 64U;

#ifdef HSMCLIENT_HOST_BUFF_ENABLE
/*
 * Driver-owned host buffer for HSM IPC structs.
 * Declared in the SysConfig-generated hsmclient_config.c so that the linker
 * section placement and alignment are controlled by the application's
 * SysConfig/linker setup.  The driver only holds an extern reference.
 */
extern uint8_t gHsmClientHostBuff[HSMCLIENT_HOST_BUFF_SIZE];
#endif

/**
 * @brief
 *        Maximum size of the HSM client message queue
 *        For Streaming Secure Boot, the break up is as follows,
 *        1 start message (includes the certificate) +
 *        1 finish message +
 *        1 ELF Header Buffer update message + 1 PHT Buffer update message
 *        1024 ELF segment update messages (including the two note segments)
 *          - PT note for boot sequence info
 *          - PT note containing Random string for decryption verification
 */
extern HsmMsg_t gHsmClientMsgQueue[];

/*==========================================================================
 *                        Static Function Declarations
 *==========================================================================*/

/**
 * @brief
 *        Calculate crc16_ccit for a given data.
 * @param data data pointer
 * @param length data length
 * @return 16 bit crc calculated from given data.
 */
static uint16_t crc16_ccit(uint8_t *data, uint16_t length);

/**
 * @brief
 *      Generic send and receive message api
 * @param HsmClient client type
 * @param timeout time to wait for interrupt from HSM before throwing timeout
 *                exception.
 * @return SystemP_SUCCESS if transaction is successful else SystemP_FAILURE.
 */
static int32_t HsmClient_SendAndRecv(HsmClient_t *HsmClient, uint32_t timeout);

/**
 * @brief
 *      Generic send message api which enqueus the message in HSM client message
 *      queue and tries to send the message via SIPC.
 *      This API is non blocking in nature.
 * @param message message that needs to be sent to the HSM core.
 * @return SystemP_SUCCESS if transaction is successful else SystemP_FAILURE.
 */
static int32_t HsmClient_EnqueueAndSendMsg(HsmMsg_t message);

/**
 * @brief
 *      Generic send message api which enqueus the message in HSM client message
 *      queue and ensures all the messages present in the queue are sent via SIPC.
 *      This API is blocking in nature.
 * @param message message that needs to be sent to the HSM core
 * @return SystemP_SUCCESS if transaction is successful else SystemP_FAILURE.
 */
static int32_t HsmClient_EnqueueAndSendMsgBlocking(HsmMsg_t message);

/**
 * @brief
 *      Generic api which waits for all responses to be received
 * @return SystemP_SUCCESS if all responses are successful else SystemP_FAILURE.
 */
static int32_t HsmClient_waitForAllResponses();

/*==============================================================================*
 *                          Static Functions definition.
 *==============================================================================*/

/* CRC 16 CCIT soft implementation */
static uint16_t crc16_ccit(uint8_t *data, uint16_t length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--)
    {
        x = crc >> 8 ^ *data++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    return crc;
}

static int32_t HsmClient_EnqueueAndSendMsg(HsmMsg_t message)
{
    int32_t status = SystemP_FAILURE;
    uint8_t localClientId = message.srcClientId;
    uint8_t remoteClientId = message.destClientId;

    message.crcMsg = crc16_ccit((uint8_t *)&message, (sizeof(HsmMsg_t) - 2));

	if (gLastEnqueuedIndex < (int32_t)(hsm_client_msg_queue_size - 1))
	{
		gHsmClientMsgQueue[++gLastEnqueuedIndex] = message;

        /* Till the boot notify is not received, simply enqueue the message */
        if (gBootNotificationReceived == SystemP_SUCCESS)
        {
            while ((gSecureBootStatus == SystemP_SUCCESS) && (gLastSentIndex < gLastEnqueuedIndex))
            {
                /*
                    Do not wait in case the SIPC software Queue is full and simply abort with error.
                    This is done in order to make this call non blocking.
                */
                status = SIPC_sendMsg(CORE_INDEX_HSM, remoteClientId, localClientId,
                                      (uint8_t *)&gHsmClientMsgQueue[gLastSentIndex + 1], ABORT_ON_FIFO_FULL);

                /* Successfully able to send the message via SIPC */
                if (status == SystemP_SUCCESS)
                {
                    gNum_HsmRequestSent++;
                    gLastSentIndex++;
                }
                /*
                    Failed to send the message because the SIPC Software FIFO is full.
                    Retry in the next call to this function or in the finish call.
                */
                else if (status == SystemP_FAILURE)
                {
                    status = SystemP_SUCCESS;
                    break;
                }
                else
                {
                    status = SystemP_FAILURE;
                    break;
                }
            }
        }
        else
        {
            status = SystemP_SUCCESS;
        }
    }
    else
    {
        status = SystemP_FAILURE;
    }

    return status;
}

static int32_t HsmClient_EnqueueAndSendMsgBlocking(HsmMsg_t message)
{
    int32_t status = SystemP_FAILURE;
    uint8_t localClientId = message.srcClientId;
    uint8_t remoteClientId = message.destClientId;

    message.crcMsg = crc16_ccit((uint8_t *)&message, (sizeof(HsmMsg_t) - 2));

	if (gLastEnqueuedIndex < (int32_t)(hsm_client_msg_queue_size - 1))
	{
		gHsmClientMsgQueue[++gLastEnqueuedIndex] = message;

        /*
            If the boot notification is not received, we wait for it indefinitely
            and once it arrives we begin sending our request packets.
        */
        status = HsmClient_checkAndWaitForBootNotification();

        if (status == SystemP_SUCCESS)
        {
            while ((gSecureBootStatus == SystemP_SUCCESS) && (gLastSentIndex < gLastEnqueuedIndex))
            {
                /*
                    In this call, we want to wait in case the SIPC software Queue is full
                    and we simply abort with error. This is done in order to make sure that
                    all the requests to HSM are sent in order.
                */
                status = SIPC_sendMsg(CORE_INDEX_HSM, remoteClientId, localClientId,
                                      (uint8_t *)&gHsmClientMsgQueue[gLastSentIndex + 1], WAIT_IF_FIFO_FULL);

                if (status == SystemP_SUCCESS)
                {
                    gNum_HsmRequestSent++;
                    gLastSentIndex++;
                }
                else
                {
                    status = SystemP_FAILURE;
                    break;
                }
            }
        }
    }
    else
    {
        status = SystemP_FAILURE;
    }

    return status;
}

static int32_t HsmClient_waitForAllResponses()
{
    while ((gSecureBootStatus == SystemP_SUCCESS) && (gNum_HsmResponseReceived < gNum_HsmRequestSent))
    {
    }

    return gSecureBootStatus;
}

static int32_t HsmClient_SendAndRecv(HsmClient_t *HsmClient, uint32_t timeout)
{
    uint8_t localClientId;
    uint8_t remoteClientId;
    int32_t status;
    uint16_t crcMsg;

    localClientId = HsmClient->ReqMsg.srcClientId;
    remoteClientId = HsmClient->ReqMsg.destClientId;

    /* Add message crc. Exclude crcMsg argument of HsmMsg_t from crc calculations*/
    HsmClient->ReqMsg.crcMsg = crc16_ccit((uint8_t *)&HsmClient->ReqMsg, (sizeof(HsmMsg_t) - 2));

    status = SIPC_sendMsg(CORE_INDEX_HSM, remoteClientId, localClientId,
                          (uint8_t *)&HsmClient->ReqMsg, WAIT_IF_FIFO_FULL);
    if (status == SystemP_SUCCESS)
    {
        status = SemaphoreP_pend(&HsmClient->Semaphore, timeout);
        if (status == SystemP_FAILURE)
        {
            return SystemP_FAILURE;
        }
        else if (status == SystemP_TIMEOUT)
        {
            DebugP_log("\r\n [HSM_CLIENT] Timeout exception \r\n");
            return SystemP_TIMEOUT;
        }
        else
        {
            crcMsg = crc16_ccit((uint8_t *)&HsmClient->RespMsg, SIPC_MSG_SIZE - 2);
            /* if the message is okay then send whatever the flag receive */
            if (crcMsg == HsmClient->RespMsg.crcMsg)
            {
                HsmClient->RespFlag = HsmClient->RespMsg.flags;
                status = SystemP_SUCCESS;
            }
            /* corrupted message received */
            else
            {
                DebugP_log("\r\n [HSM_CLIENT] Corrupted message received \r\n");
                HsmClient->RespFlag = HSM_FLAG_NACK;
                status = SystemP_FAILURE;
            }
            return status;
        }
    }
    else
    {
        return status;
    }
}

/*==============================================================================*
 *                          Public Function definition.
 *==============================================================================*/

void HsmClient_isr(uint8_t remoteCoreId, uint8_t localClientId,
                   uint8_t remoteClientId, uint8_t *msgValue, const void *args)
{
    HsmClient_t *HsmClient = (HsmClient_t *)args;

    /* here we will just post the semaphore */
    /* copy message to client response variable */
    /* As this ISR is blocking, quickly copy the message and exit ISR */
    memcpy(&HsmClient->RespMsg, msgValue, SIPC_MSG_SIZE);

    /* Check if HSM response message is not related to proc_auth_boot  
    * proc_auth_boot APIs use HsmClient_EnqueueAndSendMsg() or HsmClient_EnqueueAndSendMsgBlocking()
    * Others use HsmClient_SendAndRecv()
    * If true, post the semaphore to signal completion
    */
    if ((HsmClient->RespMsg.serType != HSM_MSG_PROC_AUTH_BOOT_START)  &&  /* Not proc_auth_boot auth start */
    (HsmClient->RespMsg.serType != HSM_MSG_PROC_AUTH_BOOT_UPDATE) &&  /* Not proc_auth_boot auth update */
    (HsmClient->RespMsg.serType != HSM_MSG_PROC_AUTH_BOOT_FINISH))    /* Not proc_auth_boot auth finish */
    {
        SemaphoreP_post(&HsmClient->Semaphore);
    }  

    /*
        Analyze the received response packet.
    */
    if (HsmClient->RespMsg.serType == HSM_MSG_BOOT_NOTIFY)
    {
        gBootNotificationReceived = SystemP_SUCCESS;
        gSecureBootStatus = SystemP_SUCCESS;

        Hsmclient_updateBootNotificationRegister();
    }
    else
    {
        if (gSecureBootStatus == SystemP_SUCCESS)
        {
            /* gNum_HsmResponseReceived must only be incremented if 
             * the call is non-blocking which is valid only for PROC_AUTH
             * (START, UPDATE, FINISH).
             */
            if ((HsmClient->RespMsg.serType == HSM_MSG_PROC_AUTH_BOOT_START)  ||  
                (HsmClient->RespMsg.serType == HSM_MSG_PROC_AUTH_BOOT_UPDATE) ||  
                (HsmClient->RespMsg.serType == HSM_MSG_PROC_AUTH_BOOT_FINISH))    
            {
                gNum_HsmResponseReceived++;
            }
            if (HsmClient->RespMsg.flags == HSM_FLAG_NACK)
            {
                gSecureBootStatus = SystemP_FAILURE;
            }
            else if (HsmClient->RespMsg.flags == HSM_FLAG_ACK)
            {
                gSecureBootStatus = SystemP_SUCCESS;
            }
            else
            {
                gSecureBootStatus = SystemP_FAILURE;
            }
        }
        else
        {
            gSecureBootStatus = SystemP_FAILURE;
        }
    }
}

int32_t HsmClient_checkAndWaitForBootNotification(void)
{
    int32_t status = SystemP_FAILURE;

    if (gHsmrtLoadStatus == HSMRT_LOAD_NOT_REQUESTED)
    {
        /*
           If HSM Runtime load is not requested up till now,
           just return Success.
       */
        status = SystemP_SUCCESS;
    }
    else if (gHsmrtLoadStatus == HSMRT_LOAD_REQUESTED)
    {
        /* Only wait if HSM Runtime load is requested up till now. */
        while (gHsmrtLoadStatus == HSMRT_LOAD_REQUESTED)
        {
        }

        if (gHsmrtLoadStatus == HSMRT_LOAD_SUCCEEDED)
        {
            /* Only wait for Boot notification HSM Runtime load is successful */
            while (gBootNotificationReceived == SystemP_FAILURE)
            {
            }
            status = SystemP_SUCCESS;
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    else if (gHsmrtLoadStatus == HSMRT_LOAD_FAILED)
    {
        status = SystemP_FAILURE;
    }
    else if (gHsmrtLoadStatus == HSMRT_LOAD_SUCCEEDED)
    {
        /* Only wait for Boot notification HSM Runtime load is successful */
        while (gBootNotificationReceived == SystemP_FAILURE)
        {
        }
        status = SystemP_SUCCESS;
    }
    else
    {
        status = SystemP_FAILURE;
    }

    return status;
}

/* return SystemP_FAILURE if clientId is greater the max or
 * A callback has been registered. already */
int32_t HsmClient_register(HsmClient_t *HsmClient, uint8_t clientId)
{
    uint8_t status;
    SIPC_FxnCallback isr;

    isr = &HsmClient_isr;

    if (HsmClient == NULL)
    {
        DebugP_log(" \r\n [HSM_CLIENT] HsmClient_t type error. \r\n");
        return SystemP_FAILURE;
    }
    else
    {
        status = SystemP_SUCCESS;
    }

    HsmClient->ClientId = clientId;

    /* register HSM_Isr and pass the pointer as args */
    status = SIPC_registerClient(clientId, isr, (void *)HsmClient);
    if (status == SystemP_SUCCESS)
    {
        SemaphoreP_constructBinary(&HsmClient->Semaphore, 0);
        DebugP_log("\r\n [HSM_CLIENT] New Client Registered with Client Id = %d\r\n ", clientId);
    }
    else
    {
        DebugP_log(" \r\n [HSM_CLIENT] Client already registered or Invalid ClientId\r\n");
    }
    return status;
}

int32_t HsmClient_init(SIPC_Params *params)
{
    /* get the params and do SIPC init */
    int32_t status;
    uint32_t selfCoreId;
    status = SIPC_init(params);
    /* TODO: keyrings initialization */
    if (status == SystemP_FAILURE)
    {
        selfCoreId = SIPC_getSelfCoreId();
        DebugP_log("[HSM_CLIENT] Secure Host initialization failed for R5F%d \r\n", selfCoreId);
    }
    return status;
}

void HsmClient_SecureBootQueueInit(uint32_t configured_hsm_client_msg_queue_size){
    /* Customize the size of the HSM client message queue*/
    hsm_client_msg_queue_size = configured_hsm_client_msg_queue_size; 
}

/* do sipc deinit */
void HsmClient_deinit(void)
{
    SIPC_deInit();
}

void HsmClient_unregister(HsmClient_t *HsmClient, uint8_t clientId)
{
    /* unregister a client */
    SIPC_unregisterClient(clientId);
}

/**
 * @brief
 *      Returns a pointer suitable for passing into an HSM IPC message.
 * @param src   Pointer to the source struct (caller-owned, any alignment).
 * @param size  Size in bytes of the struct pointed to by \p src.
 * @return Pointer to use when populating the IPC message args field.
 */
static void *HsmClient_getIPCBuffPtr(void *src, uint32_t size)
{
    void *pBuff = src;
#ifdef HSMCLIENT_HOST_BUFF_ENABLE
    if ((src != NULL) && (size <= HSMCLIENT_HOST_BUFF_SIZE) &&
        (((size % CacheP_CACHELINE_ALIGNMENT) != 0U) || (((uintptr_t)src % CacheP_CACHELINE_ALIGNMENT) != 0U)))
    {
        /* Stage the application buffer into the driver-owned host buffer */
        memcpy(gHsmClientHostBuff, src, size);
        pBuff = (void *)gHsmClientHostBuff;
    }
#endif
    return pBuff;
}

/**
 * @brief
 *      Response data was staged through the driver-owned host buffer (gHsmClientHostBuff)
 * @param dst   Pointer to the caller-owned destination struct.
 * @param size  Size in bytes of the struct pointed to by \p dst.
 */
static void HsmClient_syncIPCBuffPtr(void *dst, uint32_t size)
{
#ifdef HSMCLIENT_HOST_BUFF_ENABLE
    if ((dst != NULL) && (size <= HSMCLIENT_HOST_BUFF_SIZE) &&
        (((size % CacheP_CACHELINE_ALIGNMENT) != 0U) || (((uintptr_t)dst % CacheP_CACHELINE_ALIGNMENT) != 0U)))
    {
        /* Stage the host buffer into the application owned buffer */
        memcpy(dst, gHsmClientHostBuff, size);
    }
#endif
}

int32_t HsmClient_getVersion(HsmClient_t *HsmClient,
                             HsmVer_t *hsmVer, uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_VERSION;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(hsmVer, sizeof(HsmVer_t));

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(HsmVer_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the HsmVer struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(HsmVer_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);

    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(HsmVer_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args), sizeof(HsmVer_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(hsmVer, sizeof(HsmVer_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the hsmVer has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get version request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for getversion response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_getUID(HsmClient_t *HsmClient,
                         uint8_t *uid, uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_UID;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(uid, HSM_UID_SIZE);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, HSM_UID_SIZE);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the uid and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(HSM_UID_SIZE), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(HSM_UID_SIZE), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, HSM_UID_SIZE);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(uid, HSM_UID_SIZE);

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the getUID has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get UID request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for getUID response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_openDbgFirewall(HsmClient_t *HsmClient,
                                  uint8_t *cert,
                                  uint32_t cert_size,
                                  uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_OPEN_DBG_FIREWALLS;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(cert, cert_size);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, cert_size);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the debug cert and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0U);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(cert, cert_size);

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the OpenDbgFirewalls has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] OpenDbgFirewall request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for openDbgFirewall response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_importKeyring(HsmClient_t *HsmClient,
                                uint8_t *cert,
                                uint32_t cert_size,
                                uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_KEYRING_IMPORT;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(cert, cert_size);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, cert_size);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the keyring cert and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0U);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(cert, cert_size);

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the importKeyring has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Import Keyring request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for Import Keyring response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_readOTPRow(HsmClient_t *HsmClient,
                             NvmOtpRead_t *readRow)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_READ_OTP_ROW;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(readRow, sizeof(NvmOtpRead_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(NvmOtpRead_t));

    /*
       Write back the EfuseRead struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRead_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRead_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(NvmOtpRead_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(readRow, sizeof(NvmOtpRead_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the readRow has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Read OTP row request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for read OTP Row response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_writeOTPRow(HsmClient_t *HsmClient,
                              NvmOtpRowWrite_t *writeRow)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_WRITE_OTP_ROW;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(writeRow, sizeof(NvmOtpRowWrite_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(NvmOtpRowWrite_t));

    /*
       Write back the EfuseRowWrite struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowWrite_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowWrite_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(NvmOtpRowWrite_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(writeRow, sizeof(NvmOtpRowWrite_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the writeRow has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Write OTP row request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for Write OTP row response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_lockOTPRow(HsmClient_t *HsmClient,
                             NvmOtpRowProt_t *protRow)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_PROT_OTP_ROW;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(protRow, sizeof(NvmOtpRowProt_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(NvmOtpRowProt_t));

    /*
       Write back the EfuseRowProt struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowProt_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowProt_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(NvmOtpRowProt_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(protRow, sizeof(NvmOtpRowProt_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the row is locked by HSM server if this
             * request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Extended OTP row protection request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for extended OTP row protection response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_getOTPRowCount(HsmClient_t *HsmClient,
                                 NvmOtpRowCount_t *rowCount)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_OTP_ROW_COUNT;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(rowCount, sizeof(NvmOtpRowCount_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(NvmOtpRowCount_t));

    /*
       Write back the EfuseRowCount struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowCount_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowCount_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(NvmOtpRowCount_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(rowCount, sizeof(NvmOtpRowCount_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the rowCount has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get OTP row count request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for get OTP Row count response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_getOTPRowProtection(HsmClient_t *HsmClient,
                                      NvmOtpRowProt_t *rowProt)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_OTP_ROW_PROT;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(rowProt, sizeof(NvmOtpRowProt_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(NvmOtpRowProt_t));

    /*
       Write back the EfuseRowProt struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowProt_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(NvmOtpRowProt_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(NvmOtpRowProt_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(rowProt, sizeof(NvmOtpRowProt_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the rowProt has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get OTP row protection request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for get OTP Row protection response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_procAuthBoot(HsmClient_t *HsmClient,
                               uint8_t *cert,
                               uint32_t cert_size,
                               uint32_t timeout)
{
    /* make the message */
    int32_t status = SystemP_FAILURE;
    uint16_t crcArgs;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_PROC_AUTH_BOOT;
    HsmClient->ReqMsg.args = (void *)cert;

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)cert, cert_size);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(cert);

    /*
        Write back the cert and
        invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(cert, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* the procAuthBoot has been populated by HSM server
         * if this request has been processed correctly */
        if (HsmClient->RespFlag == HSM_FLAG_NACK)
        {
            DebugP_log("\r\n [HSM_CLIENT] Proc_Auth_Boot request NACKed by HSM server\r\n");
            status = SystemP_FAILURE;
        }
        else
        {
            /* Change the Arguments Address in Physical Address */
            HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

            /* check the integrity of args */
            crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0U);

            if (crcArgs == HsmClient->RespMsg.crcArgs)
            {
                status = SystemP_SUCCESS;
            }
            else
            {
                DebugP_log("\r\n [HSM_CLIENT] CRC check for Proc_Auth_Boot response failed \r\n");
                status = SystemP_FAILURE;
            }
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_procAuthBootStart(HsmClient_t *HsmClient,
                                    SecureBoot_Stream_t *secureBootInfo)
{
    int32_t status = SystemP_FAILURE;
    /* Create the message object */
    HsmMsg_t startMsg;

    /*populate the send message structure */
    startMsg.destClientId = HSM_CLIENT_ID_1;
    startMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    startMsg.flags = HSM_FLAG_AOP;
    startMsg.serType = HSM_MSG_PROC_AUTH_BOOT_START;
    startMsg.args = (void *)secureBootInfo;

    /* Add arg crc */
    startMsg.crcArgs = crc16_ccit((uint8_t *)secureBootInfo, sizeof(SecureBoot_Stream_t));

    /* Change the Arguments Address in Physical Address */
    startMsg.args = (void *)(uintptr_t)SOC_virtToPhy(secureBootInfo);

    /*
       Write back secure boot info object  and the data it contains
       to Shared memory invalidate the caches before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(secureBootInfo->dataIn, GET_CACHE_ALIGNED_SIZE(secureBootInfo->dataLen), CacheP_TYPE_ALLD);
    HSMCLIENT_CACHE_WB_INV(secureBootInfo, GET_CACHE_ALIGNED_SIZE(sizeof(SecureBoot_Stream_t)), CacheP_TYPE_ALLD);

    status = HsmClient_EnqueueAndSendMsg(startMsg);

    return status;
}

int32_t HsmClient_procAuthBootUpdate(HsmClient_t *HsmClient,
                                     SecureBoot_Stream_t *secureBootInfo)
{

    int32_t status = SystemP_FAILURE;
    /* Create the message object */
    HsmMsg_t updateMsg;

    /*populate the send message structure */
    updateMsg.destClientId = HSM_CLIENT_ID_1;
    updateMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    updateMsg.flags = HSM_FLAG_AOP;
    updateMsg.serType = HSM_MSG_PROC_AUTH_BOOT_UPDATE;
    updateMsg.args = (void *)secureBootInfo;

    /* Add arg crc */
    updateMsg.crcArgs = crc16_ccit((uint8_t *)secureBootInfo, sizeof(SecureBoot_Stream_t));

    /* Change the Arguments Address in Physical Address */
    updateMsg.args = (void *)(uintptr_t)SOC_virtToPhy(secureBootInfo);

    /*
       Write back secure boot info object to Shared memory
       invalidate the caches before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(secureBootInfo->dataIn, GET_CACHE_ALIGNED_SIZE(secureBootInfo->dataLen), CacheP_TYPE_ALLD);
    HSMCLIENT_CACHE_WB_INV(secureBootInfo, GET_CACHE_ALIGNED_SIZE(sizeof(SecureBoot_Stream_t)), CacheP_TYPE_ALLD);

    status = HsmClient_EnqueueAndSendMsg(updateMsg);

    return status;
}

int32_t HsmClient_procAuthBootFinish(HsmClient_t *HsmClient,
                                     SecureBoot_Stream_t *secureBootInfo)
{

    int32_t status = SystemP_FAILURE;
    /* Create the message object */
    HsmMsg_t finishMsg;

    /*populate the send message structure */
    finishMsg.destClientId = HSM_CLIENT_ID_1;
    finishMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    finishMsg.flags = HSM_FLAG_AOP;
    finishMsg.serType = HSM_MSG_PROC_AUTH_BOOT_FINISH;
    finishMsg.args = (void *)secureBootInfo;

    /* Add arg crc */
    finishMsg.crcArgs = crc16_ccit((uint8_t *)secureBootInfo, sizeof(SecureBoot_Stream_t));

    /* Change the Arguments Address in Physical Address */
    finishMsg.args = (void *)(uintptr_t)SOC_virtToPhy(secureBootInfo);

    /*
       Write back secure boot info object to Shared memory
       invalidate the caches before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(secureBootInfo, GET_CACHE_ALIGNED_SIZE(sizeof(SecureBoot_Stream_t)), CacheP_TYPE_ALLD);

    status = HsmClient_EnqueueAndSendMsgBlocking(finishMsg);

    if (status == SystemP_SUCCESS)
    {
        status = HsmClient_waitForAllResponses();
    }

    return status;
}

int32_t HsmClient_setFirewall(HsmClient_t *HsmClient,
                              FirewallReq_t *FirewallReqObj,
                              uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint16_t crcFirewallRegionArr;
    void *ipcBuff;
    FirewallRegionReq_t *pFirewallRegionArrVirt;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_SET_FIREWALL;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(FirewallReqObj, sizeof(FirewallReq_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* FirewallRegionArr is overwritten below with its physical address for the
       HSM server. Keep the original virtual address for the CRC calculation
       and to restore it once the physical address is no longer needed. Note:
       write-back/invalidate of the FirewallRegionArr contents themselves is
       the caller's responsibility, not HsmClient's. */
    pFirewallRegionArrVirt = ((FirewallReq_t *)ipcBuff)->FirewallRegionArr;

    /* Calculates CRC of the array containing firewall regions to be configured */
    crcFirewallRegionArr = crc16_ccit((uint8_t *)pFirewallRegionArrVirt, ((((FirewallReq_t *)ipcBuff)->regionCount) * sizeof(FirewallRegionReq_t)));
    ((FirewallReq_t *)ipcBuff)->crcArr = crcFirewallRegionArr;
    ((FirewallReq_t *)ipcBuff)->FirewallRegionArr = (FirewallRegionReq_t *)(uintptr_t)SOC_virtToPhy(pFirewallRegionArrVirt);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(FirewallReq_t));

    /*
       Write back the FirewallReqObj
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(FirewallReq_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(FirewallReq_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(FirewallReq_t));

        /* Restore the region array pointer to its virtual address now that the
           physical address has served its purpose for the IPC transfer */
        ((FirewallReq_t *)ipcBuff)->FirewallRegionArr = pFirewallRegionArrVirt;

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(FirewallReqObj, sizeof(FirewallReq_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the firewall regions has been configured by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Set firewall request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for set firewall response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }

    /* Ensure FirewallRegionArr is left holding a virtual address on every
       return path (e.g. if HsmClient_SendAndRecv itself failed/timed out
       before the response-side restore above ran), so the caller's struct is
       never left corrupted with a physical address */
    ((FirewallReq_t *)ipcBuff)->FirewallRegionArr = pFirewallRegionArrVirt;

    return status;
}

int32_t HsmClient_FirewallIntr(HsmClient_t *HsmClient,
                               FirewallIntrReq_t *FirewallIntrReqObj,
                               uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_SET_FIREWALL_INTR;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(FirewallIntrReqObj, sizeof(FirewallIntrReq_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(FirewallIntrReq_t));

    /*
       Write back the FirewallIntrReq_t struct
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(FirewallIntrReq_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(FirewallIntrReq_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(FirewallIntrReq_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(FirewallIntrReqObj, sizeof(FirewallIntrReq_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the firewall interrupt request has been honored by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] firewall interrupt request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for firewall interrupt response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_getDKEK(HsmClient_t *HsmClient,
                          DKEK_t *getDKEK,
                          uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_DKEK;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(getDKEK, sizeof(DKEK_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(DKEK_t));

    /*
       Write back the DKEK_t struct
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(DKEK_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(DKEK_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(DKEK_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(getDKEK, sizeof(DKEK_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the getDKEK has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get DKEK request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for get DKEK response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_keyWriter(HsmClient_t *HsmClient, KeyWriterCertHeader_t *certHeader, uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_KEYWRITER_SEND_CUST_KEY_CERT;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(certHeader, sizeof(KeyWriterCertHeader_t));

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(KeyWriterCertHeader_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the KwrCertHeader struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(KeyWriterCertHeader_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);

    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(KeyWriterCertHeader_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(KeyWriterCertHeader_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(certHeader, sizeof(KeyWriterCertHeader_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the keyWriter has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] KeyWriter customer key certificate send request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for KeyWriter customer key certificate send response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_readSWRev(HsmClient_t *HsmClient,
                            SWRev_t *readSWRev)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_READ_SWREV;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(readSWRev, sizeof(SWRev_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(SWRev_t));

    /*
       Write back the SWRev_t struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(SWRev_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(SWRev_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(SWRev_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(readSWRev, sizeof(SWRev_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the readSWRev has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Read SWRev request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for read SWRev response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_writeSWRev(HsmClient_t *HsmClient,
                             SWRev_t *writeSWRev)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_WRITE_SWREV;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(writeSWRev, sizeof(SWRev_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(SWRev_t));

    /*
       Write back the SWRev_t struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(SWRev_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(SWRev_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(SWRev_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(writeSWRev, sizeof(SWRev_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the writeSWRev has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Write SWRev request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for write SWRev response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_getRandomNum(HsmClient_t *HsmClient,
                               RNGReq_t *getRandomNum)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_RAND;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(getRandomNum, sizeof(RNGReq_t));

    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    ((RNGReq_t *)ipcBuff)->resultPtr = (uint8_t *)(uintptr_t)SOC_virtToPhy(((RNGReq_t *)ipcBuff)->resultPtr);
    ((RNGReq_t *)ipcBuff)->seedValue = (uint32_t *)(uintptr_t)SOC_virtToPhy(((RNGReq_t *)ipcBuff)->seedValue);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(RNGReq_t));

    /*
       Write back the RNGReq_t struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(RNGReq_t)), CacheP_TYPE_ALL);
    HSMCLIENT_CACHE_WB_INV(((RNGReq_t *)ipcBuff)->seedValue, GET_CACHE_ALIGNED_SIZE((((RNGReq_t *)ipcBuff)->seedSizeInDWords) * 4), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(RNGReq_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(RNGReq_t));

        ((RNGReq_t *)HsmClient->RespMsg.args)->resultPtr = (uint8_t *)SOC_phyToVirt((uint64_t)(((RNGReq_t *)HsmClient->RespMsg.args)->resultPtr));
        ((RNGReq_t *)HsmClient->RespMsg.args)->seedValue = (uint32_t *)SOC_phyToVirt((uint64_t)(((RNGReq_t *)HsmClient->RespMsg.args)->seedValue));
        HSMCLIENT_CACHE_INV((void *)((RNGReq_t *)HsmClient->RespMsg.args)->resultPtr, GET_CACHE_ALIGNED_SIZE(((uint32_t)(((RNGReq_t *)HsmClient->RespMsg.args)->resultLength))), CacheP_TYPE_ALL);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(getRandomNum, sizeof(RNGReq_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the getRandomNum has been populated by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Get Random Number request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for Get RandomNumber response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_firmwareUpdate_CertProcess(HsmClient_t *HsmClient,
                                             FirmwareUpdateReq_t *pFirmwareUpdateObject)
{
    int32_t status = SystemP_SUCCESS;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;

    /* Proceed only if no previous firmware update API calls have been made, or HsmClient_firmwareUpdate_CodeVerify API is called */
    if (SystemP_SUCCESS == status)
    {

        /*populate the send message structure */
        HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
        HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

        /* Always expect acknowledgement from HSM server */
        HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
        HsmClient->ReqMsg.serType = HSM_MSG_FW_UPDATE_CERT_PROCESS;
    }
    else
    {
        /* Do nothing */
    }

    /* Proceed only if address and size check falls within bounds */
    if (SystemP_SUCCESS == status)
    {
        /* Change the certificate start address to HSM address space */
        pFirmwareUpdateObject->pStartAddress = (uint8_t *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject->pStartAddress);
        /* Add arg crc */
        HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pFirmwareUpdateObject, sizeof(FirmwareUpdateReq_t));
        /* Change the Arguments Address in Physical Address */
        HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject);

        /*
        Write back the HsmVer struct and
        invalidate the cache before passing it to HSM
        */
        HSMCLIENT_CACHE_WB_INV(pFirmwareUpdateObject, GET_CACHE_ALIGNED_SIZE(sizeof(FirmwareUpdateReq_t)), CacheP_TYPE_ALL);
        status = HsmClient_SendAndRecv(HsmClient, timeout);

        if (status == SystemP_SUCCESS)
        {
            /* the FW update cert process request has been completed by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Firmware update certificate processing request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                /* Change the Arguments Address in Physical Address */
                HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);
                /* check the integrity of args */
                crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args), 0);
                if (crcArgs == HsmClient->RespMsg.crcArgs)
                {
                    status = SystemP_SUCCESS;
                }
                else
                {
                    DebugP_log("\r\n [HSM_CLIENT] CRC check for firmware update certificate process response failed \r\n");
                    status = SystemP_FAILURE;
                }
            }
        }
    }
    else
    {
        /* Do nothing */
    }

    return status;
}

int32_t HsmClient_firmwareUpdate_CodeProgram(HsmClient_t *HsmClient,
                                             FirmwareUpdateReq_t *pFirmwareUpdateObject)
{
    int32_t status = SystemP_SUCCESS;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;

    /* Proceed only if HsmClient_firmwareUpdate_CertProcess API call has been made */
    if (SystemP_SUCCESS == status)
    {
        /*populate the send message structure */
        HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
        HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

        /* Always expect acknowledgement from HSM server */
        HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
        HsmClient->ReqMsg.serType = HSM_MSG_FW_UPDATE_CODE_PROGRAM;
    }
    else
    {
        /* Do nothing */
    }

    /* Proceed only if address and size check falls within bounds */
    if (SystemP_SUCCESS == status)
    {
        /* Change the certificate start address to HSM address space */
        pFirmwareUpdateObject->pStartAddress = (uint8_t *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject->pStartAddress);
        /* Add arg crc */
        HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pFirmwareUpdateObject, sizeof(FirmwareUpdateReq_t));
        /* Change the Arguments Address in Physical Address */
        HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject);

        /*
        Write back the HsmVer struct and
        invalidate the cache before passing it to HSM
        */
        HSMCLIENT_CACHE_WB_INV(pFirmwareUpdateObject, GET_CACHE_ALIGNED_SIZE(sizeof(FirmwareUpdateReq_t)), CacheP_TYPE_ALL);
        status = HsmClient_SendAndRecv(HsmClient, timeout);

        if (status == SystemP_SUCCESS)
        {
            /* the FW update cert process request has been completed by HSM server
             * if this request has been processed correctly */
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Firmware update code programming request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                /* Change the Arguments Address in Physical Address */
                HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);
                /* check the integrity of args */
                crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args), 0);
                if (crcArgs == HsmClient->RespMsg.crcArgs)
                {
                    status = SystemP_SUCCESS;
                }
                else
                {
                    DebugP_log("\r\n [HSM_CLIENT] CRC check for firmware update code programming response failed \r\n");
                    status = SystemP_FAILURE;
                }
            }
        }
    }
    else
    {
        /* Do nothing */
    }

    return status;
}

int32_t HsmClient_firmwareUpdate_CodeVerify(HsmClient_t *HsmClient,
                                            FirmwareUpdateReq_t *pFirmwareUpdateObject)
{
    int32_t status = SystemP_SUCCESS;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;

    /* Proceed only if HsmClient_firmwareUpdate_CodeProgram API call has been made */
    if (SystemP_SUCCESS == status)
    {
        /*populate the send message structure */
        HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
        HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

        /* Always expect acknowledgement from HSM server */
        HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
        HsmClient->ReqMsg.serType = HSM_MSG_FW_UPDATE_CODE_VERIFY;

        /* Address and size check */
        if ((pFirmwareUpdateObject->pStartAddress == NULL) && (pFirmwareUpdateObject->dataLength == 0))
        {
            status = SystemP_SUCCESS;
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }

    /* Proceed only if address and size check falls within bounds */
    if (SystemP_SUCCESS == status)
    {
        /* Convert pDecryptionBuffer address to HSM memory space */   
        pFirmwareUpdateObject->pDecryptionBuffer = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject->pDecryptionBuffer);
        /* Add arg crc */
        HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pFirmwareUpdateObject, sizeof(FirmwareUpdateReq_t));
        /* Change the Arguments Address in Physical Address */
        HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject);

        /*
        Write back the HsmVer struct and
        invalidate the cache before passing it to HSM
        */
        HSMCLIENT_CACHE_WB_INV(pFirmwareUpdateObject, GET_CACHE_ALIGNED_SIZE(sizeof(FirmwareUpdateReq_t)), CacheP_TYPE_ALL);
        status = HsmClient_SendAndRecv(HsmClient, timeout);

        if (SystemP_SUCCESS == status)
        {
            /* the FW update cert process request has been completed by HSM server
             * if this request has been processed correctly */
            if (HSM_FLAG_NACK == HsmClient->RespFlag)
            {
                DebugP_log("\r\n [HSM_CLIENT] Firmware update certificate programming request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                /* Change the Arguments Address in Physical Address */
                HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);
                /* check the integrity of args */
                crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args), 0);
                if (crcArgs == HsmClient->RespMsg.crcArgs)
                {
                    status = SystemP_SUCCESS;
                }
                else
                {
                    DebugP_log("\r\n [HSM_CLIENT] CRC check for firmware update certificate programming response failed \r\n");
                    status = SystemP_FAILURE;
                }
            }
        }
    }
    else
    {
        /* Do nothing */
    }

    return status;
}

int32_t HsmClient_SecCfgUpdate(HsmClient_t *HsmClient,
                                            FirmwareUpdateReq_t *pFirmwareUpdateObject)
{
    int32_t status = SystemP_FAILURE;
    uint16_t crcArgs;
    uint32_t timeout = SystemP_WAIT_FOREVER;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;
    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_FW_UPDATE_SECCFG;
    /* Convert sec-cfg start address to HSM memory space */   
    pFirmwareUpdateObject->pStartAddress = (uint8_t *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject->pStartAddress);
    /* Convert pDecryptionBuffer address to HSM memory space */   
    pFirmwareUpdateObject->pDecryptionBuffer = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject->pDecryptionBuffer);
    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pFirmwareUpdateObject, sizeof(FirmwareUpdateReq_t));
    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pFirmwareUpdateObject);
    /*
     * Write back the pFirmwareUpdateObject struct and
     * invalidate the cache before passing it to HSM
     */
    HSMCLIENT_CACHE_WB_INV(pFirmwareUpdateObject, GET_CACHE_ALIGNED_SIZE(sizeof(FirmwareUpdateReq_t)), CacheP_TYPE_ALL);
    /* Send SIPC message and wait for response from HSM */
    status = HsmClient_SendAndRecv(HsmClient, timeout);
    /* Check if HSM has responded to message request */
    if (SystemP_SUCCESS == status) {
        /* Check if the service request has been NACKED by HSM or not */
        if (HSM_FLAG_NACK == HsmClient->RespFlag) {
            DebugP_log("\r\n [HSM_CLIENT] Sec-Cfg update nacked by HSM server\r\n");
            status = SystemP_FAILURE;
        } else {
            /* Change the Arguments Address in Physical Address */
            HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);
            /* check the integrity of args */
            crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args), 0);
            if (crcArgs == HsmClient->RespMsg.crcArgs) {
                status = SystemP_SUCCESS;
            } else {
                DebugP_log("\r\n [HSM_CLIENT] CRC check for Sec-Cfg update service response failed \r\n");
                status = SystemP_FAILURE;
            }
        }
    } else {
        /* Do nothing */
    }

    return status;
}

int32_t HsmClient_VerifyROTSwitchingCertificate(HsmClient_t *HsmClient,
                                                uint8_t *cert,
                                                uint32_t cert_size,
                                                uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    void *ipcBuff;

    /* populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_VERIFY_ROT_CERT;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(cert, cert_size);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, cert_size);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
    Write back the RoT cert and
    invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (SystemP_SUCCESS == status) {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0U);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(cert, cert_size);

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the RoT Switch has been populated by HSM server
             * if this request has been processed correctly */
            if (HSM_FLAG_NACK == HsmClient->RespFlag)
            {
                DebugP_log("\r\n [HSM_CLIENT] RoT Switching Certificate Verification request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for RoT Switching Certificate Verification response failed \r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (SystemP_FAILURE == status) {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_UpdateKeyRevsion(HsmClient_t *HsmClient, uint32_t timeout) {
    /* make the message */
    int32_t status;
    uint16_t crcArgs;

    /* populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_UPDATE_KEY_REV;

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)NULL, 0U);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = NULL;

    status = HsmClient_SendAndRecv(HsmClient, timeout);

    if (SystemP_SUCCESS == status) {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)(HsmClient->RespMsg.args),0);
        if (crcArgs == HsmClient->RespMsg.crcArgs) {
            /* the update key revision request has been processed by HSM server
            * if this request has been processed correctly */
            if (HSM_FLAG_NACK == HsmClient->RespFlag) {
                DebugP_log("\r\n [HSM_CLIENT] Update Key Revision request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            } else {
                status = SystemP_SUCCESS;
            }
        } else {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for update key revision response failed \r\n");
            status = SystemP_FAILURE;
        }
    } else if (SystemP_FAILURE == status) {
        /* If failure occur due to some reason */
        status = SystemP_FAILURE;
    } else {
        /* Indicate timeout error */
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_EnableFATransition(HsmClient_t *HsmClient,
                                     uint8_t *cert,
                                     uint32_t cert_size,
                                     uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;

    /* populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_ENABLE_FA;
    HsmClient->ReqMsg.args = (void *)cert;

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)cert, cert_size);

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(cert);

    /*
    Write back the FA transition cert and
    invalidate the cache before passing it to HSM
    */
    CacheP_wbInv(cert, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (SystemP_SUCCESS == status) {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(cert_size), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0U);

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the FA Transition has been processed by HSM server
             * if this request has been processed correctly */
            if (HSM_FLAG_NACK == HsmClient->RespFlag)
            {
                DebugP_log("\r\n [HSM_CLIENT] FA Transition request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for FA Transition response failed\r\n");
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (SystemP_FAILURE == status) {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}


int32_t HsmClient_configOTFARegions(HsmClient_t* HsmClient,OTFA_Config_t* OTFA_ConfigInfo,uint32_t timeout)
{
    /* make the message */
    int32_t status ;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_CONFIGURE_OTFA;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(OTFA_ConfigInfo, sizeof(OTFA_Config_t));

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t*)ipcBuff,sizeof(OTFA_Config_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void*)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the OTFA_ConfigInfo struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(OTFA_Config_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient,timeout);

    if(status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void*)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(OTFA_Config_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t*)(HsmClient->RespMsg.args),sizeof(OTFA_Config_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(OTFA_ConfigInfo, sizeof(OTFA_Config_t));

        if(crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the OTFA_ConfigInfo has been populated by HSM server
             * if this request has been processed correctly */
            if(HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Configure OTFA request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for OTFA_configuration response failed \r\n");
            status = SystemP_FAILURE ;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_readOTFARegions(HsmClient_t* HsmClient,OTFA_readRegion_t* OTFA_readRegion,uint32_t timeout)
{
    /* make the message */
    int32_t status ;
    uint16_t crcArgs;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_READ_OTFA;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(OTFA_readRegion, sizeof(OTFA_readRegion_t));

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t*)ipcBuff,sizeof(OTFA_readRegion_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void*)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the OTFA_readRegion struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(OTFA_readRegion_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient,timeout);

    if(status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address in Physical Address */
        HsmClient->RespMsg.args = (void*)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate the cache so the response written by the HSM server is
           actually read from memory, not a stale cached copy */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(OTFA_readRegion_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t*)(HsmClient->RespMsg.args),sizeof(OTFA_readRegion_t));

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(OTFA_readRegion, sizeof(OTFA_readRegion_t));

        if(crcArgs == HsmClient->RespMsg.crcArgs)
        {
            /* the OTFA_readRegion has been populated by HSM server
             * if this request has been processed correctly */
            if(HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] Read OTFA request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for OTFA_read response failed \r\n");
            status = SystemP_FAILURE ;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_secCfgValidate(HsmClient_t *HsmClient,
                                 SecCfgValidate_t *pSecCfgParams,
                                 uint32_t timeout) {
    /* make the message */
    int32_t status = SystemP_FAILURE;
    uint16_t crcArgs;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_PROC_AUTH_BOOT_SEC_CFG;
    HsmClient->ReqMsg.args = (void *)pSecCfgParams;

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pSecCfgParams, sizeof(SecCfgValidate_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pSecCfgParams);

    /*
        Write back the cert and
        invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(pSecCfgParams, GET_CACHE_ALIGNED_SIZE(sizeof(SecCfgValidate_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* the secCfgValidate response has been populated by HSM server
         * if this request has been processed correctly */
        if (HsmClient->RespFlag == HSM_FLAG_NACK)
        {
            DebugP_log("\r\n [HSM_CLIENT] Sec-Cfg validation request NACKed by HSM server\r\n");
            status = SystemP_FAILURE;
        }
        else
        {
            /* Change the Arguments Address in Physical Address */
            HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

            /* check the integrity of args */
            crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, 0);

            if (crcArgs == HsmClient->RespMsg.crcArgs)
            {
                status = SystemP_SUCCESS;
            }
            else
            {
                DebugP_log("\r\n [HSM_CLIENT] CRC check for Sec-Cfg validation response failed \r\n");
                status = SystemP_FAILURE;
            }
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

int32_t HsmClient_CryptoService(HsmClient_t *HsmClient,
                                 CryptoServiceReq_t *svcReq,
                                 CryptoServiceReq_t *respReq,
                                 uint32_t timeout)
{
    int32_t  status;
    uint16_t crcArgs;
    uint32_t tagSize = 0;
    CMACArgs_t *cmac = NULL, *respCmac = NULL;
    HMACArgs_t *hmac = NULL, *respHmac = NULL;
    GMACArgs_t *gmac = NULL, *respGmac = NULL;
    void *ipcBuff;
    CryptoServiceReq_t *effReq;
    (void)tagSize;

    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId  = HsmClient->ClientId;
    HsmClient->ReqMsg.flags        = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType      = HSM_MSG_CRYPTO_SERVICE;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(svcReq, sizeof(CryptoServiceReq_t));
    effReq  = (CryptoServiceReq_t *)ipcBuff;

    HsmClient->ReqMsg.args         = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /* wbInv data buffers while all pointers are still virtual */
    switch (effReq->algoId)
    {
        case HSM_CRYPTO_SVC_MAC_CMAC:
        {
            cmac = (CMACArgs_t *)effReq->ptrArgs;
            HSMCLIENT_CACHE_WB_INV(cmac->ptrData,
                         GET_CACHE_ALIGNED_SIZE(cmac->dataLen), CacheP_TYPE_ALL);
            if (effReq->subSvcId == HSM_CRYPTO_SVC_MAC_VERIFY)
            {
                /* flush expected tag so HSM can read it; CMAC tag is always 16 bytes */
                HSMCLIENT_CACHE_WB_INV(cmac->ptrTag,
                             GET_CACHE_ALIGNED_SIZE(16U), CacheP_TYPE_ALL);
            }
            break;
        }
        case HSM_CRYPTO_SVC_MAC_HMAC:
        {
            hmac = (HMACArgs_t *)effReq->ptrArgs;
            HSMCLIENT_CACHE_WB_INV(hmac->ptrData,
                         GET_CACHE_ALIGNED_SIZE(hmac->dataLen), CacheP_TYPE_ALL);
            if (effReq->subSvcId == HSM_CRYPTO_SVC_MAC_VERIFY)
            {
                /* flush expected tag so HSM can read it */
                tagSize = (hmac->hashMode == HSM_CRYPTO_HMAC_SHA512) ? 64U : 32U;
                HSMCLIENT_CACHE_WB_INV(hmac->ptrTag,
                             GET_CACHE_ALIGNED_SIZE(tagSize), CacheP_TYPE_ALL);
            }
            break;
        }
        case HSM_CRYPTO_SVC_MAC_GMAC:
        {
            gmac = (GMACArgs_t *)effReq->ptrArgs;
            HSMCLIENT_CACHE_WB_INV(gmac->ptrData,
                         GET_CACHE_ALIGNED_SIZE(gmac->dataLen), CacheP_TYPE_ALL);
            HSMCLIENT_CACHE_WB_INV(gmac->ptrIV,
                         GET_CACHE_ALIGNED_SIZE(gmac->ivLen), CacheP_TYPE_ALL);
            if (effReq->subSvcId == HSM_CRYPTO_SVC_MAC_VERIFY)
            {
                /* flush expected tag so HSM can read it; GMAC tag is always 16 bytes */
                HSMCLIENT_CACHE_WB_INV(gmac->ptrTag,
                             GET_CACHE_ALIGNED_SIZE(16U), CacheP_TYPE_ALL);
            }
            break;
        }
        default:
            break;
    }

    /* convert nested pointers to physical, then wbInv inner struct */
    switch (effReq->algoId)
    {
        case HSM_CRYPTO_SVC_MAC_CMAC:
        {
            cmac = (CMACArgs_t *)effReq->ptrArgs;
            cmac->ptrData = (uint8_t *)(uintptr_t)SOC_virtToPhy(cmac->ptrData);
            cmac->ptrTag  = (uint8_t *)(uintptr_t)SOC_virtToPhy(cmac->ptrTag);
            HSMCLIENT_CACHE_WB_INV(effReq->ptrArgs,
                         GET_CACHE_ALIGNED_SIZE(sizeof(CMACArgs_t)), CacheP_TYPE_ALL);
            break;
        }
        case HSM_CRYPTO_SVC_MAC_HMAC:
        {
            hmac = (HMACArgs_t *)effReq->ptrArgs;
            hmac->ptrData = (uint8_t *)(uintptr_t)SOC_virtToPhy(hmac->ptrData);
            hmac->ptrTag  = (uint8_t *)(uintptr_t)SOC_virtToPhy(hmac->ptrTag);
            HSMCLIENT_CACHE_WB_INV(effReq->ptrArgs,
                         GET_CACHE_ALIGNED_SIZE(sizeof(HMACArgs_t)), CacheP_TYPE_ALL);
            break;
        }
        case HSM_CRYPTO_SVC_MAC_GMAC:
        {
            gmac = (GMACArgs_t *)effReq->ptrArgs;
            gmac->ptrData = (uint8_t *)(uintptr_t)SOC_virtToPhy(gmac->ptrData);
            gmac->ptrTag  = (uint8_t *)(uintptr_t)SOC_virtToPhy(gmac->ptrTag);
            gmac->ptrIV   = (uint8_t *)(uintptr_t)SOC_virtToPhy(gmac->ptrIV);
            HSMCLIENT_CACHE_WB_INV(effReq->ptrArgs,
                         GET_CACHE_ALIGNED_SIZE(sizeof(GMACArgs_t)), CacheP_TYPE_ALL);
            break;
        }
        default:
            break;
    }

    /* convert outer ptrArgs to physical, CRC, then wbInv outer struct */
    effReq->ptrArgs = (void *)(uintptr_t)SOC_virtToPhy(effReq->ptrArgs);
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)effReq, sizeof(CryptoServiceReq_t));
    HSMCLIENT_CACHE_WB_INV(effReq, GET_CACHE_ALIGNED_SIZE(sizeof(CryptoServiceReq_t)), CacheP_TYPE_ALL);

    /* send to HSM */
    status = HsmClient_SendAndRecv(HsmClient, timeout);

    if (status == SystemP_SUCCESS)
    {
        /* phys to virt for outer args, then inv outer struct - done regardless of
           the ack/nack flag, since the HSM server writes a real errCode into the
           response struct on NACK too */
        HsmClient->RespMsg.args =
            (void *)SOC_phyToVirt((uint64_t)(uintptr_t)HsmClient->RespMsg.args);
        HSMCLIENT_CACHE_INV(HsmClient->RespMsg.args,
                   GET_CACHE_ALIGNED_SIZE(sizeof(CryptoServiceReq_t)), CacheP_TYPE_ALL);

        *respReq = *(CryptoServiceReq_t *)HsmClient->RespMsg.args;

        /* flush now, before any later CacheP_inv on the (unaligned) nested
         * args struct can discard this dirty write if it shares a cache line */
        CacheP_inv(respReq, GET_CACHE_ALIGNED_SIZE(sizeof(CryptoServiceReq_t)), CacheP_TYPE_ALL);

        /* check the integrity of args first, regardless of the ack/nack flag,
           so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)respReq, sizeof(CryptoServiceReq_t));

        /* phys to virt for inner ptrArgs, inv inner struct, restore nested pointers, inv written buffers */
        respReq->ptrArgs =
            (void *)SOC_phyToVirt((uint64_t)(uintptr_t)respReq->ptrArgs);

        switch (svcReq->algoId)
        {
            case HSM_CRYPTO_SVC_MAC_CMAC:
            {
                respCmac = (CMACArgs_t *)respReq->ptrArgs;
                HSMCLIENT_CACHE_INV(respReq->ptrArgs,
                           GET_CACHE_ALIGNED_SIZE(sizeof(CMACArgs_t)), CacheP_TYPE_ALL);
                respCmac->ptrData =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respCmac->ptrData);
                respCmac->ptrTag =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respCmac->ptrTag);
                /* for GENERATE: inv tag buffer to read HSM-written CMAC */
                if (svcReq->subSvcId == HSM_CRYPTO_SVC_MAC_GENERATE)
                {
                    HSMCLIENT_CACHE_INV(respCmac->ptrTag,
                               GET_CACHE_ALIGNED_SIZE(16U), CacheP_TYPE_ALL);
                }
                break;
            }
            case HSM_CRYPTO_SVC_MAC_HMAC:
            {
                respHmac = (HMACArgs_t *)respReq->ptrArgs;
                HSMCLIENT_CACHE_INV(respReq->ptrArgs,
                           GET_CACHE_ALIGNED_SIZE(sizeof(HMACArgs_t)), CacheP_TYPE_ALL);
                respHmac->ptrData =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respHmac->ptrData);
                respHmac->ptrTag =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respHmac->ptrTag);
                /* for GENERATE: inv tag buffer to read HSM-written HMAC */
                if (svcReq->subSvcId == HSM_CRYPTO_SVC_MAC_GENERATE)
                {
                    tagSize = (respHmac->hashMode == HSM_CRYPTO_HMAC_SHA512) ? 64U : 32U;
                    HSMCLIENT_CACHE_INV(respHmac->ptrTag,
                               GET_CACHE_ALIGNED_SIZE(tagSize), CacheP_TYPE_ALL);
                }
                break;
            }
            case HSM_CRYPTO_SVC_MAC_GMAC:
            {
                respGmac = (GMACArgs_t *)respReq->ptrArgs;
                HSMCLIENT_CACHE_INV(respReq->ptrArgs,
                           GET_CACHE_ALIGNED_SIZE(sizeof(GMACArgs_t)), CacheP_TYPE_ALL);
                respGmac->ptrData =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respGmac->ptrData);
                respGmac->ptrTag =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respGmac->ptrTag);
                respGmac->ptrIV =
                    (uint8_t *)SOC_phyToVirt((uint64_t)(uintptr_t)respGmac->ptrIV);
                /* for GENERATE: inv tag buffer to read HSM-written GMAC */
                if (svcReq->subSvcId == HSM_CRYPTO_SVC_MAC_GENERATE)
                {
                    HSMCLIENT_CACHE_INV(respGmac->ptrTag,
                               GET_CACHE_ALIGNED_SIZE(16U), CacheP_TYPE_ALL);
                }
                break;
            }
            default:
                break;
        }

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(svcReq, sizeof(CryptoServiceReq_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            if (HsmClient->RespFlag == HSM_FLAG_NACK)
            {
                DebugP_log("\r\n [HSM_CLIENT] CryptoService request NACKed by HSM server\r\n");
                status = SystemP_FAILURE;
            }
            else
            {
                status = SystemP_SUCCESS;
            }
        }
        else
        {
            DebugP_log("\r\n [HSM_CLIENT] CRC check for CryptoService response failed\r\n");
            status = SystemP_FAILURE;
        }
    }
    return status;
}

int32_t HsmClient_activeToDormantBankCopy(HsmClient_t *HsmClient,
                                          FlashBankCopy_t *pFlashBankCopyObject,
                                          uint32_t timeout) {
    /* make the message */
    int32_t status;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_FLASH_BANK_COPY;

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pFlashBankCopyObject, sizeof(FlashBankCopy_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pFlashBankCopyObject);

    status = HsmClient_SendAndRecv(HsmClient, timeout);

    if (status == SystemP_SUCCESS) {
        /* if this request has not been processed correctly */
        if (HsmClient->RespFlag == HSM_FLAG_NACK) {
            DebugP_log("\r\n [HSM_CLIENT] Flash Bank swap request NACKed by HSM server\r\n");
            status = SystemP_FAILURE;
        } else {
            status = SystemP_SUCCESS;
        }
    /* If failure occur due to some reason */
    } else {
        /* Do Nothing */
    }
    return status;
}

int32_t HsmClient_runTimeBankSwap(HsmClient_t *HsmClient,
                            BankSwapReq_t *pBankSwapObject)
{
    int32_t status = SystemP_FAILURE;
    uint32_t timeout = SystemP_WAIT_FOREVER;

    /* Populate destination ID */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    /* Populate Source ID */
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;
    /* Populate parameter to wait for acknowledement from HSM or not */
    HsmClient->ReqMsg.flags = HSM_FLAG_NAOP;
    /* Populate service ID */
    HsmClient->ReqMsg.serType = HSM_MSG_BANK_SWAP;
    /* Calculate CRC arguments */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)pBankSwapObject, sizeof(BankSwapReq_t));
    /* Convert C29 address to HSM readable address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(pBankSwapObject);
    /* Send message to HSM */
    status = HsmClient_SendAndRecv(HsmClient, timeout);
    /* Check if message was sent successfully */
    if (SystemP_SUCCESS == status) {
        /* Check if response received from HSM was correct */
        if (HsmClient->RespFlag == HSM_FLAG_NACK) {
            status = SystemP_FAILURE;
        } else if (HsmClient->RespFlag == HSM_FLAG_ACK) {
            status = SystemP_SUCCESS;
        } else {
            status = SystemP_FAILURE;
        }
    } else {
        /* Do nothing */
    }

    return status;
}

int32_t HsmClient_getDeviceConfig(HsmClient_t *HsmClient,
                                   DeviceConfigRead_t *pDeviceConfigObject,
                                   uint32_t timeout)
{
    /* make the message */
    int32_t status;
    uint16_t crcArgs;
    uint16_t crcConfigData;
    void *ipcBuff;

    /*populate the send message structure */
    HsmClient->ReqMsg.destClientId = HSM_CLIENT_ID_1;
    HsmClient->ReqMsg.srcClientId = HsmClient->ClientId;

    /* Always expect acknowledgement from HSM server */
    HsmClient->ReqMsg.flags = HSM_FLAG_AOP;
    HsmClient->ReqMsg.serType = HSM_MSG_GET_DEVICE_CONFIG;

    /* Stage the application buffer into the driver-owned host buffer */
    ipcBuff = HsmClient_getIPCBuffPtr(pDeviceConfigObject, sizeof(DeviceConfigRead_t));

    /* Convert pointer argument to physical address */
    ((DeviceConfigRead_t *)ipcBuff)->configData = (uint32_t *)(uintptr_t)SOC_virtToPhy(((DeviceConfigRead_t *)ipcBuff)->configData);

    /* Add arg crc */
    HsmClient->ReqMsg.crcArgs = crc16_ccit((uint8_t *)ipcBuff, sizeof(DeviceConfigRead_t));

    /* Change the Arguments Address in Physical Address */
    HsmClient->ReqMsg.args = (void *)(uintptr_t)SOC_virtToPhy(ipcBuff);

    /*
       Write back the DeviceConfigRead_t struct and
       invalidate the cache before passing it to HSM
    */
    HSMCLIENT_CACHE_WB_INV(ipcBuff, GET_CACHE_ALIGNED_SIZE(sizeof(DeviceConfigRead_t)), CacheP_TYPE_ALL);

    status = HsmClient_SendAndRecv(HsmClient, timeout);
    if (status == SystemP_SUCCESS)
    {
        /* Change the Arguments Address back to Virtual Address */
        HsmClient->RespMsg.args = (void *)SOC_phyToVirt((uint64_t)HsmClient->RespMsg.args);

        /* Invalidate cache to get updated data from HSM */
        HSMCLIENT_CACHE_INV((void *)HsmClient->RespMsg.args, GET_CACHE_ALIGNED_SIZE(sizeof(DeviceConfigRead_t)), CacheP_TYPE_ALL);

        /* check the integrity of args structure first, regardless of the
           ack/nack flag, so a corrupted response is never treated as valid data */
        crcArgs = crc16_ccit((uint8_t *)HsmClient->RespMsg.args, sizeof(DeviceConfigRead_t));

        /* Convert pointer field back to virtual address */
        ((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configData = (uint32_t *)SOC_phyToVirt((uint64_t)(((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configData));

        /* Invalidate cache for the config data buffer (use updated configSize from HSM) */
        HSMCLIENT_CACHE_INV((void *)((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configData,
              GET_CACHE_ALIGNED_SIZE(((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configSize), CacheP_TYPE_ALL);

        /* Verify the CRC of the configuration data buffer */
        crcConfigData = crc16_ccit((uint8_t *)((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configData,
                                  ((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configSize);

        /* A response was received: sync the host buffer back to the caller's
           struct */
        HsmClient_syncIPCBuffPtr(pDeviceConfigObject, sizeof(DeviceConfigRead_t));

        if (crcArgs == HsmClient->RespMsg.crcArgs)
        {
            if (crcConfigData == ((DeviceConfigRead_t *)HsmClient->RespMsg.args)->configDataCRC)
            {
                /* the device config has been populated by HSM server
                 * if this request has been processed correctly */
                if (HsmClient->RespFlag == HSM_FLAG_NACK)
                {
                    status = SystemP_FAILURE;
                }
                else
                {
                    status = SystemP_SUCCESS;
                }
            }
            else
            {
                status = SystemP_FAILURE;
            }
        }
        else
        {
            status = SystemP_FAILURE;
        }
    }
    /* If failure occur due to some reason */
    else if (status == SystemP_FAILURE)
    {
        status = SystemP_FAILURE;
    }
    /* Indicate timeout error */
    else
    {
        status = SystemP_TIMEOUT;
    }
    return status;
}

