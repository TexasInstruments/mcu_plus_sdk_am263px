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
 *  \file   pka.c
 *
 *  \brief  This file contains the implementation of PKA ( Ultra lite Security Accelerator)(Public Key Accelerator) driver
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stddef.h>
#include <kernel/dpl/ClockP.h>
#include <security_common/drivers/crypto/asym_crypt.h>
#include <drivers/hw_include/cslr.h>
#include <drivers/hw_include/cslr_soc.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define PKA_CLK_CTRL_REG_CLK_ON_SHIFT              (0)
#define PKA_CLK_CTRL_SEQ_CLK_ON_SHIFT              (1)
#define PKA_CLK_CTRL_PKCP_CLK_ON_SHIFT             (2)
#define PKA_CLK_CTRL_LNME_CLK_ON_SHIFT             (3)
#define PKA_CLK_CTRL_LNME_REG_CLK_ON_SHIFT         (4)
#define PKA_CLK_CTRL_GF2M_CLK_ON_SHIFT             (5)
#define PKA_CLK_CTRL_DATA_RAM_CLK_ON_SHIFT         (6)

#define PKA_CLK_CTRL_REG_CLK_EN_SHIFT              (16)
#define PKA_CLK_CTRL_SEQ_CLK_EN_SHIFT              (17)
#define PKA_CLK_CTRL_PKCP_CLK_EN_SHIFT             (18)
#define PKA_CLK_CTRL_LNME_CLK_EN_SHIFT             (19)
#define PKA_CLK_CTRL_LNME_REG_CLK_EN_SHIFT         (20)
#define PKA_CLK_CTRL_GF2M_CLK_EN_SHIFT             (21)
#define PKA_CLK_CTRL_DATA_RAM_CLK_EN_SHIFT         (22)

#define PKA_SEQ_CTRL_RESET_SHIFT                   (31)
#define PKA_SEQ_CTRL_RESULT_SHIFT                  (8)
#define PKA_SW_REV_FIRMWARE_VERSION_SHIFT          (16)
#define PKA_SEQ_CTRL_RESULT_MASK                   (0xffUL << 8)
#define PKA_SW_REV_FIRMWARE_VERSION_MASK           (0xfffUL << 16)

#define PKA_CLK_CTRL_REG_CLK_OFF_SHIFT             (8)
#define PKA_CLK_CTRL_SEQ_CLK_OFF_SHIFT             (9)
#define PKA_CLK_CTRL_PKCP_CLK_OFF_SHIFT            (10)
#define PKA_CLK_CTRL_LNME_CLK_OFF_SHIFT            (11)
#define PKA_CLK_CTRL_LNME_REG_CLK_OFF_SHIFT        (12)
#define PKA_CLK_CTRL_GF2M_CLK_OFF_SHIFT            (13)
#define PKA_CLK_CTRL_DATA_RAM_CLK_OFF_SHIFT        (14)

#define PKA_FUNCTION_COMPARE_SHIFT                 (10)
#define PKA_FUNCTION_RUN_SHIFT                     (15)
#define PKA_FUNCTION_RUN_MASK                      (1UL << 15)
#define PKA_FUNCTION_CMD_HI_SHIFT                  (16)
#define PKA_FUNCTION_CMD_LO_SHIFT                  (12)
#define PKA_FUNCTION_RUN_SHIFT                     (15)

#define PKA_FUNCTION_ADD_MASK                      (1UL<<4)
#define PKA_FUNCTION_SUBTRACT_MASK                 (1UL<<5)
#define PKA_FUNCTION_MODULO_MASK                   (1UL<<9)
#define PKA_FUNCTION_COPY_MASK                     (1UL<<11)
#define PKA_FUNCTION_DIVIDE_MASK                   (1UL<<8)
#define PKA_FUNCTION_MULTIPLY_MASK                 (1UL<<0)

#define PKA_COMPARE_A_LT_B_MASK                    (1UL << 1)
#define PKA_COMPARE_A_GT_B_MASK                    (1UL << 2)
#define PKA_COMPARE_A_EQ_B_MASK                    (1UL << 0)

/** PKA RAM size (4kbytes) in words */
#define PKA_RAM_SIZE_WORDS                         (1024U)

#define PKA_SEQ_CTRL_DONE_MASK                     (1UL << 8)
#define PKA_SEQ_CTRL_DONE_SHIFT                    (8)

/** Result code when a PKA firmware command was successfUL */
#define PKA_COMMAND_RESULT_SUCCESS                 (0x1U)

/*
 * Timeout values in microsecs
 */
/**
 * Timeout for register updates to take effect - 10us
 */
#define PKA_REG_TIMEOUT                            (10U)

/**
 * Timeout for compare of 2 bignums - 100us
 */
#define PKA_COMPARE_TIMEOUT                        (100U)

/**
 * Timeout for modexp CRT operation - 50ms
 */
#define PKA_MODEXP_CRT_TIMEOUT                     (50000U)

/**
 * Timeout for modexp operation - 10ms
 */
#define PKA_MODEXP_TIMEOUT                         (10000U)

/**
 * Timeout for ECDSA verify operation - 10ms
 */
#define ASYM_CRYPT_ECDSA_VERIFY_TIMEOUT                   (10000U)

/**
 * Timeout for ECDSA sign operation - 10ms
 */
#define ASYM_CRYPT_ECDSA_SIGN_TIMEOUT                     (10000U)

/** Command to PKA firmware - MODEXP_CRT */
#define PKA_MODEXP_CRT_CMD   (((uint32_t) 0x0U) << PKA_FUNCTION_CMD_HI_SHIFT) | \
    (((uint32_t) 0x1U) << PKA_FUNCTION_CMD_LO_SHIFT)

/** Command to PKA firmware - MODEXP */
#define PKA_MODEXP_CMD   (((uint32_t) 0x0U) << PKA_FUNCTION_CMD_HI_SHIFT) |    \
    (((uint32_t) 0x6U) << PKA_FUNCTION_CMD_LO_SHIFT)

/** Command to PKA firmware - ECDSA SIGN */
#define ASYM_CRYPT_ECDSA_SIGN_CMD   (((uint32_t) 0x2U) << PKA_FUNCTION_CMD_HI_SHIFT) | \
    (((uint32_t) 0x2U) << PKA_FUNCTION_CMD_LO_SHIFT)

/** Command to PKA firmware - ECDSA VERIFY */
#define ASYM_CRYPT_ECDSA_VERIFY_CMD   (((uint32_t) 0x2U) << PKA_FUNCTION_CMD_HI_SHIFT) | \
    (((uint32_t) 0x3U) << PKA_FUNCTION_CMD_LO_SHIFT)

/** Command to PKA firmware - ECmontMUL*/
#define PKA_ECMONTMUL_CMD   (((uint32_t) 0x0U) << PKA_FUNCTION_CMD_HI_SHIFT) | \
    (((uint32_t) 0x2U) << PKA_FUNCTION_CMD_LO_SHIFT)

/** Command to PKA firmware - PKA_ModInv*/
#define PKA_MODINVP_CMD   (((uint32_t) 0x0U) << PKA_FUNCTION_CMD_HI_SHIFT) | \
    (((uint32_t) 0x7U) << PKA_FUNCTION_CMD_LO_SHIFT)

#define PKA_ADDMODP        0
#define PKA_SUBMODP        1
#define PKA_MULMODP        2
#define PKA_MODULOP        3
#define PKA_MODINVP        4
#define PKA_MODEXPP        5

/** \brief device type HSSE */
#define DEVTYPE_HSSE         (0x0AU)

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */

static AsymCrypt_Return_t PKA_loadFirmware(PKA_Attrs *attrs, uint32_t inst);
static AsymCrypt_Return_t PKA_enable(PKA_Attrs *attrs, uint32_t inst);
static void PKA_disable(PKA_Attrs *attrs);

