/*
 *  Copyright (C) 2026 Texas Instruments Incorporated
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

/* This example demonstrates the implementation of DTHE SHA-384 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <security/security_common/drivers/crypto/crypto.h>
#include <security/security_common/drivers/crypto/dthe/dthe.h>
#include <security/security_common/drivers/crypto/dthe/dthe_sha.h>
#include <security/security_common/drivers/crypto/dthe/dma.h>
#include <security/security_common/drivers/crypto/dthe/dma/edma/dthe_edma.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* SHA384 length */
#define APP_CRYPTO_SHA384_LENGTH                (48U)
/* Alignment */
#define APP_CRYPTO_SHA_BUF_ALIGNMENT            (128U)
/*Input and output buf length*/
#define APP_CRYPTO_SHA_INPUT_BUF_LENGTH         (9U)
/* DTHE Public address */
#define CSL_DTHE_PUBLIC_U_BASE                  (0xCE000810U)
/* DTHE Aes Public address */
#define CSL_DTHE_PUBLIC_AES_U_BASE              (0xCE007000U)
/* DTHE Aes Public address */
#define CSL_DTHE_PUBLIC_SHA_U_BASE              (0xCE005000U)

/* EDMA config instance */
#define CONFIG_EDMA_NUM_INSTANCES               (1U)

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/*Input test buffer for sha computation */
static uint8_t gCryptoShaTestInputBuf[APP_CRYPTO_SHA_INPUT_BUF_LENGTH] = {"abcdefpra"};

/* SHA-384 test vectors, this is expected hash for the test buffer (NIST CAVP Len=72) */
static uint8_t gCryptoSha384TestSum[APP_CRYPTO_SHA384_LENGTH] =
{
    0xb7, 0x20, 0x0d, 0xf0, 0x6b, 0xb3, 0x02, 0x8b,
    0x9d, 0x64, 0x98, 0x0b, 0x26, 0x5c, 0x4f, 0x03,
    0x09, 0xbe, 0x5b, 0xb1, 0x01, 0x59, 0x6d, 0xfd,
    0x22, 0x65, 0xf0, 0x62, 0x38, 0xea, 0x9d, 0xa5,
    0xf8, 0xe6, 0xab, 0xfa, 0xc5, 0xef, 0xb2, 0xc5,
    0xf0, 0xe6, 0x96, 0x1f, 0x8b, 0x05, 0x30, 0x06
};

/* Edma handler*/
EDMA_Handle gEdmaHandle[CONFIG_EDMA_NUM_INSTANCES];

/* Public context crypto dthe, aes and sha accelerators base address */
DTHE_Attrs gDTHE_Attrs[1] =
{
    {
        /* crypto accelerator base address */
        .caBaseAddr         = CSL_DTHE_PUBLIC_U_BASE,
        /* AES base address */
        .aesBaseAddr        = CSL_DTHE_PUBLIC_AES_U_BASE,
        /* SHA base address */
        .shaBaseAddr        = CSL_DTHE_PUBLIC_SHA_U_BASE,
        /* For checking dthe driver open or close */
        .isOpen             = FALSE,
    },
};


DTHE_Config gDtheConfig[1]=
{
    {
        &gDTHE_Attrs[0],
        DMA_DISABLE,
    },
};

uint32_t gDtheConfigNum = 1;

DMA_Config gDmaConfig[1]=
{
    {
        &gEdmaHandle[0],
        &gEdmaFxns,
    },
};
uint32_t gDmaConfigNum = 1;


/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void crypto_sha_384_main(void *args)
{
    Drivers_open();
    Board_driversOpen();

    DTHE_SHA_Return_t   status;
    DTHE_Handle         shaHandle;
    DTHE_SHA_Params     shaParams;

    DebugP_log("[CRYPTO] SHA example started ...\r\n");

    /* Opening crypto driver */
    shaHandle = DTHE_open(0);
    DebugP_assert(shaHandle != NULL);

    /* Opening sha driver */
    status = DTHE_SHA_open(shaHandle);
    DebugP_assert(DTHE_SHA_RETURN_SUCCESS == status);

    /* Initialize the SHA Parameters */
    shaParams.algoType          = DTHE_SHA_ALGO_SHA384;
    shaParams.ptrDataBuffer     = (uint32_t*)&gCryptoShaTestInputBuf[0];
    shaParams.dataLenBytes      = APP_CRYPTO_SHA_INPUT_BUF_LENGTH;

    /* Performing DTHE SHA operation */
    status = DTHE_SHA_compute(shaHandle, &shaParams, TRUE);
    DebugP_assert(DTHE_SHA_RETURN_SUCCESS == status);

    /* Closing sha driver */
    status = DTHE_SHA_close(shaHandle);
    DebugP_assert(DTHE_SHA_RETURN_SUCCESS == status);

    /* Closing DTHE driver */
    if (DTHE_RETURN_SUCCESS == DTHE_close(shaHandle))
    {
        status = DTHE_SHA_RETURN_SUCCESS;
    }
    else
    {
        status = DTHE_SHA_RETURN_FAILURE;
    }

    DebugP_assert(DTHE_SHA_RETURN_SUCCESS == status);

    /*comparing result with expected test results*/
    if(memcmp(shaParams.digest, gCryptoSha384TestSum, APP_CRYPTO_SHA384_LENGTH) != 0)
    {
        DebugP_log("[CRYPTO] SHA-384 example failed!!\r\n");
    }
    else
    {
        DebugP_log("[CRYPTO] SHA-384 example completed!!\r\n");
        DebugP_log("All tests have passed!!\r\n");
    }

    Board_driversClose();
    Drivers_close();

    return;
}
