/*
 *  Copyright (C) 2022 Texas Instruments Incorporated
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

/**
 *  \file   dthe_sha.c
 *
 *  \brief  This file contains the implementation of Dthe sha driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <security_common/drivers/crypto/dthe/dthe_sha.h>
#include <security_common/drivers/crypto/dthe/dma.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/** \brief The Maximum HMAC Key Size is 128bytes or 1024bits for the SHA512. */
#define DTHE_SHA_MAX_HMAC_KEY_SIZE_BYTES        (128U)

/** \brief This is the Block Size for the SHA512 in words */
#define DTHE_SHA512_BLOCK_SIZE                  (32U)

/** \brief This is the Block Size for the SHA256 in words */
#define DTHE_SHA256_BLOCK_SIZE                  (16U)

/** \brief Maximum dataLenBytes allowed for a SHA512 block computation.
 *
 *  DTHE_SHA_writeDataBuffer() computes
 *  numBlocks = (uint16_t)((dataLenBytes / 4U) / DTHE_SHA512_BLOCK_SIZE), which
 *  must not exceed UINT16_MAX (65535) or numBlocks silently truncates. The
 *  largest dataLenBytes for which this holds is
 *  (((UINT16_MAX + 1U) * DTHE_SHA512_BLOCK_SIZE) * 4U) - 1U. */
#define DTHE_SHA512_MAX_DATA_LEN_BYTES           (8388607U)

/** \brief Maximum dataLenBytes allowed for a SHA256 block computation.
 *
 *  Same truncation risk as DTHE_SHA512_MAX_DATA_LEN_BYTES above, but for
 *  DTHE_SHA256_BLOCK_SIZE: (((UINT16_MAX + 1U) * DTHE_SHA256_BLOCK_SIZE) * 4U) - 1U. */
#define DTHE_SHA256_MAX_DATA_LEN_BYTES           (4194303U)

/** \brief This is the Data Shift Size for the SHA512 */
#define DTHE_SHA512_SHIFT_SIZE                  (5U)

/** \brief This is the Data Shift Size for the SHA256 */
#define DTHE_SHA256_SHIFT_SIZE                  (4U)

/** \brief The Maximum HMAC Key Size is 128bytes or 1024bits for the SHA512. */
#define DTHE_HMAC_SHA_MAX_KEY_SIZE_BYTES        (128U)
/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */
/* ========================================================================== */
/*                           Global variables                                */
/* ========================================================================== */
/** \brief Flag to check SHA in Progress */
Bool                    gDTHESHAInProgress;
/** \brief SHA Digest Count */
uint32_t                gDTHESHAdigestCount;
/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */
static void DTHE_SHA_setInterruptStatus(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t intStatus);
static void DTHE_SHA_setDMA(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t dmaStatus);
static void DTHE_SHA_pollContextReady(const CSL_EIP57T_SHARegs* ptrSHARegs);
static uint32_t DTHE_SHA_isContextReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs);
static void DTHE_SHA_setUseAlgoConstants(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t useAlgConstants);
static void DTHE_SHA_setCloseHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t closeHashFlag);
static void DTHE_SHA_setHMACKeyProcessing(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacKeyFlag);
static void DTHE_SHA_setHMACOuterHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacOuterHash);
static void DTHE_SHA512_setUseAlgoConstants(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t useAlgConstants);
static void DTHE_SHA512_setCloseHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t closeHashFlag);
static void DTHE_SHA512_setHMACKeyProcessing(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacKeyFlag);
static void DTHE_SHA512_setHMACOuterHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacOuterHash);
static void DTHE_SHA_setHashLength(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t length);
static void DTHE_SHA512_setHashLength(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t length);
static void DTHE_SHA_pollInputReady(const CSL_EIP57T_SHARegs* ptrSHARegs);
static void DTHE_SHA_writeDataBlock(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrDataBlock, uint8_t blockSize);
static void DTHE_SHA_pollOutputReady (const CSL_EIP57T_SHARegs* ptrSHARegs);
static void DTHE_SHA_getHashDigest(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDigest);
static void DTHE_SHA512_getHashDigest(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDigest);
static void DTHE_SHA_setAlgorithm(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algorithm);
static uint32_t DTHE_SHA_isOutputReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs);
static uint32_t DTHE_SHA_isInputReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs);
static uint32_t DTHE_SHA512_getDigestCount(const CSL_EIP57T_SHARegs* ptrSHARegs);
static uint32_t DTHE_SHA_getDigestCount(const CSL_EIP57T_SHARegs* ptrSHARegs);
static void DTHE_SHA_setHMACOuterKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey);
static void DTHE_SHA_setHMACInnerKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey);
static void DTHE_SHA512_setHMACOuterKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey);
static void DTHE_SHA512_setHMACInnerKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey);
static DTHE_SHA_Return_t DTHE_SHA_validateBlockAlignment(uint32_t algoType, uint32_t dataLenBytes, Bool isLastBlock);
static DTHE_SHA_Return_t DTHE_SHA_validateMaxDataLen(uint32_t algoType, uint32_t dataLenBytes);
static void DTHE_SHA_configureHashMode(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint8_t useAlgoConstants, uint8_t closeHash);
static void DTHE_SHA_setLengthAndBlockParams(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint32_t dataLenBytes, uint8_t* ptrBlockSize, uint32_t* ptrShiftSize);
static void DTHE_SHA_writeDataBuffer(uint32_t dmaEnable, CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDataBuffer, uint32_t dataLenBytes, uint8_t blockSize, uint32_t shiftSize);
static void DTHE_SHA_readDigestAndCount(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint32_t* ptrDigest);
static void DTHE_HMACSHA_preparePaddedKey(const DTHE_SHA_Params* ptrShaParams, uint32_t* ptrHmacPaddedKey);
static void DTHE_HMACSHA_configureKeyAndMode(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, const uint32_t* ptrHmacPaddedKey, uint32_t dataLenBytes, uint8_t* ptrBlockSize, uint32_t* ptrShiftSize);

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 *  Design: TIFSMCU-4384
 */