static void PKA_cpyz(volatile uint32_t dest[ECDSA_MAX_LENGTH], uint32_t dest_len, const uint32_t bn[ECDSA_MAX_LENGTH]);
static inline uint32_t PKA_dwAlign(uint32_t size);
static void PKA_delay(int32_t delayCount);
static uint32_t PKA_bigIntBitLen(const uint32_t bn[ECDSA_MAX_LENGTH]);
static AsymCrypt_Return_t PKA_isBigIntZero(const uint32_t bn[RSA_MAX_LENGTH]);
static CSL_Eip_29t2_ramRegs* PKA_getBaseAddress(PKA_Attrs *attrs);
static void PKA_setALength(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t size);
static void PKA_setBLength(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t size);
static void PKA_setAPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset);
static void PKA_setBPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset);
static void PKA_setCPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset);
static void PKA_setDPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset);

static AsymCrypt_Return_t PKA_ECMontMulX (CSL_Eip_29t2_ramRegs *pka_regs,
                                    const uint32_t p[EC_PARAM_MAXLEN],
                                    const uint32_t A[EC_PARAM_MAXLEN],
                                    const uint32_t x1[EC_PARAM_MAXLEN],
                                    const uint32_t k[EC_PARAM_MAXLEN],
                                    uint32_t result[EC_PARAM_MAXLEN]);

static AsymCrypt_Return_t PKA_ECMontRecoverY (AsymCrypt_Handle handle,
                                        const struct AsymCrypt_ECMontCurveP *cp,
                                        const struct AsymCrypt_ECPoint *P,
                                        const uint32_t k1[EC_PARAM_MAXLEN],
                                        const uint32_t x1[EC_PARAM_MAXLEN],
                                        uint32_t y1[EC_PARAM_MAXLEN]);

static AsymCrypt_Return_t PKA_modpMath(AsymCrypt_Handle handle,
                                const uint32_t a[EC_PARAM_MAXLEN],
                                const uint32_t b[EC_PARAM_MAXLEN],
                                const uint32_t p[EC_PARAM_MAXLEN],
                                const uint32_t op,
                                uint32_t result[EC_PARAM_MAXLEN]);

/**
 * \brief EC Montogomery Multiplication (Result = k*P)
 *
 * \param  handle  #AsymCrypt_Handle returned from #PKA_open()
 * \param cp      EC curve parameters
 * \param P       Initial Point P(x,y) on the Elliptic curve for multiplication
 * \param k       Scalar value in bigint format
 * \param ySkip   Boolean Flag, set this to 1 to skip recovery of y coordinate
 * \param Result  Result of scalar multiplication (Result = k*P), both x and y  are returned
 *                Note: If skip_y is set to 1, then y is coordinate is Not recovered and it's set to 0.
 * \return        #PKA_RETURN_SUCCESS if requested operation completed.
 *                #PKA_RETURN_FAILURE if requested operation not completed.
 */
AsymCrypt_Return_t PKA_ECMontMultiply(AsymCrypt_Handle handle,
                                const struct AsymCrypt_ECMontCurveP *cp,
                                const struct AsymCrypt_ECPoint *P,
                                const uint32_t k[EC_PARAM_MAXLEN],
                                const Bool ySkip,
                                struct AsymCrypt_ECPoint *Result);

/**
* \brief Modular Exponentiation [Result = (base^exp) % mod]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param base    Base (BigInt Form)
* \param exp     Exponent (BigInt Form)
* \param mod     Modulus (BigInt Form)
* \param Result  Result of modular exponentiation
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModPExp(AsymCrypt_Handle handle,
                                const uint32_t base[EC_PARAM_MAXLEN],
                                const uint32_t exp[EC_PARAM_MAXLEN],
                                const uint32_t mod[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

/**
* \brief Modular Multiplication  [Result = (A*B) mod P]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param A       Number A (BigInt Form)
* \param B       Number B (BigInt Form)
* \param P       Number P (BigInt Form), this is value for modulus
* \param Result  Result of modular multiplication
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModPMul(AsymCrypt_Handle handle,
                                const uint32_t A[EC_PARAM_MAXLEN],
                                const uint32_t B[EC_PARAM_MAXLEN],
                                const uint32_t P[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

/**
* \brief Modular Addition [Result = (A+B) mod P]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param A       Number A (BigInt Form)
* \param B       Number B (BigInt Form)
* \param P       Number P (BigInt Form), this is value for modulus
* \param Result  Result of modular addition
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModPAdd(AsymCrypt_Handle handle,
                                const uint32_t A[EC_PARAM_MAXLEN],
                                const uint32_t B[EC_PARAM_MAXLEN],
                                const uint32_t P[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

/**
* \brief Modular Subtraction [Result = (A-B) mod P]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param A       Number A (BigInt Form)
* \param B       Number B (BigInt Form)
* \param P       Number P (BigInt Form), this is value for modulus
* \param Result  Result of modular addition
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModPSub(AsymCrypt_Handle handle,
                                const uint32_t A[EC_PARAM_MAXLEN],
                                const uint32_t B[EC_PARAM_MAXLEN],
                                const uint32_t P[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

/**
* \brief Modulo P operation [Result = A mod P]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param A       Number A (BigInt Form)
* \param P       Number P (BigInt Form), this is value for modulus
* \param Result  Result of modulo P (A mod P)
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModuloP(AsymCrypt_Handle handle,
                                const uint32_t A[EC_PARAM_MAXLEN],
                                const uint32_t P[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

/**
* \brief Inverse (mod P) operation [Result = Inv(A) mod P]
*
* \param  handle  #AsymCrypt_Handle returned from #PKA_open()
*
* \param A       Number A (BigInt Form)
* \param P       Number P (BigInt Form, odd number), this is value for modulus
*                and it must be odd and not equal to 1.
* \param Result  Result of mod P inverse ((1/A) mod P)
* \return        #PKA_RETURN_SUCCESS if requested operation completed.
*                #PKA_RETURN_FAILURE if requested operation not completed.
*/
AsymCrypt_Return_t PKA_ModPInv(AsymCrypt_Handle handle,
                                const uint32_t A[EC_PARAM_MAXLEN],
                                const uint32_t P[EC_PARAM_MAXLEN],
                                uint32_t Result[EC_PARAM_MAXLEN]);

#if defined(SOC_AM64X) || defined(SOC_AM243X)
static CSL_Cp_aceRegs* PKA_getCaBaseAddress(PKA_Attrs *attrs);
#endif /* defined(SOC_AM64X) || defined(SOC_AM243X) */

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

static CSL_Eip_29t2_ramRegs* PKA_getBaseAddress(PKA_Attrs *attrs)
{
    CSL_Eip_29t2_ramRegs *pka_regs;
    pka_regs = (CSL_Eip_29t2_ramRegs *)attrs->pkaBaseAddr;
    return pka_regs;
}

#if defined(SOC_AM64X) || defined(SOC_AM243X)
static CSL_Cp_aceRegs* PKA_getCaBaseAddress(PKA_Attrs *attrs)
{
    CSL_Cp_aceRegs *pCaRegs = (CSL_Cp_aceRegs *)attrs->caBaseAddr;
    return pCaRegs;
}
#endif /* defined(SOC_AM64X) || defined(SOC_AM243X) */

static void PKA_setALength(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t size)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_ALENGTH, size);

}

static void PKA_setBLength(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t size)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_BLENGTH, size);
}

static void PKA_setAPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_APTR, offset);
}

static void PKA_setBPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_BPTR, offset);
}

static void PKA_setCPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_CPTR, offset);
}

static void PKA_setDPtr(CSL_Eip_29t2_ramRegs *pka_regs, uint32_t offset)
{
    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_DPTR, offset);
}

