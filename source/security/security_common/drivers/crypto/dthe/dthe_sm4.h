/********************************************************************
 * Copyright (C) 2024 Texas Instruments Incorporated.
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

/**
 *  \defgroup SECURITY_DTHE_SM4_MODULE APIs for DTHE SM4
 *  \ingroup  SECURITY_MODULE
 *
 *  This module contains APIs to program and use the DTHE SM4.
 *
 *  @{
 */

/**
 *  \file dthe_sm4.h
 *
 *  \brief This file contains the prototype of DTHE SM4 driver APIs
 */

#ifndef DTHE_SM4_H_
#define DTHE_SM4_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <crypto/dthe/dthe.h>
#include <kernel/dpl/SystemP.h>
#include <drivers/hw_include/cslr.h>
#include <crypto/dthe/hw_include/cslr_sm4.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * \brief SM4 Electronic Codebook (ECB) mode
 * \details ECB mode encrypts each block independently without using an
 *          initialization vector. Not recommended for encrypting multiple
 *          blocks of data as identical plaintext blocks produce identical
 *          ciphertext blocks.
 */
#define DTHE_SM4_ECB         (0X00000002U)

/**
 * \brief SM4 Cipher Block Chaining (CBC) mode
 * \details CBC mode XORs each plaintext block with the previous
 *          ciphertext block before encryption. Requires an initialization
 *          vector (IV) for the first block. Provides better security than
 *          ECB for multiple blocks.
 */
#define DTHE_SM4_CBC         (0X00000003U)

/**
 * \brief SM4 Counter (CTR) mode
 * \details CTR mode turns a block cipher into a stream cipher by
 *          encrypting a counter value and XORing with plaintext. Requires
 *          an initialization vector (IV). Allows parallel processing and
 *          random access to encrypted data.
 */
#define DTHE_SM4_CTR         (0X00000004U)

/**
 * \brief SM4 Output Feedback (OFB) mode
 * \details OFB mode turns a block cipher into a stream cipher by
 *          repeatedly encrypting an initialization vector. Requires an IV.
 *          Errors do not propagate.
 */
#define DTHE_SM4_OFB         (0X00000005U)

/**
 * \brief SM4 Cipher Feedback (CFB) mode
 * \details CFB mode turns a block cipher into a stream cipher by
 *          encrypting the previous ciphertext block. Requires an
 *          initialization vector (IV). Errors propagate for one block.
 */
#define DTHE_SM4_CFB         (0X00000006U)

/**
 * \brief SM4 Encryption operation
 * \details Specifies that the SM4 operation should encrypt plaintext data
 *          to produce ciphertext output.
 */
#define DTHE_SM4_ENCRYPT     (0x00000000U)

/**
 * \brief SM4 Decryption operation
 * \details Specifies that the SM4 operation should decrypt ciphertext data
 *          to produce plaintext output.
 */
#define DTHE_SM4_DECRYPT     (0x00000001U)



/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/**
 * \brief
 *  DTHE SM4 Driver Error code
 *
 * \details
 *  The enumeration describes all the possible return and error codes which
 *  the DTHE SM4 Driver can return
 */
typedef enum DTHE_SM4_Return_e
{
    /**< Success/pass return code */
    DTHE_SM4_RETURN_SUCCESS                  = 0x62F699D9U,
    /**< General or unspecified failure/error */
    DTHE_SM4_RETURN_FAILURE                  = 0x944D259AU
}DTHE_SM4_Return_t;


/**
 * \brief Parameters structure for SM4 cryptographic operations
 *
 * \details This structure contains all parameters required to perform SM4
 *          encryption or decryption operations. It specifies the algorithm
 *          mode, operation type, cryptographic keys, initialization
 *          vectors, and data buffers.
 */
