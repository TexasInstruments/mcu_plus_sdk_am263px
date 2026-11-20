/*
 *   Copyright (c) Texas Instruments Incorporated 2026
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
 *  \file    ecc_trigger.c
 *
 *  \brief    This file contains functions that provide input event triggers
 *            for the Error Correcting Code (ECC) Module application.
 *
 *  \details  ECC Safety Example module tests
 **/

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include <stdint.h>
#include <sdl/include/sdl_types.h>
#include <sdl/sdl_ecc.h>
#include "ecc_main.h"
#include <sdl/dpl/sdl_dpl.h>
#include <sdl/include/am64x_am243x/sdlr_soc_ecc_aggr.h>
#include <sdl/r5/v0/sdl_r5_utils.h>
#include <sdl/ecc/sdl_ecc_utils.h>
#include <sdl/sdl_exception.h>
#include <sdl/r5/v0/sdl_interrupt.h>
/* ========================================================================== */
/*                                Macros                                      */
/* ========================================================================== */

#define MAX_ESM_TIMEOUT 1000

/* delay for 1us*/
#define DELAY 1

#define DOUBLE_ERROR_FLIPMASK   0x5
#define SINGLE_ERROR_FLIPMASK   0x1

#define ICSSG0_DRAM0_ADDR   SDL_PRU_ICSSG0_DRAM0_SLV_RAM_BASE
#define ICSSG0_DRAM1_ADDR   SDL_PRU_ICSSG0_DRAM1_SLV_RAM_BASE
#define ICSSG0_PDSP0_ADDR   SDL_PRU_ICSSG0_PR1_PDSP0_IRAM_RAM_BASE
#define ICSSG0_PDSP1_ADDR   SDL_PRU_ICSSG0_PR1_PDSP1_IRAM_RAM_BASE
#define ICSSG0_RAM_ADDR     SDL_PRU_ICSSG0_RAM_SLV_RAM_BASE
#define ICSSG0_RTU0_ADDR    SDL_PRU_ICSSG0_PR1_RTU0_PR1_RTU0_IRAM_RAM_BASE
#define ICSSG0_RTU1_ADDR    SDL_PRU_ICSSG0_PR1_RTU1_PR1_RTU1_IRAM_RAM_BASE
#define ICSSG0_TX0_ADDR     SDL_PRU_ICSSG0_PR1_PDSP_TX0_IRAM_RAM_BASE
#define ICSSG0_TX1_ADDR     SDL_PRU_ICSSG0_PR1_PDSP_TX1_IRAM_RAM_BASE

#define ICSSG1_DRAM0_ADDR   SDL_PRU_ICSSG1_DRAM0_SLV_RAM_BASE
#define ICSSG1_DRAM1_ADDR   SDL_PRU_ICSSG1_DRAM1_SLV_RAM_BASE
#define ICSSG1_PDSP0_ADDR   SDL_PRU_ICSSG1_PR1_PDSP0_IRAM_RAM_BASE
#define ICSSG1_PDSP1_ADDR   SDL_PRU_ICSSG1_PR1_PDSP1_IRAM_RAM_BASE
#define ICSSG1_RAM_ADDR     SDL_PRU_ICSSG1_RAM_SLV_RAM_BASE
#define ICSSG1_RTU0_ADDR    SDL_PRU_ICSSG1_PR1_RTU0_PR1_RTU0_IRAM_RAM_BASE
#define ICSSG1_RTU1_ADDR    SDL_PRU_ICSSG1_PR1_RTU1_PR1_RTU1_IRAM_RAM_BASE
#define ICSSG1_TX0_ADDR     SDL_PRU_ICSSG1_PR1_PDSP_TX0_IRAM_RAM_BASE
#define ICSSG1_TX1_ADDR     SDL_PRU_ICSSG1_PR1_PDSP_TX1_IRAM_RAM_BASE

#define KB  1024U

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