AsymCrypt_Handle AsymCrypt_open(uint32_t index)
{
    uint32_t        status  = ASYM_CRYPT_RETURN_SUCCESS;
    AsymCrypt_Handle      handle  = NULL;
    PKA_Config      *config = NULL;
    PKA_Attrs       *attrs  = NULL;
    /* Check instance */
    if(index >= gPkaConfigNum)
    {
        status = ASYM_CRYPT_RETURN_FAILURE;
    }
    else
    {
        config = &gPkaConfig[index];
        DebugP_assert(NULL != config->attrs);
        attrs = config->attrs;
        if(TRUE == attrs->isOpen)
        {
            /* Handle is already opened */
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
        else
        {
            /* Enabling PKA clock and Load PKA Firmware */
            #if (((defined (SOC_AM263X)|| defined (SOC_AM263PX)|| defined (SOC_AM273X)) && !defined(__ARM_ARCH_7R__)) \
                             || defined (SOC_AM243X) \
                             || defined (SOC_AM64X))
            status = PKA_enable(config->attrs, index);
            #endif
        }
    }

    #if (((defined (SOC_AM263X)|| defined (SOC_AM263PX) || defined (SOC_AM273X)) && defined(__ARM_ARCH_7R__)))
        CSL_top_ctrlRegs * ptrTopCtrlRegs = (CSL_top_ctrlRegs *)CSL_TOP_CTRL_U_BASE;
        if(ptrTopCtrlRegs->EFUSE_DEVICE_TYPE == DEVTYPE_HSSE)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    #endif

    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        attrs->isOpen = TRUE;
        handle = (AsymCrypt_Handle) config;
    }

    return (handle);
}

AsymCrypt_Return_t AsymCrypt_close(AsymCrypt_Handle handle)
{
    AsymCrypt_Return_t status  = ASYM_CRYPT_RETURN_FAILURE;
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config = (PKA_Config *) handle;

    if((NULL != config) && (config->attrs->isOpen != (uint32_t)FALSE))
    {
        attrs = config->attrs;
        DebugP_assert(NULL != attrs);
        PKA_disable(attrs);
        attrs->isOpen = FALSE;
        /* TO disable module*/
        handle = NULL;
        status  = ASYM_CRYPT_RETURN_SUCCESS;
    }
    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAPrivate(AsymCrypt_Handle handle,
                    const uint32_t m[RSA_MAX_LENGTH],
                    const struct AsymCrypt_RSAPrivkey *k,
                    uint32_t result[RSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint64_t curTimeInUsecs, totalTimeInUsecs = 0;
    uint32_t size, offset, reg, wssize, shift, tmp, numCount;
    CSL_Eip_29t2_ramRegs *pka_regs;
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    size = k->p[0];

    /* check sizes, sizes of s and n must match. */
    if ((!((size <= 1U) || (size > ((RSA_MAX_LENGTH - 1U) >> 1)) ||
           (k->q[0] > size) || (k->dp[0] > size) || (k->dq[0] > size) ||
           (k->coefficient[0] > size) || (m[0] > (size * 2U)))))
    {
        /* Checking handle is opened or not */
        if((attrs->isOpen) && (NULL != handle))
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }
    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        PKA_setALength(pka_regs, size);
        PKA_setBLength(pka_regs, size);

        /* Vectors A has Dp[Alen], [pad], Dq[Alen] */
        offset = 0U;
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size, k->dp);
        offset += PKA_dwAlign(size);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size, k->dq);
        offset += PKA_dwAlign(size);

        /* Vectors B has p[Blen], [1], [pad] q[Blen], [1] */
        PKA_setBPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 1U, k->p);
        offset += PKA_dwAlign(size + 1U);

        /* Temporarily set A offset to compare */
        PKA_setAPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 1U, k->q);
        offset += PKA_dwAlign(size + 1U);

        /* Compare and check if p > q */
        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
               (((uint32_t) 1U) << PKA_FUNCTION_COMPARE_SHIFT) |
               (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

        /* Wait for completion */
        curTimeInUsecs = ClockP_getTimeUsec();
        /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
        while(((PKA_FUNCTION_RUN_MASK & CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION)) != 0U))
        {
            totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
        }
        totalTimeInUsecs = 0;

        if(totalTimeInUsecs > PKA_COMPARE_TIMEOUT)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            reg = CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_COMPARE);
            if ((reg & PKA_COMPARE_A_LT_B_MASK) != 0U)
            {
                /* Restore A offset to zero */
                PKA_setAPtr(pka_regs, 0U);

                /* Vectors C has qInv[Blen] */
                PKA_setCPtr(pka_regs, offset);
                PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],
                         size, k->coefficient);
                offset += PKA_dwAlign(size);

                /* Vectors D has M [2*Blen], [1], [pad]
                * WorkSpace */
                PKA_setDPtr(pka_regs, offset);
                PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],
                           size * 2U, m);

                /*
                 * Higher shift value will increase number of
                 * pre-computed odd powers and speed up
                 * exponentiation operation.
                 *
                 * Need workspace size of at least max of
                 * {3 x (BLen + 2 - (BLen MOD 2)) + 10,
                 * (shift + 1) x (BLen + 2 - (BLen MOD 2))} for
                 * computation.
                 *
                 * With PKA_RAM size of 4KB, shift value of 4
                 * will always fit, so latter expression will
                 * always be greater. Shift is determined based
                 * on that.
                 */

                /* Ger the free workspace size available for
                 * computation */

                wssize = PKA_RAM_SIZE_WORDS -
                     (offset + PKA_dwAlign((size * 2U) + 1U));
                tmp = size + (2U - (size & 1U));

                /* Compute best shift for fast computation */
                shift = (wssize / tmp) - 1U;

                /* Maximum shift value is 16 but restrict to 8
                 * max due to diminishing returns on performance
                 * for additional odd powers */
                if (shift > 8U)
                {
                    shift = 8U;
                }

                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_SHIFT, shift);

                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, PKA_MODEXP_CRT_CMD |
                       (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

                /* Wait for completion */
                totalTimeInUsecs = 0;
                curTimeInUsecs = ClockP_getTimeUsec();
                /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
                while(((PKA_SEQ_CTRL_DONE_MASK & CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL)) != ((uint32_t) 1U) << PKA_SEQ_CTRL_DONE_SHIFT))
                {
                    totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
                }

                totalTimeInUsecs = 0;

                if(totalTimeInUsecs > PKA_MODEXP_CRT_TIMEOUT)
                {
                    status = ASYM_CRYPT_RETURN_FAILURE;
                }

                if(status == ASYM_CRYPT_RETURN_SUCCESS)
                {
                    reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
                    if ((reg & PKA_SEQ_CTRL_RESULT_MASK) == (PKA_COMMAND_RESULT_SUCCESS << PKA_SEQ_CTRL_RESULT_SHIFT))
                    {
                        /* Copy result vector from dptr
                         */
                        result[0] = size * 2U;

                        for(numCount = 0; numCount < (size * 2U); numCount++)
                        {
                            result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                        }

                        status = ASYM_CRYPT_RETURN_SUCCESS;
                    }
                    else
                    {
                        status = ASYM_CRYPT_RETURN_FAILURE;
                    }
                }
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }
    return (status);
}

