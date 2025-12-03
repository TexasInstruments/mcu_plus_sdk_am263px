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

/**
 *  \file   dthe_sm4.c
 *
 *  \brief  This file contains the implementation of Dthe sm4 driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <string.h>
#include <crypto/dthe/dthe_sm4.h>

/* ========================================================================== */
/*                           Macro Definitions                                */
/* ========================================================================== */

/* SM4 Register Bit Positions and Masks */
#define DTHESM4_IO_BUF_CTRL_STAT_INPUT_READY_BIT_POS        (6U)
#define DTHESM4_IO_BUF_CTRL_STAT_OUTPUT_READY_BIT_POS       (0U)
#define DTHESM4_IO_BUF_CTRL_STAT_MASK                       (0x3FU)

/* SM4 Mode Register Bit Positions */
#define DTHESM4_MODE_ENCRYPT_DECRYPT_BIT_POS                (0U)
#define DTHESM4_MODE_KEY_PROVIDED_BIT_POS                   (1U)
#define DTHESM4_MODE_ECB_BIT_POS                            (2U)
#define DTHESM4_MODE_CBC_BIT_POS                            (3U)
#define DTHESM4_MODE_CTR_BIT_POS                            (4U)
#define DTHESM4_MODE_OFB_BIT_POS                            (5U)
#define DTHESM4_MODE_CFB_BIT_POS                            (6U)

/* SM4 SYSCONFIG Register Bit Positions for DMA */
#define DTHESM4_SYSCONFIG_DMA_START_BIT_POS                 (5U)
#define DTHESM4_SYSCONFIG_DMA_END_BIT_POS                   (8U)

/* SM4 IO Buffer Control Status Values */
#define DTHESM4_IO_BUF_CTRL_KEY_MODE_AVAILABLE              (0x0CU)
#define DTHESM4_IO_BUF_CTRL_KEY_MODE_IV_AVAILABLE           (0x2CU)
#define DTHESM4_IO_BUF_CTRL_DATA_IN_AVAILABLE               (0x02U)
#define DTHESM4_IO_BUF_CTRL_OUTPUT_AVAILABLE                (0x01U)

/* SM4 Block and Data Size Constants */
#define DTHESM4_BLOCK_SIZE_BYTES                            (16U)
#define DTHESM4_BLOCK_SIZE_WORDS                            (4U)
#define DTHESM4_WORD_SIZE_BYTES                             (4U)
#define DTHESM4_PARTIAL_BLOCK_BUFFER_SIZE                   (32U)

/* SM4 Key and IV Array Indices */
#define DTHESM4_KEY_IV_INDEX_0                              (0U)
#define DTHESM4_KEY_IV_INDEX_1                              (1U)
#define DTHESM4_KEY_IV_INDEX_2                              (2U)
#define DTHESM4_KEY_IV_INDEX_3                              (3U)

/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */

static void DTHE_SM4_pollInputBufferAvailable(const CSL_SM4Regs *ptrSm4Registers);
static void DTHE_SM4_setKey(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrKey[4]);
static void DTHE_SM4_setIV(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrIV[4]);
static void DTHE_SM4_setOpType(CSL_SM4Regs *ptrSm4Registers, uint32_t opType, uint32_t algoType);
static void DTHE_SM4_writeDataBlock(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrData[4]);
static void DTHE_SM4_setDataAvailable(CSL_SM4Regs *ptrSm4Registers, uint32_t val);
static void DTHE_SM4_pollOutputReady(const CSL_SM4Regs *ptrSm4Registers);
static void DTHE_SM4_readDataBlock(const CSL_SM4Regs *ptrSm4Registers, uint32_t ptrData[4]);
static void DTHE_SM4_setOutputBufferAvailable(CSL_SM4Regs *ptrSm4Registers, uint8_t outputBufferAvailable);
static void DTHE_SM4_clearIV(CSL_SM4Regs *ptrSm4Registers);
static void DTHE_SM4_clearKey(CSL_SM4Regs *ptrSm4Registers);
static void DTHE_SM4_clearContext(CSL_SM4Regs *ptrSm4Registers);
static void DTHE_SM4_disableDma(CSL_SM4Regs *ptrSm4Registers);

/* ========================================================================== */
/*                        Internal  Function Definitions                      */
/* ========================================================================== */