#if defined (M4F_CORE)
/* Initialization structure for MCU ESM instance */
static SDL_ESM_config ECC_Test_esmInitConfig_MCU =
{
    .esmErrorConfig = {0u, 3u}, /* Self test error config */
    /* MCU ESM events 0-2 enabled, all HIGH priority */
    .enableBitmap = {0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                    },
    .priorityBitmap = {0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                      },
    .errorpinBitmap = {0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                      },
};

static SDL_ESM_config ECC_Test_esmInitConfig_MAIN =
{
    .esmErrorConfig = {1u, 8u}, /* Self test error config */
    /* ICSSG0 SEC=event 13 (word[0] bit 13), DED=event 76 (word[2] bit 12)
     * ICSSG1 SEC=event 14 (word[0] bit 14), DED=event 77 (word[2] bit 13) */
    .enableBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u,
                    },
    .priorityBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u,
                      },
    .errorpinBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u,
                      },
};
#endif

#if defined (R5F_CORE)
static SDL_ESM_config ECC_Test_esmInitConfig_MAIN =
{
    .esmErrorConfig = {1u, 8u}, /* Self test error config */
    /* ICSSG0 SEC=event 13 (word[0] bit 13), DED=event 76 (word[2] bit 12)
     * ICSSG1 SEC=event 14 (word[0] bit 14), DED=event 77 (word[2] bit 13) */
    .enableBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                     0x00000000u,
                    },
    .priorityBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u,
                      },
    .errorpinBitmap = {0x00006000u, 0x00000000u, 0x00003000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
                       0x00000000u,
                      },
};

extern uint32_t testCounter;

/* This is the list of exception handle and the parameters */
const SDL_R5ExptnHandlers ECC_Test_R5ExptnHandlers =
{
    .udefExptnHandler = &SDL_EXCEPTION_undefInstructionExptnHandler,
    .swiExptnHandler = &SDL_EXCEPTION_swIntrExptnHandler,
    .pabtExptnHandler = &SDL_EXCEPTION_prefetchAbortExptnHandler,
    .dabtExptnHandler = &SDL_EXCEPTION_dataAbortExptnHandler,
    .irqExptnHandler = &SDL_EXCEPTION_irqExptnHandler,
    .fiqExptnHandler = &SDL_EXCEPTION_fiqExptnHandler,
    .udefExptnHandlerArgs = ((void *)0u),
    .swiExptnHandlerArgs = ((void *)0u),
    .pabtExptnHandlerArgs = ((void *)0u),
    .dabtExptnHandlerArgs = ((void *)0u),
    .irqExptnHandlerArgs = ((void *)0u),
};

void ECC_Test_undefInstructionExptnCallback(void)
{
    DebugP_log("\r\nUndefined Instruction exception");
}

void ECC_Test_swIntrExptnCallback(void)
{
    DebugP_log("\r\nSoftware interrupt exception");
}

void ECC_Test_prefetchAbortExptnCallback(void)
{
    DebugP_log("\r\nPrefetch Abort exception");
}

void ECC_Test_dataAbortExptnCallback(void)
{
    DebugP_log("\r\nData Abort exception");
}

void ECC_Test_irqExptnCallback(void)
{
    DebugP_log("\r\nIrq exception");
}

void ECC_Test_fiqExptnCallback(void)
{
    DebugP_log("\r\nFiq exception");
}

void ECC_Test_exceptionInit(void)
{
    SDL_EXCEPTION_CallbackFunctions_t exceptionCallbackFunctions =
            {
             .udefExptnCallback = ECC_Test_undefInstructionExptnCallback,
             .swiExptnCallback = ECC_Test_swIntrExptnCallback,
             .pabtExptnCallback = ECC_Test_prefetchAbortExptnCallback,
             .dabtExptnCallback = ECC_Test_dataAbortExptnCallback,
             .irqExptnCallback = ECC_Test_irqExptnCallback,
             .fiqExptnCallback = ECC_Test_fiqExptnCallback,
            };

    /* Initialize SDL exception handler */
    SDL_EXCEPTION_init(&exceptionCallbackFunctions);
    /* Register SDL exception handler */
    Intc_RegisterExptnHandlers(&ECC_Test_R5ExptnHandlers);

    return;
}
#endif

