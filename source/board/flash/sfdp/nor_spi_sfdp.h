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
 *  \ingroup BOARD_MODULE
 *  \defgroup DRV_SFDP_API_MODULE SFDP API for single pin mode
 *
 *   These APIs are used to parse various SFDP parameter tables read from a NOR SPI flash
 *
 *  @{
 */

/**
 *  \file nor_spi_sfdp.h
 *
 *  \brief SFDP API/interface file.
 */

#ifndef NOR_SPI_SFDP_H_
#define NOR_SPI_SFDP_H_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <stdint.h>
#include <kernel/dpl/SystemP.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/SemaphoreP.h>
#include <drivers/hw_include/csl_types.h>
#include <board/flash/flash_config.h> /* Needed for defines */

#ifdef __cplusplus
extern "C" {
#endif

/* SFDP Signature DWORD */
#define NOR_SPI_SFDP_SIGNATURE     (0x50444653U)

/* SFDP Revisions */
#define NOR_SPI_SFDP_JESD216_MAJOR     (1U)
#define NOR_SPI_SFDP_JESD216_MINOR     (0U)
#define NOR_SPI_SFDP_JESD216A_MINOR    (5U)
#define NOR_SPI_SFDP_JESD216B_MINOR    (6U)
#define NOR_SPI_SFDP_JESD216C_MINOR    (7U)
#define NOR_SPI_SFDP_JESD216D_MINOR    (8U)
#define NOR_SPI_SFDP_JESD216E_MINOR    (9U)
#define NOR_SPI_SFDP_JESD216F_MINOR    (0xAU)
#define NOR_SPI_SFDP_JESD216G_MINOR    (0xBU)
#define NOR_SPI_SFDP_JESD216H_MINOR    (0xCU)

/* SFDP Maximum numbers of parameter headers (only JEDEC ones included)*/
/* Increased from 13 to 18 to accommodate tables added in JESD216D through H:
 *   +1 Secure Packet Table (FF8Eh, JESD216D)
 *   +1 GRAM Table (FF0Fh, JESD216G)
 *   +1 SPI Safety Extensions / CRC Table (FF90h, JESD216G)
 *   +1 SFDP CRC-32 Table (FF11h, JESD216H)
 *   +1 ECC Table (FF12h, JESD216H)
 */
#define NOR_SPI_SFDP_NPH_MAX       (18U)

/* SFDP offsets */
#define NOR_SPI_SFDP_HEADER_START_OFFSET        (0x00U)
#define NOR_SPI_SFDP_FIRST_PARAM_HEADER_OFFSET  (0x08U)
#define NOR_SPI_SFDP_SECOND_PARAM_HEADER_OFFSET (0x10U)

/* SFDP Parameter table IDs supported by JEDEC*/
#define NOR_SPI_SFDP_BASIC_PARAM_TABLE_ID            (0xFF00)
#define NOR_SPI_SFDP_SECTOR_MAP_TABLE_ID             (0xFF81)
#define NOR_SPI_SFDP_RPMC_TABLE_ID                   (0xFF03)
#define NOR_SPI_SFDP_4BYTE_ADDR_INSTR_TABLE_ID       (0xFF84)
#define NOR_SPI_SFDP_PROFILE_TABLE_ID                (0xFF05)
#define NOR_SPI_SFDP_PROFILE_2_TABLE_ID              (0xFF06)
#define NOR_SPI_SFDP_SCCR_TABLE_ID                   (0xFF87)
#define NOR_SPI_SFDP_SCCR_MULTISPI_OFFSETS_TABLE_ID  (0xFF88)
#define NOR_SPI_SFDP_SCCR_PROFILE_2_TABLE_ID         (0xFF09)
#define NOR_SPI_SFDP_OCTAL_CMD_SEQ_TABLE_ID          (0xFF0A)
#define NOR_SPI_SFDP_LONG_LATENCY_NVM_MSP_TABLE_ID   (0xFF8B)
#define NOR_SPI_SFDP_QUAD_IO_WITH_DS_TABLE_ID        (0xFF0C)
#define NOR_SPI_SFDP_QUAD_CMD_SEQ_TABLE_ID           (0xFF8D)
#define NOR_SPI_SFDP_SECURE_PACKET_TABLE_ID          (0xFF8E) /* Added in JESD216D */
#define NOR_SPI_SFDP_GRAM_TABLE_ID                   (0xFF0F) /* Added in JESD216G: Generic Register and Command Map */
#define NOR_SPI_SFDP_SPI_SAFETY_EXT_TABLE_ID         (0xFF90) /* Added in JESD216G: SPI Safety Extensions / CRC */
#define NOR_SPI_SFDP_CRC32_TABLE_ID                  (0xFF11) /* Added in JESD216H: SFDP CRC-32 */
#define NOR_SPI_SFDP_ECC_TABLE_ID                    (0xFF12) /* Added in JESD216H: ECC */

/* SFDP Number of DWORDS in BFPT for different revisions */
#define NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216         (9)
#define NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216B        (16)
/* JESD216C/D/E added DWORDs 17-20 (octal read, byte order, OE requirements, max speed) */
#define NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216C        (20)
/* JESD216F/G/H added DWORDs 21-23 (DTR half-duplex fast read: 1S-1D-1D, 1S-2D-2D, 1S-4D-4D, 4S-4D-4D) */
#define NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216F        (23)

/* SFDP read command address type in 8D */
#define NOR_SPI_SFDP_OCTAL_READ_ADDR_MSB_0           (0)
#define NOR_SPI_SFDP_OCTAL_READ_ADDR_LSB_0           (1)

#define NOR_SPI_CMD_EXT_TYPE_REPEAT  (0x00U)
#define NOR_SPI_CMD_EXT_TYPE_INVERSE (0x01U)
#define NOR_SPI_CMD_EXT_TYPE_NONE    (0x02U)

typedef struct NorSpi_SfdpMainHeader_s
{
    uint32_t signature;
    /*  SFDP Signature. "SFDP" in ASCII */

    uint8_t minorRev;
    /* SFDP minor revision number */

    uint8_t majorRev;
    /* SFDP minor revision number */

    uint8_t numParamHeaders;
    /* Number of param headers. Zero-based. 0 means 1 param header */

    uint8_t accessProtocol;
    /* SFDP Access Protocol */

} NorSpi_SfdpMainHeader;

typedef struct NorSpi_SfdpParamHeader_s
{
    uint8_t paramIdLsb;
    /* LSB of parameter ID */

    uint8_t paramTableMinorRev;
    /* Minor revision number of parameter table */

    uint8_t paramTableMajorRev;
    /* Major revision number of parameter table */

    uint8_t paramTableLength;
    /* Number of DWORDs in the parameter table. One based field. So 1 means 1 DWORD */

    uint8_t paramTablePtr[3];
    /* Start of parameter header's corresponding table. This address should be DWORD aligned */

    uint8_t paramIdMsb;
    /* MSB of parameter ID */

} NorSpi_SfdpParamHeader;

typedef struct NorSpi_SfdpBasicFlashParamTable_s
{
    /* 1st DWORD - has info about DTR support, number of address bytes and Quad and Dual Fast read support */
    /* 2nd DWORD - flash size. If b31 is 0, 30:0 is size in bits. If b31 is 1, 30:0 is N, where 2^N is size in bits */
    /* 3rd DWORD - 1-1-4 and 1-4-4 fast read wait states, mode bit clocks and instruction */
    /* 4th DWORD - 1-1-2 and 1-2-2 fast read wait states, mode bit clocks and instruction */
    /* 5th DWORD - has info on whether 2-2-2 and 4-4-4 mode are supported */
    /* 6th DWORD - 2-2-2 fast read wait states, mode bit clocks and instruction */
    /* 7th DWORD - 4-4-4 fast read wait states, mode bit clocks and instruction */
    /* 8th DWORD - erase types 1 and 2 : Sizes and their instructions */
    /* 9th DWORD - erase types 3 and 4 : Sizes and their instructions */
    /* 10th DWORD - erase times for all 4 types of erases */
    /* 11th DWORD - pageSize, chip erase time, page program time, byte program time */
    /* 12th DWORD - suspend/resume support, intervals, latency etc */
    /* 13th DWORD - suspend/resume instructions */
    /* 14th DWORD - Deep power down support, instructions, status polling supported modes */
    /* 15th DWORD - hold and reset details, quad enable requirements, 0-4-4 and 4-4-4 mode enabling */
    /* 16th DWORD - 4 byte addressing mode entry/exit, support, soft reset sequences, volatile and non-volatile status register support */
    /* 17th DWORD - 1-1-8 and 1-8-8 fast read wait states, mode bit clocks and instruction */
    /* 18th DWORD - DQS support, byte order, command extension type in 8D mode */
    /* 19th DWORD - Octal enable requirements, 0-8-8 mode support, entry and exit, 8-8-8 enable disable sequence */
    /* 20th DWORD - Maximum operational speed for 4-4-4 and 8-8-8 modes */
    /* 21st DWORD - DTR fast read support flags: 1S-1D-1D, 1S-2D-2D, 1S-4D-4D, 4S-4D-4D (added in JESD216F) */
    /* 22nd DWORD - 1S-1D-1D and 1S-2D-2D fast read wait states, mode clocks and instruction (added in JESD216F) */
    /* 23rd DWORD - 1S-4D-4D and 4S-4D-4D fast read wait states, mode clocks and instruction (added in JESD216F) */
    uint32_t dwords[23];

} NorSpi_SfdpBasicFlashParamTable;

typedef struct NorSpi_SfdpProfile1ParamTable_s
{
    uint32_t dwords[5];

} NorSpi_SfdpProfile1ParamTable;

typedef struct NorSpi_SfdpSectorMapParamTable_s
{
    uint32_t dwords[22];

} NorSpi_SfdpSectorMapParamTable;

typedef struct NorSpi_SfdpSCCRParamTable_s
{
    uint32_t dwords[28];

} NorSpi_SfdpSCCRParamTable;

typedef struct NorSpi_Sfdp4ByteAddressingParamTable_s
{
    uint32_t dwords[2];

} NorSpi_Sfdp4ByteAddressingParamTable;

/**
 *  \brief NOR SPI SFDP Structure
 */

typedef struct NorSpi_SfdpHeader_s
{
    NorSpi_SfdpMainHeader      sfdpHeader;
    /* The SFDP header */

    NorSpi_SfdpParamHeader firstParamHeader;
    /* First and mandatory parameter table header */

} NorSpi_SfdpHeader;

/**
 *  \brief Consolidated data structure to hold SFDP data
 */

typedef struct
{
    uint32_t flashSize;
    uint32_t pageSize;
    uint8_t  manfId;
    uint16_t deviceId;
    uint8_t  numSupportedEraseTypes;
    uint8_t  cmdExtType;
    uint8_t  byteOrder;
    uint8_t  addrByteSupport;
    uint8_t  fourByteAddrEnSeq;
    uint8_t  fourByteAddrDisSeq;
    uint8_t  dtrSupport;
    uint8_t  deviceBusyType;
    FlashCfg_EraseConfig eraseCfg;
    uint8_t  rstType;
    uint8_t  cmdWren;
    uint8_t  cmdRdsr;
    uint8_t  srWip;
    uint8_t  srWel;
    uint8_t  cmdChipErase;
    FlashCfg_ReadIDConfig idCfg;
    FlashCfg_ProtoEnConfig protos[FLASH_CFG_MAX_PROTO];
    uint8_t  xspiWipRdCmd;
    uint32_t xspiWipReg;
    uint32_t xspiWipBit;
    uint32_t flashWriteTimeout;
    uint32_t flashBusyTimeout;
    uint32_t chipEraseTimeout;

    /* Fields added for JESD216H compliance */

    /* BFPT DWORD 16 bits [7:0]: volatile/non-volatile status register write support.
     * Each bit indicates a supported write-enable/write mechanism:
     *   bit 0: Write enable (06h) required before all commands
     *   bit 1: Write enable (06h) required for volatile SR write
     *   bit 2: Write enable latch not required for volatile SR write
     *   bit 3: 0x50 volatile write enable supported
     *   bit 4: Non-volatile SR write supported
     *   bit 5: Volatile SR write supported (using 06h WREN)
     *   bit 6: Volatile SR write using 50h
     *   bit 7: Reserved
     */
    uint8_t  nvRegSupport;

    /* BFPT DWORD 19 bits [19:16]: 0-8-8 mode entry sequence.
     * Each bit indicates a supported entry method into 0-8-8 mode.
     * 0x0 = not supported.
     */
    uint8_t  mode088EntrySeq;

    /* BFPT DWORD 19 bits [15:12]: 0-8-8 mode exit sequence.
     * Each bit indicates a supported exit method from 0-8-8 mode.
     * 0x0 = not supported.
     */
    uint8_t  mode088ExitSeq;

    /* BFPT DWORD 20 bits [31:16]: maximum operational speed for 4-4-4 mode in MHz.
     * 0 = not specified.
     */
    uint16_t maxSpeed444;

    /* BFPT DWORD 20 bits [15:0]: maximum operational speed for 8-8-8 mode in MHz.
     * 0 = not specified.
     */
    uint16_t maxSpeed888;

} NorSpi_SfdpGenericDefines;

/* ========================================================================== */
/*                             Function Definitions                           */
/* ========================================================================== */

/**
 *  \brief  This function returns the name of the parameter table given the table ID
 *
 *  \param  paramTableId 16 bit ID of the parameter table
 *
 *  \return The name of the parameter table as a string
 */
char* NorSpi_Sfdp_getParameterTableName(uint32_t paramTableId);

/**
 *  \brief  This function returns the Parameter Table Pointer (PTP) of the parameter table given the parameter header
 *
 *  \param  paramHeader Parameter header for the table
 *
 *  \return PTP as a uint32_t address
 */
uint32_t NorSpi_Sfdp_getPtp(NorSpi_SfdpParamHeader *paramHeader);

/**
 *  \brief  This function parses the Basic Flash Parameter Table (BFPT) and fills the norSpiDevDefines structure with the parsed information
 *
 *  \param  bfpt           Pointer to the BFPT (allocated by user)
 *  \param  norSpiDefines  Pointer to the generic xSPI device definitions structure (allocated by user)
 *  \param  numDwords      Number of double words actually present in the table (this has to be read from the param header and passed)
 *
 *  \return SystemP_SUCCESS if parsing is successful, otherwise failure.
 */
int32_t NorSpi_Sfdp_parseBfpt(NorSpi_SfdpBasicFlashParamTable *bfpt, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords);

/**
 *  \brief  This function parses the xSPI Flash Profile 1.0 Table and fills the norSpiDevDefines structure with the parsed information
 *
 *  \param  xpt1           Pointer to the xSPI Flash Profile 1.0 Table (allocated by user)
 *  \param  norSpiDefines  Pointer to the generic xSPI device definitions structure (allocated by user)
 *  \param  numDwords      Number of double words actually present in the table (this has to be read from the param header and passed)
 *
 *  \return SystemP_SUCCESS if parsing is successful, otherwise failure.
 */
int32_t NorSpi_Sfdp_parseXpt1(NorSpi_SfdpProfile1ParamTable *xpt1, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords);

/**
 *  \brief  This function parses the 4 Byte Addressing Information Table (4BAIT) and fills the norSpiDevDefines structure with the parsed information
 *
 *  \param  fourBait       Pointer to the 4BAIT (allocated by user)
 *  \param  norSpiDefines  Pointer to the generic xSPI device definitions structure (allocated by user)
 *  \param  numDwords      Number of double words actually present in the table (this has to be read from the param header and passed)
 *
 *  \return SystemP_SUCCESS if parsing is successful, otherwise failure.
 */
int32_t NorSpi_Sfdp_parse4bait(NorSpi_Sfdp4ByteAddressingParamTable *fourBait, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords);

/**
 *  \brief  This function parses the Status, Control and Configuration Registers (SCCR) Table and fills the norSpiDevDefines structure with the parsed information
 *
 *  \param  sccr           Pointer to the SCCR Table (allocated by user)
 *  \param  norSpiDefines  Pointer to the generic xSPI device definitions structure (allocated by user)
 *  \param  numDwords      Number of double words actually present in the table (this has to be read from the param header and passed)
 *
 *  \return SystemP_SUCCESS if parsing is successful, otherwise failure.
 */
int32_t NorSpi_Sfdp_parseSccr(NorSpi_SfdpSCCRParamTable *sccr, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords);

/**
 *  \brief  This function parses the Sector Map Parameter Table (SMPT) and fills the norSpiDevDefines structure with the parsed information
 *
 *  \param  smpt           Pointer to the SMPT (allocated by user)
 *  \param  norSpiDefines  Pointer to the generic xSPI device definitions structure (allocated by user)
 *  \param  numDwords      Number of double words actually present in the table (this has to be read from the param header and passed)
 *
 *  \return SystemP_SUCCESS if parsing is successful, otherwise failure.
 */
int32_t NorSpi_Sfdp_parseSmpt(NorSpi_SfdpSectorMapParamTable *smpt, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords);

/** @} */
#ifdef __cplusplus
}
#endif

#endif /* NOR_SPI_SFDP_H_ */