/**
 * \brief Polls until input buffer is available for writing by host
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 *
 *
*/
static void DTHE_SM4_pollInputBufferAvailable(const CSL_SM4Regs *ptrSm4Registers)
{
    uint32_t done = 0U;

    //
    /* Wait for SM4 hardware to be ready to accept new input data */
    //
    while (0U == done)
    {
        done = CSL_FEXTR (ptrSm4Registers->SM4_IO_BUF_CTRL_STAT,
                          DTHESM4_IO_BUF_CTRL_STAT_INPUT_READY_BIT_POS,
                          DTHESM4_IO_BUF_CTRL_STAT_INPUT_READY_BIT_POS);
    }

    return;
}


/**
 * \brief Configures the key in the SM4 module
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param ptrKey           Pointer to the 128bit key to be used.
 *
 *
*/
static void DTHE_SM4_setKey(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrKey[4])
{
    ptrSm4Registers->SM4_KEY_IN_0 = ptrKey[DTHESM4_KEY_IV_INDEX_0];
    ptrSm4Registers->SM4_KEY_IN_1 = ptrKey[DTHESM4_KEY_IV_INDEX_1];
    ptrSm4Registers->SM4_KEY_IN_2 = ptrKey[DTHESM4_KEY_IV_INDEX_2];
    ptrSm4Registers->SM4_KEY_IN_3 = ptrKey[DTHESM4_KEY_IV_INDEX_3];

    return;
}


/**
 * \brief Sets the Initialization Vector (IV) in the SM4 module
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param ptrIV            Pointer to the IV data to be used.
 *
 *
*/
static void DTHE_SM4_setIV(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrIV[4])
{
    const uint32_t *ptrIvLocal = ptrIV;
    
    //
    /* Initialize IV registers to ensure clean state */
    //
    ptrSm4Registers->SM4_IV_IN_OUT_0 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_1 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_2 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_3 = 0U;

    //
    /* Load IV based on specified size to support variable-length */
    /* initialization vectors */
    //
    ptrSm4Registers->SM4_IV_IN_OUT_0 = ptrIvLocal[DTHESM4_KEY_IV_INDEX_0];
    ptrSm4Registers->SM4_IV_IN_OUT_1 = ptrIvLocal[DTHESM4_KEY_IV_INDEX_1];
    ptrSm4Registers->SM4_IV_IN_OUT_2 = ptrIvLocal[DTHESM4_KEY_IV_INDEX_2];
    ptrSm4Registers->SM4_IV_IN_OUT_3 = ptrIvLocal[DTHESM4_KEY_IV_INDEX_3];

    return;
}


/**
 * \brief Sets the mode of operation and type of SM4 operation
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param opType           For encryption - set to 0 for decryption - set to 1
 * \param algoType         CFB(0x6), OFB(0x5), CTR(0x4), CBC(0x3), ECB(0x2)
 *
 *
*/
static void DTHE_SM4_setOpType(CSL_SM4Regs *ptrSm4Registers, uint32_t opType, uint32_t algoType)
{
    //
    /* Configure hardware for encryption or decryption operation */
    //
   if (opType == DTHE_SM4_ENCRYPT)
   {
       //
       /* Enable encryption mode */
       //
       CSL_FINSR(ptrSm4Registers->SM4_MODE,
                 DTHESM4_MODE_ENCRYPT_DECRYPT_BIT_POS,
                 DTHESM4_MODE_ENCRYPT_DECRYPT_BIT_POS,
                 (uint32_t)0|opType);
   }
   else
   {
       //
       /* Enable decryption mode with user-provided key */
       //
       CSL_FINSR(ptrSm4Registers->SM4_MODE,
                 DTHESM4_MODE_ENCRYPT_DECRYPT_BIT_POS,
                 DTHESM4_MODE_ENCRYPT_DECRYPT_BIT_POS,
                 (uint32_t)0|opType);
       CSL_FINSR(ptrSm4Registers->SM4_MODE,
                 DTHESM4_MODE_KEY_PROVIDED_BIT_POS,
                 DTHESM4_MODE_KEY_PROVIDED_BIT_POS,
                 0x0U);
   }

    //
    /* Select the block cipher mode of operation */
    //
    if (algoType == DTHE_SM4_CFB)
    {
        //
        /* Enable Cipher Feedback mode */
        //
        CSL_FINSR(ptrSm4Registers->SM4_MODE,
                  DTHESM4_MODE_CFB_BIT_POS,
                  DTHESM4_MODE_CFB_BIT_POS,
                  1U);
    }
    
    if (algoType == DTHE_SM4_OFB)
    {
        //
        /* Enable Output Feedback mode */
        //
        CSL_FINSR(ptrSm4Registers->SM4_MODE,
                  DTHESM4_MODE_OFB_BIT_POS,
                  DTHESM4_MODE_OFB_BIT_POS,
                  1U);
    }
    
    if (algoType == DTHE_SM4_CTR)
    {
        //
        /* Enable Counter mode */
        //
        CSL_FINSR(ptrSm4Registers->SM4_MODE,
                  DTHESM4_MODE_CTR_BIT_POS,
                  DTHESM4_MODE_CTR_BIT_POS,
                  1U);
    }
    
    if (algoType == DTHE_SM4_CBC)
    {
        //
        /* Enable Cipher Block Chaining mode */
        //
        CSL_FINSR(ptrSm4Registers->SM4_MODE,
                  DTHESM4_MODE_CBC_BIT_POS,
                  DTHESM4_MODE_CBC_BIT_POS,
                  1U);
    }
    
    if (algoType == DTHE_SM4_ECB)
    {
        //
        /* Enable Electronic Codebook mode */
        //
        CSL_FINSR(ptrSm4Registers->SM4_MODE,
                  DTHESM4_MODE_ECB_BIT_POS,
                  DTHESM4_MODE_ECB_BIT_POS,
                  1U);
    }
    
    return;
}