extern int32_t SDL_ESM_applicationCallbackFunction(SDL_ESM_Inst esmInstType,
                                                   SDL_ESM_IntType esmIntType,
                                                   uint32_t grpChannel,
                                                   uint32_t index,
                                                   uint32_t intSrc,
                                                   void *arg);

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/* ---- Table-driven memory init ------------------------------------------- */

typedef struct
{
    SDL_ECC_MemType    memType;
    SDL_ECC_MemSubType ramId;
    uint32_t           physAddr; /* used by M4F for SDL_DPL_addrTranslate */
    uint32_t           size;
    const char        *name;
} ECC_MemInitEntry_t;

#define ECC_MEM_INIT_TABLE_SIZE (18u)

static const ECC_MemInitEntry_t gECC_MemInitTable[ECC_MEM_INIT_TABLE_SIZE] =
{
    /* ICSSG0 */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)ICSSG0_DRAM0_ADDR,  SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_SIZE,    "ICSSG0 DRAM0"  },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)ICSSG0_DRAM1_ADDR,  SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_SIZE,    "ICSSG0 DRAM1"  },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_PDSP0_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_SIZE,  "ICSSG0 PDSP0"  },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_PDSP1_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_SIZE,  "ICSSG0 PDSP1"  },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_RAM_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_SIZE,              "ICSSG0 RAM"    },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_RTU0_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_SIZE, "ICSSG0 RTU0" },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_RTU1_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_SIZE, "ICSSG0 RTU1" },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_TX0_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_SIZE, "ICSSG0 TX0"  },
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG0_TX1_ADDR, SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_SIZE, "ICSSG0 TX1"  },
    /* ICSSG1 */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)ICSSG1_DRAM0_ADDR,  SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_SIZE,    "ICSSG1 DRAM0"  },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)ICSSG1_DRAM1_ADDR,  SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_SIZE,    "ICSSG1 DRAM1"  },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_PDSP0_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_SIZE,  "ICSSG1 PDSP0"  },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_PDSP1_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_SIZE,  "ICSSG1 PDSP1"  },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_RAM_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_SIZE,              "ICSSG1 RAM"    },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_RTU0_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_SIZE, "ICSSG1 RTU0" },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_RTU1_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_SIZE, "ICSSG1 RTU1" },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_TX0_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_SIZE, "ICSSG1 TX0"  },
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)ICSSG1_TX1_ADDR, SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_SIZE, "ICSSG1 TX1"  },
};

/* ---- Table-driven inject tests ------------------------------------------ */

typedef struct
{
    SDL_ECC_MemType    memType;
    SDL_ECC_MemSubType subType;
    uint32_t           errAddr;      /* absolute address for pErrMem */
    uint32_t           injectType;   /* SDL_ECC_InjectErrorType */
    uint32_t           flipBitMask;
    const char        *desc;
} ECC_InjectTestCase_t;

/*
 * 36 UCs total (UC-1 .. UC-36).
 * Order: ICSSG0 DRAM0 (DED, SEC), ICSSG0 DRAM1 (DED, SEC),
 *        ICSSG1 DRAM0 (DED, SEC), ICSSG1 DRAM1 (DED, SEC),
 *        then ICSSG0 PDSP0..TX1 (DED, SEC each),
 *        then ICSSG1 PDSP0..TX1 (DED, SEC each).
 */
#define ECC_INJECT_TEST_TABLE_SIZE (36u)

