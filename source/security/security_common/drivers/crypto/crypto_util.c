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
 *  \file   pka_util.c
 *
 *  \brief  This file contains the utility functions of AsymCrypt driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <security_common/drivers/crypto/crypto_util.h>

/* ========================================================================== */
/*                          Macros and Typedefs                               */
/* ========================================================================== */

/** \brief  Max supported EM length in bytes for the PSS functions below
 *          (bounds their internal stack scratch buffers). Covers up to a
 *          4096-bit RSA modulus, the largest key size used in this repo. */
#define CRYPTO_UTIL_PSS_MAX_EM_LEN     (512U)

/** \brief  Max supported digest length in bytes (SHA-512) */
#define CRYPTO_UTIL_PSS_MAX_HASH_LEN   (64U)

/**
 *  \brief  PSS verification state shared across the Crypto_PSSVerify_*
 *          helper functions below (kept in one struct to keep each
 *          helper's parameter count down).
 */
struct Crypto_PSSVerifyCtx {
    /** Digest length for typeOfAlgo, in bytes */
    uint32_t hashLenInBytes;
    /** Expected salt length, in bytes */
    uint32_t saltLenInBytes;
    /** Length of the maskedDB field of EM, in bytes */
    uint32_t dbLen;
    /** Length of the zero padding at the start of DB, in bytes */
    uint32_t psLen;
    /** Mask clearing the bits above emBits in DB[0]/EM[0] */
    uint32_t lowMask;
};

/* ========================================================================== */
/*                     Static Function Declarations                           */
/* ========================================================================== */

static uint32_t Crypto_getHashLenBytes(uint32_t typeOfAlgo);

static uint32_t Crypto_PSSVerify_Validate(uint32_t modBits, const uint8_t *EM, uint32_t emLenInBytes,
                                           struct Crypto_PSSVerifyCtx *ctx);

static uint32_t Crypto_PSSVerify_RecoverDB(Crypto_ShaCallback shaCbFxn, const uint8_t *EM,
                                            const struct Crypto_PSSVerifyCtx *ctx, uint8_t *H, uint8_t *DB);

static uint32_t Crypto_PSSVerify_CheckHash(Crypto_ShaCallback shaCbFxn, const uint8_t *msgHash,
                                            const uint8_t *salt, const struct Crypto_PSSVerifyCtx *ctx,
                                            const uint8_t *expectedH);

/* ========================================================================== */
/*                      Static Function Definitions                          */
/* ========================================================================== */

/**
 *  \brief  Maps a \ref Crypto_AlgoTypes value to its digest length in bytes.
 *
 *  \return Digest length in bytes, or 0 for an unrecognized typeOfAlgo.
 */
static uint32_t Crypto_getHashLenBytes(uint32_t typeOfAlgo)
{
    uint32_t hashLen = 0U;

    switch(typeOfAlgo)
    {
        case HASH_ALG_SHA1:
            hashLen = 20U;
        break;
        case HASH_ALG_SHA2_256:
            hashLen = 32U;
        break;
        case HASH_ALG_SHA2_512:
            hashLen = 64U;
        break;
        case HASH_ALG_SHA2_384:
            hashLen = 48U;
        break;
        default:
        break;
    }

    return hashLen;
}

/**
 *  \brief  Validates EM's length/trailer and unmasks the leftmost bits
 *          allowed by modBits. Populates ctx->dbLen/psLen/lowMask.
 *
 *  \param  modBits  Bit length of the RSA modulus n
 *  \param  EM       Encoded message recovered from the signature
 *  \param  emLenInBytes  Length of EM in bytes
 *  \param  ctx      In: hashLenInBytes/saltLenInBytes. Out: dbLen/psLen/lowMask.
 *
 *  \return 0 if EM is well-formed so far, non-zero otherwise
 */
static uint32_t Crypto_PSSVerify_Validate(uint32_t modBits, const uint8_t *EM, uint32_t emLenInBytes,
                                           struct Crypto_PSSVerifyCtx *ctx)
{
    uint32_t topBits, highMask;
    uint32_t ret = 1U;

