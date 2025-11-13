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
 *  \file   pka_eddsa.c
 *
 *  \brief  This file contains the SW implementation of Edwards-curve Digital
 *          Signature Algorithm (EdDSA) using PKA driver and SHA driver.
 *
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <crypto/asym_crypt.h>
#include <string.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define SHA_512_LEN_BYTE        (64U)
#define ED_SIZE_32_BYTE         (32U)
#define ED_SIZE_64_BYTE         (64U)

/* ========================================================================== */
/*                         Internal Global variables                                  */
/* ========================================================================== */

/*Ed25519 Curve Parameter in Edward form ax^2+y^2 = 1+dx^2y^2*/
static const struct AsymCrypt_EdCurveParam gEd25519Param =
{
    { /*Prime */
        8UL,
        0xffffffedUL, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
        0xffffffffUL, 0xffffffffUL, 0xffffffffUL, 0x7fffffffUL,
    },
    { /*Order */
        8UL,
        0x5cf5d3edUL, 0x5812631aUL, 0xa2f79cd6UL, 0x14def9deUL,
        0x00000000UL, 0x00000000UL, 0x00000000UL, 0x10000000UL,
    },
    {  /*Cofactor h*/
        1UL,
        0x08UL,
    },
    {  /*a*/
        8UL,
        0xffffffecUL, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
        0xffffffffUL, 0xffffffffUL, 0xffffffffUL, 0x7fffffffUL,
    },
    {  /*d*/
        8UL,
        0x135978a3UL, 0x75eb4dcaUL, 0x4141d8abUL, 0x00700a4dUL,
        0x7779e898UL, 0x8cc74079UL, 0x2b6ffe73UL, 0x52036ceeUL,
    },
    {
        {   /*g.x*/
            8UL,
            0x8F25D51AUL, 0xC9562D60UL, 0x9525A7B2UL, 0x692CC760UL,
            0xFDD6DC5CUL, 0xC0A4E231UL, 0xCD6E53FEUL, 0x216936D3UL,
        },
        {   /*g.y*/
            8UL,
            0x66666658UL, 0x66666666UL, 0x66666666UL, 0x66666666UL,
            0x66666666UL, 0x66666666UL, 0x66666666UL, 0x66666666UL,
        },
    },
};

/*Curve 25519 Parameter in Montogomery form By^2=x^3+Ax^2+x*/
static const struct AsymCrypt_ECMontCurveP gMontCurve25519Param =
{
    { /*Prime */
        8UL,
        0xffffffedUL, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
        0xffffffffUL, 0xffffffffUL, 0xffffffffUL, 0x7fffffffUL,
    },
    { /*Order */
        8UL,
        0x5cf5d3edUL, 0x5812631aUL, 0xa2f79cd6UL, 0x14def9deUL,
        0x00000000UL, 0x00000000UL, 0x00000000UL, 0x10000000UL,
    },
    {  /*Cofactor h*/
        1UL,
        0x08UL,
    },
    {  /*A*/
        1UL,
        0x00076d06UL,
    },
    {  /*B*/
        1UL,
        0x01UL,
    },
    {
        {   /*g.x*/
            1UL,
            0x00000009UL,
        },
        {   /*g.y*/
            8UL,
            0x7eced3d9UL, 0x29e9c5a2UL, 0x6d7c61b2UL, 0x923d4d7eUL,
            0x7748d14cUL, 0xe01edd2cUL, 0xb8a086b4UL, 0x20ae19a1UL,
        },
    },
} ;

/* Constant 'alpha' for ed25519 curve, equal to sqrt(-486664) mod p, where p =2^255-19*/
static const uint32_t gAlpha[EC_PARAM_MAXLEN] =
{
    8UL,
    0x00ba81e7UL, 0x3391fb55UL, 0xb482e57dUL, 0x3a5e2c2eUL,
    0xfc03b081UL, 0x2d84f723UL, 0x9f5ff944UL, 0x70d9120bUL,
};

/* Exponent-1 used for ed25519 operation, equal (p+3)/8 mod p, where p =2^255-19 */
static const uint32_t gExp1[EC_PARAM_MAXLEN] =
{
    8UL,
    0xfffffffeUL, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
    0xffffffffUL, 0xffffffffUL, 0xffffffffUL, 0x0fffffffUL,

};

/* Exponent-2 used for ed25519 operation, equal (p-1)/4 mod p, where p =2^255-19 */
static const uint32_t gExp2[EC_PARAM_MAXLEN] =
{
    8UL,
    0xfffffffbUL, 0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
    0xffffffffUL, 0xffffffffUL, 0xffffffffUL, 0x1fffffffUL,

};