static const ECC_InjectTestCase_t gECC_InjectTestTable[ECC_INJECT_TEST_TABLE_SIZE] =
{
    /*
     * The errAddr parameter (ie, address where error is injected) of below entries
     * is given a random offset of 0-5KB from the starting address of the memory
     */
    /* UC-1:  ICSSG0 DRAM0 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)(ICSSG0_DRAM0_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 DRAM0 DED" },
    /* UC-2:  ICSSG0 DRAM0 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)(ICSSG0_DRAM0_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 DRAM0 SEC" },
    /* UC-3:  ICSSG0 DRAM1 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)(ICSSG0_DRAM1_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 DRAM1 DED" },
    /* UC-4:  ICSSG0 DRAM1 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)(ICSSG0_DRAM1_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 DRAM1 SEC" },
    /* UC-5:  ICSSG1 DRAM0 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)(ICSSG1_DRAM0_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 DRAM0 DED" },
    /* UC-6:  ICSSG1 DRAM0 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
      (uint32_t)(ICSSG1_DRAM0_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 DRAM0 SEC" },
    /* UC-7:  ICSSG1 DRAM1 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)(ICSSG1_DRAM1_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 DRAM1 DED" },
    /* UC-8:  ICSSG1 DRAM1 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
      (uint32_t)(ICSSG1_DRAM1_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 DRAM1 SEC" },
    /* UC-9: ICSSG0 PDSP0 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_PDSP0_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 PDSP0 DED" },
    /* UC-10: ICSSG0 PDSP0 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_PDSP0_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 PDSP0 SEC" },
    /* UC-11: ICSSG0 PDSP1 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_PDSP1_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 PDSP1 DED" },
    /* UC-12: ICSSG0 PDSP1 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_PDSP1_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 PDSP1 SEC" },
    /* UC-13: ICSSG0 RAM DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RAM_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 RAM DED" },
    /* UC-14: ICSSG0 RAM SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RAM_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 RAM SEC" },
    /* UC-15: ICSSG0 RTU0 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RTU0_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 RTU0 DED" },
    /* UC-16: ICSSG0 RTU0 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RTU0_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 RTU0 SEC" },
    /* UC-17: ICSSG0 RTU1 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RTU1_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 RTU1 DED" },
    /* UC-18: ICSSG0 RTU1 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_RTU1_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 RTU1 SEC" },
    /* UC-19: ICSSG0 TX0 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_TX0_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 TX0 DED" },
    /* UC-20: ICSSG0 TX0 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_TX0_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 TX0 SEC" },
    /* UC-21: ICSSG0 TX1 DED */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_TX1_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG0 TX1 DED" },
    /* UC-22: ICSSG0 TX1 SEC */
    { SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG0_TX1_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG0 TX1 SEC" },
    /* UC-23: ICSSG1 PDSP0 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_PDSP0_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 PDSP0 DED" },
    /* UC-24: ICSSG1 PDSP0 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_PDSP0_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 PDSP0 SEC" },
    /* UC-25: ICSSG1 PDSP1 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_PDSP1_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 PDSP1 DED" },
    /* UC-26: ICSSG1 PDSP1 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_PDSP1_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 PDSP1 SEC" },
    /* UC-27: ICSSG1 RAM DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RAM_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 RAM DED" },
    /* UC-28: ICSSG1 RAM SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RAM_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 RAM SEC" },
    /* UC-29: ICSSG1 RTU0 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RTU0_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 RTU0 DED" },
    /* UC-30: ICSSG1 RTU0 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RTU0_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 RTU0 SEC" },
    /* UC-31: ICSSG1 RTU1 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RTU1_ADDR + 0*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 RTU1 DED" },
    /* UC-32: ICSSG1 RTU1 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_RTU1_ADDR + 1*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 RTU1 SEC" },
    /* UC-33: ICSSG1 TX0 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_TX0_ADDR + 2*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 TX0 DED" },
    /* UC-34: ICSSG1 TX0 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_TX0_ADDR + 3*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 TX0 SEC" },
    /* UC-35: ICSSG1 TX1 DED */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_TX1_ADDR + 4*KB), SDL_INJECT_ECC_ERROR_FORCING_2BIT_ONCE,
      DOUBLE_ERROR_FLIPMASK, "ICSSG1 TX1 DED" },
    /* UC-36: ICSSG1 TX1 SEC */
    { SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
      SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
      (uint32_t)(ICSSG1_TX1_ADDR + 5*KB), SDL_INJECT_ECC_ERROR_FORCING_1BIT_ONCE,
      SINGLE_ERROR_FLIPMASK, "ICSSG1 TX1 SEC" },
};

