/*
 *  Copyright (c) Texas Instruments Incorporated 2024
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
 *  \file ti_enet_open_close.h
 *
 *  \brief Enet open header file.
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <enet.h>
#include <include/core/enet_types.h>

#include <kernel/dpl/EventP.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/*!
 * \brief  Initialize Enet Driver from Application.
 *
 */
void EnetApp_driverInit();

/*!
 * \brief  Denitialize Enet Driver from Application.
 *
 */
void EnetApp_driverDeInit();

/*!
 * \brief  Opens Enet Driver from Application.
 *
 * \param enetType  Enet Peripheral type
 * \param instId    Enet Peripheral instance id
 */
extern int32_t EnetApp_driverOpen(Enet_Type enetType, uint32_t instId);

/*!
 * \brief  Closes Enet Driver from Application when MCM is disabled
 *
 * \param enetType  Enet Peripheral type
 * \param instId    Enet Peripheral instance id
 */
void EnetApp_driverClose(Enet_Type enetType, uint32_t instId);

void EnetApp_updateCpswInitCfg(Enet_Type enetType,  uint32_t instId,   Cpsw_Cfg *cpswCfg);

void EnetApp_initPhyStateHandlerTask(EventP_Object* pEvent);

void EnetApp_phyStateHandler();


EnetMacPort_PreemptVerifyStatus EnetApp_doIetVerification(Enet_Handle hEnet, const uint32_t coreId, const Enet_MacPort macPort);

/* ========================================================================== */
/*                       Static Function Definitions                          */
/* ========================================================================== */

/* None */