/* =========================================================================================================== */
/*                                     Internal Function Declarations                                          */
/* =========================================================================================================== */

static AsymCrypt_Return_t AsymCrypt_Ed25519_edScalarMultiply(AsymCrypt_Handle handle,
                        const uint32_t k[EC_PARAM_MAXLEN],
                        const struct AsymCrypt_EddsaPoint *P,
                        struct AsymCrypt_EddsaPoint *Result);
static AsymCrypt_Return_t AsymCrypt_Ed25519_edPointAdd(AsymCrypt_Handle handle,
                        const struct AsymCrypt_EddsaPoint *P1,
                        const struct AsymCrypt_EddsaPoint *P2,
                        struct AsymCrypt_EddsaPoint *Result);
static AsymCrypt_Return_t AsymCrypt_Ed25519_convMontToEd(AsymCrypt_Handle handle,
                        const uint32_t u[EC_PARAM_MAXLEN],
                        const uint32_t v[EC_PARAM_MAXLEN],
                        uint32_t x[EC_PARAM_MAXLEN],
                        uint32_t y[EC_PARAM_MAXLEN]);
static AsymCrypt_Return_t AsymCrypt_Ed25519_convEdToMont(AsymCrypt_Handle handle,
                        const uint32_t x[EC_PARAM_MAXLEN],
                        const uint32_t y[EC_PARAM_MAXLEN],
                        uint32_t u[EC_PARAM_MAXLEN],
                        uint32_t v[EC_PARAM_MAXLEN]);
static AsymCrypt_Return_t AsymCrypt_Eddsa_concatArray(uint8_t *ptrArr1, uint32_t len1,
                                            uint8_t *ptrArr2, uint32_t len2,
                                            uint8_t *ptrArr3, uint32_t *len3);
static void AsymCrypt_Eddsa_bigIntToOctet(const uint32_t *source, uint32_t *destLenByte, uint8_t *dest);
static void AsymCrypt_Eddsa_octetToBigInt(const uint8_t *source, uint32_t sourceSizeByte, uint32_t *dest);
static void AsymCrypt_Eddsa_octetLEtoBE(const uint8_t *source, uint32_t sizeInByte, uint8_t *dest);
static void AsymCrypt_Ed25519_octetBEtoLE(const uint8_t *source, uint32_t sizeInByte, uint8_t *dest);
static void AsymCrypt_Ed25519_octetLEtoBE(const uint8_t *source, uint32_t sizeInByte, uint8_t *dest);
static void AsymCrypt_Ed25519_encodeEdPoint(const struct AsymCrypt_EddsaPoint *P, uint8_t *R, uint32_t *sizeInByte);
static AsymCrypt_Return_t  AsymCrypt_Ed25519_decodeEdPoint(AsymCrypt_Handle handle,
                                               const uint8_t *encodedPoint,
                                               uint32_t sizeInByte,
                                               struct AsymCrypt_EddsaPoint *P);

extern AsymCrypt_Return_t PKA_ECMontMultiply(AsymCrypt_Handle handle,
                                        const struct AsymCrypt_ECMontCurveP *cp,
                                        const struct AsymCrypt_ECPoint *P,
                                        const uint32_t k[EC_PARAM_MAXLEN],
                                        const Bool ySkip,
                                        struct AsymCrypt_ECPoint *Result);

extern AsymCrypt_Return_t PKA_ModPExp(AsymCrypt_Handle handle,
                                        const uint32_t base[EC_PARAM_MAXLEN],
                                        const uint32_t exp[EC_PARAM_MAXLEN],
                                        const uint32_t mod[EC_PARAM_MAXLEN],
                                        uint32_t Result[EC_PARAM_MAXLEN]);

extern AsymCrypt_Return_t PKA_ModPMul(AsymCrypt_Handle handle,
                                        const uint32_t A[EC_PARAM_MAXLEN],
                                        const uint32_t B[EC_PARAM_MAXLEN],
                                        const uint32_t P[EC_PARAM_MAXLEN],
                                        uint32_t Result[EC_PARAM_MAXLEN]);

extern AsymCrypt_Return_t PKA_ModPAdd(AsymCrypt_Handle handle,
                                        const uint32_t A[EC_PARAM_MAXLEN],
                                        const uint32_t B[EC_PARAM_MAXLEN],
                                        const uint32_t P[EC_PARAM_MAXLEN],
                                        uint32_t Result[EC_PARAM_MAXLEN]);