AsymCrypt_Return_t AsymCrypt_RSAPublic(AsymCrypt_Handle handle,
                    const uint32_t m[RSA_MAX_LENGTH],
                    const struct AsymCrypt_RSAPubkey *k,
                    uint32_t result[RSA_MAX_LENGTH])
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint64_t curTimeInUsecs, totalTimeInUsecs = 0;
    uint32_t size, offset, reg, numCount;
    CSL_Eip_29t2_ramRegs *pka_regs;
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    size = k->n[0];

    /* check sizes, sizes of s and n must match. */
    if ((!((size <= 1U) || (size > (RSA_MAX_LENGTH - 1U)) ||
           (m[0] != size) || (k->e[0] > (RSA_MAX_LENGTH - 1U)))))
    {
        /* Checking handle is opened or not */
        if((attrs->isOpen) && (NULL != handle))
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        offset = 0U;
        for(numCount = 0; numCount < (k->e[0]); numCount++)
        {
            pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount] = k->e[1 + numCount];
        }

        offset += PKA_dwAlign(k->e[0]);
        PKA_setBPtr(pka_regs, offset);
        for(numCount = 0; numCount < (k->n[0]); numCount++)
        {
            pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount] = k->n[1 + numCount];
        }
        PKA_setBLength(pka_regs, k->n[0]);

        /* Vectors B and C must be followed by an empty 32-bit buffer
         * word */
        offset += PKA_dwAlign(k->n[0] + 1U);

        PKA_setCPtr(pka_regs, offset);

        for(numCount = 0; numCount < m[0]; numCount++)
        {
            pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount] = m[1 + numCount];
        }

        /* check if m is less than n. temporarily use Aptr and Asize */
        PKA_setAPtr(pka_regs, offset);

        PKA_setALength(pka_regs, m[0]);

        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
               (((uint32_t) 1U) << PKA_FUNCTION_COMPARE_SHIFT) |
               (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

        /* Wait for completion */
        curTimeInUsecs = ClockP_getTimeUsec();
        /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
        while(((PKA_FUNCTION_RUN_MASK & CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION)) != 0U))
        {
            totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
        }

        totalTimeInUsecs = 0;

        if(totalTimeInUsecs > PKA_COMPARE_TIMEOUT)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }

        if (status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            reg = CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_COMPARE);
            if ((reg & PKA_COMPARE_A_LT_B_MASK) != 0U)
            {
                /* Restore the value of Aptr and Aoffset to
                 *point to E */
                PKA_setAPtr(pka_regs, 0U);
                PKA_setALength(pka_regs, k->e[0]);

                /* To save PKA-RAM space, the MODEXP operation
                 * allows the input (M) to be located at the
                 * start of the workspace.
                 * So PKA_CPTR and PKA_DPTR  are allowed to be
                 * identical
                 */
                PKA_setDPtr(pka_regs, offset);
                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_SHIFT, 1U);

                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, PKA_MODEXP_CMD |
                       (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

                /* Wait for completion */
                totalTimeInUsecs = 0;
                curTimeInUsecs = ClockP_getTimeUsec();
                /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
                while(((PKA_SEQ_CTRL_DONE_MASK & CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL)) != ((uint32_t) 1U) << PKA_SEQ_CTRL_DONE_SHIFT))
                {
                    totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
                }

                totalTimeInUsecs = 0;

                if(totalTimeInUsecs > PKA_MODEXP_TIMEOUT)
                {
                    status = ASYM_CRYPT_RETURN_FAILURE;
                }

                if (status == ASYM_CRYPT_RETURN_SUCCESS)
                {
                    reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
                    if ((reg & PKA_SEQ_CTRL_RESULT_MASK) ==
                        (PKA_COMMAND_RESULT_SUCCESS <<
                         PKA_SEQ_CTRL_RESULT_SHIFT))
                    {
                        /*
                         * Success
                         * Copy result vector from dptr
                         */
                        result[0] = size;
                        for(numCount = 0; numCount < size; numCount++)
                        {
                            result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                        }
                        status = ASYM_CRYPT_RETURN_SUCCESS;
                    }
                    else
                    {
                        status = ASYM_CRYPT_RETURN_FAILURE;
                    }
                }
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
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
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint64_t curTimeInUsecs, totalTimeInUsecs = 0;
    uint32_t offset, reg, size, numCount;
    uint32_t bn_one[2] = { 1U, 1U };
    CSL_Eip_29t2_ramRegs *pka_regs;
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    size = cp->prime[0];

    /* check sizes */
    if ((!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0]) || (size < cp->a[0]) ||
           (size < cp->b[0]) || (size < cp->g.x[0]) ||
           (size < cp->g.y[0]) || (size < priv[0]) ||
           (size < h[0]) || (size < k[0]))) &&
           (PKA_bigIntBitLen(cp->order) >= PKA_bigIntBitLen(h)))
    {
        /* Checking handle is opened or not */
        if((attrs->isOpen) && (NULL != handle))
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        PKA_setALength(pka_regs, size);
        PKA_setBLength(pka_regs, size);

        offset = 0U;
        /* Vector B has p, a, b, gz, gy and Rz (must be 1) */
        PKA_setBPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->prime);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->a);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->b);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->order);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->g.x);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->g.y);
        offset += PKA_dwAlign(size + 2U);

        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, bn_one);
        offset += PKA_dwAlign(size + 2U);

        /* Vector C has h */
        PKA_setCPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, h);
        offset += PKA_dwAlign(size + 2U);

        PKA_setAPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, priv);
        offset += PKA_dwAlign(size + 2U);

        PKA_setDPtr(pka_regs, offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, k);

        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, ASYM_CRYPT_ECDSA_SIGN_CMD | (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

        /* Wait for completion */
        curTimeInUsecs = ClockP_getTimeUsec();
        /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
        while(((PKA_SEQ_CTRL_DONE_MASK & CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL)) != ((uint32_t) 1U) << PKA_SEQ_CTRL_DONE_SHIFT) )
        {
            totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
        }

        totalTimeInUsecs = 0;

        if(totalTimeInUsecs > ASYM_CRYPT_ECDSA_SIGN_TIMEOUT)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }

        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
            if((reg & PKA_SEQ_CTRL_RESULT_MASK) == (PKA_COMMAND_RESULT_SUCCESS << PKA_SEQ_CTRL_RESULT_SHIFT))
            {
                sig->r[0] = size;
                for(numCount = 0; numCount < size; numCount++)
                {
                    sig->r[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                }
                offset += PKA_dwAlign(size + 2U);
                sig->s[0] = size;
                for(numCount = 0; numCount < size; numCount++)
                {
                    sig->s[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                }
                status = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
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
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint64_t curTimeInUsecs, totalTimeInUsecs = 0;
    uint32_t offset, reg, size;
    uint32_t bn_one[2] = { 1U, 1U };
    CSL_Eip_29t2_ramRegs *pka_regs;
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    size = cp->prime[0];

    /* check sizes */
    if ((!((size <= 2U) || (size > (ECDSA_MAX_LENGTH - 1U)) ||
           (size != cp->order[0]) || (size < cp->a[0]) ||
           (size < cp->b[0]) || (size < cp->g.x[0]) ||
           (size < cp->g.y[0]) || (size < pub->x[0]) ||
           (size < pub->y[0]) || (size < sig->r[0]) ||
           (size < sig->s[0]) || (size < h[0]))) &&
            (PKA_bigIntBitLen(cp->order) >= PKA_bigIntBitLen(h)) &&
            PKA_isBigIntZero(sig->r) && PKA_isBigIntZero(sig->s))
    {
        /* Checking handle is opened or not */
        if((attrs->isOpen) && (NULL != handle))
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            pka_regs = PKA_getBaseAddress(attrs);

            PKA_setALength(pka_regs, size);
            PKA_setBLength(pka_regs, size);

            offset = 0;
            /* Vector B has p, a, b, gz, gy and Rz (must be 1) */
            PKA_setBPtr(pka_regs, offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->prime);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->a);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->b);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->order);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->g.x);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, cp->g.y);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, bn_one);
            offset += PKA_dwAlign(size + 2U);

            /* Vector C has h */
            PKA_setCPtr(pka_regs, offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, h);
            offset += PKA_dwAlign(size + 2U);

            /* Vector A has px, py and R'z (must be 1) */
            PKA_setAPtr(pka_regs, offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, pub->x);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, pub->y);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, bn_one);
            offset += PKA_dwAlign(size + 2U);

            PKA_setDPtr(pka_regs, offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, sig->r);
            offset += PKA_dwAlign(size + 2U);

            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size + 2U, sig->s);

            CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, ASYM_CRYPT_ECDSA_VERIFY_CMD | (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

            /* Wait for completion */
            curTimeInUsecs = ClockP_getTimeUsec();
            /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
            while(((PKA_SEQ_CTRL_DONE_MASK & CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL)) != ((uint32_t) 1U) << PKA_SEQ_CTRL_DONE_SHIFT) )
            {
                totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
            }

            totalTimeInUsecs = 0;

            if(totalTimeInUsecs > ASYM_CRYPT_ECDSA_VERIFY_TIMEOUT)
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
            else
            {
                reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
                if((reg & PKA_SEQ_CTRL_RESULT_MASK) == (PKA_COMMAND_RESULT_SUCCESS << PKA_SEQ_CTRL_RESULT_SHIFT))
                {
                    status = ASYM_CRYPT_RETURN_SUCCESS;
                }
                else
                {
                    status = ASYM_CRYPT_RETURN_FAILURE;
                }
            }
        }
    }
    return (status);
}

AsymCrypt_Return_t AsymCrypt_ECDSAKeyGenPrivate(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp, 
                        uint32_t priv[ECDSA_MAX_LENGTH])
{
    /* This is not supported for PKA Engine */
    return ASYM_CRYPT_RETURN_FAILURE;
}

