/*
* Copyright 2024-25 by Cryptography Research, Inc. (Rambus)
* All rights reserved.  Unauthorized use (including, without limitation,
* distribution and copying) is strictly prohibited.  All use
* requires, and is subject to, explicit written authorization and
* nondisclosure agreements with your supplier or Cryptography Research (Rambus). 
*/

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
 *  \file   pke.c
 *
 *  \brief  This file contains the implementation of PKE ( Ultra lite Security Accelerator)(Public Key Accelerator) driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stddef.h>
#include <modules/ecdsa/ecdsa.h>
#include <crypto/asym_crypt.h>
#include <crypto/rng/rng.h>
#include <modules/crypto/crypto_rng_interface.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/*
 * Timeout values in microsecs
 */
/**
 * Timeout for register updates to take effect - 10us
 */
#define PKE_REG_TIMEOUT                            (10U)

/**
 * Timeout for compare of 2 bignums - 100us
 */
#define PKE_COMPARE_TIMEOUT                        (100U)

/**
 * Timeout for modexp CRT operation - 50ms
 */
#define PKE_MODEXP_CRT_TIMEOUT                     (50000U)

/**
 * Timeout for modexp operation - 10ms
 */
#define PKE_MODEXP_TIMEOUT                         (10000U)

/**
 * Timeout for ECDSA verify operation - 10ms
 */
#define PKE_ECDSA_VERIFY_TIMEOUT                   (10000U)

/**
 * Timeout for ECDSA sign operation - 10ms
 */
#define PKE_ECDSA_SIGN_TIMEOUT                     (10000U)

/** \brief device type HSSE */
#define DEVTYPE_HSSE         (0x0AU)

/** \brief NULL handle for AsymCrypt */
#define ASYM_CRYPT_NULL_HANDLE ((AsymCrypt_Handle)NULL)

/** \brief PKE NO ERROR */
#define PKE_NO_ERROR_STATUS                        (0)

/** \brief Any status other than 0 is fault */
#define PKE_FAULT_STATUS                           (-15)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */

static uint32_t PKE_countLeadingZeros(uint32_t x);
static uint32_t PKE_bigIntBitLen(const uint32_t bn[ECDSA_MAX_LENGTH]);
static AsymCrypt_Return_t PKE_isBigIntZero(const uint32_t bn[RSA_MAX_LENGTH]);
static AsymCrypt_Return_t PKE_getPrimeCurveId (const struct AsymCrypt_ECPrimeCurveP *curveParams, uint32_t *pkeCurveType);

cri_pke_context_t 	gPKEContext;
cri_pke_t 			gPKE;

RNG_Handle     pke_rng_handle   = NULL;
uint8_t signatureRPrime[68];

uint32_t pke_temp_buff[RSA_MAX_LENGTH] = {0U};

extern const uint32_t numPrimeCurves;
extern const ECDSA_primeCurve primeCurves[];

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

AsymCrypt_Handle AsymCrypt_open(uint32_t index)
{
    AsymCrypt_Handle handle;

    /* Open rng instance */
    pke_rng_handle = gRngHandle;
    DebugP_assert(pke_rng_handle != NULL);

    RNG_setup(pke_rng_handle);

    gPKEContext.copy_flags = 0U;
    gPKEContext.resp_flags = 0U;

    /* Flush all the errors and clears the memories of PKE RAM */
    (void)cri_pke_flush(&gPKEContext);

    gPKE = cri_pke_open(&gPKEContext);
    if(gPKE == NULL)
    {
        handle = NULL;
    }
    else
    {
        handle = &gPKE;
    }

    return handle;
}