extern AsymCrypt_Return_t PKA_ModPSub(AsymCrypt_Handle handle,
                                        const uint32_t A[EC_PARAM_MAXLEN],
                                        const uint32_t B[EC_PARAM_MAXLEN],
                                        const uint32_t P[EC_PARAM_MAXLEN],
                                        uint32_t Result[EC_PARAM_MAXLEN]);

extern AsymCrypt_Return_t PKA_ModPInv(AsymCrypt_Handle handle,
                                        const uint32_t A[EC_PARAM_MAXLEN],
                                        const uint32_t P[EC_PARAM_MAXLEN],
                                        uint32_t Result[EC_PARAM_MAXLEN]);

/* =========================================================================================================== */
/*                                    Function Definitions                                                     */
/* =========================================================================================================== */

AsymCrypt_Return_t AsymCrypt_EddsaSign(AsymCrypt_Handle handle,
                            AsymCrypt_ExecuteShaCallback shaCbFxn,
                            const struct AsymCrypt_EddsaKey *key,
                            const uint8_t *ptrData,
                            const uint32_t dataSizeByte,
                            struct AsymCrypt_EddsaSig *sig,
                            AsymCrypt_EdCurveType_t input_curve)
{
    PKA_Config      *config;
    PKA_Attrs       *attrs;
    AsymCrypt_Return_t    status = ASYM_CRYPT_RETURN_FAILURE;
    uint8_t *ptrdataInput = NULL;
    uint8_t               hash[EDDSA_ED448_HASH_LEN];
    uint8_t               k0[EDDSA_ED448_HASH_LEN/2], k1[EDDSA_ED448_HASH_LEN/2];
    uint32_t              r[EDDSA_ED448_HASH_LEN];
    uint8_t               sigR[EDDSA_ED448_HASH_LEN], sigS[EDDSA_ED448_HASH_LEN];
    uint32_t              sigRsize, sigSsize;
    uint32_t              S[EC_PARAM_MAXLEN];
    uint8_t               r_temp[EC_PARAM_MAXLEN];
    uint32_t              k0_temp[EC_PARAM_MAXLEN*2], hash_temp[EC_PARAM_MAXLEN*2];
    uint32_t              S_temp[EC_PARAM_MAXLEN*2];
    uint8_t               tempBuf[EDDSA_ED448_HASH_LEN*2U];
    struct AsymCrypt_EddsaPoint R;

    uint32_t hash_len = 0;
    uint32_t key_len = 0;

    config  = (PKA_Config *)handle;
    attrs   = config->attrs;

    /* Checking handle and shaHandle is opened or not */
    if ((!attrs->isOpen) || (handle == NULL) || (shaCbFxn == NULL)|| (ptrData == NULL)|| (key == NULL) || (sig == NULL)) {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        hash_len = EDDSA_ED25519_HASH_LEN;
        key_len = EDDSA_ED25519_KEY_LEN;
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        hash_len = EDDSA_ED448_HASH_LEN;
        key_len = EDDSA_ED448_KEY_LEN;
    } else {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    if (status  == ASYM_CRYPT_RETURN_SUCCESS) {
        /* Backup User Data before using the space */
        ptrdataInput = (uint8_t*)(ptrData - hash_len);
        memcpy(tempBuf, ptrdataInput, hash_len);

        /* Get private key hash (SHA/SHAKE digest) */
        status = shaCbFxn((uint8_t*)&(key->privKey), key_len, hash);

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Copy 1st part of privKey hash */
            memcpy(k0, hash, key_len);

            /* Copy 2nd part of privKey hash */
            memcpy(k1, (hash + key_len), key_len);

            /* Copy 1st part of privKey hash, and clamp it */
            if (input_curve == ASYM_CRYPT_CURVE_TYPE_EDDSA_25519) {
                k0[0] &= 0xF8;
                k0[31] &= 0x7F;
                k0[31] |= 0x40;
            } else {
                k0[0] &= 0xFC;
                k0[31] &= 0x00;
                for (uint8_t i = 1; i <= 31 ; i++) {
                    k0[i] |= 0x80;
                }
            }

            /* Copy k1 || M into data input */
            ptrdataInput += key_len;
            memcpy(ptrdataInput, k1, key_len);

            /* Store the nonce i.e. r = Hash(k1||M) */
            status = shaCbFxn(ptrdataInput, (dataSizeByte + key_len), r_temp);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Conversion to BigInt as PKA only operates on BigInt */
            AsymCrypt_Eddsa_octetToBigInt(r_temp, hash_len, r);

            /* Get R value i.e. R = rxG */
            status = AsymCrypt_Ed25519_edScalarMultiply(handle, r, &gEd25519Param.g, &R);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Encode R */
            AsymCrypt_Ed25519_encodeEdPoint(&R, sigR, &sigRsize);

            /* Change k0 into BigInt */
            AsymCrypt_Eddsa_octetToBigInt(k0, key_len, k0_temp);

            /* Backup User Data before using the space */
            ptrdataInput = (uint8_t *)(ptrData - hash_len);

            /* Get Hash(R||A||M)k0 */
            /* Signature R to be appended in memory */
            memcpy(ptrdataInput, sigR, key_len);
            /* Public Key to be appended in the memory */
            memcpy(&ptrdataInput[key_len], key->pubKey, key_len);
            status = shaCbFxn((uint8_t*)ptrdataInput, (dataSizeByte + (2U*key_len)), hash);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Get s = r + H.k0 */
            AsymCrypt_Eddsa_octetToBigInt(hash, hash_len, hash_temp);
            status = PKA_ModPMul(handle, hash_temp, k0_temp, gEd25519Param.order, S_temp);
            if (status == ASYM_CRYPT_RETURN_SUCCESS) {
                status = PKA_ModPAdd(handle, r, S_temp, gEd25519Param.order, S);
                if (status == ASYM_CRYPT_RETURN_SUCCESS) {
                    /* Encode S */
                    AsymCrypt_Eddsa_bigIntToOctet(S, &sigSsize, sigS);

                    /* Copy signature: R and S value to sig*/
                    memcpy(sig->R, sigR, sigRsize);
                    memcpy(sig->s, sigS, sigSsize);
                }
            }
        }

        /* Copy back data from tempBuf*/
        ptrdataInput =  (uint8_t*)(ptrData - hash_len);
        memcpy(ptrdataInput, tempBuf, hash_len);
    }

    return status;
}