AsymCrypt_Return_t AsymCrypt_ECDSAKeyGenPublic(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECPrimeCurveP *cp, 
                        struct AsymCrypt_ECPoint *pub, 
                        const uint32_t priv[ECDSA_MAX_LENGTH])
{
    /* This is not supported for PKA Engine */
    return ASYM_CRYPT_RETURN_FAILURE;
}

AsymCrypt_Return_t PKA_ECMontMultiply(AsymCrypt_Handle handle,
                        const struct AsymCrypt_ECMontCurveP *cp,
                        const struct AsymCrypt_ECPoint *P,
                        const uint32_t k[EC_PARAM_MAXLEN],
						const Bool ySkip,
                        struct AsymCrypt_ECPoint *Result)
{
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
	AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t k1[EC_PARAM_MAXLEN];

    /* Checking handle is opened or not */
    if ((attrs->isOpen) && (NULL != handle)) {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if (status ==  ASYM_CRYPT_RETURN_SUCCESS) {
        status = PKA_ModuloP(handle, k, cp->order, k1);
    }

    if (status ==  ASYM_CRYPT_RETURN_SUCCESS) {

       pka_regs = PKA_getBaseAddress(attrs);

       status = PKA_ECMontMulX(pka_regs, cp->prime, cp->A, P->x, k1, Result->x);

        if (status == ASYM_CRYPT_RETURN_SUCCESS) {
            if (ySkip == FALSE) {
                /* Recover y coordinate of the point*/
                status = PKA_ECMontRecoverY(handle, cp, P, k1, Result->x, Result->y);
            } else {
               /* Skip recovery of y coordinate, set y to bn_0*/
               Result->y[0] = 1;
               Result->y[1] = 0;
            }
        }
    }

    return status;
}

AsymCrypt_Return_t PKA_ModPInv(AsymCrypt_Handle handle,
                        const uint32_t A[EC_PARAM_MAXLEN],
                        const uint32_t P[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t size  = 0, numCount = 0, offset =0;
    volatile uint32_t reg = 0, completeStatus = 0;

	/* Checking handle is opened or not */
    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        PKA_setALength(pka_regs, A[0]);
        PKA_setBLength(pka_regs, P[0]);

        offset = 0;
        PKA_setAPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], A[0], A);

        offset += PKA_dwAlign(A[0]);
        PKA_setBPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], P[0], P);

        offset += PKA_dwAlign(P[0]);
        PKA_setCPtr(pka_regs,0);
        PKA_setDPtr(pka_regs,offset);

        size = P[0];

        if(((P[0]==1) && (P[1] == 1)) || (P[1] & 1) == 0)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
        else
        {
            CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                      (PKA_MODINVP_CMD | PKA_FUNCTION_RUN_MASK)) ;

            completeStatus = 0;
            while(completeStatus != 1)
            {
                reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
                completeStatus = (reg & PKA_SEQ_CTRL_RESULT_MASK) >> PKA_SEQ_CTRL_RESULT_SHIFT;
            }
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }

        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            Result[0] = size;
            for(numCount = 0; numCount < size; numCount++)
            {
                Result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
            }

            while((Result[size] == 0U) && Result[0] > 1U)
            {
                Result[0]-- ;
                size = Result[0];
            }
        }
    }

    return status;
}

AsymCrypt_Return_t PKA_ModuloP(AsymCrypt_Handle handle,
                        const uint32_t A[EC_PARAM_MAXLEN],
                        const uint32_t P[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{
    PKA_Config           *config;
    PKA_Attrs            *attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    AsymCrypt_Return_t   status = ASYM_CRYPT_RETURN_FAILURE;
    uint32_t             size=0 ;
    volatile uint32_t    reg=0, offset =0, completeStatus = 0;
    uint32_t             lenA=0, lenP=0, numCount = 0, curPos=0;
    uint64_t             tempWord = 0UL, tempRem = 0UL;

    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    /* Checking handle is opened or not */
    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    /*Get A and P length with most significant non-zero 32-bit word*/
    lenA = A[0];
    while((A[lenA] == 0) && lenA > 1)
    {
        lenA--;
    }
    lenP = P[0];
    while((P[lenP] == 0) && lenP > 1)
    {
        lenP--;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        /*Check for Valid length*/
        if((lenA > 1) && (lenP > 1) && lenA >= lenP)
        {
            pka_regs = PKA_getBaseAddress(attrs);

            PKA_setALength(pka_regs, lenA);
            PKA_setBLength(pka_regs, lenP);

            offset = 0;
            PKA_setAPtr(pka_regs,offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], lenA, A);

            offset += PKA_dwAlign(lenA);
            PKA_setBPtr(pka_regs,offset);
            PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], lenP, P);

            offset += PKA_dwAlign(lenP);
            PKA_setCPtr(pka_regs,offset);

            CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                (PKA_FUNCTION_MODULO_MASK | PKA_FUNCTION_RUN_MASK)) ;

            completeStatus = 0;
            while( completeStatus != 1)
            {
                 reg = PKA_FUNCTION_RUN_MASK & CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION);
                 if(reg == 0)
                 {
                    completeStatus = 1;
                 }

            }

            if(completeStatus == 1U)
            {
                Result[0] = lenP;
                for(numCount = 0; numCount < Result[0]; numCount++)
                {
                    Result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                }

                size =  Result[0];
                while((Result[size] == 0U) && Result[0] > 1U)
                {
                    Result[0]-- ;
                    size = Result[0];
                }
                status = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }

        }
        else if((lenA >= 1) && (lenP > 1) && lenA < lenP)
        {
            /*lenA < lenP with most signifcant 32-bit word non-zero => A < P, hence A % P = A*/
            Result[0] = lenA;
            for(numCount = 1; numCount<=lenA; numCount++)
            {
                Result[numCount] = A[numCount];
            }
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else if((lenA == 1) && (lenP == 1) && (P[lenP] != 0))
        {
            Result[0] = 1U;
            Result[1] = A[1] % P[1];
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else if((lenA > 1) && (lenP == 1) && (P[lenP] != 0))
        {
            Result[0] = 1U;
            tempWord = A[lenA];
            tempRem = tempWord%P[1];
            curPos = lenA-1;
            while(curPos>=1)
            {
                tempWord = ((tempRem<<32U) & 0xFFFFFFFF00000000UL)|(A[curPos] & 0xFFFFFFFFUL);
                tempRem = tempWord % P[1];
                curPos--;
            }
            Result[1] = tempRem % P[1];
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return status;
}

AsymCrypt_Return_t PKA_ModPMul(AsymCrypt_Handle handle,
                        const uint32_t A[EC_PARAM_MAXLEN],
                        const uint32_t B[EC_PARAM_MAXLEN],
                        const uint32_t P[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{

    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;

    status = PKA_modpMath(handle,A,B,P,PKA_MULMODP,Result);

    return status;
}

AsymCrypt_Return_t PKA_ModPAdd(AsymCrypt_Handle handle,
                        const uint32_t A[EC_PARAM_MAXLEN],
                        const uint32_t B[EC_PARAM_MAXLEN],
                        const uint32_t P[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;

    status = PKA_modpMath(handle,A,B,P,PKA_ADDMODP,Result);

    return status;
}

AsymCrypt_Return_t PKA_ModPSub(AsymCrypt_Handle handle,
                        const uint32_t A[EC_PARAM_MAXLEN],
                        const uint32_t B[EC_PARAM_MAXLEN],
                        const uint32_t P[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;

    uint32_t offset = 0;
    volatile uint32_t reg = 0;
    uint32_t tempResult[EC_PARAM_MAXLEN];

    /* Checking handle is opened or not */
    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if (status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);
        PKA_setALength(pka_regs, A[0]);
        PKA_setBLength(pka_regs, B[0]);

        PKA_setAPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],A[0],A);

        offset+=PKA_dwAlign(A[0]);

        PKA_setBPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],A[0],B);

        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                   (((uint32_t) 1U) << PKA_FUNCTION_COMPARE_SHIFT) |
                   (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

        while((PKA_FUNCTION_RUN_MASK & CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION)) !=0)
        {
            /*wait*/
        }

        reg = CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_COMPARE);

        if((status == ASYM_CRYPT_RETURN_SUCCESS) && ((reg & PKA_COMPARE_A_GT_B_MASK) != 0U))
        {
            pka_regs = PKA_getBaseAddress(attrs);
            status = PKA_modpMath(handle,A,B,P,PKA_SUBMODP,Result);
        }
        else if((status == ASYM_CRYPT_RETURN_SUCCESS) && ((reg & PKA_COMPARE_A_LT_B_MASK) != 0U))
        {
            pka_regs = PKA_getBaseAddress(attrs);
            status = PKA_modpMath(handle,B,A,P,PKA_SUBMODP,tempResult);
            if(status == ASYM_CRYPT_RETURN_SUCCESS)
            {
                status = PKA_modpMath(handle,P,tempResult,P,PKA_SUBMODP,Result);
            }
        }
        else if((status == ASYM_CRYPT_RETURN_SUCCESS) && ((reg & PKA_COMPARE_A_EQ_B_MASK) != 0U))
        {
            Result[0] = 1;
            Result[1] = 0;
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return status;
}

AsymCrypt_Return_t PKA_ModPExp(AsymCrypt_Handle handle,
                        const uint32_t base[EC_PARAM_MAXLEN],
                        const uint32_t exp[EC_PARAM_MAXLEN],
                        const uint32_t mod[EC_PARAM_MAXLEN],
                        uint32_t Result[EC_PARAM_MAXLEN])
{
    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    uint32_t size = 0U, offset=0, numCount=0;

    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    volatile uint32_t reg=0, completeStatus = 0;

    /* Checking handle is opened or not */
    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        offset = 0;
        size = exp[0];
        PKA_setALength(pka_regs,size);
        PKA_setAPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],size,exp);

        offset += PKA_dwAlign(size);
        size = mod[0];
        PKA_setBLength(pka_regs,size);
        PKA_setBPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],size,mod);

        offset += PKA_dwAlign(size+1);
        PKA_setCPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset],size,base);

        offset += PKA_dwAlign(size+1);
        PKA_setDPtr(pka_regs,offset);

        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_SHIFT, 1U);

        CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, PKA_MODEXP_CMD |
                (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT));

        while(completeStatus != 1)
        {
            reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
            completeStatus = (reg & PKA_SEQ_CTRL_RESULT_MASK) >> PKA_SEQ_CTRL_RESULT_SHIFT;
        }

        if ((reg & PKA_SEQ_CTRL_RESULT_MASK) == (PKA_COMMAND_RESULT_SUCCESS <<PKA_SEQ_CTRL_RESULT_SHIFT))
        {
            /*
            * Success
            * Copy result vector from dptr
            */
            Result[0] = size;
            for(numCount = 0; numCount < size; numCount++)
            {
                Result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
            }
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
                status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }
    return status;
}