DTHE_SHA_Return_t DTHE_SHA_open(DTHE_Handle handle)
{
    DTHE_SHA_Return_t status  = DTHE_SHA_RETURN_FAILURE;
    DTHE_Config     *config = NULL;
    DTHE_Attrs      *attrs  = NULL;
    CSL_EIP57T_SHARegs     *ptrShaRegs;

    if(NULL != handle)
    {
        status = DTHE_SHA_RETURN_SUCCESS;
    }

    if(status == DTHE_SHA_RETURN_SUCCESS)
    {
        config              = (DTHE_Config *) handle;
        attrs               = config->attrs;
        ptrShaRegs          = (CSL_EIP57T_SHARegs *)attrs->shaBaseAddr;
        gDTHESHAInProgress  = FALSE;

        /* Disable all interrupts */
        DTHE_SHA_setInterruptStatus(ptrShaRegs, 0U);
    }

    return (status);
}

/**
 *  Design: TIFSMCU-4382
 */

DTHE_SHA_Return_t DTHE_SHA_close(DTHE_Handle handle)
{
    DTHE_SHA_Return_t   status  = DTHE_SHA_RETURN_FAILURE;
    DTHE_Config         *config = NULL;
    DTHE_Attrs          *attrs  = NULL;
    CSL_EIP57T_SHARegs  *ptrShaRegs;

    if(NULL != handle)
    {
        status = DTHE_SHA_RETURN_SUCCESS;
    }

    if(status == DTHE_SHA_RETURN_SUCCESS)
    {
        config              = (DTHE_Config *) handle;
        attrs               = config->attrs;
        ptrShaRegs          = (CSL_EIP57T_SHARegs *)attrs->shaBaseAddr;
        if(gDTHESHAInProgress == FALSE)
        {
            /* Disable all interrupts */
            DTHE_SHA_setInterruptStatus(ptrShaRegs, 0U);

            gDTHESHAInProgress = 0;
            gDTHESHAdigestCount = 0;
        }
        else
        {
            status  = DTHE_SHA_RETURN_FAILURE;
        }
    }

    return (status);
}

/**
 *  Design: TIFSMCU-4383
 */

DTHE_SHA_Return_t DTHE_SHA_compute(DTHE_Handle handle, DTHE_SHA_Params* ptrShaParams, Bool isLastBlock)
{
    DTHE_SHA_Return_t       status = DTHE_SHA_RETURN_SUCCESS;
    uint8_t                 useAlgoConstants;
    uint8_t                 closeHash;
    uint8_t                 blockSize = 0U;
    uint32_t                shiftSize = 0U;
    uint32_t                dataLenBytes = 0U;
    DTHE_Config             *config = NULL;
    DTHE_Attrs              *attrs  = NULL;
    CSL_EIP57T_SHARegs      *ptrShaRegs = NULL;

    if((NULL == handle) || (NULL == ptrShaParams))
    {
        status = DTHE_SHA_RETURN_FAILURE;
    }

    if(status == DTHE_SHA_RETURN_SUCCESS)
    {
        config              = (DTHE_Config *) handle;
        attrs               = config->attrs;
        ptrShaRegs          = (CSL_EIP57T_SHARegs *)attrs->shaBaseAddr;
        dataLenBytes        = ptrShaParams->dataLenBytes;

        DTHE_SHA_setAlgorithm(ptrShaRegs, ptrShaParams->algoType);

        /* Sanity Checking: Ensure that the data length does not exceed the
         * maximum supported for the selected algorithm, else the block count
         * computed in DTHE_SHA_writeDataBuffer() would silently truncate. */
        status = DTHE_SHA_validateMaxDataLen(ptrShaParams->algoType, dataLenBytes);
    }

    if (status == DTHE_SHA_RETURN_SUCCESS)
    {
        /* Sanity Checking: Any data buffer except the last block should be aligned as per
         * the SHA Size. For SHA256 this is 64byte while for SHA512 this should be 128byte */
        status = DTHE_SHA_validateBlockAlignment(ptrShaParams->algoType, dataLenBytes, isLastBlock);
    }

    /* Perform the SHA Computation */
    if (status == DTHE_SHA_RETURN_SUCCESS)
    {
        /* Ensure that the SHA IP Block is ready to receive data: */
        DTHE_SHA_pollContextReady (ptrShaRegs);

        /* Is this the first block which is being passed to the SHA Engine? */
        if (gDTHESHAInProgress == FALSE)
        {
            /* Yes: For the first block we will use the algorithm constants */
            useAlgoConstants = 1U;
        }
        else
        {
            /* No: For all other blocks we will not use the algorithm constants */
            useAlgoConstants = 0U;
        }

        /* Is this the last block? */
        if (isLastBlock == TRUE)
        {
            /* Yes: Close the Hash */
            closeHash = 1U;
        }
        else
        {
            /* No: Dont close the hash there are more data blocks. */
            closeHash = 0U;
        }

        /* Update the Hash Mode: */
        DTHE_SHA_configureHashMode(ptrShaRegs, ptrShaParams->algoType, useAlgoConstants, closeHash);

        /* Write the length of the data and setup the block & shift size: */
        DTHE_SHA_setLengthAndBlockParams(ptrShaRegs, ptrShaParams->algoType, dataLenBytes, &blockSize, &shiftSize);

        /* Write the data buffer to the SHA Engine: */
        DTHE_SHA_writeDataBuffer(config->dmaEnable, ptrShaRegs, ptrShaParams->ptrDataBuffer, dataLenBytes, blockSize, shiftSize);

        /* Poll till the intermediate hash results are available: */
        DTHE_SHA_pollOutputReady (ptrShaRegs);

        /* Get the digest count and value: */
        DTHE_SHA_readDigestAndCount(ptrShaRegs, ptrShaParams->algoType, &ptrShaParams->digest[0]);

        if( isLastBlock == TRUE )
        {
            /* SHA Computation is in progress: */
            gDTHESHAInProgress = FALSE;
        }
        else
        {
            /* SHA Computation is in progress: */
            gDTHESHAInProgress = TRUE;
        }
    }
    return (status);
}