    if ((ctx->hashLenInBytes != 0U) &&
        (emLenInBytes >= (ctx->hashLenInBytes + ctx->saltLenInBytes + 2U)) &&
        (emLenInBytes <= CRYPTO_UTIL_PSS_MAX_EM_LEN) &&
        (EM[emLenInBytes - 1U] == 0xBCU))
    {
        ret = 0U;
    }

    if (ret == 0U)
    {
        ctx->dbLen = emLenInBytes - ctx->hashLenInBytes - 1U;
        ctx->psLen = emLenInBytes - ctx->hashLenInBytes - ctx->saltLenInBytes - 2U;

        topBits      = 8U - ((8U * emLenInBytes) - modBits + 1U);
        ctx->lowMask = ((uint32_t)1U << (topBits & 0x1FU)) - 1U;
        highMask     = ~ctx->lowMask;

        if ((EM[0U] & (uint8_t)(highMask & 0xFFU)) != 0U)
        {
            ret = 1U;
        }
    }

    return ret;
}

/**
 *  \brief  Recovers maskedDB via MGF1, then checks its zero-padding/0x01
 *          separator, per RFC 8017 Section 9.1.2 steps 9-12.
 *
 *  \param  shaCbFxn  Hash callback, refer \ref Crypto_ShaCallback
 *  \param  EM        Encoded message recovered from the signature
 *  \param  ctx       Populated by \ref Crypto_PSSVerify_Validate
 *  \param  H         Scratch buffer, at least ctx->hashLenInBytes long
 *  \param  DB        Recovered DB, caller-allocated, at least ctx->dbLen long
 *
 *  \return 0 if DB's padding is well-formed, non-zero otherwise
 */
static uint32_t Crypto_PSSVerify_RecoverDB(Crypto_ShaCallback shaCbFxn, const uint8_t *EM,
                                            const struct Crypto_PSSVerifyCtx *ctx, uint8_t *H, uint8_t *DB)
{
    uint32_t i;
    uint32_t ret;

    (void)memcpy(H, &EM[ctx->dbLen], ctx->hashLenInBytes);
    ret = Crypto_MGF1(shaCbFxn, ctx->hashLenInBytes, H, ctx->hashLenInBytes, DB, ctx->dbLen);

    if (ret == 0U)
    {
        for (i = 0U; i < ctx->dbLen; i++)
        {
            DB[i] ^= EM[i];
        }
        DB[0U] &= (uint8_t)(ctx->lowMask & 0xFFU);

        for (i = 0U; i < ctx->psLen; i++)
        {
            if (DB[i] != 0x00U)
            {
                ret = 1U;
            }
        }

        if ((ret == 0U) && (DB[ctx->psLen] != 0x01U))
        {
            ret = 1U;
        }
    }

    return ret;
}

/**
 *  \brief  Recomputes H' = Hash(0x00^8 || mHash || salt) and compares it
 *          against the H recovered from EM, per RFC 8017 Section 9.1.2
 *          steps 13-14.
 *
 *  \param  shaCbFxn  Hash callback, refer \ref Crypto_ShaCallback
 *  \param  msgHash   mHash, precomputed hash of the message
 *  \param  salt      Salt recovered from DB, ctx->saltLenInBytes long
 *  \param  ctx       Populated by \ref Crypto_PSSVerify_Validate
 *  \param  expectedH H recovered from EM, ctx->hashLenInBytes long
 *
 *  \return 0 if H' matches expectedH, non-zero otherwise
 */