/* Function to recover y coordinate of point P1(x1,y1) on Montogomery curve
*  y coordinate is recovered using : y1 = ((x3 - x2)*((x1 - x)^2))/4By,
*  Here P(x,y) is generator point of curve, P1(x1,y1) is the point whose y coordinate is recovered
*  x2, x3 corresponds to x coordinate of point P2, P3 such that P2 = P1+P, P3= P1-P
*  B is curve parameter coefficient in Montogomery curve, for Curve25519 its value = 1
*/
static AsymCrypt_Return_t PKA_ECMontRecoverY (AsymCrypt_Handle handle,
                                        const struct AsymCrypt_ECMontCurveP *cp,
                                        const struct AsymCrypt_ECPoint *P,
                                        const uint32_t k1[EC_PARAM_MAXLEN],
                                        const uint32_t x1[EC_PARAM_MAXLEN],
                                        uint32_t y1[EC_PARAM_MAXLEN])
{
    AsymCrypt_Return_t         status = ASYM_CRYPT_RETURN_FAILURE;
    PKA_Config           *config;
    PKA_Attrs            *attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    uint32_t             x2[EC_PARAM_MAXLEN], x3[EC_PARAM_MAXLEN];
    static uint32_t      temp1[EC_PARAM_MAXLEN], temp2[EC_PARAM_MAXLEN], temp3[EC_PARAM_MAXLEN];
    uint32_t             numCount=0;
    const uint32_t       bn_4[2] = {1,4};

    config  = (PKA_Config *) handle;
    attrs   = config->attrs;

    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);

        for(numCount=0;numCount<=k1[0];numCount++)
        {
            temp1[numCount] = k1[numCount];
        }
        for(numCount=1;numCount<=k1[0];numCount++)
        {
            if(k1[numCount] < 0xFFFFFFFFUL)
            {
                temp1[numCount] = k1[numCount]+1;
                break;
            }
            else
            {
                temp1[numCount] = 0x0UL;
            }
        }
        if(numCount == (k1[0]+1))
        {
            if(numCount < EC_PARAM_MAXLEN)
            {
                temp1[0] = k1[0]+1;
                temp1[numCount] = 0x1UL;
                status = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }
        }
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ECMontMulX(pka_regs, cp->prime, cp->A, P->x, temp1, x2);
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        for(numCount=0;numCount<=k1[0];numCount++)
        {
            temp1[numCount] = k1[numCount];
        }
        for(numCount=1;numCount<=k1[0];numCount++)
        {
            if(k1[numCount]  > 0x0UL)
            {
                temp1[numCount] = k1[numCount]-1;
                break;
            }
            else
            {
                temp1[numCount] = 0xFFFFFFFFUL;
            }
        }
        if(numCount <(k1[0]))
        {
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else if (numCount == (k1[0]))
        {
            if(temp1[numCount] == 0x0UL)
            {
                temp1[0] = k1[0]-1 ;
            }
            status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ECMontMulX(pka_regs, cp->prime, cp->A, P->x, temp1, x3);
    }

    /*Get Numerator expression in temp3 [((x3 - x2)*(x1 - x)^2)]*/
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPSub(handle,x1,P->x,cp->prime,temp1);
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPMul(handle,temp1,temp1,cp->prime,temp2);
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPSub(handle,x3,x2,cp->prime,temp1);
    }
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPMul(handle,temp1,temp2,cp->prime,temp3);
    }

    /*Get Denominator expression [4By], B is always 1*/
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPMul(handle,bn_4,P->y,cp->prime,temp1);
    }

    /* temp2 = (1/4By)*/
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        status = PKA_ModPInv(handle,temp1,cp->prime,temp2);
    }

    /* Get result y1 */
    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
            status = PKA_ModPMul(handle,temp3,temp2,cp->prime,y1);
    }
    return status;
}

/*Funtion to perform scalar multiplication of point P1(x1,y1) on Montogomery curve
* Only x coordinate is used : result(x)  = k*P1(x1), here k is the scalar
* p is the prime modulus of curve, and A is curve coefficent (By^2 = x^3 + Ax^2 + x , B = 1)
*/
static AsymCrypt_Return_t PKA_ECMontMulX (CSL_Eip_29t2_ramRegs *pka_regs,
                                    const uint32_t p[EC_PARAM_MAXLEN],
                                    const uint32_t A[EC_PARAM_MAXLEN],
                                    const uint32_t x1[EC_PARAM_MAXLEN],
                                    const uint32_t k[EC_PARAM_MAXLEN],
                                    uint32_t result[EC_PARAM_MAXLEN])
{

	AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;
    volatile int32_t size = 0U, offset=0, reg=0, numCount=0;
    volatile uint32_t completeStatus =0;



    PKA_setALength(pka_regs, k[0]);
    PKA_setBLength(pka_regs, p[0]);

    offset =0U;

    size=k[0];
    PKA_setAPtr(pka_regs, offset);
    PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size, k);
    offset += PKA_dwAlign(size);

    size=p[0];
    PKA_setBPtr(pka_regs, offset);
    PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size+2U, p);

    offset += PKA_dwAlign(size+2U);

    PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size+2U, A);
    offset += PKA_dwAlign(size+2U);

    PKA_setCPtr(pka_regs, offset);
    PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size+2U, x1);
    offset += PKA_dwAlign(size+2U);

    PKA_setDPtr(pka_regs, offset);
    PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], size+2U, result);

    CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION, (PKA_ECMONTMUL_CMD | (((uint32_t) 1U) << PKA_FUNCTION_RUN_SHIFT)));
    while(completeStatus != 1)
    {
        reg = CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL);
        completeStatus = (reg & PKA_SEQ_CTRL_RESULT_MASK) >> PKA_SEQ_CTRL_RESULT_SHIFT;
    }

    if ((reg & PKA_SEQ_CTRL_RESULT_MASK) ==
        (PKA_COMMAND_RESULT_SUCCESS <<
        PKA_SEQ_CTRL_RESULT_SHIFT))
    {
        /*
        * Success
        * Copy result vector from dptr
        */
        result[0] = size;
        for(numCount = 0; numCount < size; numCount++)
        {
            result[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
        }
        status = ASYM_CRYPT_RETURN_SUCCESS;
        }
        else
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    return status;
}