/* ---- SDL_ECC_init configs (subtype lists required by SDL_ECC_init) ------- */

#define MAIN_ICSSG0_BORG_TEST_MEM_SECTIONS (9u)
#define MAIN_ICSSG1_BORG_TEST_MEM_SECTIONS (9u)

static SDL_ECC_MemSubType ECC_Test_ICSSG0_subMemTypeList[MAIN_ICSSG0_BORG_TEST_MEM_SECTIONS] =
{
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
};

static SDL_ECC_MemSubType ECC_Test_ICSSG1_subMemTypeList[MAIN_ICSSG1_BORG_TEST_MEM_SECTIONS] =
{
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM0_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_DRAM1_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP0_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP1_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_RAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU0_PDSP_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_RTU1_PDSP_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX0_IRAM_ECC_RAM_ID,
    SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR_ICSS_G_CORE_PR1_PDSP_TX1_IRAM_ECC_RAM_ID,
};

static SDL_ECC_InitConfig_t ECC_Test_ICSSG0ECCInitConfig =
{
    .numRams = MAIN_ICSSG0_BORG_TEST_MEM_SECTIONS,
    .pMemSubTypeList = &(ECC_Test_ICSSG0_subMemTypeList[0]),
};

static SDL_ECC_InitConfig_t ECC_Test_ICSSG1ECCInitConfig =
{
    .numRams = MAIN_ICSSG1_BORG_TEST_MEM_SECTIONS,
    .pMemSubTypeList = &(ECC_Test_ICSSG1_subMemTypeList[0]),
};

/* ---- Init / inject helpers ---------------------------------------------- */

static uint32_t gArg;

int32_t ECC_Test_EsmInitHandlerInit(SDL_ESM_Inst esmInstType);
int32_t ECC_Example_init(void);

/*********************************************************************
 * @fn      ECC_Example_init
 *
 * @param   None
 *
 * @return  0 : Success; < 0 for failures
 */
