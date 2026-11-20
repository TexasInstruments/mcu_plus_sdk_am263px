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
 *  \file     ecc_main.c
 *
 * \brief This file demonstrates using the Error Correcting Code Module (ECC),
 *         utilizing the ECC and ESM Software Diagnostic Reference (SDL) functions.
 *
 *  \details  ESM Safety Example module tests
 **/

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */
#include "ecc_main.h"
#include <drivers/sciclient.h>
#include <sdl/dpl/sdl_dpl.h>
#include <kernel/dpl/TimerP.h>
#include <kernel/dpl/DebugP.h>
#include <sdl/sdl_ecc.h>
#include <dpl_interface.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
/* ========================================================================== */
/*                                Macros                                      */
/* ========================================================================== */

#define AUX_NUM_DEVICES 2

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */
volatile bool esmError = false;
volatile SDL_ECC_MemType gEccMemType = 0u;
volatile SDL_Ecc_AggrIntrSrc gEccIntrSrc = SDL_ECC_AGGR_INTR_SRC_NONE;
volatile uint32_t gMainEsmIntSrc = 0u;
volatile SDL_ECC_MemSubType gEccRamId = 0u;
/* ========================================================================== */
/*                 Internal Function Declarations                             */
/* ========================================================================== */

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/* ========================================================================== */
/*                 Internal Function Definitions                              */
/* ========================================================================== */

void setUp(void)
{
    /* Do nothing */
}

void tearDown(void)
{
    /* Do nothing */
}



int32_t SDL_ESM_applicationCallbackFunction(SDL_ESM_Inst esmInst,
                                            SDL_ESM_IntType esmIntrType,
                                            uint32_t grpChannel,
                                            uint32_t index,
                                            uint32_t intSrc,
                                            void *arg)
{

    SDL_ECC_MemType eccmemtype;
    SDL_Ecc_AggrIntrSrc eccIntrSrc;
    SDL_ECC_ErrorInfo_t eccErrorInfo;
    int32_t retVal;
    retVal = SDL_ECC_getESMErrorInfo(esmInst, intSrc, &eccmemtype, &eccIntrSrc);

    /*
     * In M4F, the callback will fire twice - once for the MAIN ESM ECC interrupt and once for the interrupt routed
     * to MCU ESM. In the latter case, eccmemtype and eccIntrSrc will have garbage values because this is not an ECC
     * interrupt. Hence, we check for pass value of above call and only populate globals if it is an ECC interrupt
     */
    if (retVal == SDL_PASS)
    {
        gEccMemType    = eccmemtype;
        gEccIntrSrc    = eccIntrSrc;
        gMainEsmIntSrc = intSrc;
    }

    /* Any additional customer specific actions can be added here */
    retVal = SDL_ECC_getErrorInfo(eccmemtype, eccIntrSrc, &eccErrorInfo);

    if (retVal == SDL_PASS)
    {
        gEccRamId = eccErrorInfo.memSubType;
    }

    if (eccErrorInfo.injectBitErrCnt != 0)
    {
        SDL_ECC_clearNIntrPending(eccmemtype, eccErrorInfo.memSubType, eccIntrSrc, SDL_ECC_AGGR_ERROR_SUBTYPE_INJECT, eccErrorInfo.injectBitErrCnt);
    }
    else
    {
        SDL_ECC_clearNIntrPending(eccmemtype, eccErrorInfo.memSubType, eccIntrSrc, SDL_ECC_AGGR_ERROR_SUBTYPE_NORMAL, eccErrorInfo.bitErrCnt);
    }

    retVal = SDL_ECC_ackIntr(eccmemtype, eccIntrSrc);
    esmError = true;
    return retVal;
}

/* SDL_ECC_applicationCallbackFunction is expected to be defined by the application.
 * It is required by the SDL ECC module. It is called by the SDL ECC module to notify
 * the application of certain ECC errors that are reported as Exception events.
 * Note, however, that it is not executed in this example, and so is a stub */
void SDL_ECC_applicationCallbackFunction(SDL_ECC_MemType eccMemType,
                                         uint32_t errorSrc,
                                         uint32_t address,
                                         uint32_t ramId,
                                         uint64_t bitErrorOffset,
                                         uint32_t bitErrorGroup)
{
    /* Any additional customer specific actions can be added here */
}

uint32_t aux_devices[AUX_NUM_DEVICES] =
{
  TISCI_DEV_PRU_ICSSG1,
  TISCI_DEV_PRU_ICSSG0,
};

static int32_t sdlApp_dplInit(void)
{
    SDL_ErrType_t ret = SDL_PASS;
    int32_t status = SDL_PASS;

    ret = SDL_TEST_dplInit();
    if (ret != SDL_PASS)
    {
        DebugP_log("\rError: Init Failed\r\n");
    }

    for (int i = 0; i < AUX_NUM_DEVICES; i++)
    {
        /* Power up RTI */
        status = Sciclient_pmSetModuleState(aux_devices[i],
                                            TISCI_MSG_VALUE_DEVICE_SW_STATE_ON,
                                            TISCI_MSG_FLAG_AOP,
                                            SystemP_WAIT_FOREVER);

        if (status != SDL_PASS)
        {
            DebugP_log("\r   Sciclient_pmSetModuleState 0x%x ...FAILED: retValue %d\r\n",
                        aux_devices[i], status);
            ret = SDL_EFAIL;
        }
    }

    return ret;
}

void ECC_Example_app(void *args)
{
    int32_t    testResult;
    /* Open drivers to open the UART driver for console */
    Drivers_open();
    Board_driversOpen();
  	testResult = ECC_funcTest();
  	if (testResult == SDL_PASS)
    {
        DebugP_log("\r\nAll tests have passed. \r\n");
    }
    else
    {
        DebugP_log("\r\nSome tests have failed. \r\n");
        Board_driversClose();
        Drivers_close();
    }
}

void ecc_app_runner(void)
{
    ECC_Example_app(ECC_Example_app);
}

void ecc_main(void *args)
{
    sdlApp_dplInit();

    DebugP_log("\r\nECC Example Application\r\n");
    (void)ecc_app_runner();
    while (true);
}

/* Nothing past this point */