static uint32_t Crypto_PSSVerify_CheckHash(Crypto_ShaCallback shaCbFxn, const uint8_t *msgHash,
                                            const uint8_t *salt, const struct Crypto_PSSVerifyCtx *ctx,
                                            const uint8_t *expectedH)
{
    uint8_t  mPrime[8U + CRYPTO_UTIL_PSS_MAX_HASH_LEN + CRYPTO_UTIL_PSS_MAX_HASH_LEN];
    uint8_t  HPrime[CRYPTO_UTIL_PSS_MAX_HASH_LEN];
    uint32_t ret;

    (void)memset(mPrime, 0x00, 8U);
    (void)memcpy(&mPrime[8U], msgHash, ctx->hashLenInBytes);
    (void)memcpy(&mPrime[8U + ctx->hashLenInBytes], salt, ctx->saltLenInBytes);

    ret = shaCbFxn(mPrime, 8U + ctx->hashLenInBytes + ctx->saltLenInBytes, HPrime);

    if (ret == 0U)
    {
        if (memcmp(HPrime, expectedH, ctx->hashLenInBytes) != 0)
        {
            ret = 1U;
        }
    }

    return ret;
}

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void Crypto_Uint8ToUint32(const uint8_t *source, uint32_t sourceLengthInBytes, uint32_t *dest)
{
    uint32_t i, t = 0;
    uint32_t *destPtr = dest;

    for (i=0; i< sourceLengthInBytes; i++)
    {
        t = (t << 8U) | source[i];
        if ((i & 3U) == 3U) {
            *destPtr = t;
            destPtr = destPtr + 1U;
            t = 0;
        }
    }
    if ((i & 3U) != 0U)
    {
        *destPtr = t << ((4U-(i&3U)) << 3U);
    }
    return;
}

void Crypto_Uint32ToUint8(const uint32_t *src, uint32_t sourceLengthInBytes, uint8_t *dest)
{
    uint32_t i, t;
    uint32_t numWords = sourceLengthInBytes / 4U;

    for (i=0U; i< numWords; i++)
    {
        t = src[i];
        dest[(i * 4U)]        = (uint8_t)((t >> 24U) & 0xFFU);
        dest[(i * 4U) + 1U]   = (uint8_t)((t >> 16U) & 0xFFU);
        dest[(i * 4U) + 2U]   = (uint8_t)((t >> 8U) & 0xFFU);
        dest[(i * 4U) + 3U]   = (uint8_t)(t & 0xFFU);
    }
    return;
}

void Crypto_Uint32ToBigInt(uint32_t *source, uint32_t sourceLengthInWords, uint32_t *dest)
{
    uint32_t i, t = 0, t2 = 0;
    t2 = sourceLengthInWords / 2U;

    for(i=0;i<t2;i++)
    {
        t = source[i];
        source[i] = source[sourceLengthInWords - 1U - i];
        source[sourceLengthInWords - 1U - i] = t;
    }
    dest[0] = sourceLengthInWords;
    for(i=0; i < sourceLengthInWords; i++)
    {
        dest[1U + i] = source[i];
    }

    return;
}

void Crypto_bigIntToUint32(uint32_t *source, uint32_t sourceLengthInWords, uint32_t *dest)
{
    uint32_t i, t = 0, t2 = 0;
    t2 = (sourceLengthInWords / 2U)+1U;

    for(i=1U; i<t2; i++)
    {
        t = source[i];
        source[i] = source[sourceLengthInWords-(i-1U)];
        source[sourceLengthInWords-(i-1U)] = t;
    }
    for(i=0; i < sourceLengthInWords; i++)
    {
        dest[i] = source[i+1U];
    }

    return;
}