typedef struct DTHE_SM4_Params_t
{
    /**
     * \brief Algorithm mode to be performed by the SM4 Driver
     *
     * \details Specifies the block cipher mode of operation. Valid values are:
     *          - DTHE_SM4_ECB: Electronic Codebook mode (no IV required)
     *          - DTHE_SM4_CBC: Cipher Block Chaining mode (IV required)
     *          - DTHE_SM4_CTR: Counter mode (IV required)
     *          - DTHE_SM4_OFB: Output Feedback mode (IV required)
     *          - DTHE_SM4_CFB: Cipher Feedback mode (IV required)
     */
    uint32_t            algoType;

    /**
     * \brief Operation type to be performed by the SM4 Driver
     *
     * \details Specifies whether to encrypt or decrypt data. Valid values are:
     *          - DTHE_SM4_ENCRYPT: Encrypt plaintext to ciphertext
     *          - DTHE_SM4_DECRYPT: Decrypt ciphertext to plaintext
     */
    uint32_t            opType;

    /**
     * \brief Pointer to the 128-bit encryption/decryption key
     *
     * \details Points to an array of 4 uint32_t values (16 bytes total)
     *          representing the SM4 key. The key must remain valid for the
     *          duration of the operation. SM4 uses a fixed 128-bit key
     *          size.
     *
     * \note The key array must be properly aligned (4-byte alignment).
     * \warning Ensure the key is stored securely and cleared from memory
     *          after use.
     */
    uint32_t*           ptrKey;

    /**
     * \brief Pointer to the Initialization Vector (IV)
     *
     * \details Points to an array of 4 uint32_t values (16 bytes total)
     *          representing the initialization vector. Required for all
     *          modes except ECB. The IV must remain valid for the duration
     *          of the operation.
     *
     * \note For ECB mode, this parameter is ignored and can be NULL.
     * \note For other modes (CBC, CTR, OFB, CFB), this parameter is
     *       mandatory.
     * \note The IV array must be properly aligned (4-byte alignment).
     */
    uint32_t*           ptrIV;

    /**
     * \brief Size of the data to be processed in bytes
     *
     * \details Specifies the total length of input data to be encrypted or
     *          decrypted. The data is processed in 16-byte blocks, with
     *          partial blocks handled automatically.
     *
     * \note For decryption operations, this value must be 4-byte aligned.
     * \note The actual data processed may be padded to block boundaries
     *       internally.
     */
    uint32_t            dataLenBytes;

    /**
     * \brief Pointer to the encrypted data buffer
     *
     * \details Usage depends on the operation type:
     *
     * <b>Decryption Operation (opType = DTHE_SM4_DECRYPT):</b>
     * - INPUT: Points to the ciphertext data to be decrypted
     * - The buffer must contain at least dataLenBytes of valid
     *   ciphertext
     *
     * <b>Encryption Operation (opType = DTHE_SM4_ENCRYPT):</b>
     * - OUTPUT: Points to the buffer where encrypted data will be
     *   written
     * - The buffer must be large enough to hold dataLenBytes of
     *   ciphertext
     *
     * \note The buffer must remain valid for the duration of the
     *       operation.
     * \note The buffer must be properly aligned (4-byte alignment).
     * \note For in-place operation, this can point to the same buffer as
     *       ptrPlainTextData.
     */
    uint32_t*           ptrEncryptedData;

    /**
     * \brief Pointer to the plaintext data buffer
     *
     * \details Usage depends on the operation type:
     *
     * <b>Decryption Operation (opType = DTHE_SM4_DECRYPT):</b>
     * - OUTPUT: Points to the buffer where decrypted plaintext will be
     *   written
     * - The buffer must be large enough to hold dataLenBytes of
     *   plaintext
     *
     * <b>Encryption Operation (opType = DTHE_SM4_ENCRYPT):</b>
     * - INPUT: Points to the plaintext data to be encrypted
     * - The buffer must contain at least dataLenBytes of valid plaintext
     *
     * \note The buffer must remain valid for the duration of the
     *       operation.
     * \note The buffer must be properly aligned (4-byte alignment).
     * \note For in-place operation, this can point to the same buffer as
     *       ptrEncryptedData.
     */
    uint32_t*           ptrPlainTextData;

}DTHE_SM4_Params;

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* ========================================================================== */
/*                              Function Definitions                          */
/* ========================================================================== */

/**
 * \brief               Function to open and initialize the DTHE SM4 Driver.
 *
 * \details             This function initializes the SM4 cryptographic
 *                      engine by clearing any previous context data and
 *                      disabling DMA operations. It must be called before
 *                      performing any SM4 encryption or decryption
 *                      operations. The function prepares the hardware for
 *                      subsequent SM4 operations.
 *
 * \param  handle       [in] DTHE handle returned from DTHE_open(). This
 *                      handle provides access to the DTHE hardware
 *                      instance and its configuration. Must not be NULL.
 *
 * \return              DTHE_SM4_RETURN_SUCCESS if the SM4 driver was
 *                      successfully opened and initialized.
 *                      DTHE_SM4_RETURN_FAILURE if the handle is NULL or
 *                      initialization failed.
 *
 * \pre                 DTHE_open() must be called first to obtain a valid
 *                      DTHE handle.
 *
 * \post                SM4 hardware is ready for encryption/decryption
 *                      operations.
 *                      All previous context (keys, IVs) is cleared.
 *
 * \sa                  DTHE_SM4_close(), DTHE_SM4_execute()
 */
DTHE_SM4_Return_t DTHE_SM4_open(DTHE_Handle handle);

