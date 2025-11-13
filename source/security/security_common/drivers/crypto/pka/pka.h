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
 *  \defgroup SECURITY_PKA_MODULE APIs for PKA
 *  \ingroup  SECURITY_MODULE
 *
 *  This module contains APIs to program and use the PKA.
 *
 *  @{
 */

/**
 *  \file pka.h
 *
 *  \brief This file contains the prototype of PKA driver APIs
 */

#ifndef PKA_H_
#define PKA_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#if defined (SOC_AM64X) || (SOC_AM243X)
#include <security_common/drivers/crypto/pka/soc/am64x_am243x/eip29t2_firmware.h>
#else
#include <security_common/drivers/crypto/pka/eip29t2_firmware.h>
#endif
#include <security_common/drivers/crypto/pka/hw_include/cslr_cp_ace.h>
#include <kernel/dpl/SystemP.h>
#include <security_common/drivers/crypto/crypto_util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/** \brief PKA attributes */
typedef struct
{
    /*
     * SOC configuration
     */
	uint32_t				caBaseAddr;
	/**< Crypto Accelerator Base Adders*/
    uint32_t                pkaBaseAddr;
    /**< PKA Base address */
	uint32_t                isOpen;
    /**< Flag to indicate whether the instance is opened already */
} PKA_Attrs;

/** \brief PKA driver context */
typedef struct
{
    PKA_Attrs             *attrs;
    /**< Driver params passed during open */
} PKA_Config;

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/** \brief Externally defined driver configuration array */
extern PKA_Config            gPkaConfig[];
/** \brief Externally defined driver configuration Num */
extern uint32_t             gPkaConfigNum;

/* ========================================================================== */
/*                              Function Definitions                          */
/* ========================================================================== */


#ifdef __cplusplus
}
#endif

#endif /* PKA_H_ */

/** @} */