/**
 *  Design: TIFSMCU-4376
 */

DTHE_SHA_Return_t DTHE_HMACSHA_compute(DTHE_Handle handle, DTHE_SHA_Params* ptrShaParams)
{
    DTHE_SHA_Return_t       status = DTHE_SHA_RETURN_FAILURE;
    uint8_t                 blockSize = 0U;
    uint32_t                shiftSize = 0U;
    uint32_t                dataLenBytes = 0U;
    uint32_t                hmacPaddedKey[DTHE_HMAC_SHA_MAX_KEY_SIZE_BYTES/4U];
    DTHE_Config             *config = NULL;
    DTHE_Attrs              *attrs  = NULL;
    CSL_EIP57T_SHARegs      *ptrShaRegs = NULL;

    if((NULL != handle) && (NULL != ptrShaParams))
    {
        status = DTHE_SHA_RETURN_SUCCESS;
    }

    if(status == DTHE_SHA_RETURN_SUCCESS)
    {
        config              = (DTHE_Config *) handle;
        attrs               = config->attrs;
        ptrShaRegs          = (CSL_EIP57T_SHARegs *)attrs->shaBaseAddr;
        dataLenBytes        = ptrShaParams->dataLenBytes;
        DTHE_SHA_setAlgorithm(ptrShaRegs, ptrShaParams->algoType);

        /* Sanity Check: The HMAC Key Size cannot be greater than the MAX allowed. */
        if (ptrShaParams->keySize > DTHE_HMAC_SHA_MAX_KEY_SIZE_BYTES)
        {
            /* Long HMAC Keys are not supported. */
            status = DTHE_SHA_RETURN_FAILURE;
        }
        else
        {
            /* Sanity Checking: Ensure that the data length does not exceed the
             * maximum supported for the selected algorithm, else the block count
             * computed in DTHE_SHA_writeDataBuffer() would silently truncate. */
            status = DTHE_SHA_validateMaxDataLen(ptrShaParams->algoType, dataLenBytes);
        }
    }

    if(status == DTHE_SHA_RETURN_SUCCESS)
    {
        /* Initialize the padded key and copy the key if one was provided: */
        DTHE_HMACSHA_preparePaddedKey(ptrShaParams, &hmacPaddedKey[0]);

        /* Configure the HMAC keys, hash mode and the block & shift size for the algorithm: */
        DTHE_HMACSHA_configureKeyAndMode(ptrShaRegs, ptrShaParams->algoType, &hmacPaddedKey[0], dataLenBytes, &blockSize, &shiftSize);

        /* Write the data buffer to the SHA Engine: */
        DTHE_SHA_writeDataBuffer(config->dmaEnable, ptrShaRegs, ptrShaParams->ptrDataBuffer, dataLenBytes, blockSize, shiftSize);

        /* Poll till the intermediate hash results are available: */
        DTHE_SHA_pollOutputReady (ptrShaRegs);

        /* Get the digest count and value: */
        DTHE_SHA_readDigestAndCount(ptrShaRegs, ptrShaParams->algoType, &ptrShaParams->digest[0]);

        /* SHA Computation is in progress: */
        gDTHESHAInProgress = FALSE;
    }
    return (status);
}
/* ========================================================================== */
/*                         Internal Function Definitions                      */
/* ========================================================================== */
/**
 *  \brief  The function is used to enable/disable the interrupts
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *
 *  \param  intStatus       Flag which is used to enable(1)/disable(0) the interrupts
 *
 */
static void DTHE_SHA_setInterruptStatus(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t intStatus)
{
    CSL_FINSR(ptrSHARegs->SYSCONFIG, 2U, 2U, (((uint32_t)0) | intStatus));
    return;
}


/**
 *  \brief  The function is used to enable/disable the DMA
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *
 *  \param  dmaStatus       Flag which is used to enable(1)/disable(0) the DMA operation
 *
 */
static void DTHE_SHA_setDMA(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t dmaStatus)
{
    /* SDMA_en */
    CSL_FINSR (ptrSHARegs->SYSCONFIG, 3U, 3U, (((uint32_t)0) | dmaStatus));
}

