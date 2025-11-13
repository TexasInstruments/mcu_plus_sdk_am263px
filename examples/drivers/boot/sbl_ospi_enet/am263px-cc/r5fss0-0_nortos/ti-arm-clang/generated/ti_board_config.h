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

/*
 * Auto generated file 
 */

#ifndef TI_BOARD_CONFIG_H
#define TI_BOARD_CONFIG_H

#include "ti_drivers_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void Board_init(void);
void Board_deinit(void);

/*
 * EEPROM
 */
#include <board/eeprom.h>

/* EEPROM Instance Macros */
#define CONFIG_EEPROM0 (0U)
#define CONFIG_EEPROM_NUM_INSTANCES (1U)

/*
 * FLASH
 */
#include <board/flash.h>

/* FLASH Instance Macros */
#define CONFIG_FLASH0 (0U)
#define CONFIG_FLASH_NUM_INSTANCES (1U)


/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <enet_board.h>

/* ========================================================================== */
/*                                 Macros                                     */
/* ========================================================================== */
/*!
 * \brief Define indicating application has enabled custom board configuration.
 *
 * Custom boards are board not supported by MCU+SDK out of the box
 */
#define ENETBOARD_SYSCFG_CUSTOM_BOARD                   (0)

/*!
 * \brief AM263PX Common Processor Board (CPB) id.
 */
#define ENETBOARD_CPB_ID                      (ENETPHY_BIT(0U))

/*!
 * \brief AM263PX dummy board, used for MAC loopback.
 */
#define ENETBOARD_LOOPBACK_ID                 (ENETPHY_BIT(1U))

/*!
 * \brief AM263PX EVM base configuration: CPB and loopback.
 */
#define ENETBOARD_AM263PX_EVM                  (ENETBOARD_CPB_ID | \
                                               ENETBOARD_LOOPBACK_ID)

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

uint32_t EnetBoard_getId(void);

void EnetBoard_getMiiConfig(EnetMacPort_Interface *mii);


#ifdef __cplusplus
}
#endif

#endif /* TI_BOARD_CONFIG_H */