/**
 * \brief Perform modular arithmetic operation on bigInt numbers
 *
 * \param a       Input number (operand 1 in bigInt form)
 * \param b       Input number (operand 2 in bigInt form)
 * \param p       Modulus p (in bigInt form)
 * \param op      Arithmetic operation (+, - or *) to be performed
 * \param result  Output result (a op b) mod p
 *
 * \return        ASYM_CRYPT_RETURN_SUCCESS if requested operation completed.
 *                ASYM_CRYPT_RETURN_FAILURE if requested operation not completed.
 */
static AsymCrypt_Return_t PKA_modpMath(AsymCrypt_Handle handle,
                               const uint32_t a[EC_PARAM_MAXLEN],
                               const uint32_t b[EC_PARAM_MAXLEN],
                               const uint32_t p[EC_PARAM_MAXLEN],
                               const uint32_t op,
                               uint32_t result[EC_PARAM_MAXLEN])
{
    AsymCrypt_Return_t status= ASYM_CRYPT_RETURN_FAILURE;
    uint32_t size=0, numCount = 0, offset = 0;
    uint32_t max_size_ab = 0;
    uint32_t completeStatus = 0;
    volatile uint32_t reg = 0;
    uint32_t tempResult[EC_PARAM_MAXLEN] = {0};

    PKA_Config      *config;
    PKA_Attrs *attrs;
    config  = (PKA_Config *) handle;
    attrs   = config->attrs;
    CSL_Eip_29t2_ramRegs *pka_regs;
    /* Checking handle is opened or not */
    if((attrs->isOpen) && (NULL != handle))
    {
        status = ASYM_CRYPT_RETURN_SUCCESS;
    }

    if(status == ASYM_CRYPT_RETURN_SUCCESS)
    {
        pka_regs = PKA_getBaseAddress(attrs);
        PKA_setALength(pka_regs, a[0]);
        PKA_setBLength(pka_regs, b[0]);

        if(a[0] >= b[0])
        {
            max_size_ab = a[0];
        }
        else
        {
            max_size_ab = b[0];
        }

        offset = 0;
        PKA_setAPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], a[0], a);

        offset += PKA_dwAlign(a[0]);
        PKA_setBPtr(pka_regs,offset);
        PKA_cpyz(&pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset], b[0], b);

        offset += PKA_dwAlign(b[0]);
        PKA_setCPtr(pka_regs,offset);
        switch(op)
        {
            case PKA_ADDMODP :
            {
                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                        (PKA_FUNCTION_ADD_MASK | PKA_FUNCTION_RUN_MASK)) ;
                size = max_size_ab+1U;
                break;
            }
            case PKA_SUBMODP :
            {
                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                        (PKA_FUNCTION_SUBTRACT_MASK | PKA_FUNCTION_RUN_MASK)) ;
                size = max_size_ab;
                break;
            }
            case PKA_MULMODP :
            {
                CSL_REG_WR(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION,
                        (PKA_FUNCTION_MULTIPLY_MASK | PKA_FUNCTION_RUN_MASK)) ;
                size = a[0]+b[0];
                break;
            }
            default :
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
                break;
            }
        }

        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            completeStatus = 0;
            while(completeStatus != 1)
            {
                reg = PKA_FUNCTION_RUN_MASK & CSL_REG_RD(&pka_regs->EIP_27B_EIP27_REGISTERS.PKA_FUNCTION);
                if(reg == 0)
                {
                    completeStatus = 1;
                }
            }

            if(completeStatus == 1U)
            {
                tempResult[0] = size;
                for(numCount = 0; numCount < tempResult[0]; numCount++)
                {
                    tempResult[1 + numCount] = pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[offset + numCount];
                }

                while((tempResult[size] == 0U) && tempResult[0] > 1U)
                {
                    tempResult[0]-- ;
                    size = tempResult[0];
                }
                status = ASYM_CRYPT_RETURN_SUCCESS;
            }
            else
            {
                status = ASYM_CRYPT_RETURN_FAILURE;
            }

        }
        if(status == ASYM_CRYPT_RETURN_SUCCESS)
        {
            status = PKA_ModuloP(handle,tempResult,p,result);
        }
    }

    return status;
}

static AsymCrypt_Return_t PKA_enable(PKA_Attrs *attrs, uint32_t inst)
{
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_SUCCESS;
    uint32_t reg;
    uint64_t curTimeInUsecs, totalTimeInUsecs = 0;

    /* PKA Base address */
    CSL_Eip_29t2_ramRegs *pka_regs = PKA_getBaseAddress(attrs);

    /* Engine enable registers are available only in am64x and am243x */
#if defined(SOC_AM64X) || defined(SOC_AM243X)
    /* Crypto Accelerator Base Adders*/
    CSL_Cp_aceRegs *pCaRegs = PKA_getCaBaseAddress(attrs);

    /* Enable PKA engine modules */
    reg = CSL_REG_RD(&pCaRegs->UPDATES.ENGINE_ENABLE);

    CSL_FINS(reg, CP_ACE_UPDATES_ENGINE_ENABLE_PKA_EN, 1u);

    CSL_REG_WR(&pCaRegs->UPDATES.ENGINE_ENABLE, reg);

    reg = CSL_CP_ACE_CMD_STATUS_PKA_EN_MASK;

    /* Wait for PKA engine to be Enable */
    curTimeInUsecs = ClockP_getTimeUsec();
    /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an
     * checks need to be added back */
    while(((reg & CSL_REG_RD(&pCaRegs->MMR.CMD_STATUS)) != reg))
    {
        totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
    }

    totalTimeInUsecs = 0;

    if(totalTimeInUsecs > PKA_REG_TIMEOUT)
    {
        status = ASYM_CRYPT_RETURN_FAILURE;
    }
#endif
    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        CSL_REG_WR(&pka_regs->EIP_29T2_RAM_HOST_REGISTERS.PKA_CLK_CTRL,
            (((uint32_t) 1U) << PKA_CLK_CTRL_REG_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_SEQ_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_PKCP_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_REG_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_GF2M_CLK_ON_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_DATA_RAM_CLK_ON_SHIFT));

        reg = (((uint32_t) 1U) << PKA_CLK_CTRL_REG_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_SEQ_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_PKCP_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_REG_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_GF2M_CLK_EN_SHIFT) |
            (((uint32_t) 1U) << PKA_CLK_CTRL_DATA_RAM_CLK_EN_SHIFT);

        /* Wait for PKA internal clocks to be active */
        totalTimeInUsecs = 0;
        curTimeInUsecs = ClockP_getTimeUsec();
        /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
        while(((reg & CSL_REG_RD(&pka_regs->EIP_29T2_RAM_HOST_REGISTERS.PKA_CLK_CTRL)) != reg))
        {
            totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
        }

        totalTimeInUsecs = 0;

        if(totalTimeInUsecs > PKA_REG_TIMEOUT)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }
    if(ASYM_CRYPT_RETURN_SUCCESS == status)
    {
        status = PKA_loadFirmware(attrs, inst);
    }
    if(ASYM_CRYPT_RETURN_SUCCESS != status)
    {
        PKA_disable(attrs);
        /*
         * If firmware is already marked as loaded, check the PKA internal
         * clocks are active as a sanity check.
         */
        reg = (((uint32_t) 1U) << PKA_CLK_CTRL_REG_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_SEQ_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_PKCP_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_REG_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_GF2M_CLK_EN_SHIFT) |
              (((uint32_t) 1U) << PKA_CLK_CTRL_DATA_RAM_CLK_EN_SHIFT);
        /* Wait for PKA internal clocks to be active */
        totalTimeInUsecs = 0;
        curTimeInUsecs = ClockP_getTimeUsec();
        /* TODO: MCUSDK-8047: Timeout values need to be calibrated for SoCs an checks need to be added back */
        while(((reg & CSL_REG_RD(&pka_regs->EIP_29T2_RAM_HOST_REGISTERS.PKA_CLK_CTRL)) != reg))
        {
            totalTimeInUsecs = ClockP_getTimeUsec() - curTimeInUsecs;
        }

        totalTimeInUsecs = 0;

        if(totalTimeInUsecs > PKA_REG_TIMEOUT)
        {
            status = ASYM_CRYPT_RETURN_FAILURE;
        }
    }

    return (status);
}