AsymCrypt_Return_t AsymCrypt_EddsaVerify(AsymCrypt_Handle handle,
                            AsymCrypt_ExecuteShaCallback shaCbFxn,
                            const uint8_t pubKey[EDDSA_MAX_KEY_LEN],
                            const uint8_t *ptrData,
                            const uint32_t dataSizeByte,
                            const struct AsymCrypt_EddsaSig *sig,
                            AsymCrypt_EdCurveType_t input_curve)
{
    AsymCrypt_Return_t     status = ASYM_CRYPT_RETURN_FAILURE;
    PKA_Config       *config;
    PKA_Attrs        *attrs;
    uint32_t         S_bigint[EC_PARAM_MAXLEN*2];
    uint32_t         u[EC_PARAM_MAXLEN*2];
    uint8_t          hash[EDDSA_ED448_HASH_LEN];
    uint8_t          tempBuff[256];
    uint8_t          *ptrdataInput = NULL;
    struct AsymCrypt_EddsaPoint pubKeyPoint, R, V1, V2, tempEdpoint;
    uint32_t hash_len = 0;
    uint32_t key_len = 0;

    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    /* Checking handle and shaHandle is opened or not */
    if ((!attrs->isOpen) || (handle == NULL) || (shaCbFxn == NULL)|| (ptrData == NULL) || (sig == NULL)) {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        hash_len = EDDSA_ED25519_HASH_LEN;
        key_len = EDDSA_ED25519_KEY_LEN;
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        hash_len = EDDSA_ED448_HASH_LEN;
        key_len = EDDSA_ED448_KEY_LEN;
    } else {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    if (status  == ASYM_CRYPT_RETURN_SUCCESS) {
        /* Decode Public Key */
        status = AsymCrypt_Ed25519_decodeEdPoint(handle, pubKey, key_len, &pubKeyPoint);

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            status = AsymCrypt_Ed25519_decodeEdPoint(handle, sig->R, key_len, &R);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            AsymCrypt_Eddsa_octetToBigInt(sig->s, key_len, S_bigint);

            status = AsymCrypt_Ed25519_edScalarMultiply(handle, S_bigint, &gEd25519Param.g, &V1);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Copy 64 bytes before message(ptrData) to tempBuf */
            ptrdataInput =  (uint8_t*)(ptrData - hash_len);
            memcpy(tempBuff, ptrdataInput, hash_len);

            /* Get SHA-512 Hash(sig.R||pubKey||msg) */
            memcpy(ptrdataInput, sig->R, key_len);
            memcpy(&ptrdataInput[key_len], pubKey, key_len);

            status = shaCbFxn((uint8_t*)ptrdataInput, dataSizeByte + (2U*key_len), hash);
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            AsymCrypt_Eddsa_octetToBigInt(hash, hash_len, u);

            status = AsymCrypt_Ed25519_edScalarMultiply(handle, u, &pubKeyPoint, &tempEdpoint);

            if (status == ASYM_CRYPT_RETURN_SUCCESS) {
                status = AsymCrypt_Ed25519_edPointAdd(handle, &R, &tempEdpoint, &V2);
            }
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            /* Get verification result */
            if (memcmp(V1.x, V2.x, V1.x[0]) == 0) {
                if (memcmp(V1.y, V2.y, V1.y[0]) == 0) {
                /* PKE Ed25519 Verification Signature matches*/
                status = ASYM_CRYPT_RETURN_SUCCESS;
                } else {
                    /* PKE Ed25519 Verification Signature does not matches*/
                    status = ASYM_CRYPT_RETURN_FAILURE;
                }
            } else {
                /* PKE Ed25519 Verification Signature does not matches*/
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
        }

        /* Restore back hash_len Bytes from tempBuff */
        memcpy(ptrdataInput, tempBuff, hash_len);
    }

    return status;
}

AsymCrypt_Return_t AsymCrypt_EddsaGetPubKey(AsymCrypt_Handle handle,
                                    AsymCrypt_ExecuteShaCallback shaCbFxn,
                                    uint8_t privKey[EDDSA_MAX_KEY_LEN],
                                    uint8_t pubKey[EDDSA_MAX_KEY_LEN],
                                    AsymCrypt_EdCurveType_t input_curve)
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    PKA_Config   *config;
    PKA_Attrs    *attrs;
    uint8_t      privKeyHash[64];
    uint32_t     s[EC_PARAM_MAXLEN];
    struct AsymCrypt_EddsaPoint Q;
    uint32_t     pubKeySizeByte;
    uint32_t     key_len = 0U;

    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    /* Checking handle and shaHandle is opened or not */
    if (!(attrs->isOpen) || (NULL == handle) || (shaCbFxn == NULL) || (privKey == NULL) || (pubKey == NULL)) {
        status = ASYM_CRYPT_RETURN_FAILURE;
    } else {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if (ASYM_CRYPT_CURVE_TYPE_EDDSA_25519 == input_curve) {
        key_len = EDDSA_ED25519_KEY_LEN;
    } else if (ASYM_CRYPT_CURVE_TYPE_EDDSA_448 == input_curve) {
        key_len = EDDSA_ED448_KEY_LEN;
    } else {
        status  = ASYM_CRYPT_RETURN_FAILURE;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS) {
        /* Get privKey hash*/
        status = shaCbFxn((uint8_t*)privKey, key_len, privKeyHash);

        /*Only first half of privatekey hash is used, clamp the fist half and clear the second half*/
        if (input_curve == ASYM_CRYPT_CURVE_TYPE_EDDSA_25519) {
            privKeyHash[0] &= 0xF8;
            privKeyHash[31] &= 0x7F;
            privKeyHash[31] |= 0x40;
        } else {
            privKeyHash[0] &= 0xFC;
            privKeyHash[31] &= 0x00;
            for (uint8_t i = 1; i <= 31 ; i++) {
                privKeyHash[i] |= 0x80;
            }
        }

        memset((uint8_t*)&privKeyHash[key_len], 0U, key_len);

        /*Recover public key point and encode it*/
        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            AsymCrypt_Eddsa_octetToBigInt(privKeyHash, key_len, s);

            status = AsymCrypt_Ed25519_edScalarMultiply(handle, s, &gEd25519Param.g, &Q);

            if (status == ASYM_CRYPT_RETURN_SUCCESS) {
                AsymCrypt_Ed25519_encodeEdPoint(&Q, pubKey, &pubKeySizeByte);
            }
        }
    }

    return status;
}