void Crypto_PKCSPaddingForSign(const uint8_t *shaHash, uint32_t keyLengthInBytes, uint32_t typeOfAlgo, uint8_t *output)
{
    /**
     *  \brief  ASN.1 DER DigestInfo prefixes for EMSA-PKCS1-v1_5 encoding,
     *          as per RFC 8017 Section 9.2 Note 1.
     *  @{
     */
    static const uint8_t gCryptoSha1DigestInfo[] =
    {
        0x30U, 0x21U, 0x30U, 0x09U, 0x06U, 0x05U, 0x2bU, 0x0eU,
        0x03U, 0x02U, 0x1aU, 0x05U, 0x00U, 0x04U, 0x14U
    };

    static const uint8_t gCryptoSha256DigestInfo[] =
    {
        0x30U, 0x31U, 0x30U, 0x0dU, 0x06U, 0x09U, 0x60U, 0x86U,
        0x48U, 0x01U, 0x65U, 0x03U, 0x04U, 0x02U, 0x01U, 0x05U,
        0x00U, 0x04U, 0x20U
    };

    static const uint8_t gCryptoSha384DigestInfo[] =
    {
        0x30U, 0x41U, 0x30U, 0x0dU, 0x06U, 0x09U, 0x60U, 0x86U,
        0x48U, 0x01U, 0x65U, 0x03U, 0x04U, 0x02U, 0x02U, 0x05U,
        0x00U, 0x04U, 0x30U
    };

    static const uint8_t gCryptoSha512DigestInfo[] =
    {
        0x30U, 0x51U, 0x30U, 0x0dU, 0x06U, 0x09U, 0x60U, 0x86U,
        0x48U, 0x01U, 0x65U, 0x03U, 0x04U, 0x02U, 0x03U, 0x05U,
        0x00U, 0x04U, 0x40U
    };
    /** @} */

    uint32_t  i, psLen = 0, offset = 0;
    uint32_t shaLen = Crypto_getHashLenBytes(typeOfAlgo);
    const uint8_t *digestInfo = NULL;
    uint32_t digestInfoLen = 0;

    switch(typeOfAlgo)
    {
        case HASH_ALG_SHA1:
            digestInfo = gCryptoSha1DigestInfo;
            digestInfoLen = sizeof(gCryptoSha1DigestInfo);
        break;
        case HASH_ALG_SHA2_256:
            digestInfo = gCryptoSha256DigestInfo;
            digestInfoLen = sizeof(gCryptoSha256DigestInfo);
        break;
        case HASH_ALG_SHA2_512:
            digestInfo = gCryptoSha512DigestInfo;
            digestInfoLen = sizeof(gCryptoSha512DigestInfo);
        break;
        case HASH_ALG_SHA2_384:
            digestInfo = gCryptoSha384DigestInfo;
            digestInfoLen = sizeof(gCryptoSha384DigestInfo);
        break;
        default:
        break;
    }

    if (keyLengthInBytes >= (3U + digestInfoLen + shaLen))
    {
        psLen = keyLengthInBytes - 3U - digestInfoLen - shaLen;

        output[offset] = 0x00U;
        offset++;
        output[offset] = 0x01U;
        offset++;

        for(i = 0; i< psLen; i++)
        {
            output[offset+i] = 0xFFU;
        }
        offset += psLen;

        output[offset] = 0x00U;
        offset++;

        for(i = 0; i < digestInfoLen; i++)
        {
            output[offset + i] = digestInfo[i];
        }
        offset += digestInfoLen;

        for(i = 0; i < shaLen; i++)
        {
            output[offset + i] = shaHash[i];
        }
    }

    return;
}

uint32_t Crypto_MGF1(Crypto_ShaCallback shaCbFxn, uint32_t hashLenInBytes,
                      uint8_t *seedBuf, uint32_t seedLenInBytes,
                      uint8_t *mask, uint32_t maskLenInBytes)
{
    uint8_t  digest[CRYPTO_UTIL_PSS_MAX_HASH_LEN];
    uint32_t maskBytes = 0U, counter = 0U, toCopy;
    uint32_t ret = 0U;

    while (maskBytes < maskLenInBytes)
    {
        seedBuf[seedLenInBytes]      = (uint8_t)((counter >> 24U) & 0xFFU);
        seedBuf[seedLenInBytes + 1U] = (uint8_t)((counter >> 16U) & 0xFFU);
        seedBuf[seedLenInBytes + 2U] = (uint8_t)((counter >> 8U)  & 0xFFU);
        seedBuf[seedLenInBytes + 3U] = (uint8_t)(counter & 0xFFU);

        ret = shaCbFxn(seedBuf, seedLenInBytes + 4U, digest);
        if (ret != 0U)
        {
            break;
        }

        toCopy = maskLenInBytes - maskBytes;
        if (toCopy > hashLenInBytes)
        {
            toCopy = hashLenInBytes;
        }
        (void)memcpy(&mask[maskBytes], digest, toCopy);
        maskBytes += toCopy;
        counter++;
    }

    return ret;
}