/**
 * \brief Writes the data and passes it to the SM4 engine
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param ptrData          Pointer to the data to be written.
 *
 *
*/
static void DTHE_SM4_writeDataBlock(CSL_SM4Regs *ptrSm4Registers, const uint32_t ptrData[4U])
{
    ptrSm4Registers->SM4_DATA_IN_0 = ptrData[DTHESM4_KEY_IV_INDEX_0];
    ptrSm4Registers->SM4_DATA_IN_1 = ptrData[DTHESM4_KEY_IV_INDEX_1];
    ptrSm4Registers->SM4_DATA_IN_2 = ptrData[DTHESM4_KEY_IV_INDEX_2];
    ptrSm4Registers->SM4_DATA_IN_3 = ptrData[DTHESM4_KEY_IV_INDEX_3];
    
    return;
}


/**
 * \brief Sets that input data is available
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param val              Indicates inputs available
 *
 *
*/
static void DTHE_SM4_setDataAvailable(CSL_SM4Regs *ptrSm4Registers, uint32_t val)
{
    ptrSm4Registers->SM4_IO_BUF_CTRL_STAT = DTHESM4_IO_BUF_CTRL_STAT_MASK & val;

    return;
}


/** \brief Polls until the SM4 block has available data which can be read out
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 *
 *
*/
static void DTHE_SM4_pollOutputReady(const CSL_SM4Regs *ptrSm4Registers)
{
    uint32_t     done = 0U;

    //
    /* Wait for SM4 hardware to complete processing and have output */
    /* data ready */
    //
    while (0U == done)
    {
        done = CSL_FEXTR (ptrSm4Registers->SM4_IO_BUF_CTRL_STAT,
                          DTHESM4_IO_BUF_CTRL_STAT_OUTPUT_READY_BIT_POS,
                          DTHESM4_IO_BUF_CTRL_STAT_OUTPUT_READY_BIT_POS);
    }

    return;
}


/**
 * \brief Reads the data from the SM4 engine
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers
 * \param ptrData          Pointer to the data buffer populated by the API
 *
 *
*/
static void DTHE_SM4_readDataBlock(const CSL_SM4Regs *ptrSm4Registers, uint32_t ptrData[4])
{
    ptrData[DTHESM4_KEY_IV_INDEX_0] = ptrSm4Registers->SM4_DATA_OUT_0;
    ptrData[DTHESM4_KEY_IV_INDEX_1] = ptrSm4Registers->SM4_DATA_OUT_1;
    ptrData[DTHESM4_KEY_IV_INDEX_2] = ptrSm4Registers->SM4_DATA_OUT_2;
    ptrData[DTHESM4_KEY_IV_INDEX_3] = ptrSm4Registers->SM4_DATA_OUT_3;

    return;
}