int32_t ECC_Example_init(void)
{
    int32_t retValue = 0;
    void *ptr = (void *)&gArg;
    SDL_ErrType_t result;
    uint32_t i;

#if defined (R5F_CORE)
    /* Initialise exception handler */
    ECC_Test_exceptionInit();
    DebugP_log("\r\nECC_Test_init: Exception init complete ");
#endif

    if (retValue == 0)
    {
#if defined (R5F_CORE)
        /* Initialise ECC memory for all RAM IDs in both aggregators */
        for (i = 0u; ((i < ECC_MEM_INIT_TABLE_SIZE) && (retValue == 0)); i++)
        {
            result = SDL_ECC_initMemory(gECC_MemInitTable[i].memType,
                                        gECC_MemInitTable[i].ramId);
            if (result != SDL_PASS)
            {
                DebugP_log("\r\nECC_Test_init: Error initializing %s ECC Memory: result = %d",
                           gECC_MemInitTable[i].name, result);
                retValue = -1;
            }
            else
            {
                DebugP_log("\r\nECC_Test_init: Initialize of %s ECC Memory is complete ",
                           gECC_MemInitTable[i].name);
            }
        }
#endif
#if defined (M4F_CORE)
        /*
         * SDL_ECC_initMemory uses memStartAddr directly without address translation.
         * From M4F, first translate each RAM physical address and then scrub manually,
         * mirroring what SDL_ECC_memoryRefresh does internally in case of R5F core.
         */
        for (i = 0u; ((i < ECC_MEM_INIT_TABLE_SIZE) && (retValue == 0)); i++)
        {
            uint32_t j;
            volatile uint32_t tmpVal;
            uint32_t *translatedAddr = (uint32_t *)SDL_DPL_addrTranslate((uint64_t)gECC_MemInitTable[i].physAddr, gECC_MemInitTable[i].size);
            if (translatedAddr == (uint32_t *)(-1))
            {
                DebugP_log("\r\nECC_Test_init: Address translation failed for %s",
                           gECC_MemInitTable[i].name);
                retValue = -1;
            }
            else
            {
                for (j = 0u; j < (gECC_MemInitTable[i].size >> 2u); j++)
                {
                    tmpVal = translatedAddr[j];
                    translatedAddr[j] = tmpVal;
                }
                DebugP_log("\r\nECC_Test_init: Initialize of %s ECC Memory is complete",
                           gECC_MemInitTable[i].name);
            }
        }
#endif
    }

    if (retValue == SDL_APP_TEST_PASS)
    {
        /* Initialize MAIN ESM module */
        result = SDL_ESM_init(SDL_ESM_INST_MAIN_ESM0, &ECC_Test_esmInitConfig_MAIN,
                              SDL_ESM_applicationCallbackFunction, ptr);
        if (result != SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\nECC_Example_init: Error initializing MAIN ESM: result = %d", result);
            retValue = SDL_APP_TEST_FAILED;
        }
        else
        {
            DebugP_log("\r\nECC_Example_init: Init MAIN ESM complete");
        }
    }

#if defined(M4F_CORE)
    /*
     * In AM64x, the interrupts corresponding to PRU ICSSG0/1 ECC aggregators, are only routed to Main ESM instance.
     * However, the Main ESM instance does not have an interrupt line to the M4F core. So, M4F core cannot use the
     * Main ESM instance directly - it can only use the MCU ESM instance. Instead, we make use of an interrupt line
     * between Main ESM and MCU ESM. This notifies the MCU ESM whenever there is an error in the Main ESM instance.
     * So, when we trigger an ECC error, we first get an error in the Main ESM. Then, the MCU ESM is notified of an
     * event in the Main ESM (ECC error). This notification to MCU ESM is used to trigger an event which can be used
     * by our M4F application to detect and handle the error. Internal SDL APIs then find out the true source of the
     * event (within the Main ESM) and clear it accordingly after the call to the configured ESM handler.
     */
    if (retValue == SDL_APP_TEST_PASS)
    {
        /* Initialize MCU ESM module */
        result = SDL_ESM_init(SDL_ESM_INST_MCU_ESM0, &ECC_Test_esmInitConfig_MCU,
                              SDL_ESM_applicationCallbackFunction, ptr);
        if (result != SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\nECC_Example_init: Error initializing MCU ESM: result = %d", result);
            retValue = SDL_APP_TEST_FAILED;
        }
        else
        {
            DebugP_log("\r\nECC_Example_init: Init MCU ESM complete");
        }
    }
#endif

    if (retValue == SDL_APP_TEST_PASS)
    {
        /* Initialize ECC for ICSSG0 */
        result = SDL_ECC_init(SDL_PRU_ICSSG0_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
                              &ECC_Test_ICSSG0ECCInitConfig);
        if (result != SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\nSDTF_init: Error initializing ICSSG0 ECC: result = %d", result);
            retValue = SDL_APP_TEST_FAILED;
        }
        else
        {
            DebugP_log("\r\nSDTF_init: ICSSG0 ECC Init complete");
        }
    }

    if (retValue == SDL_APP_TEST_PASS)
    {
        /* Initialize ECC for ICSSG1 */
        result = SDL_ECC_init(SDL_PRU_ICSSG1_ICSS_G_16FF_CORE_BORG_ECC_AGGR,
                              &ECC_Test_ICSSG1ECCInitConfig);
        if (result != SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\nSDTF_init: Error initializing ICSSG1 ECC: result = %d", result);
            retValue = SDL_APP_TEST_FAILED;
        }
        else
        {
            DebugP_log("\r\nSDTF_init: ICSSG1 ECC Init complete");
        }
    }

    return retValue;
}

/*
 * Generic inject function used by all 36 UCs.
 * Arms the ECC error via SDL, then reads the inject address to trigger it.
 */