uint32_t Crypto_PSSPaddingForSign(Crypto_ShaCallback shaCbFxn, uint32_t typeOfAlgo,
                                   const struct Crypto_PSSSaltInfo *saltInfo,
                                   uint32_t modBits, uint32_t emLenInBytes, uint8_t *output)
{
    const uint8_t *msgHash = saltInfo->msgHash;
    const uint8_t *salt = saltInfo->salt;
    uint32_t saltLenInBytes = saltInfo->saltLenInBytes;
    uint32_t hashLenInBytes = Crypto_getHashLenBytes(typeOfAlgo);
    uint32_t dbLen = 0U, psLen = 0U, i, topBits, lowMask;
    uint8_t  mPrime[8U + CRYPTO_UTIL_PSS_MAX_HASH_LEN + CRYPTO_UTIL_PSS_MAX_HASH_LEN];
    uint8_t  H[CRYPTO_UTIL_PSS_MAX_HASH_LEN + 4U];
    uint8_t  dbMask[CRYPTO_UTIL_PSS_MAX_EM_LEN];
    uint8_t  dbByte;
    uint32_t ret = 1U;

    if ((hashLenInBytes != 0U) &&
        (emLenInBytes >= (hashLenInBytes + saltLenInBytes + 2U)) &&
        (emLenInBytes <= CRYPTO_UTIL_PSS_MAX_EM_LEN))
    {
        ret = 0U;
    }

    if (ret == 0U)
    {
        dbLen = emLenInBytes - hashLenInBytes - 1U;
        psLen = emLenInBytes - hashLenInBytes - saltLenInBytes - 2U;

        (void)memset(mPrime, 0x00, 8U);
        (void)memcpy(&mPrime[8U], msgHash, hashLenInBytes);
        (void)memcpy(&mPrime[8U + hashLenInBytes], salt, saltLenInBytes);

        ret = shaCbFxn(mPrime, 8U + hashLenInBytes + saltLenInBytes, H);
    }

    if (ret == 0U)
    {
        ret = Crypto_MGF1(shaCbFxn, hashLenInBytes, H, hashLenInBytes, dbMask, dbLen);
    }

    if (ret == 0U)
    {
        for (i = 0U; i < dbLen; i++)
        {
            if (i < psLen)
            {
                dbByte = 0x00U;
            }
            else if (i == psLen)
            {
                dbByte = 0x01U;
            }
            else
            {
                dbByte = salt[i - psLen - 1U];
            }
            output[i] = dbByte ^ dbMask[i];
        }

        topBits = 8U - ((8U * emLenInBytes) - modBits + 1U);
        lowMask = ((uint32_t)1U << (topBits & 0x1FU)) - 1U;
        output[0U] &= (uint8_t)(lowMask & 0xFFU);

        (void)memcpy(&output[dbLen], H, hashLenInBytes);
        output[emLenInBytes - 1U] = 0xBCU;
    }

    return ret;
}

uint32_t Crypto_PSSVerify(Crypto_ShaCallback shaCbFxn, uint32_t typeOfAlgo,
                           const uint8_t *msgHash, uint32_t saltLenInBytes,
                           uint32_t modBits, const uint8_t *EM, uint32_t emLenInBytes)
{
    struct Crypto_PSSVerifyCtx ctx = { 0 };
    uint8_t  H[CRYPTO_UTIL_PSS_MAX_HASH_LEN + 4U];
    uint8_t  DB[CRYPTO_UTIL_PSS_MAX_EM_LEN];
    uint32_t ret;

    ctx.hashLenInBytes = Crypto_getHashLenBytes(typeOfAlgo);
    ctx.saltLenInBytes = saltLenInBytes;

    ret = Crypto_PSSVerify_Validate(modBits, EM, emLenInBytes, &ctx);

    if (ret == 0U)
    {
        ret = Crypto_PSSVerify_RecoverDB(shaCbFxn, EM, &ctx, H, DB);
    }

    if (ret == 0U)
    {
        ret = Crypto_PSSVerify_CheckHash(shaCbFxn, msgHash, &DB[ctx.psLen + 1U], &ctx, &EM[ctx.dbLen]);
    }

    return ret;
}