/**
 * \brief Makes the output buffer available again after reading the
 *        output data
 * After retrieving all desired result data from the output buffer, the
 * Host must write a '1' to this bit to clear it. This makes the output
 * buffer available again for writing by the widebus SM4 hash engine.
 *
 * \param ptrSm4Registers         Pointer to the SM4 Registers
 * \param outputBufferAvailable   Flag to set the bit
 *
 *
*/
static void DTHE_SM4_setOutputBufferAvailable(CSL_SM4Regs *ptrSm4Registers, uint8_t outputBufferAvailable)
{
    CSL_FINSR(ptrSm4Registers->SM4_IO_BUF_CTRL_STAT,
              DTHESM4_IO_BUF_CTRL_STAT_OUTPUT_READY_BIT_POS,
              DTHESM4_IO_BUF_CTRL_STAT_OUTPUT_READY_BIT_POS,
              (uint32_t)0|outputBufferAvailable);

    return;
}


/**
 * \brief Clears the Initialization Vector (IV) in the SM4 module
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers.
 *
 *
 */
static void DTHE_SM4_clearIV(CSL_SM4Regs *ptrSm4Registers)
{
    ptrSm4Registers->SM4_IV_IN_OUT_0 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_1 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_2 = 0U;
    ptrSm4Registers->SM4_IV_IN_OUT_3 = 0U;

    return;
}


/** \brief Clears the key registers
 *
 *  \param ptrSm4Registers  Pointer to the SM4 Registers
 *
 *
*/
static void DTHE_SM4_clearKey(CSL_SM4Regs *ptrSm4Registers)
{
    ptrSm4Registers->SM4_KEY_IN_0 = 0U;
    ptrSm4Registers->SM4_KEY_IN_1 = 0U;
    ptrSm4Registers->SM4_KEY_IN_2 = 0U;
    ptrSm4Registers->SM4_KEY_IN_3 = 0U;

    return;
}


/**
 * \brief Clears the context fields of sm4 (key and iv registers)
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers.
 *
 *
*/
static void DTHE_SM4_clearContext(CSL_SM4Regs *ptrSm4Registers)
{
    ptrSm4Registers->SM4_IO_BUF_CTRL_STAT  = 0x0U;
    ptrSm4Registers->SM4_MODE = 0x0U;
    DTHE_SM4_clearIV(ptrSm4Registers);
    DTHE_SM4_clearKey(ptrSm4Registers);
}


/**
 * \brief Disables the DMA for SM4
 *
 * \param ptrSm4Registers  Pointer to the SM4 Registers.
 *
 *
*/
static void DTHE_SM4_disableDma(CSL_SM4Regs *ptrSm4Registers)
{
   CSL_FINSR(ptrSm4Registers->SM4_SYSCONFIG,
             DTHESM4_SYSCONFIG_DMA_END_BIT_POS,
             DTHESM4_SYSCONFIG_DMA_START_BIT_POS,
             0x0U);
   return;
}

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */


DTHE_SM4_Return_t DTHE_SM4_open(DTHE_Handle handle)
{
    DTHE_SM4_Return_t status      = DTHE_SM4_RETURN_FAILURE;
    DTHE_Config       *config     = (DTHE_Config *)NULL;
    DTHE_Attrs        *attrs      = (DTHE_Attrs  *)NULL;
    CSL_SM4Regs       *ptrSm4Regs = (CSL_SM4Regs *)NULL;

    if(NULL != handle)
    {
        status  = DTHE_SM4_RETURN_SUCCESS;
    }

    if(status  == DTHE_SM4_RETURN_SUCCESS)
    {
        config          = (DTHE_Config *)handle;
        attrs           = config->attrs;
        ptrSm4Regs      = (CSL_SM4Regs *)(uintptr_t)attrs->sm4BaseAddr;

        //
        /* Ensure clean state by removing any residual cryptographic */
        /* material */
        //
        DTHE_SM4_clearContext(ptrSm4Regs);

        //
        /* Configure for CPU-driven operation instead of DMA */
        //
        DTHE_SM4_disableDma(ptrSm4Regs);
    }

    return (status);
}


DTHE_SM4_Return_t DTHE_SM4_close(DTHE_Handle handle)
{
    DTHE_SM4_Return_t  status      = DTHE_SM4_RETURN_FAILURE;
    DTHE_Config        *config     = (DTHE_Config *)NULL;
    DTHE_Attrs         *attrs      = (DTHE_Attrs  *)NULL;
    CSL_SM4Regs        *ptrSm4Regs = (CSL_SM4Regs *)NULL;

    if(NULL != handle)
    {
        status  = DTHE_SM4_RETURN_SUCCESS;
    }

    if(status  == DTHE_SM4_RETURN_SUCCESS)
    {
        config          = (DTHE_Config *)handle;
        attrs           = config->attrs;
        ptrSm4Regs      = (CSL_SM4Regs *)(uintptr_t)attrs->sm4BaseAddr;

        DTHE_SM4_disableDma(ptrSm4Regs);
    }

    return (status);
}