static AsymCrypt_Return_t PKA_loadFirmware(PKA_Attrs *attrs, uint32_t inst)
{
    volatile int32_t i;
    AsymCrypt_Return_t status = ASYM_CRYPT_RETURN_FAILURE;

    CSL_Eip_29t2_ramRegs *pka_regs = PKA_getBaseAddress(attrs);

    /* Put EIP-29t2 (PKA) in reset, set bit 31 of PKA_SEQ_CTRL */
    CSL_REG_WR(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL, ((uint32_t) 1U) << PKA_SEQ_CTRL_RESET_SHIFT);

    /*
     * Copy firmware to PKA program memory, which is aliased at the PKA_RAM
     * location when then PKA is in reset
     */
    for (i = 0; i < EIP29T2_FW_IMAGE_LEN_WORDS; i++) {
        pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[i] = eip29t2_fw_image[i];
    }

    /* Take EIP-29t2 (PKA) out of reset */
    CSL_REG_WR(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL, ((uint32_t) 0U) << PKA_SEQ_CTRL_RESET_SHIFT);

    /* Wait for a few cycles */
    PKA_delay(10U);

    /* Check status in PKA_SEQ_CTRL */
    if((CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL) & PKA_SEQ_CTRL_RESULT_MASK) ==  ((uint32_t)1U) << PKA_SEQ_CTRL_RESULT_SHIFT)
    {   /* Put EIP-29t2 (PKA) back into reset */
        CSL_REG_WR(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL, ((uint32_t) 1U) << PKA_SEQ_CTRL_RESET_SHIFT);

        /* Verify the firmware content in the PKA program RAM */
        for (i = 0; i < EIP29T2_FW_IMAGE_LEN_WORDS; i++)
        {
            if (pka_regs->EIP_29T2_RAM_PKA_RAM.PKA_RAM[i] != eip29t2_fw_image[i])
            {
                break;
            }
        }
        if (i == EIP29T2_FW_IMAGE_LEN_WORDS)
        {
            /* Take EIP-29t2 (PKA) out of reset */
            CSL_REG_WR(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL,
                   ((uint32_t) 0U) << PKA_SEQ_CTRL_RESET_SHIFT);

            /* Wait for a few cycles */
            PKA_delay(10U);

            /* Check status in PKA_SEQ_CTRL */
            if ((CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SEQ_CTRL) & PKA_SEQ_CTRL_RESULT_MASK) == (((uint32_t) 1U) << PKA_SEQ_CTRL_RESULT_SHIFT))
            {
                /* Wait for a few cycles */
                PKA_delay(50U);

                /* Check the firmware revision, 3 nibbles of
                 * major, minor
                 * and patch level are located from bits 27:16
                 **/
                if ((CSL_REG_RD(&pka_regs->EIP_28PX12_GF2_2PRAM_EIP28_REGISTERS.PKA_SW_REV) & PKA_SW_REV_FIRMWARE_VERSION_MASK) == (EIP29T2_FW_VERSION << PKA_SW_REV_FIRMWARE_VERSION_SHIFT))
                {
                    status = ASYM_CRYPT_RETURN_SUCCESS;
                }
            }
        }
    }
    return (status);
}

static void PKA_disable(PKA_Attrs *attrs)
{
    /* Engine enable registers are available only in am64x and am243x */
#if defined(SOC_AM64X) || defined(SOC_AM243X)
    uint32_t reg;
#endif
    /* PKA Base address */
    CSL_Eip_29t2_ramRegs *pka_regs = PKA_getBaseAddress(attrs);

    /* Engine enable registers are available only in am64x and am243x */
#if defined(SOC_AM64X) || defined(SOC_AM243X)
    /* Crypto Accelerator Base Adders*/
    CSL_Cp_aceRegs *pCaRegs = PKA_getCaBaseAddress(attrs);

    reg = CSL_REG_RD(&pCaRegs->UPDATES.ENGINE_ENABLE);

    /* Disable PKA Engine */
    CSL_FINS(reg, CP_ACE_UPDATES_ENGINE_ENABLE_PKA_EN, 0u);

    CSL_REG_WR(&pCaRegs->UPDATES.ENGINE_ENABLE, reg);
#endif

#if (((defined (SOC_AM263X)|| defined (SOC_AM263PX)|| defined (SOC_AM273X)) && !defined(__ARM_ARCH_7R__)) \
                             || defined (SOC_AM243X) \
                             || defined (SOC_AM64X))
    CSL_REG_WR(&pka_regs->EIP_29T2_RAM_HOST_REGISTERS.PKA_CLK_CTRL,
        (((uint32_t) 1U) << PKA_CLK_CTRL_REG_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_SEQ_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_PKCP_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_LNME_REG_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_GF2M_CLK_OFF_SHIFT) |
        (((uint32_t) 1U) << PKA_CLK_CTRL_DATA_RAM_CLK_OFF_SHIFT));
#else
    (void) pka_regs;
#endif
    return;
}

static void PKA_delay(int32_t delayCount)
{
    volatile int32_t tempDelay = delayCount;

    while(tempDelay != 0UL)
    {
        tempDelay = tempDelay -1U;
    }
    return;
}

/**
 * \brief Copy to destination with zero padding if necessary
 *
 * Copy with zero padding, useful to copy bigint operands to PKA RAM
 *
 * \param dest Destination address to copy to
 * \param dest_len Length of the destination buffer
 * \param bn Bigint to copy from
 */
static void PKA_cpyz(volatile uint32_t dest[ECDSA_MAX_LENGTH],
               uint32_t dest_len, const uint32_t bn[ECDSA_MAX_LENGTH])
{
    uint32_t i;

    for (i = 0U; (i < bn[0]) && (i < dest_len); i++) {
        dest[i] = bn[i + 1U];
    }
    for (; i < dest_len; i++) {
        dest[i] = 0U;
    }
    return;
}

/** Get a double-word aligned number for bigints used by PKA */
static inline uint32_t PKA_dwAlign(uint32_t size)
{
    return (size + (size & 1U));
}

uint32_t PKA_countLeadingZeros(uint32_t x)
{
    uint32_t bit_count = 0, lz = 0;

    bit_count = sizeof(x)*8;

    /* Left shift until Most significant bit doesn become 1 */

    while( (x & (1 << (bit_count - 1))) == 0)
    {
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
static uint32_t PKA_bigIntBitLen(const uint32_t bn[ECDSA_MAX_LENGTH])
{
    uint32_t i, status;

    for (i = bn[0]; i > 0U; i--)
    {
        if (bn[i] != 0U)
        {
            break;
        }
    }
    if (i == 0U)
    {
        status = 0U;
    }
    else
    {
        status = (i * 32U) - PKA_countLeadingZeros((int32_t) bn[i]);
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
static AsymCrypt_Return_t PKA_isBigIntZero(const uint32_t bn[RSA_MAX_LENGTH])
{
    uint32_t i;
    AsymCrypt_Return_t ret = ASYM_CRYPT_RETURN_SUCCESS;

    for (i = 0U; i <= bn[0]; i++)
    {
        if (bn[i] != 0U)
        {
            ret = ASYM_CRYPT_RETURN_FAILURE;
            break;
        }
    }
    return (ret);
}