static int32_t runECCInjectTest(const ECC_InjectTestCase_t *tc)
{
    SDL_ErrType_t result;
    int32_t retVal = SDL_APP_TEST_PASS;
    SDL_ECC_InjectErrorConfig_t injectErrorConfig;

    memset(&injectErrorConfig, 0, sizeof(injectErrorConfig));
    injectErrorConfig.pErrMem    = (uint32_t *)tc->errAddr;
    injectErrorConfig.flipBitMask = tc->flipBitMask;

    result = SDL_ECC_injectError(tc->memType, tc->subType,
                                 (SDL_ECC_InjectErrorType)tc->injectType,
                                 &injectErrorConfig);

    /* WRAPPER-type RAMs require an explicit memory read to trigger the armed ECC error */
    if (result == SDL_APP_TEST_PASS)
    {
        volatile uint32_t triggerVal;
#if defined (M4F_CORE)
        volatile uint32_t *triggerAddr = (volatile uint32_t *)SDL_DPL_addrTranslate((uint64_t)injectErrorConfig.pErrMem, sizeof(uint32_t));
#else
        volatile uint32_t *triggerAddr = (volatile uint32_t *)injectErrorConfig.pErrMem;
#endif
        if (triggerAddr != (uint32_t *)(-1))
        {
            triggerVal = *triggerAddr;
            (void)triggerVal;
        }
        else
        {
            DebugP_log("\r\n Address translation failed for %s", tc->desc);
            result = SDL_APP_TEST_FAILED;
        }
    }

    if (result != SDL_APP_TEST_PASS)
    {
        DebugP_log("\r\n %s: test failed", tc->desc);
        retVal = SDL_APP_TEST_FAILED;
    }

    return retVal;
}

static int32_t ECC_sdlFuncTest(void)
{
    int32_t result;
    int32_t retVal = SDL_APP_TEST_PASS;
    uint32_t ucIdx;
    uint32_t timeOutCnt = 0;

    DebugP_log("\r\n ESM Safety Example tests: starting");

    for (ucIdx = 0u;
         ucIdx < ECC_INJECT_TEST_TABLE_SIZE && retVal == SDL_APP_TEST_PASS;
         ucIdx++)
    {
        DebugP_log("\r\n %s: starting", gECC_InjectTestTable[ucIdx].desc);

        result = runECCInjectTest(&gECC_InjectTestTable[ucIdx]);

        if (result == SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\n Waiting for ESM Interrupt");
            do
            {
                /* SDL_DPL_delay will add a delay for DELAY number of us */
                SDL_DPL_delay(DELAY);
                timeOutCnt++;
                if (timeOutCnt > MAX_ESM_TIMEOUT)
                {
                    result = SDL_EFAIL;
                    break;
                }
            } while (esmError == false);
        }

        if (result == SDL_APP_TEST_PASS)
        {
            DebugP_log("\r\nUC-%u: IntSrc: %d, ECC MemType: %d, ECC RamId: %d, ECC IntrSrc: %d",
                       ucIdx + 1u, gMainEsmIntSrc, gEccMemType, gEccRamId, gEccIntrSrc);
            esmError = false;
            timeOutCnt = 0;
        }

        if (result != SDL_APP_TEST_PASS)
        {
            retVal = SDL_APP_TEST_FAILED;
            DebugP_log("\r\nESM_ECC_Example_run: UC-%u has failed...", ucIdx + 1u);
        }
    }

    if (retVal == SDL_APP_TEST_PASS)
    {
        DebugP_log("\r\n ECC Safety Example tests: success");
    }
    else
    {
        DebugP_log("\r\n ECC Safety Example tests: failed");
    }

    return retVal;
}

/* ECC Function module test */
int32_t ECC_funcTest(void)
{
    int32_t testResult;

    testResult = ECC_Example_init();

    if (testResult != SDL_APP_TEST_PASS)
    {
        DebugP_log("\r\n ECC Safety Example tests: unsuccessful");
        return SDL_EFAIL;
    }

    testResult = ECC_sdlFuncTest();

    return (testResult);
}

/* Nothing past this point */