/**
 *  \brief  The function is used to poll until the SHA IP block is ready to
 *          receive the context
 *
 *  \param  ptrSHARegs      Pointer to the SHA Driver Context Object
 *
 */
static void DTHE_SHA_pollContextReady(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    uint32_t     done = 0U;

    /* Loop around till the condition is met: */
    while (done == 0U)
    {
        done = DTHE_SHA_isContextReadyIRQ(ptrSHARegs);
    }
    return;
}

/**
 *  \brief  The function is used to get the context ready IRQ status
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *
 *  \retval                 1 - Available for a new context
 *                          0 - Not available for a new context
 */
static uint32_t DTHE_SHA_isContextReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    return CSL_FEXTR (ptrSHARegs->IRQSTATUS, 3U, 3U);
}

/**
 *  \brief  The function is used to enable/disable the usage of the algorithm
 *      constants
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param useAlgConstants  Flag which will use enable(1)/disable(0) the usage of the algorithm constants
 *
 */
static void DTHE_SHA_setUseAlgoConstants(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t useAlgConstants)
{
    CSL_FINSR (ptrSHARegs->HASH_MODE, 3U, 3U, (((uint32_t)0) | useAlgConstants));
    return;
}

/**
 *  \brief  The function is used to close/continue the hash algorithm
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param closeHashFlag    Flag which will close(1)/continue(0) the hash algorithm
 *
 */
static void DTHE_SHA_setCloseHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t closeHashFlag)
{
    CSL_FINSR (ptrSHARegs->HASH_MODE, 4U, 4U, (((uint32_t)0) | closeHashFlag));
    return;
}

/**
 *  \brief The function is used to set the HMAC Key processing
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param hmacKeyFlag      Flag which will enable(1)/disable(0) the HMAC key processing
 *
 */
static void DTHE_SHA_setHMACKeyProcessing(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacKeyFlag)
{
    CSL_FINSR (ptrSHARegs->HASH_MODE, 5U, 5U, (((uint32_t)0) | hmacKeyFlag));
    return;
}

/**
 *  \brief  The function is used to set the HMAC Outer Hash flag
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  hmacOuterHash   Flag which will enable(1)/disable(0) the HMAC outer hash is performed
 *                          on the hash digest when the inner hash has finished
 *
 */
static void DTHE_SHA_setHMACOuterHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacOuterHash)
{
    CSL_FINSR (ptrSHARegs->HASH_MODE, 7U, 7U, (((uint32_t)0) | hmacOuterHash));
    return;
}

/**
 *  \brief  The function is used to enable/disable the usage of the algorithm
 *      constants
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param useAlgConstants  Flag which will use enable(1)/disable(0) the usage of the algorithm constants
 *
 */
static void DTHE_SHA512_setUseAlgoConstants(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t useAlgConstants)
{
    CSL_FINSR (ptrSHARegs->HASH512_MODE, 3U, 3U, (((uint32_t)0) | useAlgConstants));
    return;
}

/**
 *  \brief  The function is used to close/continue the hash algorithm
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param closeHashFlag    Flag which will close(1)/continue(0) the hash algorithm
 *
 */
static void DTHE_SHA512_setCloseHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t closeHashFlag)
{
    CSL_FINSR (ptrSHARegs->HASH512_MODE, 4U, 4U, (((uint32_t)0) | closeHashFlag));
    return;
}

/**
 *  \brief  The function is used to set the HMAC Key processing
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param hmacKeyFlag      Flag which will enable(1)/disable(0) the HMAC key processing
 *
 */
static void DTHE_SHA512_setHMACKeyProcessing(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacKeyFlag)
{
    CSL_FINSR (ptrSHARegs->HASH512_MODE, 5U, 5U, (((uint32_t)0) | hmacKeyFlag));
    return;
}

/**
 *  \brief  The function is used to set the HMAC Outer Hash flag
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param hmacOuterHash    Flag which will enable(1)/disable(0) the HMAC outer hash is performed
 *                          on the hash digest when the inner hash has finished
 *
 */
static void DTHE_SHA512_setHMACOuterHash(CSL_EIP57T_SHARegs* ptrSHARegs, uint8_t hmacOuterHash)
{
    CSL_FINSR (ptrSHARegs->HASH_MODE, 7U, 7U, (((uint32_t)0) | hmacOuterHash));
    return;
}

/**
 *  \brief  The function is used to set the length of the block to be processed
 *          for the hash operation
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param length           Length of the data
 *
 */
static void DTHE_SHA_setHashLength(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t length)
{
    ptrSHARegs->LENGTH = length;
    return;
}

/**
 *  \brief  The function is used to set the length of the block to be processed
 *          for the hash operation
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param length           Length of the data
 *
 */
static void DTHE_SHA512_setHashLength(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t length)
{
    ptrSHARegs->HASH512_LENGTH = length;
    return;
}

/**
 *  \brief  The function is used to poll until the SHA IP block is ready to
 *          receive data
 *
 *  \param ptrSHARegs       Pointer to the SHA Driver Context Object
 *
 */
static void DTHE_SHA_pollInputReady(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    uint32_t     done = 0U;

    /* Loop around till the condition is met: */
    while (done == 0U)
    {
        done = DTHE_SHA_isInputReadyIRQ(ptrSHARegs);
    }
    return;
}