/* =========================================================================================================== */
/*                                     Internal Function Definitions                                        */
/* =========================================================================================================== */

/* Function to perform scalar multiplication on Edward Curve: Result = k*P */
static AsymCrypt_Return_t AsymCrypt_Ed25519_edScalarMultiply(AsymCrypt_Handle handle,
                        const uint32_t k[EC_PARAM_MAXLEN],
                        const struct AsymCrypt_EddsaPoint *P,
                        struct AsymCrypt_EddsaPoint *Result)
{
    AsymCrypt_Return_t       status = ASYM_CRYPT_RETURN_FAILURE;
    struct AsymCrypt_ECPoint ecPointP, tempResult;

    /*Convert Ed25519 point (x,y) to Montogomery space (u,v)*/
    status = AsymCrypt_Ed25519_convEdToMont(handle, P->x, P->y, ecPointP.x, ecPointP.y);

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ECMontMultiply(handle, &gMontCurve25519Param, &ecPointP, k,
                                    FALSE, &tempResult);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = AsymCrypt_Ed25519_convMontToEd(handle, tempResult.x, tempResult.y,
                                          Result->x, Result->y);
    }

    return status;
}

/* Function to perform point addition for ED25519 curve in Edward form (ax^2+y^2 = 1+dx^2y^2)
*  Result(x,y) = P1(x1,y1)+P2(x2,y2)=((x1y2+y1x2)/(1+dx1x2y1y2),(y1y2-ax1x2)/(1-dx1x2y1y2))
*/
static AsymCrypt_Return_t AsymCrypt_Ed25519_edPointAdd(AsymCrypt_Handle handle,
                        const struct AsymCrypt_EddsaPoint *P1,
                        const struct AsymCrypt_EddsaPoint *P2,
                        struct AsymCrypt_EddsaPoint *Result)
{
    AsymCrypt_Return_t   status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t       temp1[EC_PARAM_MAXLEN], temp2[EC_PARAM_MAXLEN];
    uint32_t       temp3[EC_PARAM_MAXLEN], temp4[EC_PARAM_MAXLEN];
    uint32_t       temp5[EC_PARAM_MAXLEN];
    uint32_t       X3[EC_PARAM_MAXLEN], Y3[EC_PARAM_MAXLEN];
    uint32_t       i = 0;
    const uint32_t bn_1[] = {1U, 1U};

