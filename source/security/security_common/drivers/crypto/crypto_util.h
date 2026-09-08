/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
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
 *  \defgroup SECURITY_CRYPTO_UTIL_MODULE APIs for Crypto utilities
 *  \ingroup  SECURITY_MODULE
 *
 *  This module contains APIs to program and use the PKA.
 *
 *  @{
 */

/**
 *  \file crypto_util.h
 *
 *  \brief This file contains the prototype of crypto_util driver APIs
 */

#ifndef PKA_UTIL_H_
#define PKA_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif


/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/**
 *  \anchor Crypto_AlgoTypes
 *  \name crypto Algo Types
 *  @{
 */
/** \brief Hash Algo SHA-1 */
#define HASH_ALG_SHA1             (0x0U)
/** \brief Hash Algo SHA-256 */
#define HASH_ALG_SHA2_256         (0x1U)
/** \brief Hash Algo SHA-512 */
#define HASH_ALG_SHA2_512         (0x2U)
/** \brief Hash Algo SHA-384 */
#define HASH_ALG_SHA2_384         (0x3U)
/** @} */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                              Function Declarations                          */
/* ========================================================================== */

/**
 *  \brief  Function to convert uint8_t to uint32_t format
 *
 *  \param  source  Uint8_t type buffer for conversion
 *
 *  \param  sourceLengthInBytes  Length of source buffer in bytes
 *
 *  \param  dest    Resultant uint32_t buffer stored in dest
 */
void Crypto_Uint8ToUint32(const uint8_t *source, uint32_t sourceLengthInBytes, uint32_t *dest);

/**
 *  \brief  Function to convert uint32_t to uint8_t format
 *
 *  \param  src  Uint32_t type buffer for conversion
 *
 *  \param  sourceLengthInBytes  Length of source buffer in bytes
 *
 *  \param  dest Resultant uint8_t buffer stored in dest
 */
void Crypto_Uint32ToUint8(const uint32_t *src, uint32_t sourceLengthInBytes, uint8_t *dest);

/**
 *  \brief  Function to convert uint32_t to Bigint format
 *
 *  \param  source                  uint32_t type buffer for conversion
 *
 *  \param  sourceLengthInWords     length of source buffer in words
 *
 *  \param  dest                    Resultant bigint buffer stored in dest
 */
void Crypto_Uint32ToBigInt(uint32_t *source, uint32_t sourceLengthInWords, uint32_t *dest);

/**
 *  \brief  Function to convert Bigint to uint32_t format
 *
 *  \param  source                  Uint32_t type buffer for conversion
 *
 *  \param  sourceLengthInWords     Length of source buffer in words
 *
 *  \param  dest                    Resultant uint32 buffer stored in dest
 */
void Crypto_bigIntToUint32(uint32_t *source, uint32_t sourceLengthInWords, uint32_t *dest);

/**
 *  \brief  Padding function for sign
 *
 *  Builds the full EMSA-PKCS1-v1_5 encoding as per RFC 8017 Section 9.2:
 *  EM = 0x00 || 0x01 || PS (0xFF..) || 0x00 || DigestInfo (ASN.1 DER) || Hash
 *
 *  \param  shaHash             Calculated Hash of the message for padding
 *
 *  \param  keyLengthInBytes    Used while padding to match key and padded mesage size.
 *                              Must be >= 3 + digestInfoLen + hash length for typeOfAlgo,
 *                              otherwise output is left untouched.
 *
 *  \param  typeOfAlgo          Used while padding to check sha length, refer \ref Crypto_AlgoTypes
 *
 *  \param  output              Resultant padded buffer stored in dest
 */
void Crypto_PKCSPaddingForSign(const uint8_t *shaHash, uint32_t keyLengthInBytes, uint32_t typeOfAlgo, uint8_t *output);

/**
 *  \brief  Callback used by Crypto_MGF1/Crypto_PSSPaddingForSign/Crypto_PSSVerify
 *          to compute a hash digest. Same parameter shape as
 *          AsymCrypt_ExecuteShaCallback (asym_crypt.h) so existing SHA wrapper
 *          functions can be passed in directly without an adapter.
 *
 *  \note   Crypto_MGF1/Crypto_PSSPaddingForSign/Crypto_PSSVerify share
 *          internal static scratch buffers (to keep their stack footprint
 *          small) and are therefore NOT reentrant: do not call them
 *          concurrently from multiple contexts (e.g. an ISR and main, or
 *          multiple RTOS tasks) or recursively/nested within each other.
 *
 *  \param  inputBuf        Buffer to hash
 *
 *  \param  inputLenBytes   Length of inputBuf in bytes
 *
 *  \param  digestBuf       Resultant digest buffer (caller-sized to hashLenInBytes)
 *
 *  \return 0 on success, non-zero on failure
 */
typedef uint32_t (*Crypto_ShaCallback)(uint8_t *inputBuf, uint32_t inputLenBytes, uint8_t *digestBuf);

/**
 *  \brief  Message hash and salt inputs to \ref Crypto_PSSPaddingForSign,
 *          bundled to keep the function's parameter count down.
 *
 *  \param  msgHash         mHash, precomputed hash of the message, hashLenInBytes long
 *
 *  \param  salt            Random salt, saltLenInBytes long
 *
 *  \param  saltLenInBytes  Salt length (RFC 8017 recommends saltLen == hashLen)
 */
struct Crypto_PSSSaltInfo {
    /** mHash, precomputed hash of the message, hashLenInBytes long */
    const uint8_t *msgHash;
    /** Random salt, saltLenInBytes long */
    const uint8_t *salt;
    /** Salt length (RFC 8017 recommends saltLen == hashLen) */
    uint32_t       saltLenInBytes;
};

/**
 *  \brief  MGF1 mask generation function, refer to RFC 8017 Appendix B.2.1
 *          https://www.rfc-editor.org/rfc/rfc8017#appendix-B.2.1
 *
 *  \param  shaCbFxn        Hash callback, refer \ref Crypto_ShaCallback
 *
 *  \param  hashLenInBytes  Digest length produced by shaCbFxn (e.g. 32 for SHA-256)
 *
 *  \param  seedBuf         Seed buffer; MUST have (seedLenInBytes + 4) bytes of
 *                          writable space - the trailing 4 bytes are scratch used
 *                          to append the big-endian counter before each hash call
 *
 *  \param  seedLenInBytes  Length of the seed (excluding the 4-byte counter scratch)
 *
 *  \param  mask            Resultant mask, caller-allocated, maskLenInBytes long
 *
 *  \param  maskLenInBytes  Requested mask length
 *
 *  \return 0 on success, non-zero if shaCbFxn ever fails
 */
uint32_t Crypto_MGF1(Crypto_ShaCallback shaCbFxn, uint32_t hashLenInBytes,
                      uint8_t *seedBuf, uint32_t seedLenInBytes,
                      uint8_t *mask, uint32_t maskLenInBytes);

/**
 *  \brief  Builds the EMSA-PSS encoding for RSA signing, refer to RFC 8017
 *          Section 9.1.1 https://www.rfc-editor.org/rfc/rfc8017#section-9.1.1
 *
 *  EM = maskedDB || H || 0xBC, where
 *  M' = 0x00^8 || mHash || salt, H = Hash(M'),
 *  DB = 0x00^psLen || 0x01 || salt, maskedDB = DB XOR MGF1(H, dbLen),
 *  with the leftmost bits of maskedDB[0] cleared per emBits = modBits - 1.
 *
 *  \param  shaCbFxn        Hash callback, refer \ref Crypto_ShaCallback
 *
 *  \param  typeOfAlgo      Hash algorithm used for mHash/H/MGF1, refer \ref Crypto_AlgoTypes
 *
 *  \param  saltInfo        Message hash and salt, refer \ref Crypto_PSSSaltInfo
 *
 *  \param  modBits         Bit length of the RSA modulus n
 *
 *  \param  emLenInBytes    Length of the output EM buffer (ceil(modBits/8))
 *
 *  \param  output          Resultant EM buffer, emLenInBytes long
 *
 *  \return 0 on success, non-zero on failure (shaCbFxn failure or emLenInBytes too small)
 */
uint32_t Crypto_PSSPaddingForSign(Crypto_ShaCallback shaCbFxn, uint32_t typeOfAlgo,
                                   const struct Crypto_PSSSaltInfo *saltInfo,
                                   uint32_t modBits, uint32_t emLenInBytes, uint8_t *output);

/**
 *  \brief  Verifies an EMSA-PSS encoding for RSA signature verification,
 *          refer to RFC 8017 Section 9.1.2
 *          https://www.rfc-editor.org/rfc/rfc8017#section-9.1.2
 *
 *  Recovers the salt from EM itself (unlike re-running Crypto_PSSPaddingForSign,
 *  which requires the original salt and is therefore NOT valid verification).
 *
 *  \param  shaCbFxn        Hash callback, refer \ref Crypto_ShaCallback
 *
 *  \param  typeOfAlgo      Hash algorithm used for mHash/H/MGF1, refer \ref Crypto_AlgoTypes
 *
 *  \param  msgHash         mHash, precomputed hash of the message, hashLenInBytes long
 *
 *  \param  saltLenInBytes  Expected salt length
 *
 *  \param  modBits         Bit length of the RSA modulus n
 *
 *  \param  EM              Encoded message recovered from the signature (RSA public op output)
 *
 *  \param  emLenInBytes    Length of EM in bytes
 *
 *  \return 0 if consistent (verification passed), non-zero if inconsistent or on failure
 */
uint32_t Crypto_PSSVerify(Crypto_ShaCallback shaCbFxn, uint32_t typeOfAlgo,
                           const uint8_t *msgHash, uint32_t saltLenInBytes,
                           uint32_t modBits, const uint8_t *EM, uint32_t emLenInBytes);


#ifdef __cplusplus
}
#endif

#endif /* PKA_UTIL_H_ */
/** @} */