/**
 *  \brief  The function is used write the data block. Data Block sizes
 *          are specific to the size of algorithm selected.
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param ptrDataBlock     Pointer to the data block to be written
 *  \param blockSize        Block Size. This is selected on the basis of the algorithm.
 *
 */
static void DTHE_SHA_writeDataBlock(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrDataBlock, uint8_t blockSize)
{
    uint8_t index;
    for (index = 0U; index < blockSize; index = index + 1U)
    {
        ptrSHARegs->DATA_IN[0] = ptrDataBlock[index];
    }
    return;
}

/**
 *  \brief  The function is used to poll until the SHA IP block is ready with
 *          the results
 *
 *  \param ptrSHARegs       Pointer to the SHA Driver Context Object
 *
 */
static void DTHE_SHA_pollOutputReady (const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    uint32_t     done = 0U;

    /* Loop around till the condition is met: */
    while (done == 0U)
    {
        done = DTHE_SHA_isOutputReadyIRQ(ptrSHARegs);
    }
    return;
}

/**
 *  \brief  The function is used to get the hash inner digest for SHA-256.
 *          The SHA Digest in this case is 32bytes.
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  ptrDigest       Pointer to the digest populated by the API
 *
 */
static void DTHE_SHA_getHashDigest(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDigest)
{
    uint8_t index;
    for (index = 0U; index < 8U; index++)
    {
        ptrDigest[index] = ptrSHARegs->IDIGEST[index];
    }
    return;
}

/**
 *  \brief  The function is used to get the hash inner digest for SHA512.
 *          The SHA Digest in this case is 64bytes.
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  ptrDigest       Pointer to the digest populated by the API
 *
 */
static void DTHE_SHA512_getHashDigest(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDigest)
{
    uint8_t index;
    for (index = 0U; index < 16U; index++)
    {
        ptrDigest[index] = ptrSHARegs->HASH512_IDIGEST[index];
    }
    return;
}

/**
 *  \brief  The function is used to set the selected algorithm
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param algorithm        Algorithm to be executed
 *
 */
static void DTHE_SHA_setAlgorithm(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algorithm)
{
    uint32_t     value;

    switch (algorithm)
    {
        case CSL_EIP57T_SHAAlgo_MD5:
            value = 0U;
            break;

        case CSL_EIP57T_SHAAlgo_SHA384:
            value = 1U;
            break;

        case CSL_EIP57T_SHAAlgo_SHA1:
            value = 2U;
            break;
        
        case CSL_EIP57T_SHAAlgo_SHA512:
            value = 3U;
            break;
        
        case CSL_EIP57T_SHAAlgo_SHA224:
            value = 4U;
            break;
        
        default:
            value = 6U;
            break;
    }
    
    CSL_FINSR (ptrSHARegs->HASH_MODE, 2U, 0U, value);
    return;
}

/**
 *  \brief The function is used to get the output ready IRQ status
 *
 *  \param ptrSHARegs Pointer to the EIP57T SHA Registers
 *
 *  \return 1 - (Partial) result is available
 *          0 - (Partial) result is not available
 */
static uint32_t DTHE_SHA_isOutputReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    return CSL_FEXTR (ptrSHARegs->IRQSTATUS, 0U, 0U);
}

/**
 *  \brief The function is used to get the input ready IRQ status
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *
 *  \return 1 - Data FIFO is ready to receive data
 *          0 - Data FIFO is not ready to receive data
 */
static uint32_t DTHE_SHA_isInputReadyIRQ(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    return CSL_FEXTR (ptrSHARegs->IRQSTATUS, 1U, 1U);
}

/**
 *  \brief The function is used to get the digest count for the SHA512
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *
 *  \retval                 Digest Count
 */
static uint32_t DTHE_SHA512_getDigestCount(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    return ptrSHARegs->HASH512_DIGEST_COUNT;
}

/**
 *  \brief The function is used to get the digest count
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *
 *  \retval                 Digest Count
 */
static uint32_t DTHE_SHA_getDigestCount(const CSL_EIP57T_SHARegs* ptrSHARegs)
{
    return ptrSHARegs->DIGEST_COUNT;
}

/**
 *  \brief  The function is used to write the lower 256bits (32bytes) of the HMAC Key
 *          to the Outer Digest. Padding is done outside this function.
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param ptrHMACKey       Pointer to the lower 256bits to be configured
 *
 */
static void DTHE_SHA_setHMACOuterKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey)
{
    uint8_t index;
    for (index = 0U; index < 8U; index = index + 1U)
    {
        ptrSHARegs->ODIGEST[index] = ptrHMACKey[index];
    }
    return;
}

/**
 *  \brief The function is used to write the upper 256bits (32bytes) of the HMAC Key
 *         to the Inner Digest.
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param ptrHMACKey       Pointer to the upper 256bits to be configured
 *
 */
static void DTHE_SHA_setHMACInnerKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey)
{
    uint8_t index;
    for (index = 0U; index < 8U; index = index + 1U)
    {
        ptrSHARegs->IDIGEST[index] = ptrHMACKey[index];
    }
    return;
}

/**
 *  \brief The function is used to write the lower 512bits (64bytes) of the HMAC Key
 *         to the Outer Digest. Padding is done outside this function.
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param ptrHMACKey       Pointer to the lower 512bits to be configured
 *
 */
static void DTHE_SHA512_setHMACOuterKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey)
{
    uint8_t index;
    for (index = 0U; index < 16U; index = index + 1U)
    {
        ptrSHARegs->HASH512_ODIGEST[index] = ptrHMACKey[index];
    }
    return;
}