    /*Calculate X3*/
    status = PKA_ModPMul(handle, P1->x, P2->y, gEd25519Param.prime, temp1);

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, P2->x, P1->y, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp1, temp2, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, gEd25519Param.d, temp3, gEd25519Param.prime, temp4);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPAdd(handle, bn_1, temp4, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, temp3, gEd25519Param.prime, temp5);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPAdd(handle, temp1, temp2, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp3, temp5, gEd25519Param.prime, X3);
    }

    /*Calculate Y3*/
    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, P1->x, P2->x, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, gEd25519Param.a, temp3, gEd25519Param.prime, temp1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, P1->y, P2->y, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPSub(handle, bn_1, temp4, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, temp3, gEd25519Param.prime, temp5);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPSub(handle, temp2, temp1, gEd25519Param.prime, temp3);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp3, temp5, gEd25519Param.prime, Y3);
    }

    /* Copy X3, Y3 to Result*/
    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        for (i= 0; i <= X3[0]; i++) {
            Result->x[i] = X3[i];
        }

        for (i= 0; i <= Y3[0]; i++) {
            Result->y[i] = Y3[i];
        }
    }

    return status;
}

/* Function to convert Montogomery Curve point (u,v) to Edward Curve form (x,y)*/
static AsymCrypt_Return_t AsymCrypt_Ed25519_convMontToEd(AsymCrypt_Handle handle,
                        const uint32_t u[EC_PARAM_MAXLEN],
                        const uint32_t v[EC_PARAM_MAXLEN],
                        uint32_t x[EC_PARAM_MAXLEN],
                        uint32_t y[EC_PARAM_MAXLEN])
{
    AsymCrypt_Return_t   status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t       temp1[EC_PARAM_MAXLEN], temp2[EC_PARAM_MAXLEN];
    const uint32_t bn_1[] = {1U, 1U};

    /* x = gAlpha*u/v mod p, here gAlpha is constant equal to sqrt(-486664) mod p, where p =2^255-19 */
    status = PKA_ModPMul(handle, gAlpha, u, gEd25519Param.prime, temp1);

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, v, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp1, temp2, gEd25519Param.prime, x);
    }

    /* y = (u-1)/(u+1) mod p, where p =2^255-19 */
    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPAdd(handle, u, bn_1, gEd25519Param.prime, temp1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, temp1, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPSub(handle, u, bn_1, gEd25519Param.prime, temp1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp1, temp2, gEd25519Param.prime, y);
    }

    return status;
}