DTHE_SM4_Return_t DTHE_SM4_execute(DTHE_Handle handle, const DTHE_SM4_Params* ptrParams)
{
    DTHE_SM4_Return_t status               = DTHE_SM4_RETURN_FAILURE;
    DTHE_Config       *config              = (DTHE_Config *)NULL;
    DTHE_Attrs        *attrs               = (DTHE_Attrs  *)NULL;
    CSL_SM4Regs       *ptrSm4Regs          = (CSL_SM4Regs *)NULL;
    uint32_t          *ptrWordInputBuffer  = (uint32_t *)NULL;
    uint32_t          *ptrWordOutputBuffer = (uint32_t *)NULL;
    uint32_t           dataLenWords;
    uint32_t           numBlocks;
    uint32_t           partialDataSize;
    uint32_t           index;
    uint32_t           numBytes = 0U;
    uint8_t            inPartialBlock[DTHESM4_PARTIAL_BLOCK_BUFFER_SIZE];
    uint8_t            outPartialBlock[DTHESM4_PARTIAL_BLOCK_BUFFER_SIZE];
    uint32_t           blockOffset;

    if ((NULL != handle) && (NULL != ptrParams))
    {
        status  = DTHE_SM4_RETURN_SUCCESS;
    }

    if (status  == DTHE_SM4_RETURN_SUCCESS)
    {
       config          = (DTHE_Config *)handle;
       attrs           = config->attrs;
       ptrSm4Regs      = (CSL_SM4Regs *)(uintptr_t)attrs->sm4BaseAddr;

        //
        /* Validate decryption input meets hardware alignment */
        /* requirements */
        //
        if ((ptrParams->opType == DTHE_SM4_DECRYPT) &&
            ((ptrParams->dataLenBytes % DTHESM4_WORD_SIZE_BYTES) != 0U))
        {
            status = DTHE_SM4_RETURN_FAILURE;
        }

        //
        /* Validate algorithm mode is supported by hardware */
        //
        if (status  == DTHE_SM4_RETURN_SUCCESS)
        {
            if ((DTHE_SM4_ECB > ptrParams->algoType) ||
                (DTHE_SM4_CFB < ptrParams->algoType))
            {
                status = DTHE_SM4_RETURN_FAILURE;
            }
        }

        //
        /* Validate operation type is either encryption or decryption */
        //
        if (status  == DTHE_SM4_RETURN_SUCCESS)
        {
            if ((DTHE_SM4_ENCRYPT != ptrParams->opType) &&
                (DTHE_SM4_DECRYPT != ptrParams->opType))
            {
                status = DTHE_SM4_RETURN_FAILURE;
            }
        }
    }

    if (status  == DTHE_SM4_RETURN_SUCCESS)
    {
        //
        /* Prepare hardware to accept cryptographic configuration */
        //
        DTHE_SM4_pollInputBufferAvailable(ptrSm4Regs);

        //
        /* Load encryption/decryption key into hardware */
        //
        DTHE_SM4_setKey(ptrSm4Regs, ptrParams->ptrKey);

        //
        /* Configure initialization vector for chaining modes */
        //
        if (ptrParams->algoType != DTHE_SM4_ECB)
        {
            //
            /* Load IV for modes that require chaining between blocks */
            //
            DTHE_SM4_setIV(ptrSm4Regs, ptrParams->ptrIV);
        }

        //
        /* Configure hardware for requested cipher mode and operation */
        //
        DTHE_SM4_setOpType(ptrSm4Regs, ptrParams->opType, ptrParams->algoType);

        if(ptrParams->opType == DTHE_SM4_ECB)
        {
            //
            /* Signal hardware that key and mode configuration are ready */
            //
            DTHE_SM4_setDataAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_KEY_MODE_AVAILABLE);
        }
        else
        {
            //
            /* Signal hardware that key, mode, and IV configuration are */
            /* ready */
            //
            DTHE_SM4_setDataAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_KEY_MODE_IV_AVAILABLE);
        }

        //
        /* Determine data flow direction based on operation type */
        //
        if (ptrParams->opType == DTHE_SM4_ENCRYPT)
        {
            //
            /* Setup for encryption: plaintext input, ciphertext output */
            //
            ptrWordInputBuffer  = ptrParams->ptrPlainTextData;
            ptrWordOutputBuffer = ptrParams->ptrEncryptedData;
        }
        else
        {
            //
            /* Setup for decryption: ciphertext input, plaintext output */
            //
            ptrWordInputBuffer  = ptrParams->ptrEncryptedData;
            ptrWordOutputBuffer = ptrParams->ptrPlainTextData;
        }

        //
        /* Calculate processing parameters for block-based operation */
        //
        dataLenWords = ptrParams->dataLenBytes / DTHESM4_WORD_SIZE_BYTES;
        //
        /* Identify any remaining data that doesn't fill a complete */
        /* block */
        //
        partialDataSize = ptrParams->dataLenBytes % DTHESM4_BLOCK_SIZE_BYTES;
        //
        /* Determine how many complete 16-byte blocks can be processed */
        //
        numBlocks = (dataLenWords / DTHESM4_BLOCK_SIZE_WORDS);

        //
        /* Process all complete data blocks through the cryptographic */
        /* engine */
        //
        for (index = 0U; index < numBlocks; index++)
        {
            blockOffset = index * DTHESM4_BLOCK_SIZE_WORDS;
            
            //
            /* Ensure hardware is ready to accept next block */
            //
            DTHE_SM4_pollInputBufferAvailable(ptrSm4Regs);
            //
            /* Submit block for encryption or decryption */
            //
            DTHE_SM4_writeDataBlock(ptrSm4Regs, &ptrWordInputBuffer[blockOffset]);
            //
            /* Notify hardware that input block is complete */
            //
            DTHE_SM4_setDataAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_DATA_IN_AVAILABLE);

            //
            /* Track progress through the data */
            //
            numBytes = numBytes + (DTHESM4_BLOCK_SIZE_WORDS * sizeof(uint32_t));

            //
            /* Wait for hardware to complete cryptographic operation */
            //
            DTHE_SM4_pollOutputReady(ptrSm4Regs);
            //
            /* Retrieve processed block from hardware */
            //
            DTHE_SM4_readDataBlock(ptrSm4Regs, &ptrWordOutputBuffer[blockOffset]);

            //
            /* Release output buffer for next operation */
            //
            DTHE_SM4_setOutputBufferAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_OUTPUT_AVAILABLE);
        }

        //
        /* Handle final partial block if data length is not */
        /* block-aligned */
        //
        if(partialDataSize != 0U)
        {
            blockOffset = index * DTHESM4_BLOCK_SIZE_WORDS;
            
            //
            /* Ensure hardware is ready for final block */
            //
            DTHE_SM4_pollInputBufferAvailable(ptrSm4Regs);
            //
            /* Prepare zero-padded buffers for partial block processing */
            //
            (void)memset ((void *)&inPartialBlock[0], 0, sizeof(inPartialBlock));
            (void)memset ((void *)&outPartialBlock[0], 0, sizeof(outPartialBlock));

           //
           /* Transfer partial data into padded block */
           //
            (void)memcpy ((void *)&inPartialBlock[0],
                    (const void *)&ptrWordInputBuffer[blockOffset],
                    partialDataSize);

            //
            /* Process padded block through cryptographic engine */
            //
            DTHE_SM4_writeDataBlock(ptrSm4Regs, (const uint32_t *)(const void *)&inPartialBlock[0U]);
           //
           /* Notify hardware that final block is ready */
           //
            DTHE_SM4_setDataAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_DATA_IN_AVAILABLE);

            //
            /* Wait for final block processing to complete */
            //
            DTHE_SM4_pollOutputReady(ptrSm4Regs);
            //
            /* Retrieve final processed block */
            //
            DTHE_SM4_readDataBlock(ptrSm4Regs, (uint32_t *)(void *)&outPartialBlock[0U]);

            //
            /* Release output buffer after final operation */
            //
            DTHE_SM4_setOutputBufferAvailable(ptrSm4Regs, DTHESM4_IO_BUF_CTRL_OUTPUT_AVAILABLE);

            //
            /* Extract only the valid bytes from padded output block */
            //
            (void)memcpy ((void *)&ptrWordOutputBuffer[blockOffset],
                    (const void *)&outPartialBlock[0U],
                    partialDataSize);

            //
            /* Update total bytes processed including partial block */
            //
            numBytes = numBytes + partialDataSize;
        }

        //
        /* Verify all requested data was successfully processed */
        //
        if(numBytes != ptrParams->dataLenBytes)
        {
            status = DTHE_SM4_RETURN_FAILURE;
        }
    }

    return status;
}