/**
 * \brief               Execute SM4 encryption or decryption operation
 *                      with specified parameters.
 *
 * \details             This function performs SM4 cryptographic
 *                      operations (encryption or decryption) using the
 *                      specified algorithm mode (ECB, CBC, CTR, OFB, or
 *                      CFB). The function:
 *                      - Validates input parameters (operation type,
 *                        algorithm type, data alignment)
 *                      - Configures the SM4 hardware with the provided
 *                        key and IV (if applicable)
 *                      - Processes data in 16-byte blocks
 *                      - Handles partial blocks (less than 16 bytes) for
 *                        the last block
 *                      - Returns the encrypted or decrypted data in the
 *                        output buffer
 *
 *                      Supported algorithm modes:
 *                      - DTHE_SM4_ECB: Electronic Codebook (no IV required)
 *                      - DTHE_SM4_CBC: Cipher Block Chaining (IV required)
 *                      - DTHE_SM4_CTR: Counter mode (IV required)
 *                      - DTHE_SM4_OFB: Output Feedback (IV required)
 *                      - DTHE_SM4_CFB: Cipher Feedback (IV required)
 *
 * \param  handle       [in] DTHE handle returned from DTHE_open(). Must
 *                      not be NULL.
 *
 * \param  ptrParams    [in] Pointer to DTHE_SM4_Params structure
 *                      containing:
 *                      - algoType: Algorithm mode (ECB, CBC, CTR, OFB,
 *                        CFB)
 *                      - opType: Operation type (DTHE_SM4_ENCRYPT or
 *                        DTHE_SM4_DECRYPT)
 *                      - ptrKey: Pointer to 128-bit (16-byte) key array
 *                        (4 x uint32_t)
 *                      - ptrIV: Pointer to initialization vector
 *                        (required for all modes except ECB)
 *                      - dataLenBytes: Length of data in bytes (must be
 *                        4-byte aligned for decryption)
 *                      - ptrEncryptedData: Input buffer for decryption,
 *                        output buffer for encryption
 *                      - ptrPlainTextData: Input buffer for encryption,
 *                        output buffer for decryption
 *                      Must not be NULL.
 *
 * \return              DTHE_SM4_RETURN_SUCCESS if the operation completed
 *                      successfully.
 *                      DTHE_SM4_RETURN_FAILURE if:
 *                      - handle or ptrParams is NULL
 *                      - Invalid algorithm type (not ECB, CBC, CTR, OFB,
 *                        or CFB)
 *                      - Invalid operation type (not ENCRYPT or DECRYPT)
 *                      - Data length not 4-byte aligned for decryption
 *                      - Mismatch between processed bytes and expected
 *                        data length
 *
 * \pre                 - DTHE_SM4_open() must be called successfully
 *                        before this function
 *                      - All buffer pointers in ptrParams must point to
 *                        valid memory
 *                      - Key must be 128 bits (16 bytes, 4 x uint32_t)
 *                      - IV must be provided for all modes except ECB
 *                      - For decryption, dataLenBytes must be 4-byte aligned
 *
 * \post                - Output buffer contains encrypted or decrypted
 *                        data
 *                      - SM4 hardware state is updated
 *
 * \note                - The function processes data in 16-byte blocks
 *                      - Partial blocks (< 16 bytes) are handled
 *                        automatically
 *                      - Input and output buffers can be the same for
 *                        in-place operation
 *                      - The function is blocking and waits for hardware
 *                        completion
 *
 * \warning             Ensure buffers are large enough to hold the output
 *                      data. For encryption, output size equals input
 *                      size. For decryption with padding, output may be
 *                      smaller than input.
 *
 * \sa                  DTHE_SM4_open(), DTHE_SM4_close(), DTHE_SM4_Params
 */
DTHE_SM4_Return_t DTHE_SM4_execute(DTHE_Handle handle,
    const DTHE_SM4_Params* ptrParams);

/**
 * \brief               Function to close and cleanup the DTHE SM4 Driver.
 *
 * \details             This function disables the SM4 cryptographic
 *                      engine by disabling DMA operations. It should be
 *                      called when SM4 operations are complete to properly
 *                      release hardware resources. After calling this
 *                      function, DTHE_SM4_open() must be called again
 *                      before performing new operations.
 *
 * \param  handle       [in] DTHE handle returned from DTHE_open(). Must
 *                      not be NULL.
 *
 * \return              DTHE_SM4_RETURN_SUCCESS if the SM4 driver was
 *                      successfully closed.
 *                      DTHE_SM4_RETURN_FAILURE if the handle is NULL or
 *                      close operation failed.
 *
 * \pre                 DTHE_SM4_open() must have been called successfully.
 *
 * \post                SM4 hardware DMA is disabled.
 *                      SM4 operations cannot be performed until
 *                      DTHE_SM4_open() is called again.
 *
 * \note                This function does not clear sensitive data (keys,
 *                      IVs) from hardware. If security requires clearing
 *                      such data, call DTHE_SM4_open() which clears the
 *                      context before closing.
 *
 * \sa                  DTHE_SM4_open(), DTHE_SM4_execute()
 */
DTHE_SM4_Return_t DTHE_SM4_close(DTHE_Handle handle);

#ifdef __cplusplus
}
#endif

#endif
/** @} */