/* Function to convert Edward Curve point (x,y) to Montogomery Curve form (u,v)*/
static AsymCrypt_Return_t AsymCrypt_Ed25519_convEdToMont(AsymCrypt_Handle handle,
                        const uint32_t x[EC_PARAM_MAXLEN],
                        const uint32_t y[EC_PARAM_MAXLEN],
                        uint32_t u[EC_PARAM_MAXLEN],
                        uint32_t v[EC_PARAM_MAXLEN])
{
    AsymCrypt_Return_t   status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t       temp1[EC_PARAM_MAXLEN], temp2[EC_PARAM_MAXLEN];
    const uint32_t bn_1[] = {1U, 1U};

    /* u = (1+y)/(1-y) */
    status = PKA_ModPSub(handle, bn_1, y, gEd25519Param.prime, temp1);

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, temp1, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPAdd(handle, bn_1, y, gEd25519Param.prime, temp1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp1, temp2, gEd25519Param.prime, u);
    }

    /* v =(gAlpha*u/x) */
    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, gAlpha, u, gEd25519Param.prime, temp1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, x, gEd25519Param.prime, temp2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, temp1, temp2, gEd25519Param.prime, v);
    }

    return status;
}

/* Function to concatanate two arrays of given lengths*/
static AsymCrypt_Return_t AsymCrypt_Eddsa_concatArray(uint8_t *ptrArr1, uint32_t len1,
                                            uint8_t *ptrArr2, uint32_t len2,
                                            uint8_t *ptrArr3, uint32_t *len3)
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t i = 0;

    *len3 = len1+len2;

    for (i = 0; i < (*len3); i++) {
        if (i < len1) {
            ptrArr3[i] = ptrArr1[i];
        } else {
            ptrArr3[i] = ptrArr2[i];
        }
    }

    if (*len3 != 0) {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    return status;
}

/* Function to convert bigInt value to octet string (uint8 array)*/
static void AsymCrypt_Eddsa_bigIntToOctet(const uint32_t *source, uint32_t *destLenByte, uint8_t *dest)
{
    uint32_t sourceLenWord = 0;
    uint32_t srcCtr = 0, destCtr = 0;

    sourceLenWord = source[0];
    *destLenByte = sourceLenWord*4;

    destCtr = 0;

    for (srcCtr = 1; srcCtr <= sourceLenWord; srcCtr++) {
        dest[destCtr]  =  source[srcCtr] & 0xFFU;
        dest[destCtr+1]= (source[srcCtr] >> 8)  & 0xFFU;
        dest[destCtr+2]= (source[srcCtr] >> 16) & 0xFFU;
        dest[destCtr+3]= (source[srcCtr] >> 24) & 0xFFU;
        destCtr+=4;
    }
}

/* Function to convert octet string (uint8 array) to bigInt value*/
static void AsymCrypt_Eddsa_octetToBigInt(const uint8_t *source, uint32_t sourceSizeByte, uint32_t *dest)
{
    uint32_t destLenWord = 0;
    uint32_t srcCtr = 0, destCtr = 0;
    uint8_t  remBytes = 0;

    destLenWord = (sourceSizeByte + 3)/4;
    remBytes = (sourceSizeByte % 4);

    dest[0] = destLenWord;
    srcCtr = 0;

    for (destCtr = 1; destCtr <= destLenWord; destCtr++) {
        dest[destCtr] = (((source[srcCtr+3]<<24) & 0xFF000000UL) |
                         ((source[srcCtr+2]<<16) & 0x00FF0000UL) |
                         ((source[srcCtr+1]<<8)  & 0x0000FF00UL) |
                         ((source[srcCtr])       & 0x000000FFUL)) ;
        srcCtr+=4;
    }

    switch (remBytes) {
        case 0:
        {
            break;
        }
        case 1:
        {
            dest[destLenWord] &= 0x00FFFFFFUL;
            break;
        }
        case 2:
        {
            dest[destLenWord] &= 0x0000FFFFUL;
            break;
        }
        case 3:
        {
            dest[destLenWord] &= 0x000000FFUL;
            break;
        }
        default:
        {
            break;
        }
    }
}

/* Function to convert uint8 array from little endian to big endian */
static void AsymCrypt_Eddsa_octetLEtoBE(const uint8_t *source, uint32_t sizeInByte, uint8_t *dest)
{
    uint32_t i = 0, j = 0;
    j = sizeInByte-1;
    for (i = 0; i< sizeInByte; i++) {
        dest[i] = source[j];
        j--;
    }
}

/* Function to convert uint8 array from big endian to little endian */
static void AsymCrypt_Eddsa_octetBEtoLE(const uint8_t *source, uint32_t sizeInByte, uint8_t *dest)
{
    uint32_t i = 0, j = 0;
    j = sizeInByte-1;
    for (i = 0; i< sizeInByte; i++) {
        dest[i] = source[j];
        j--;
    }
}

