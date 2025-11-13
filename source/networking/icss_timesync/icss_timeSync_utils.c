/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
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

#include "icss_timeSync_utils.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/**Starting offset for an IP header in TCP/IP packet*/
#define START_OF_IP_HEADER (14)
/**Starting offset for checksum in IP header in TCP/IP packet*/
#define START_OF_IP_CHECKSUM (START_OF_IP_HEADER + 10)
/**standard size for IP header in TCP/IP packet*/
#define DEFAULT_IP_HEADER_SIZE 20

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void TimeSync_addWord(uint8_t *src, uint32_t word)
{
    *(src) = (word >> 24) & 0xff;
    *(src + 1) = (word >> 16) & 0xff;
    *(src + 2) = (word >> 8) & 0xff;
    *(src + 3) = word & 0xff;
}

void TimeSync_addHalfWord(volatile uint8_t *src, uint16_t halfWord)
{
    *(src) = (halfWord >> 8) & 0xff;
    *(src + 1) = (halfWord) & 0xff;
}

uint32_t TimeSync_calcChecksum(uint8_t *packet, uint16_t len)
{
    uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */
    uint16_t shortVal;
    uint8_t byte1, byte2;

    while(len > 1)
    {
        byte1 = *(packet++);
        byte2 = *(packet++);

        shortVal = byte1;
        shortVal = (shortVal << 8 | byte2);

        sum += shortVal;

        if(sum & 0xffff0000)   /* if high order bit set, fold */
        {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        len -= 2;
    }

    if(len)         /* take care of left over byte */
    {
        sum += (uint16_t) * (uint8_t *)packet;
    }

    while(sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;

}

void TimeSync_calcIPChecksum(uint8_t *packet)
{
    uint8_t *src;
    uint32_t checksum;

    /*Reset the checksum to zero to recalculate*/
    src = packet + START_OF_IP_CHECKSUM;
    *(src++) = 0;
    *(src) = 0;

    src = packet + START_OF_IP_HEADER;

    checksum = TimeSync_calcChecksum(src, DEFAULT_IP_HEADER_SIZE);

    src = packet + START_OF_IP_CHECKSUM;
    *(src++) = (checksum >> 8) & 0xff;
    *(src) = checksum & 0xff;
}

void TimeSync_convEndianess(volatile void *src, volatile void *dst, uint8_t numBytes)
{
    uint8_t i;
    uint8_t *srcPtr = (uint8_t *)src;
    uint8_t *dstPtr = (uint8_t *)dst;

    /*If multiple of 2*/
    if((numBytes & 0x1) == 0)
    {
        dstPtr = dstPtr + numBytes - 1;

        for(i = 0; i < numBytes; i++)
        {
            *(dstPtr--) = *(srcPtr++);
        }
    }

}

void TimeSync_convEnd6to8(volatile void *src, void *dst)
{

    uint8_t *srcPtr = (uint8_t *)src;
    uint8_t *dstPtr = (uint8_t *)dst;

    dstPtr[0] = srcPtr[5];
    dstPtr[1] = srcPtr[4];
    dstPtr[2] = srcPtr[3];
    dstPtr[3] = srcPtr[2];
    dstPtr[4] = srcPtr[1];
    dstPtr[5] = srcPtr[0];

}

uint32_t TimeSync_getIEPCountValue(TimeSync_ParamsHandle_t timeSyncHandle)
{
    uint32_t iepCountVal = 0;
    
    switch(timeSyncHandle->timeSyncConfig.icssIepClkFreq)
    {
        case ICSS_IEP_CLK_FREQ_200MHZ:
            iepCountVal = 5; /* Since IEP is operating at 200 MHz, use counter increment value of 5 */
            break;
        case ICSS_IEP_CLK_FREQ_250MHZ:
            iepCountVal = 4; /* Since IEP is operating at 250 MHz, use counter increment value of 4 */
            break;
        case ICSS_IEP_CLK_FREQ_333MHZ:
            iepCountVal = 3; /* Since IEP is operating at 333 MHz, use counter increment value of 3 */
            break;
        default:
            iepCountVal = 5; /* Use counter increment value of 5 by default */
            break;
    }

    return iepCountVal;
}

uint32_t TimeSync_prepareIEPCfgCommand(TimeSync_ParamsHandle_t timeSyncHandle, compensationType compType)
{
    uint32_t iepCfgCmd = 0;
    uint32_t cntEnable, defaultInc, cmpInc;

    /* Enable the counter */
    cntEnable = 1U;
    /* Get the default counter increment value depending on the IEP Clock frequency */
    defaultInc = TimeSync_getIEPCountValue(timeSyncHandle);
    /* Get the counter increment value depending when the compensation is active */
    switch(compType)
    {
        case PTP_NO_COMPENSATION_REQUIRED:
            /* Since no compensation is required, compensation increment is set the same as default increment */
            cmpInc = defaultInc; 
            break;
        case PTP_TIME_RECEIVER_FASTER:
            /* Since the time receiver is faster, it needs to slow down. Therefore, set the compensation increment to 0 */
            cmpInc = 0U;
            break;
        case PTP_TIME_TRANSMITTER_FASTER:
            /* Since the time transmitter is faster, time receiver needs to run faster. Therefore, set the compensation increment to twice the value of the default increment */
            cmpInc = defaultInc * 2U; 
            break;
        default:
            cmpInc = defaultInc; 
            break;
    }

    /* Prepare the command for the IEP ICSS_IEP_GLOBAL_CFG register */
    /* Set CNT_ENABLE field in ICSS_IEP_GLOBAL_CFG register */
    iepCfgCmd |= (cntEnable & CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_CNT_ENABLE_MAX) << CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_CNT_ENABLE_SHIFT;
    /* Set DEFAULT_INC field in ICSS_IEP_GLOBAL_CFG register */
    iepCfgCmd |= (defaultInc & CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_DEFAULT_INC_MAX) << CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_DEFAULT_INC_SHIFT;
    /* Set CMP_INC field in ICSS_IEP_GLOBAL_CFG register */
    iepCfgCmd |= (cmpInc & CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_CMP_INC_MAX) << CSL_ICSS_PR1_IEP0_SLV_GLOBAL_CFG_REG_CMP_INC_SHIFT;

    return iepCfgCmd;
}