/**
 *  \brief The function is used to write the upper 512bits (64bytes) of the HMAC Key
 *         to the Inner Digest.
 *
 *  \param ptrSHARegs       Pointer to the EIP57T SHA Registers
 *  \param ptrHMACKey       Pointer to the upper 512bits to be configured
 *
 */
static void DTHE_SHA512_setHMACInnerKey(CSL_EIP57T_SHARegs* ptrSHARegs, const uint32_t* ptrHMACKey)
{
    uint8_t index;
    for (index = 0U; index < 16U; index = index + 1U)
    {
        ptrSHARegs->HASH512_IDIGEST[index] = ptrHMACKey[index];
    }
    return;
}

/**
 *  \brief  The function checks that a non-final data block is aligned to
 *          the SHA block size for the selected algorithm.
 *
 *  \param  algoType        Algorithm to be executed
 *  \param  dataLenBytes    Size of the data in bytes
 *  \param  isLastBlock     Used for singleshot and multishot sha
 *
 *  \retval                 #DTHE_SHA_RETURN_SUCCESS if the data length is aligned
 *                          #DTHE_SHA_RETURN_FAILURE if the data length is not aligned
 */
static DTHE_SHA_Return_t DTHE_SHA_validateBlockAlignment(uint32_t algoType, uint32_t dataLenBytes, Bool isLastBlock)
{
    DTHE_SHA_Return_t status = DTHE_SHA_RETURN_SUCCESS;

    /* Sanity Checking: Any data buffer except the last block should be aligned as per
     * the SHA Size. For SHA256 this is 64byte while for SHA512 this should be 128byte */
    if (isLastBlock == FALSE)
    {
        if (algoType == DTHE_SHA_ALGO_SHA256)
        {
            if ((dataLenBytes % (DTHE_SHA256_BLOCK_SIZE * sizeof(uint32_t))) != 0U)
            {
                /* Error: Ensure that the data length is a word multiple. */
                status = DTHE_SHA_RETURN_FAILURE;
            }
        }
        else
        {
            if ((dataLenBytes % (DTHE_SHA512_BLOCK_SIZE * sizeof(uint32_t))) != 0U)
            {
                /* Error: Ensure that the data length is a word multiple. */
                status = DTHE_SHA_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

/**
 *  \brief  The function checks that the data length does not exceed the
 *          maximum value supported for the selected algorithm, so that the
 *          block count computed in DTHE_SHA_writeDataBuffer() cannot
 *          silently truncate when narrowed to uint16_t.
 *
 *  \param  algoType        Algorithm to be executed
 *  \param  dataLenBytes    Size of the data in bytes
 *
 *  \retval                 #DTHE_SHA_RETURN_SUCCESS if the data length is within range
 *                          #DTHE_SHA_RETURN_FAILURE if the data length is too large
 */
static DTHE_SHA_Return_t DTHE_SHA_validateMaxDataLen(uint32_t algoType, uint32_t dataLenBytes)
{
    DTHE_SHA_Return_t status = DTHE_SHA_RETURN_SUCCESS;

    if (algoType == DTHE_SHA_ALGO_SHA256)
    {
        if (dataLenBytes > DTHE_SHA256_MAX_DATA_LEN_BYTES)
        {
            status = DTHE_SHA_RETURN_FAILURE;
        }
    }
    else
    {
        if (dataLenBytes > DTHE_SHA512_MAX_DATA_LEN_BYTES)
        {
            status = DTHE_SHA_RETURN_FAILURE;
        }
    }

    return (status);
}

/**
 *  \brief  The function is used to update the Hash Mode for a plain SHA
 *          computation, resetting the HMAC processing bits.
 *
 *  \param  ptrSHARegs          Pointer to the EIP57T SHA Registers
 *  \param  algoType            Algorithm to be executed
 *  \param  useAlgoConstants    Flag which will use enable(1)/disable(0) the usage of the algorithm constants
 *  \param  closeHash           Flag which will close(1)/continue(0) the hash algorithm
 *
 */
static void DTHE_SHA_configureHashMode(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint8_t useAlgoConstants, uint8_t closeHash)
{
    if (algoType == DTHE_SHA_ALGO_SHA256)
    {
        DTHE_SHA_setUseAlgoConstants(ptrSHARegs, useAlgoConstants);
        DTHE_SHA_setCloseHash(ptrSHARegs, closeHash);

        /* Reset the HMAC Processing: */
        DTHE_SHA_setHMACKeyProcessing(ptrSHARegs, 0U);
        DTHE_SHA_setHMACOuterHash(ptrSHARegs, 0U);
    }
    else
    {
        DTHE_SHA512_setUseAlgoConstants(ptrSHARegs, useAlgoConstants);
        DTHE_SHA512_setCloseHash(ptrSHARegs, closeHash);

        /* Reset the HMAC Processing: */
        DTHE_SHA512_setHMACKeyProcessing(ptrSHARegs, 0U);
        DTHE_SHA512_setHMACOuterHash(ptrSHARegs, 0U);
    }
    return;
}

/**
 *  \brief  The function is used to set the data length and derive the
 *          block & shift size for the selected algorithm.
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  algoType        Algorithm to be executed
 *  \param  dataLenBytes    Size of the data in bytes
 *  \param  ptrBlockSize    Pointer populated with the block size for the algorithm
 *  \param  ptrShiftSize    Pointer populated with the shift size for the algorithm
 *
 */
static void DTHE_SHA_setLengthAndBlockParams(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint32_t dataLenBytes, uint8_t* ptrBlockSize, uint32_t* ptrShiftSize)
{
    if (algoType == DTHE_SHA_ALGO_SHA256)
    {
        /* SHA256: Setup the block & shift size: */
        *ptrBlockSize = DTHE_SHA256_BLOCK_SIZE;
        *ptrShiftSize = DTHE_SHA256_SHIFT_SIZE;

        /* Set the data length: */
        DTHE_SHA_setHashLength(ptrSHARegs, dataLenBytes);
    }
    else
    {
        /* SHA512: Setup the block & shift size: */
        *ptrBlockSize = DTHE_SHA512_BLOCK_SIZE;
        *ptrShiftSize = DTHE_SHA512_SHIFT_SIZE;

        /* Set the data length: */
        DTHE_SHA512_setHashLength(ptrSHARegs, dataLenBytes);
    }
    return;
}

/**
 *  \brief  The function writes the full data buffer to the SHA Engine,
 *          using DMA when available and falling back to a polled write
 *          of the full blocks, any partial block and any left over bytes.
 *
 *  \param  dmaEnable       Flag indicating whether DMA is enabled for the DTHE instance
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  ptrDataBuffer   Pointer to the Plain Text data buffer
 *  \param  dataLenBytes    Size of the data in bytes
 *  \param  blockSize       Block Size. This is selected on the basis of the algorithm.
 *  \param  shiftSize       Shift Size. This is selected on the basis of the algorithm.
 *
 */
static void DTHE_SHA_writeDataBuffer(uint32_t dmaEnable, CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t* ptrDataBuffer, uint32_t dataLenBytes, uint8_t blockSize, uint32_t shiftSize)
{
    DMA_Handle              dmaHandle = NULL;
    DMA_Return_t            dmaStatus = DMA_RETURN_FAILURE;
    uint32_t                index = 0U;
    uint32_t                dataLenWords;
    uint32_t                numPartialWords;
    uint32_t                partialWord = 0U;
    uint32_t                numBytes    = 0U;
    uint16_t                numBlocks;
    uint8_t                 *ptrByteDataBuffer;

    /* Determine the data length in words: */
    dataLenWords = dataLenBytes / 4U;

    /* Compute the number of blocks: */
    numBlocks = (uint16_t)(dataLenWords / blockSize);

    /* Compute the number of partial words which need to be handled seperately */
    numPartialWords = dataLenWords % blockSize;

    if ((dmaEnable == DMA_ENABLE) && (numBlocks > 0U))
    {
        dmaHandle = DMA_open(0);
        dmaStatus = DMA_Config_TxChannel(dmaHandle, ptrDataBuffer, (uint32_t *)&ptrSHARegs->DATA_IN[0], numBlocks, (uint16_t)blockSize, DMA_SHA_ENABLE);
    }

    if (dmaStatus == DMA_RETURN_SUCCESS)
    {
        /* Compute the number of full blocks which need to be processed: */
        (void)DMA_enableTxTransferRegion(dmaHandle);
        DTHE_SHA_setDMA(ptrSHARegs, 1U);
        (void)DMA_startTxChannel(dmaHandle);

        (void)DMA_WaitForTxTransfer(dmaHandle);

        DTHE_SHA_setDMA(ptrSHARegs, 0U);

        (void)DMA_disableTxCh(dmaHandle);

        /* Compute the number of bytes which have been processed: */
        numBytes = numBytes + (uint32_t)((uint32_t)numBlocks * (uint32_t)blockSize * sizeof(uint32_t));
        index = numBlocks;
    }
    else
    {
        /* Compute the number of full blocks which need to be processed: */
        for (index = 0U; index < numBlocks; index = index + 1U)
        {
            /* Ensure that the SHA IP Block is ready to receive data: */
            DTHE_SHA_pollInputReady(ptrSHARegs);

            /* Write the data block: */
            DTHE_SHA_writeDataBlock(ptrSHARegs,
                                    &ptrDataBuffer[index << shiftSize],
                                    blockSize);

            /* Compute the number of bytes which have been processed: */
            numBytes = numBytes + (uint32_t)((uint32_t)blockSize * sizeof(uint32_t));
        }
    }

    /* Process any left over data: */
    if (numPartialWords != 0U)
    {
        /* Ensure that the SHA IP Block is ready to receive data: */
        DTHE_SHA_pollInputReady(ptrSHARegs);

        /* Write the data block: */
        DTHE_SHA_writeDataBlock(ptrSHARegs,
                                &ptrDataBuffer[index << shiftSize],
                                (uint8_t)numPartialWords);

        /* Compute the number of bytes which have been processed: */
        numBytes = numBytes + (numPartialWords * sizeof(uint32_t));
    }

    /* Do we need to account for some additional bytes? */
    if (dataLenBytes != numBytes)
    {
        /* Get the pointer to the data buffer in bytes which will be written: */
        ptrByteDataBuffer = (uint8_t*)ptrDataBuffer;

        /* Copy into the partial word: */
        (void)memcpy ((void *)&partialWord, (void *)&ptrByteDataBuffer[numBytes], (dataLenBytes - numBytes));

        /* Ensure that the SHA IP Block is ready to receive data: */
        DTHE_SHA_pollInputReady(ptrSHARegs);

        /* Write the data block: */
        DTHE_SHA_writeDataBlock(ptrSHARegs, &partialWord, 1U);
    }
    return;
}

/**
 *  \brief  The function is used to read back the hash digest and digest
 *          count for the selected algorithm.
 *
 *  \param  ptrSHARegs      Pointer to the EIP57T SHA Registers
 *  \param  algoType        Algorithm to be executed
 *  \param  ptrDigest       Pointer to the digest populated by the API
 *
 */
static void DTHE_SHA_readDigestAndCount(const CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, uint32_t* ptrDigest)
{
    if (algoType == DTHE_SHA_ALGO_SHA256)
    {
        DTHE_SHA_getHashDigest(ptrSHARegs, ptrDigest);
        gDTHESHAdigestCount = DTHE_SHA_getDigestCount(ptrSHARegs);
    }
    else
    {
        DTHE_SHA512_getHashDigest(ptrSHARegs, ptrDigest);
        gDTHESHAdigestCount = DTHE_SHA512_getDigestCount(ptrSHARegs);
    }
    return;
}

/**
 *  \brief  The function initializes the HMAC padded key and copies in the
 *          caller supplied key, if one was provided.
 *
 *  \param  ptrShaParams        Pointer to the parameters to be used to execute the driver
 *  \param  ptrHmacPaddedKey    Pointer to the padded key buffer to be initialized
 *
 */
static void DTHE_HMACSHA_preparePaddedKey(const DTHE_SHA_Params* ptrShaParams, uint32_t* ptrHmacPaddedKey)
{
    /* Initialize the padded key */
    (void)memset ((void *)ptrHmacPaddedKey, 0, DTHE_HMAC_SHA_MAX_KEY_SIZE_BYTES);

    /* Copy the key if one was provided: */
    if (ptrShaParams->keySize != 0U)
    {
        /* Copy the key data into the HMAC Padded Key: */
        (void)memcpy ((void *)ptrHmacPaddedKey, (const void*)ptrShaParams->ptrKey, ptrShaParams->keySize);
    }
    return;
}

/**
 *  \brief  The function is used to write the HMAC keys to the Outer/Inner
 *          Digest, update the Hash Mode for HMAC processing and derive the
 *          block & shift size for the selected algorithm.
 *
 *  \param  ptrSHARegs          Pointer to the EIP57T SHA Registers
 *  \param  algoType            Algorithm to be executed
 *  \param  ptrHmacPaddedKey    Pointer to the padded HMAC key
 *  \param  dataLenBytes        Size of the data in bytes
 *  \param  ptrBlockSize        Pointer populated with the block size for the algorithm
 *  \param  ptrShiftSize        Pointer populated with the shift size for the algorithm
 *
 */
static void DTHE_HMACSHA_configureKeyAndMode(CSL_EIP57T_SHARegs* ptrSHARegs, uint32_t algoType, const uint32_t* ptrHmacPaddedKey, uint32_t dataLenBytes, uint8_t* ptrBlockSize, uint32_t* ptrShiftSize)
{
    /* Which algorithm are we executing? */
    if (algoType == DTHE_SHA_ALGO_SHA256)
    {
        /* SHA256: Outer & Inner Keys are 256bits = 32bytes = 8words */
        DTHE_SHA_setHMACOuterKey(ptrSHARegs, &ptrHmacPaddedKey[0U]);
        DTHE_SHA_setHMACInnerKey(ptrSHARegs, &ptrHmacPaddedKey[8U]);

        /* HMAC Processing:-
        *  - Algorithm Constants are not used
        *  - Compute the hash and close it here. */
        DTHE_SHA_setUseAlgoConstants(ptrSHARegs, 0U);
        DTHE_SHA_setCloseHash(ptrSHARegs, 1U);
        DTHE_SHA_setHMACKeyProcessing(ptrSHARegs, 1U);
        DTHE_SHA_setHMACOuterHash(ptrSHARegs, 1U);

        /* Set the data length: */
        DTHE_SHA_setHashLength(ptrSHARegs, dataLenBytes);

        /* SHA256: Setup the block & shift size: */
        *ptrBlockSize = DTHE_SHA256_BLOCK_SIZE;
        *ptrShiftSize = DTHE_SHA256_SHIFT_SIZE;
    }
    else
    {
        /* SHA512: Outer & Inner Keys are 512bits = 64bytes = 16words */
        DTHE_SHA512_setHMACOuterKey(ptrSHARegs, &ptrHmacPaddedKey[0U]);
        DTHE_SHA512_setHMACInnerKey(ptrSHARegs, &ptrHmacPaddedKey[16U]);

        /* HMAC Processing:-
        *  - Algorithm Constants are not used
        *  - Compute the hash and close it here. */
        DTHE_SHA512_setUseAlgoConstants(ptrSHARegs, 0U);
        DTHE_SHA512_setCloseHash(ptrSHARegs, 1U);
        DTHE_SHA512_setHMACKeyProcessing(ptrSHARegs, 1U);
        DTHE_SHA512_setHMACOuterHash(ptrSHARegs, 1U);

        /* Set the data length: */
        DTHE_SHA512_setHashLength(ptrSHARegs, dataLenBytes);

        /* SHA512: Setup the block & shift size: */
        *ptrBlockSize = DTHE_SHA512_BLOCK_SIZE;
        *ptrShiftSize = DTHE_SHA512_SHIFT_SIZE;
    }
    return;
}