AsymCrypt_Return_t AsymCrypt_close(AsymCrypt_Handle handle)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;

    /* Open rng instance */
    if(RNG_RETURN_FAILURE != RNG_close(pke_rng_handle))
    {
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    }
    else
    {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAPrivate(AsymCrypt_Handle handle,
                    const uint32_t m[RSA_MAX_LENGTH],
                    const struct AsymCrypt_RSAPrivkey *k,
                    uint32_t result[RSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t pubmod_bitsize = k->n[0U]*4U;
    uint32_t exp_size = k->e[0U]*4U;
    uint32_t size = (k->d[0U])/2U;

    struct cri_rsa_key pke_rsa_key_ctx = {
        .bits = (pubmod_bitsize*8U),
        .flags = 0,
        .n = (uint8_t *)&k->n[1U],
        .e = (uint8_t *)&k->e[1U],
        .elength = exp_size,
        .d1 = (uint8_t *)&k->d[1U],
        .d2 = NULL,
        .message = (uint8_t *)&m[1U],
        .signature = (uint8_t *)&result[1U]
    };

    /* check sizes, sizes of s and n must match. */
    if ((!((size <= 1U) || (size > ((RSA_MAX_LENGTH - 1U) >> 1U)) ||
        (m[0U] > (size * 2U)))))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        pkeStatus = cri_pke_rsa_sign(&gPKEContext, &pke_rsa_key_ctx);
        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            result[0U] = k->n[0U];
            status  = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAPublic(AsymCrypt_Handle handle,
                    const uint32_t m[RSA_MAX_LENGTH],
                    const struct AsymCrypt_RSAPubkey *k,
                    uint32_t result[RSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t pubmod_bitsize = k->n[0U]*4U;
    uint32_t exp_size = k->e[0U]*4U;
    uint32_t size = k->n[0U];

    struct cri_rsa_key pke_rsa_key_ctx = {
        .bits = (pubmod_bitsize*8U),
        .flags = 0,
        .n = (uint8_t *)&k->n[1U],
        .e = (uint8_t *)&k->e[1U],
        .elength = exp_size,
        .d1 = NULL,
        .d2 = NULL,
        .signature = (uint8_t *)&m[1U],
        .message = (uint8_t *)&result[1U]
    };

    /* check sizes, sizes of s and n must match. */
    if ((!((size <= 1U) || (size > (RSA_MAX_LENGTH - 1U)) ||
           (m[0U] != size) || (k->e[0U] > (RSA_MAX_LENGTH - 1U)))))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        pkeStatus = cri_pke_rsa_pub(&gPKEContext, &pke_rsa_key_ctx);
        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            result[0U] = k->n[0U];
            status  = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAKeyGenPrivate(AsymCrypt_Handle handle,
                    struct AsymCrypt_RSAPrivkey *k,
                    uint32_t keybitsize)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t pubmod_bitsize = keybitsize;
    uint32_t exp_size = k->e[0U]*4U;
    uint32_t size = (keybitsize/(8U*4U));

    struct cri_rsa_key pke_rsa_key_ctx = {
        .bits = pubmod_bitsize,
        .flags = 0,
        .n = (uint8_t *)&k->n[1U],
        .e = (uint8_t *)&k->e[1U],
        .elength = exp_size,
        .d1 = (uint8_t *)&k->d[1U],
        .d2 = NULL,
        .message = NULL,
        .signature = NULL
    };

    /* check sizes, sizes of s and n must match. */
    if (!((size <= 1U) || (size > (RSA_MAX_LENGTH - 1U))))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }
    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        pkeStatus = cri_pke_rsa_key_gen(&gPKEContext, &pke_rsa_key_ctx);
        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            k->n[0U] = size;
            k->d[0U] = size;
            status  = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAKeyGenPublic(AsymCrypt_Handle handle,
                    const struct AsymCrypt_RSAPrivkey *privKey,
                    struct AsymCrypt_RSAPubkey *pubKey,
                    uint32_t keybitsize)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t size = (keybitsize/(8U*4U));

    /* check sizes, sizes of s and n must match. */
    if (!((size <= 1U) || (size > (RSA_MAX_LENGTH - 1U))))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        /* Copy the RSA modulus i.e. n from the Private Key */
        (void)memcpy(&pubKey->n[1U], &privKey->n[1U], privKey->n[0U]*4U);

        /* Copy the RSA exponent i.e. e from the Public Key */
        (void)memcpy(&pubKey->e[1U], &privKey->e[1U], privKey->e[0U]*4U);

        pubKey->n[0U] = privKey->n[0U];
        pubKey->e[0U] = privKey->e[0U];
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_ECDSASign(AsymCrypt_Handle handle,
                    const struct AsymCrypt_ECPrimeCurveP *cp,
                    const uint32_t priv[ECDSA_MAX_LENGTH],
                    const uint32_t k[ECDSA_MAX_LENGTH],
                    const uint32_t h[ECDSA_MAX_LENGTH],
                    struct AsymCrypt_ECDSASig *sig)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t curveType = 0U;
    cri_ecc_curve_t curve;
    uint32_t bigEndianHash[ECDSA_MAX_LENGTH];
    uint8_t littleEndianHash[ECDSA_MAX_LENGTH*4U];
    uint32_t size = cp->prime[0U];
    uint32_t nonce[ECDSA_MAX_LENGTH*2U];
    uint32_t* noncePtr = NULL;

    /* check sizes */
    if ((!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0U]) || (size < cp->a[0U]) ||
           (size < cp->b[0U]) || (size < cp->g.x[0U]) ||
           (size < cp->g.y[0U]) || (size < priv[0U]) ||
           (size < k[0U]))))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {

        if((k == NULL) || (PKE_isBigIntZero(&k[0U]) == ASYM_CRYPT_RETURN_SUCCESS))
        {
            /* Signing operation will generate random nonce, if random 'k' is not provided or set to 0*/
            noncePtr = NULL;
        }
        else
        {
            /* Get Nonce Value if not null, pad remaining bytes with 0
             * Nonce length can be at max 2*curveLen for PKE, any extra bits will be truncated in signing operation
             * For Deterministic ECDSA, k is not NULL and nonceLen (k[0U]*4U) is less than 2*curveLen
             */
            (void)memset(nonce,0,sizeof(nonce));
            (void)memcpy(nonce,&k[1U],k[0U]*4U);
            noncePtr = &nonce[0U];
        }

        /* Get the size of input hash */
        size = h[0U];

        /* PKE only supports Hash as a BigEndian input */
        Crypto_bigIntToUint32((uint32_t *)&h[0U], size, (uint32_t *)&bigEndianHash[0U]);
        
        /* PKE only supports Hash as a BigEndian input */
        Crypto_Uint32ToUint8((uint32_t *)&bigEndianHash[0U], size*4U, (uint8_t *)&littleEndianHash[0U]);

        /* Mapping the curve parameters as input to curve type */
        status = PKE_getPrimeCurveId(cp, &curveType);
        if(ASYM_CRYPT_RETURN_SUCCESS == status)
        {
            /* Get curve id based on the cri_ecc_curve_t param set */
            curve = cri_pke_get_curve(curveType);

            /* Get signature */
            pkeStatus = cri_pke_ecdsa_sign_extended(gPKE, curve, &priv[1U], NULL, noncePtr, &littleEndianHash[0U], size*4U, &sig->r[1U], &sig->s[1U]);

            sig->r[0U] = cp->prime[0U];
            sig->s[0U] = cp->prime[0U];

            /* Revert the input back to original state */
            Crypto_Uint32ToBigInt((uint32_t *)&bigEndianHash[0U], size, (uint32_t *)&h[0U]);

            if (pkeStatus == PKE_NO_ERROR_STATUS)
            {
                status  = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_ECDSAVerify(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp,
                        const struct AsymCrypt_ECPoint *pub,
                        const struct AsymCrypt_ECDSASig *sig,
                        const uint32_t h[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t curveType = 0;
    cri_ecc_curve_t curve;
    uint32_t bigEndianHash[ECDSA_MAX_LENGTH];
    uint8_t littleEndianHash[ECDSA_MAX_LENGTH*4U];
    uint32_t size = cp->prime[0U];

    /* check sizes */
    if ((!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0U]) || (size < cp->a[0U]) ||
           (size < cp->b[0U]) || (size < cp->g.x[0U]) ||
           (size < cp->g.y[0U]) || (size < pub->x[0U]) ||
           (size < pub->y[0U]) || (size < sig->r[0U]) ||
           (size < sig->s[0U]))) &&
            (PKE_isBigIntZero(sig->r) != ASYM_CRYPT_RETURN_SUCCESS) &&
            (PKE_isBigIntZero(sig->s) != ASYM_CRYPT_RETURN_SUCCESS))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        /* Get the size of input hash */
        size = h[0U];

        /* PKE only supports Hash as a BigEndian input */
        Crypto_bigIntToUint32((uint32_t *)&h[0U], size, (uint32_t *)&bigEndianHash[0U]);
        
        /* PKE only supports Hash as a BigEndian input */
        Crypto_Uint32ToUint8((uint32_t *)&bigEndianHash[0U], size*4U, (uint8_t *)&littleEndianHash[0U]);
        
        /* Mapping the curve parameters as input to curve type */
        status = PKE_getPrimeCurveId(cp, &curveType);
        if(ASYM_CRYPT_RETURN_SUCCESS == status)
        {
            /* Get curve id based on the cri_ecc_curve_t param set */
            curve = cri_pke_get_curve(curveType);

            /* Call the ECDSA Verify function */
            pkeStatus = cri_pke_ecdsa_verify_hash(gPKE, curve, &pub->x[1U], &pub->y[1U], &littleEndianHash[0U], size*4U, &sig->r[1U], &sig->s[1U], &signatureRPrime);

            /* Revert the input back to original state */
            Crypto_Uint32ToBigInt((uint32_t *)&bigEndianHash[0U], size, (uint32_t *)&h[0U]);

            if (pkeStatus == PKE_NO_ERROR_STATUS)
            {
                status  = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_ECDSAKeyGenPrivate(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp,
                        uint32_t priv[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t curveType = 0;
    cri_ecc_curve_t curve;
    uint32_t size = cp->prime[0U];

    /* check sizes */
    if (!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0U]) || (size < cp->a[0U]) ||
           (size < cp->b[0U]) || (size < cp->g.x[0U]) ||
           (size < cp->g.y[0U])))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {      
        /* Mapping the curve parameters as input to curve type */
        status = PKE_getPrimeCurveId(cp, &curveType);
        if(ASYM_CRYPT_RETURN_SUCCESS == status)
        {
            /* Get curve id based on the cri_ecc_curve_t param set */
            curve = cri_pke_get_curve(curveType);

            /* Call the ECDSA KeyGen function to generate private key */
            pkeStatus = cri_pke_ecc_private_keygen(gPKE, curve, &priv[1U]);

            if (pkeStatus == PKE_NO_ERROR_STATUS)
            {
                status  = ASYM_CRYPT_RETURN_SUCCESS;
                priv[0U] = size;
            }
            else
            {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_ECDSAKeyGenPublic(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp,
                        struct AsymCrypt_ECPoint *pub,
                        const uint32_t priv[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t curveType = 0;
    cri_ecc_curve_t curve;
    uint32_t size = cp->prime[0U];

    /* check sizes */
    if (!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0U]) || (size < cp->a[0U]) ||
           (size < cp->b[0U]) || (size < cp->g.x[0U]) ||
           (size < cp->g.y[0U]) || (size < priv[0U])))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {      
        /* Mapping the curve parameters as input to curve type */
        status = PKE_getPrimeCurveId(cp, &curveType);
        if(ASYM_CRYPT_RETURN_SUCCESS == status)
        {
            /* Get curve id based on the cri_ecc_curve_t param set */
            curve = cri_pke_get_curve(curveType);

            /* Call the ECDSA KeyGen function to generate private key */
            pkeStatus = cri_pke_ecdsa_keygen(gPKE, curve, &priv[1U], &pub->x[1U], &pub->y[1U]);

            if (pkeStatus == PKE_NO_ERROR_STATUS)
            {
                status  = ASYM_CRYPT_RETURN_SUCCESS;
                pub->x[0U] = size;
                pub->y[0U] = size;
            }
            else
            {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_EddsaSign(AsymCrypt_Handle handle,
                             AsymCrypt_ExecuteShaCallback shaCbFxn,
                             const struct AsymCrypt_EddsaKey *key,
                             const uint8_t *ptrData,
                             const uint32_t dataSizeByte,
                             struct AsymCrypt_EddsaSig *sig,
                             AsymCrypt_EdCurveType_t input_curve)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    cri_ecc_curve_t curve = NULL;
    uint8_t r[EDDSA_ED448_HASH_LEN];
    uint8_t hash[EDDSA_ED448_HASH_LEN];
    uint8_t *ptrdataInput = NULL;
    uint8_t tempBuf[EDDSA_ED448_HASH_LEN*2U];
    uint8_t k0[EDDSA_ED448_HASH_LEN];
    uint8_t k1[EDDSA_ED448_HASH_LEN];
    uint32_t hash_len;
    uint32_t key_len;

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        hash_len = EDDSA_ED25519_HASH_LEN;
        key_len = EDDSA_ED25519_KEY_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED25519);
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        hash_len = EDDSA_ED448_HASH_LEN;
        key_len = EDDSA_ED448_KEY_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED448);
    }
    else 
    {   
        hash_len = 0U;
        key_len  = 0U;
    }

    if ((curve == NULL) || (handle == NULL) || (shaCbFxn == NULL)|| (ptrData == NULL)|| (key == NULL) || (sig == NULL)) {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        /* Backup User Data before using the space */
        ptrdataInput = (uint8_t*)(ptrData - hash_len);
        (void)memcpy(tempBuf, ptrdataInput, hash_len);

        /* Get private key hash (SHA/SHAKE digest) */
        status = shaCbFxn((uint8_t*)&(key->privKey), key_len, hash);

        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            /* Copy 1st part of privKey hash */
            (void)memcpy(k0, hash, key_len);

            /* Copy 2nd part of privKey hash */
            (void)memcpy(k1, (hash + key_len), key_len);

            /* Copy 1st part of privKey hash, and clamp it */
            if (input_curve == ASYM_CRYPT_CURVE_TYPE_EDDSA_25519) {
                k0[0U] &= 0xF8;
                k0[31] &= 0x7F;
                k0[31] |= 0x40;
            } else {
                k0[0U] &= 0xFC;
                k0[31] &= 0x00;
                for (uint8_t i = 1; i <= 31 ; i++) {
                    k0[i] |= 0x80;
                }
            }

            /* Copy k1 || M into data input */
            ptrdataInput += key_len;
            (void)memcpy(ptrdataInput, k1, key_len);

            /* Store the nonce i.e. r = Hash(k1||M) */
            status = shaCbFxn(ptrdataInput, (dataSizeByte + key_len), r);
        }

        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            /* Get 'R' value of signature */
            pkeStatus = cri_pke_eddsa_sign_phase1(gPKE, curve, r, k0, sig->R);
            if (pkeStatus == PKE_NO_ERROR_STATUS) {
                status = ASYM_CRYPT_RETURN_SUCCESS;
            } else {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
        }

        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            /* Get SHA-512 Hash(sig.R||pubKey||msg) */
            ptrdataInput =  (uint8_t*)(ptrData - hash_len);
            (void)memcpy(&ptrdataInput[0U], sig->R, key_len);
            (void)memcpy(&ptrdataInput[key_len], key->pubKey, key_len);
            status = shaCbFxn(ptrdataInput, (dataSizeByte + (2U*key_len)), hash);
        }

        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            /* Get 'S' value of signature */
            pkeStatus = cri_pke_eddsa_sign_phase2(gPKE, curve, hash, sig->s);
        }

        /* Copy back data from tempBuf*/
        ptrdataInput =  (uint8_t*)(ptrData - hash_len);
        (void)memcpy(ptrdataInput, tempBuf, hash_len);
    }

    if (pkeStatus == PKE_NO_ERROR_STATUS) {
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    } else {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    return (status);

}

AsymCrypt_Return_t AsymCrypt_EddsaVerify(AsymCrypt_Handle handle,
                              AsymCrypt_ExecuteShaCallback shaCbFxn,
                              const uint8_t pubKey[EDDSA_MAX_KEY_LEN],
                              const uint8_t *ptrData,
                              const uint32_t dataSizeByte,
                              const struct AsymCrypt_EddsaSig *sig,
                              AsymCrypt_EdCurveType_t input_curve)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    cri_ecc_curve_t curve = NULL;
    uint32_t curvelen = 0;
    uint8_t    *ptrdataInput = NULL;
    uint8_t tempBuff[EDDSA_ED448_HASH_LEN];
    uint8_t hash[EDDSA_ED448_HASH_LEN];
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t hash_len;
    uint32_t key_len;

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        hash_len = EDDSA_ED25519_HASH_LEN;
        key_len = EDDSA_ED25519_KEY_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED25519);
        curvelen = cri_pke_get_curve_length(curve);
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        hash_len = EDDSA_ED448_HASH_LEN;
        key_len = EDDSA_ED448_KEY_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED448);
        curvelen = cri_pke_get_curve_length(curve);
    }else {
        /* Do Nothing, added to avoid MISRA.IF.NO_ELSE.*/
    }

    if ((curve == NULL) || (handle == NULL) || (shaCbFxn == NULL)|| (ptrData == NULL) || (sig == NULL)) {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        /* Copy 64 bytes before message(ptrData) to tempBuf */
        ptrdataInput = (uint8_t*)(ptrData - hash_len);
        (void)memcpy(tempBuff, ptrdataInput, hash_len);

        /* Get SHA-512 Hash(sig.R||pubKey||msg) */
        (void)memcpy(&ptrdataInput[0U], sig->R, key_len);
        (void)memcpy(&ptrdataInput[key_len], pubKey, key_len);

        status = shaCbFxn(ptrdataInput, dataSizeByte + (2U*key_len), hash);

        /*Restore back 64 Bytes from tempBuff*/
        (void)memcpy(ptrdataInput, tempBuff, hash_len);

        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            pkeStatus = cri_pke_eddsa_verify(gPKE, curve, pubKey, hash, curvelen, sig->R, sig->s, signatureRPrime);

            if (pkeStatus == PKE_NO_ERROR_STATUS) {
                /* PKE Eddsa Verify operation success*/
                status  = ASYM_CRYPT_RETURN_FAILURE;

                if (memcmp(sig->R, signatureRPrime, key_len) == 0) {
                    /* PKE Eddsa Verification Signature matches*/
                    status = ASYM_CRYPT_RETURN_SUCCESS;
                } else {
                    /* PKE Eddsa Verification Signature does not matches*/
                    status = ASYM_CRYPT_RETURN_FAILURE;
                }
            } else {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        } else {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}


AsymCrypt_Return_t AsymCrypt_EddsaGetPubKey(AsymCrypt_Handle handle,
                                   AsymCrypt_ExecuteShaCallback shaCbFxn,
                                   uint8_t privKey[EDDSA_MAX_KEY_LEN],
                                   uint8_t pubKey[EDDSA_MAX_KEY_LEN],
                                   AsymCrypt_EdCurveType_t input_curve)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    cri_ecc_curve_t curve = NULL;
    uint8_t privKeyHash[64];
    const uint8_t *privKeyPtr = &privKey[0U];
    uint32_t key_len = 0U;

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        key_len = EDDSA_ED25519_KEY_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED25519);
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        key_len = EDDSA_ED448_HASH_LEN;
        curve = cri_pke_get_curve(CRI_ECC_CURVE_ED448);
    }else {
        /* Do Nothing, added to avoid MISRA.IF.NO_ELSE.*/
    }

    if ((curve == NULL) || (shaCbFxn == NULL) || (privKey == NULL) || (pubKey == NULL)) {
        status = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        /*Get 64 byte SHA-512 Hash of private key*/
        status = shaCbFxn(privKey, key_len, privKeyHash);

        /*Only first half of privatekey hash is used, clamp the fist half and clear the second half*/
        if (input_curve == ASYM_CRYPT_CURVE_TYPE_EDDSA_25519) {
            privKeyHash[0U] &= 0xF8;
            privKeyHash[31] &= 0x7F;
            privKeyHash[31] |= 0x40;
        } else {
            privKeyHash[0U] &= 0xFC;
            privKeyHash[31] &= 0x00;
            for (uint8_t i = 1; i <= 31 ; i++) {
                privKeyHash[i] |= 0x80;
            }
        }

        (void)memset((uint8_t*)&privKeyHash[key_len], 0U, key_len);

        /*Get publicKey for the given privateKey
        * Note:-
        * This function cri_pke_eddsa_sign_phase1() returns "[nonce]*G" in encoded form which is 'R' (sig.R) in the EdDSA signature.
        * Nonce (64-byte value passed as 3rd parameter in this function), and G is base point of Ed25519 curve (G is stored in PKE-Rom)).
        * Since pubKey = [privKeyHash_clamped]*G in encoded form, where privKeyHash_clamped is first half of clamped sha512-hash of private Key,
        * So this function is used here to generate the public key from the given private key.
        * The 4th parameter (i.e. 2nd half clamped of privKeyHash) is used to prepare for cri_pke_eddsa_sign_phase2() in signature 'S' (sig.S) generation.
        * For generating public key, 4th parameter is dummy (so passing privKeyPtr just to ensure a valid pointer is passed).
        */
        pkeStatus = cri_pke_eddsa_sign_phase1(gPKE, curve, privKeyHash, privKeyPtr, pubKey);
        if (pkeStatus == PKE_NO_ERROR_STATUS) {
            /* Clear PKE Ram*/
           pkeStatus = cri_pke_clear_ram();
        }
    }

    if (pkeStatus == PKE_NO_ERROR_STATUS) {
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    } else {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_EcdhGenSharedSecret(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp,
                        const uint32_t priv[ECDSA_MAX_LENGTH],
                        const struct AsymCrypt_ECPoint *pubKey,
                        struct AsymCrypt_ECPoint *ecShSecret)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t curveType = 0;
    cri_ecc_curve_t curve;
    uint32_t size = cp->prime[0U];

    /* check sizes */
    if ((!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0U]) || (size < cp->a[0U]) ||
           (size < cp->b[0U]) || (size < cp->g.x[0U]) ||
           (size < cp->g.y[0U])  || (size < pubKey->x[0U]) ||
           (size < pubKey->y[0U]) || (size < priv[0U])))) {
        /* Checking handle is opened or not */
        if (NULL != handle) {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if (ASYM_CRYPT_RETURN_SUCCESS == status) {
        /* Mapping the curve parameters as input to curve type */
        status = PKE_getPrimeCurveId(cp, &curveType);
        if (ASYM_CRYPT_RETURN_SUCCESS == status) {
            /* Get curve id based on the cri_ecc_curve_t param set */
            curve = cri_pke_get_curve(curveType);

            /* Get signature */
            pkeStatus = cri_pke_ecdh(gPKE, curve, &priv[1U], &pubKey->x[1U], &ecShSecret->x[1U]);
            ecShSecret->x[0U] = pubKey->x[0U];

            if (0 == pkeStatus) {
                status  = ASYM_CRYPT_RETURN_SUCCESS;
            } else {
                status  = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_SM2DSASign(AsymCrypt_Handle handle,
                    const uint32_t priv[ECDSA_MAX_LENGTH],
                    const uint32_t k[ECDSA_MAX_LENGTH],
                    const uint32_t h[ECDSA_MAX_LENGTH],
                    struct AsymCrypt_SM2DSASig *sig)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    cri_ecc_curve_t curve;
    uint32_t bigEndianHash[ECDSA_MAX_LENGTH];
    uint8_t littleEndianHash[ECDSA_MAX_LENGTH*4U];
    uint32_t nonce[ECDSA_MAX_LENGTH*2U];
    uint32_t* noncePtr = NULL;
    uint32_t size = 8U;
    uint32_t curvelen = 0;

    /* check sizes */
    if (!((size < priv[0U]) || (size < h[0U]) || (size < k[0U])))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        if((k == (uint32_t *)NULL) || (PKE_isBigIntZero(&k[0U]) == ASYM_CRYPT_RETURN_SUCCESS))
        {
            /* Signing operation will generate random nonce, if random 'k' is not provided or set to 0*/
            noncePtr = (uint32_t *)NULL;
        }
        else
        {
            /* Get Nonce Value if not null, pad remaining bytes with 0
             * Nonce length can be at max 2*curveLen for PKE, any extra bits will be truncated in signing operation
             * For Deterministic SM2DSA, k is not NULL and nonceLen (k[0U]*4U) is less than 2*curveLen
             */
            (void)memset(nonce, 0, sizeof(nonce));
            (void)memcpy(nonce, &k[1U], k[0U]*4U);
            noncePtr = &nonce[0U];
        }

        /* PKE only supports Hash as a BigEndian input */
        Crypto_bigIntToUint32((uint32_t *)&h[0U], size, (uint32_t *)&bigEndianHash[0U]);
        
        /* PKE only supports Hash as a BigEndian input */
        Crypto_Uint32ToUint8((uint32_t *)&bigEndianHash[0U], size*4U, (uint8_t *)&littleEndianHash[0U]);

        /* Get SM2 curve */
        curve = cri_pke_get_curve(CRI_ECC_CURVE_SM2);
        /* Get curve length of SM2 */
        curvelen = cri_pke_get_curve_length(curve);

        /* Get signature */
        pkeStatus = cri_pke_ecdsa_sign_extended(gPKE, curve, &priv[1U], NULL, noncePtr,  &littleEndianHash[0U], curvelen, &sig->r[1U], &sig->s[1U]);

        sig->r[0U] = size;
        sig->s[0U] = size;

        /* Revert the input back to original state */
        Crypto_Uint32ToBigInt((uint32_t *)&bigEndianHash[0U], size, (uint32_t *)&h[0U]);

        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            status  = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_SM2DSAVerify(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPoint *pub,
                        const struct AsymCrypt_SM2DSASig *sig,
                        const uint32_t h[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    cri_ecc_curve_t curve;
    uint32_t bigEndianHash[ECDSA_MAX_LENGTH];
    uint8_t littleEndianHash[ECDSA_MAX_LENGTH*4U];
    uint32_t size = 8U;
    uint32_t curvelen = 0;

    /* check sizes */
    if ((!((size < pub->x[0U]) || (size < pub->y[0U]) || (size < sig->r[0U]) ||
           (size < sig->s[0U]) || (size < h[0U]))) &&
            (PKE_isBigIntZero(sig->r) != ASYM_CRYPT_RETURN_SUCCESS) &&
             (PKE_isBigIntZero(sig->s) != ASYM_CRYPT_RETURN_SUCCESS))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        /* PKE only supports Hash as a BigEndian input */
        Crypto_bigIntToUint32((uint32_t *)&h[0U], size, (uint32_t *)&bigEndianHash[0U]);
        
        /* PKE only supports Hash as a BigEndian input */
        Crypto_Uint32ToUint8((uint32_t *)&bigEndianHash[0U], size*4U, (uint8_t *)&littleEndianHash[0U]);

        /* Get SM2 curve */
        curve = cri_pke_get_curve(CRI_ECC_CURVE_SM2);
        /* Get curve length of SM2 */
        curvelen = cri_pke_get_curve_length(curve);

        /* Verify signature */
        pkeStatus = cri_pke_ecdsa_verify_hash(gPKE, curve, &pub->x[1U], &pub->y[1U], &littleEndianHash[0U], curvelen, &sig->r[1U], &sig->s[1U], &signatureRPrime);

        /* Revert the input back to original state */
        Crypto_Uint32ToBigInt((uint32_t *)&bigEndianHash, size, (uint32_t *)&h[0U]);

        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            status  = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_SM2DSAKeyGenPrivate(AsymCrypt_Handle handle,
                        uint32_t priv[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t size = 8U;
    cri_ecc_curve_t curve;

    /* Checking handle is opened or not */
    if(ASYM_CRYPT_NULL_HANDLE != handle)
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {      
        /* Get SM2 curve */
        curve = cri_pke_get_curve(CRI_ECC_CURVE_SM2);

        /* Call the ECDSA KeyGen function to generate private key */
        pkeStatus = cri_pke_ecc_private_keygen(gPKE, curve, &priv[1U]);

        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            status  = ASYM_CRYPT_RETURN_SUCCESS;
            priv[0U] = size;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

AsymCrypt_Return_t AsymCrypt_SM2DSAKeyGenPublic(AsymCrypt_Handle handle,
                        struct AsymCrypt_ECPoint *pub,
                        const uint32_t priv[ECDSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    int pkeStatus = PKE_FAULT_STATUS;
    uint32_t size = 8U;
    cri_ecc_curve_t curve;

    /* check sizes */
    if (!(size < priv[0U]))
    {
        /* Checking handle is opened or not */
        if(ASYM_CRYPT_NULL_HANDLE != handle)
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {      
        /* Get SM2 curve */
        curve = cri_pke_get_curve(CRI_ECC_CURVE_SM2);

        /* Call the ECDSA KeyGen function to generate private key */
        pkeStatus = cri_pke_ecdsa_keygen(gPKE, curve, &priv[1U], &pub->x[1U], &pub->y[1U]);

        if (pkeStatus == PKE_NO_ERROR_STATUS)
        {
            status  = ASYM_CRYPT_RETURN_SUCCESS;
            pub->x[0U] = size;
            pub->y[0U] = size;
        }
        else
        {
            status  = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

/* ========================================================================== */
/*                 Internal Function Definitions                              */
/* ========================================================================== */

static uint32_t PKE_countLeadingZeros(uint32_t x)
{
    uint32_t bit_count = 0, lz = 0;

    bit_count = sizeof(x)*8;

    /* Left shift until Most significant bit doesn become 1 */

    while ((x & (1 << (bit_count - 1))) == 0) {
        x <<= 1;
        lz++;
    }

    return (lz);
}

/**
 * \brief Return the size in bits of a bigint
 *
 * \param bn Input number
 *
 * \return Length in bits of the big number
 */
static uint32_t PKE_bigIntBitLen(const uint32_t bn[ECDSA_MAX_LENGTH])
{
    uint32_t i, status;

    for (i = bn[0U]; i > 0U; i--) {
        if (bn[i] != 0U) {
            break;
        }
    }

    if (i == 0U) {
        status = 0U;
    } else {
        status = (i * 32U) - PKE_countLeadingZeros((int32_t) bn[i]);
    }

    return (status);
}

/**
 * \brief Check if the bigint is zero
 *
 * \param bn Input number
 *
 * \return ASYM_CRYPT_RETURN_SUCCESS if the number if zero
 */
static AsymCrypt_Return_t PKE_isBigIntZero(const uint32_t bn[RSA_MAX_LENGTH])
{
    uint32_t i;
    AsymCrypt_Return_t ret = ASYM_CRYPT_RETURN_SUCCESS;

    for (i = 0U; i <= bn[0U]; i++) {
        if (bn[i] != 0U) {
            ret = ASYM_CRYPT_RETURN_FAILURE;
            break;
        }
    }
    return (ret);
}

static AsymCrypt_Return_t PKE_getPrimeCurveId (const struct AsymCrypt_ECPrimeCurveP *curveParams, uint32_t *pkeCurveType)
{
    AsymCrypt_Return_t retVal = ASYM_CRYPT_RETURN_SUCCESS;
    uint32_t i = 0U;

    if(ASYM_CRYPT_RETURN_SUCCESS == retVal)
    {
        /* Check if the curveId is supported */
        for (i = 0; i < numPrimeCurves; i++) {
            if (memcmp(&curveParams->prime[0U], primeCurves[i].prime, 4U*curveParams->prime[0U]) != 0)
            {
                retVal = ASYM_CRYPT_RETURN_FAILURE;
            }
            else
            {
                /* Check if the curveId is supported */
                if (memcmp(&curveParams->a[0U], primeCurves[i].A, 4U*curveParams->a[0U]) != 0)
                {
                    retVal = ASYM_CRYPT_RETURN_FAILURE;
                }

                if (memcmp(&curveParams->b[0U], primeCurves[i].B, 4U*curveParams->b[0U]) != 0)
                {
                    retVal = ASYM_CRYPT_RETURN_FAILURE;
                }

                if (memcmp(&curveParams->order[0U], primeCurves[i].order, 4U*curveParams->order[0U]) != 0)
                {
                    retVal = ASYM_CRYPT_RETURN_FAILURE;
                }
                else
                {
                    retVal = ASYM_CRYPT_RETURN_SUCCESS;
                    *pkeCurveType = primeCurves[i].curveType;
                    break;
                }
            }
        }
    }

    return retVal;
}
