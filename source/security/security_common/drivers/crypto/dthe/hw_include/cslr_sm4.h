/********************************************************************
 * Copyright (C) 2025 Texas Instruments Incorporated.
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
 *
*/
#ifndef CSLR_SM4_H_
#define CSLR_SM4_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <drivers/hw_include/cslr.h>
#include <stdint.h>


/**************************************************************************
* Register Overlay Structure for __ALL__
**************************************************************************/
typedef struct {
    volatile uint32_t SM4_DATA_IN_0;
    volatile uint32_t SM4_DATA_IN_1;
    volatile uint32_t SM4_DATA_IN_2;
    volatile uint32_t SM4_DATA_IN_3;
    volatile uint8_t  Resv_32[16U];
    volatile uint32_t SM4_KEY_IN_0;
    volatile uint32_t SM4_KEY_IN_1;
    volatile uint32_t SM4_KEY_IN_2;
    volatile uint32_t SM4_KEY_IN_3;
    volatile uint8_t  Resv_64[16U];
    volatile uint32_t SM4_PARAM_IN_0;
    volatile uint32_t SM4_PARAM_IN_1;
    volatile uint32_t SM4_PARAM_IN_2;
    volatile uint32_t SM4_PARAM_IN_3;
    volatile uint32_t SM4_IO_BUF_CTRL_STAT;
    volatile uint32_t SM4_MODE;
    volatile uint32_t SM4_DATA_OUT_0;
    volatile uint32_t SM4_DATA_OUT_1;
    volatile uint32_t SM4_DATA_OUT_2;
    volatile uint32_t SM4_DATA_OUT_3;
    volatile uint32_t SM4_IV_IN_OUT_0;
    volatile uint32_t SM4_IV_IN_OUT_1;
    volatile uint32_t SM4_IV_IN_OUT_2;
    volatile uint32_t SM4_IV_IN_OUT_3;
    volatile uint8_t  Resv_248[128U]; 
    volatile uint32_t SM4_CONFIG;
    volatile uint32_t SM4_VERSION;
    volatile uint32_t SM4_SYSCONFIG;
    volatile uint32_t SM4_IRQSTATUS;
    volatile uint32_t SM4_IRQENABLE;
    volatile uint32_t SM4_ENABLE;
    volatile uint32_t SM4_LENGTH;
} CSL_SM4Regs;

#ifdef __cplusplus
}
#endif
#endif