/*Funtion to encode the Ed-Point fro P(x,y) form to encoded hex string (uint8 array)
* Encoding scheme is based on section 7.2 of NIST FIPS 186-5 specification.
* Note: bigInt form of a number is its representation as 32-bit (word) little endian array (LSW first),
*       with array[0] = size of array in words, array[1] = LSW, array[wordSize] = MSW
*/
static void AsymCrypt_Ed25519_encodeEdPoint(const struct AsymCrypt_EddsaPoint *P, uint8_t *encodedPoint, uint32_t *sizeInByte)
{
    uint8_t Y[ED_SIZE_32_BYTE];
    uint8_t xLsBit = 0, yMsByte = 0;
    uint32_t i = 0;

    xLsBit = P->x[1] & 0x1U;

    AsymCrypt_Eddsa_bigIntToOctet(P->y, sizeInByte, Y);
    if (*sizeInByte < 32U) {
        for (i = 32U-(*sizeInByte); i< 32U; i++) {
            Y[i] = 0;
        }
        *sizeInByte = ED_SIZE_32_BYTE;
    }

    yMsByte = Y[*sizeInByte -1];
    Y[*sizeInByte -1] = ((0x7FU & yMsByte) | (xLsBit << 7));
    memcpy(encodedPoint, Y, *sizeInByte);
}

/*Function to decode the encoded Ed-Point into P(x,y) form
* Decoding scheme is based on section 7.3 of NIST FIPS 186-5 specification.
* Note: bigInt form of a number is its representation as 32-bit (word) little endian array (LSW first),
*       with array[0] = size of array in words, array[1] = LSW, array[wordSize] = MSW
*/
static AsymCrypt_Return_t  AsymCrypt_Ed25519_decodeEdPoint(AsymCrypt_Handle handle,
                                               const uint8_t *encodedPoint,
                                               uint32_t sizeInByte,
                                               struct AsymCrypt_EddsaPoint *P)
{
    AsymCrypt_Return_t   status = ASYM_CRYPT_RETURN_FAILURE;
    uint8_t        temp1[ED_SIZE_32_BYTE];
    uint32_t       U[EC_PARAM_MAXLEN], V[EC_PARAM_MAXLEN];
    uint32_t       Y2[EC_PARAM_MAXLEN], X2[EC_PARAM_MAXLEN];
    uint32_t       W[EC_PARAM_MAXLEN], W2[EC_PARAM_MAXLEN];
    uint32_t       temp_bn1[EC_PARAM_MAXLEN], temp_bn2[EC_PARAM_MAXLEN];
    uint32_t       x0 = 0, i = 0;
    uint32_t       X_temp[EC_PARAM_MAXLEN],I[EC_PARAM_MAXLEN];
    const uint32_t bn_1[] = {1,1},bn_2[]={1,2};

    memcpy(temp1, encodedPoint, sizeInByte);
    x0 =  (temp1[sizeInByte-1] & 0x80U)>>7U;
    temp1[sizeInByte-1] &= 0x7FU;

    AsymCrypt_Eddsa_octetToBigInt(temp1, sizeInByte, P->y);

    status = PKA_ModPMul(handle, P->y, P->y, gEd25519Param.prime, Y2);

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPSub(handle, Y2, bn_1, gEd25519Param.prime, U);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, gEd25519Param.d, Y2, gEd25519Param.prime, temp_bn1);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPAdd(handle, temp_bn1, bn_1, gEd25519Param.prime, V);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPInv(handle, V, gEd25519Param.prime, temp_bn2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, U, temp_bn2, gEd25519Param.prime, X2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPExp(handle, X2, gExp1, gEd25519Param.prime, W);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPMul(handle, W, W, gEd25519Param.prime, W2);
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModPSub(handle, W2, X2, gEd25519Param.prime, temp_bn1);
    }

    if (temp_bn1[0] == 1 && temp_bn1[1] == 0) {
        X_temp[0] = W[0];
        for (i = 1; i <= W[0]; i++) {
            X_temp[i] = W[i];
        }
    } else {
        status = PKA_ModPExp(handle, bn_2, gExp2, gEd25519Param.prime, I);
        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            status = PKA_ModPMul(handle, W, I, gEd25519Param.prime, X_temp);
        }
    }

    if ((X_temp[1] % 2) == x0) {
        P->x[0] = X_temp[0];
        for (i = 1; i<= X_temp[0]; i++) {
            P->x[i] = X_temp[i];
        }
    } else {
        status = PKA_ModPSub(handle, gEd25519Param.prime, X_temp, gEd25519Param.prime, P->x);
    }

    return status;
}
