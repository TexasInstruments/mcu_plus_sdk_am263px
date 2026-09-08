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
 *  \file nor_spi_sfdp.c
 *
 *  \brief File containing SFDP Driver APIs implementation.
 *
 */

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

/* This is needed for memset/memcpy */
#include <string.h>
#include <board/flash/sfdp/nor_spi_sfdp.h>
#include <drivers/hw_include/cslr.h>

/* ========================================================================== */
/*                             Macro Definitions                              */
/* ========================================================================== */

#define NOR_SPI_SFDP_GET_BITFIELD(val, start, end) ( (val >> start) & ((1U << (end-start+1))-1) )

/* ========================================================================== */
/*                       Internal Function Declarations                       */
/* ========================================================================== */

static void NorSpi_Sfdp_getEraseSizes(NorSpi_SfdpBasicFlashParamTable *bfpt, NorSpi_SfdpGenericDefines *norSpiDefines);
static uint8_t NorSpi_Sfdp_getDummyBitPattern(NorSpi_SfdpSCCRParamTable *sccr, uint32_t dummyClk);


/*===========================================================================*/
/*                            Structure Definitions                          */
/*===========================================================================*/

/* None */

/*===========================================================================*/
/*                            Global Variables                               */
/*===========================================================================*/

uint32_t gNorSpi_Sfdp_ParamTableIds[NOR_SPI_SFDP_NPH_MAX] = {
    NOR_SPI_SFDP_BASIC_PARAM_TABLE_ID,
    NOR_SPI_SFDP_SECTOR_MAP_TABLE_ID,
    NOR_SPI_SFDP_RPMC_TABLE_ID,
    NOR_SPI_SFDP_4BYTE_ADDR_INSTR_TABLE_ID,
    NOR_SPI_SFDP_PROFILE_TABLE_ID,
    NOR_SPI_SFDP_PROFILE_2_TABLE_ID,
    NOR_SPI_SFDP_SCCR_TABLE_ID,
    NOR_SPI_SFDP_SCCR_MULTISPI_OFFSETS_TABLE_ID,
    NOR_SPI_SFDP_SCCR_PROFILE_2_TABLE_ID,
    NOR_SPI_SFDP_OCTAL_CMD_SEQ_TABLE_ID,
    NOR_SPI_SFDP_LONG_LATENCY_NVM_MSP_TABLE_ID,
    NOR_SPI_SFDP_QUAD_IO_WITH_DS_TABLE_ID,
    NOR_SPI_SFDP_QUAD_CMD_SEQ_TABLE_ID,
    NOR_SPI_SFDP_SECURE_PACKET_TABLE_ID,    /* JESD216D */
    NOR_SPI_SFDP_GRAM_TABLE_ID,             /* JESD216G */
    NOR_SPI_SFDP_SPI_SAFETY_EXT_TABLE_ID,  /* JESD216G */
    NOR_SPI_SFDP_CRC32_TABLE_ID,           /* JESD216H */
    NOR_SPI_SFDP_ECC_TABLE_ID,             /* JESD216H */
};

char* gNorSpi_Sfdp_ParamTableNames[NOR_SPI_SFDP_NPH_MAX] = {
    "BASIC PARAMETER TABLE",
    "SECTOR MAP TABLE",
    "RPMC TABLE",
    "4 BYTE ADDRESSING MODE INSTRUCTIONS TABLE",
    "NOR SPI PROFILE TABLE ",
    "NOR SPI PROFILE 2.0 TABLE",
    "STATUS CONTROL AND CONFIGURATION REGISTER MAP TABLE",
    "STATUS CONTROL AND CONFIGURATION REGISTER MAP MULTISPI OFFSETS TABLE",
    "STATUS CONTROL AND CONFIGURATION REGISTER MAP NOR SPI PROFILE 2.0  TABLE",
    "OCTAL DDR MODE COMMAND SEQUENCE TABLE",
    "LONG LATENCY NVM MEDIA SPECIFIC PARAMETER TABLE",
    "QUAD IO WITH DS TABLE",
    "QUAD DDR MODE COMMAND SEQUENCE TABLE",
    "SECURE PACKET TABLE",
    "GENERIC REGISTER AND COMMAND MAP TABLE",
    "SPI SAFETY EXTENSIONS CRC TABLE",
    "SFDP CRC-32 TABLE",
    "ECC TABLE",
};

static uint32_t gSectorIdx = 4;
static uint32_t gBlockIdx  = 4;

/*===========================================================================*/
/*                            Function Definitions                           */
/*===========================================================================*/
char* NorSpi_Sfdp_getParameterTableName(uint32_t paramTableId)
{
    uint32_t i, idx = NOR_SPI_SFDP_NPH_MAX;
    char *p = NULL;

    for(i = 0; i < NOR_SPI_SFDP_NPH_MAX; i++)
    {
        if(paramTableId == gNorSpi_Sfdp_ParamTableIds[i])
        {
            idx = i;
            break;
        }
    }

    if(idx == NOR_SPI_SFDP_NPH_MAX)
    {
        /* Unrecognized param table */
        p = NULL;
    }
    else
    {
        p = gNorSpi_Sfdp_ParamTableNames[idx];
    }

    return p;
}

uint32_t NorSpi_Sfdp_getPtp(NorSpi_SfdpParamHeader *paramHeader)
{
    uint32_t ptp = 0xFFFFFFFFU;

    if(paramHeader != NULL)
    {
        ptp = (uint32_t)(paramHeader->paramTablePtr[2] << 16) |
              (uint32_t)(paramHeader->paramTablePtr[1] << 8)  |
              (uint32_t)(paramHeader->paramTablePtr[0]);
    }

    return ptp;
}

int32_t NorSpi_Sfdp_parseBfpt(NorSpi_SfdpBasicFlashParamTable *bfpt, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords)
{
    int32_t status = SystemP_SUCCESS;

    /* Some NOR SPI standards */
    norSpiDefines->cmdWren = 0x06;
    norSpiDefines->cmdRdsr = 0x05;
    norSpiDefines->idCfg.cmd = 0x9F;
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_1S].cmdRd = 0x03;
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_1S].cmdWr = 0x02;

    /* Always set this to 5. RDID will keep reading 5+1 ID bytes in 1s mode, this way no need to change with 8D/4D */
    norSpiDefines->idCfg.numBytes = 5;

    /* Number of address bytes */
    norSpiDefines->addrByteSupport = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[0], 17, 18);

    /* DTR support */
    norSpiDefines->dtrSupport = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[0], 19, 19);

    /* Flash Size */
    norSpiDefines->flashSize = 0U;

    if(NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[1], 31, 31) == 0)
    {
        norSpiDefines->flashSize = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[1], 0, 30) + 1;
    }
    else
    {
        uint32_t n = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[1], 0, 30);

        if(n > 63)
        {
            DebugP_logError("Bad flash size read from SFDP !! \r\n");
            norSpiDefines->flashSize = 0U;
        }
        else
        {
            norSpiDefines->flashSize = 1U << n;
        }
    }
    /* Convert to bytes */
    norSpiDefines->flashSize >>= 3;

    /* Check for 1-1-4 mode */
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[2], 24, 31);
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].cmdWr = 0x02;

    if (norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].cmdRd != 0)
    {
        /* 1-1-4 mode is supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].modeClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[2], 21, 23);
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[2], 16, 20);
    }

    /* Check for 1-1-2 mode */
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[3], 8, 15);
    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].cmdWr = 0x02;

    if (norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].cmdRd != 0)
    {
        /* 1-1-2 mode is supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].modeClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[3], 5, 7);
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[3], 0, 4);
    }

    /* Check for 4-4-4 mode */
    if(NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[4], 4, 4) != 0)
    {
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 24, 31);
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].cmdWr = 0x02;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].modeClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 21, 23);
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 16, 20);

        if(norSpiDefines->dtrSupport)
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 24, 31);
            norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].cmdWr = 0x02;
            norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].modeClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 21, 23);
            norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[6], 16, 20);
        }
    }

    /* Erase types : Of all the ones supported, smallest would be sector and largest would be block */
    /* This will set the erase command and type from BFPT. Will be updated with 4BAIT and SMPT parsing */
    NorSpi_Sfdp_getEraseSizes(bfpt, norSpiDefines);

    /* Check for JESD216A. That has only 9 DWORDS */
    if(numDwords > NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216)
    {
        /* Page size and timeouts */
        uint32_t count = 0U;
        uint32_t unit = 0U; /* All time units in μs */

        /* Page size */
        norSpiDefines->pageSize = (1 << (NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[10], 4, 7)));

        /* Page program timeout */
        if(NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[10], 13, 13) == 1)
        {
            unit = 64;
        }
        else
        {
            unit = 8;
        }

        count = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[10], 8, 12);

        norSpiDefines->flashWriteTimeout = (count+1)*unit;

        /* Chip Erase / Bulk Erase Timeout */
        switch(NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[10], 29, 30))
        {
            case 0:
                unit = 16 * 1000; /* 16 ms */
                break;

            case 1:
                unit = 256 * 1000; /* 256 ms */
                break;

            case 2:
                unit = 4 * 1000 * 1000; /* 4 s */
                break;

            case 3:
                unit = 64 * 1000 * 1000; /* 64 s */
                break;

            default:
                unit = 0U; /* Should not hit */
                break;
        }

        count = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[10], 24, 28);

        norSpiDefines->flashBusyTimeout = norSpiDefines->chipEraseTimeout = (count + 1) * unit;

        /* Device busy polling */
        uint32_t devBusy = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[13], 2, 7);

        if(devBusy & 0x01)
        {
            norSpiDefines->deviceBusyType = 1;
            norSpiDefines->srWip = 0;
            norSpiDefines->srWel = 1;
        }
        if(devBusy & 0x02)
        {
            norSpiDefines->deviceBusyType = 0;
        }

        /* Quad Enable Requirement */
        uint32_t qeType = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[14], 20, 22);
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].enableType = qeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].enableType = qeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].enableType = qeType;

        /* 4-4-4 enable sequence */
        uint32_t qeSeq = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[14], 4, 8);
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].enableSeq = qeSeq;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].enableSeq = qeSeq;

        /* 4 byte addressing mode enable and disable sequences*/
        norSpiDefines->fourByteAddrEnSeq  = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[15], 24, 31);
        norSpiDefines->fourByteAddrDisSeq = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[15], 14, 23);

        /* Soft Reset Type */
        norSpiDefines->rstType = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[15], 8, 13);

        /* Volatile/non-volatile status register write support (JESD216B DWORD 16 bits [7:0]).
         * Bit map:
         *   bit 0: Write enable (06h) required before all commands
         *   bit 1: Write enable (06h) required for volatile SR write
         *   bit 2: Write enable latch not required for volatile SR write
         *   bit 3: 0x50 volatile write enable supported
         *   bit 4: Non-volatile SR write supported
         *   bit 5: Volatile SR write supported (using 06h WREN)
         *   bit 6: Volatile SR write using 50h
         */
        norSpiDefines->nvRegSupport = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[15], 0, 7);

        /* Check for JESD216B. That has only 16 DWORDS */
        if(numDwords > NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216B)
        {
            /* Octal protocols setup */
            /* Check for 1-1-8 mode */
            norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[16], 24, 31);
            norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].cmdWr = 0x02;

            if(norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].cmdRd != 0)
            {
                /* 1-1-8 mode is supported */
                norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].modeClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[16], 21, 23);
                norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[16], 16, 20);
            }

            /* Byte order */
            norSpiDefines->byteOrder = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[17], 31, 31);

            /* 8D mode command extension */
            norSpiDefines->cmdExtType = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[17], 29, 30);

            /* OE Type */
            uint32_t oeType = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[18], 20, 22);
            norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].enableType = oeType;
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].enableType = oeType;
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].enableType = oeType;

            /* 0-8-8 mode entry and exit sequences (JESD216C DWORD 19 bits [19:12]).
             * Each field is a 4-bit bitmap; each set bit indicates a supported method.
             * Entry bits [19:16]:
             *   bit 16 (0b0001): No entry sequence needed; device powers up in 0-8-8
             *   bit 17 (0b0010): Entry by sending dummy clocks after CS# de-assertion
             *   bit 18 (0b0100): Entry by command (device-specific)
             *   bit 19 (0b1000): Entry via register write (device-specific)
             * Exit bits [15:12]:
             *   bit 12 (0b0001): No exit sequence needed
             *   bit 13 (0b0010): Exit by 8-clock idle pattern on DQ[0:7] with CS# de-asserted
             *   bit 14 (0b0100): Soft reset using sequences in DWORD 16 bits [13:8]
             *   bit 15 (0b1000): Power cycle required
             */
            norSpiDefines->mode088EntrySeq = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[18], 16, 19);
            norSpiDefines->mode088ExitSeq  = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[18], 12, 15);

            /* 8-8-8 Mode enable sequence
             * Bits [8:4] encode the 8s-8s-8s enable sequence. Encoding updated in JESD216H:
             *   bit 4  (0b00001): Write enable volatile bit in CFR2V (Reg 5)
             *   bit 5  (0b00010): Write 0x01 to CFR2V[1] to enter 8s mode
             *   bit 6  (0b00100): Issue command 0x81 (write to volatile register) to enter 8s mode
             *   bit 7  (0b01000): Write EFh to addressable register at address 00h (via GRAM, JESD216H)
             *   bit 8  (0b10000): Write B7h to addressable register at address 00h (via GRAM, JESD216H)
             * Bits [3:0] encode the 8s-8s-8s disable / return-to-1s-1s-1s sequence. Updated in JESD216H:
             *   bit 0  (0b0001): Soft reset (use sequences from BFPT DWORD 16)
             *   bit 1  (0b0010): Write FFh to addressable register at address 00h (via GRAM, JESD216H)
             *   bit 2  (0b0100): Power cycle
             *   bit 3  (0b1000): 8-clock sequence on SI/SO with CS# deasserted
             */
            uint32_t oeSeq = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[18], 4, 8);
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].enableSeq = oeSeq;

            if(norSpiDefines->dtrSupport)
            {
                norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].enableSeq = oeSeq;
            }

            /* DWORD 20 (index [19]): Maximum operational speeds (JESD216C).
             * bits [31:16]: max speed for 4-4-4 mode in MHz (0 = not specified)
             * bits [15:0]:  max speed for 8-8-8 mode in MHz (0 = not specified)
             */
            norSpiDefines->maxSpeed444 = (uint16_t)NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[19], 16, 31);
            norSpiDefines->maxSpeed888 = (uint16_t)NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[19], 0, 15);

            /* Check for JESD216F/G/H. Those have 23 DWORDs (DWORDs 21-23 added in JESD216F) */
            if(numDwords > NOR_SPI_SFDP_BFPT_MAX_DWORDS_JESD216C)
            {
                /* DWORD 21 (index [20]): DTR fast read support flags
                 * bit 3: 1S-1D-1D supported
                 * bit 4: 1S-2D-2D supported
                 * bit 5: 1S-4D-4D supported
                 * bit 6: 4S-4D-4D supported
                 */
                uint32_t dtrSupport21 = bfpt->dwords[20];

                /* DWORD 22 (index [21]): 1S-1D-1D and 1S-2D-2D fast read parameters */
                if(NOR_SPI_SFDP_GET_BITFIELD(dtrSupport21, 3, 3))
                {
                    /* 1S-1D-1D read. No special enable sequence — mode is entered by
                     * issuing the DTR read command directly (enableType = 0). */
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].cmdRd       = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 24, 31);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].modeClksRd  = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 21, 23);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 16, 20);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].isDtr        = TRUE;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].cmdWr        = 0x02;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].enableType   = 0;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_1D_1D].enableSeq    = 0;
                }

                if(NOR_SPI_SFDP_GET_BITFIELD(dtrSupport21, 4, 4))
                {
                    /* 1S-2D-2D read. No special enable sequence — mode is entered by
                     * issuing the DTR read command directly (enableType = 0). */
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].cmdRd       = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 8, 15);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].modeClksRd  = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 5, 7);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[21], 0, 4);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].isDtr        = TRUE;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].cmdWr        = 0x02;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].enableType   = 0;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_2D_2D].enableSeq    = 0;
                }

                /* DWORD 23 (index [22]): 1S-4D-4D and 4S-4D-4D fast read parameters */
                if(NOR_SPI_SFDP_GET_BITFIELD(dtrSupport21, 5, 5))
                {
                    /* 1S-4D-4D read. Uses the same Quad Enable Requirements as 1S-1S-4S
                     * because the flash must still be in quad-enabled state to output 4
                     * data wires. Inherit qeType and qeSeq already set for 1S-1S-4S. */
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].cmdRd       = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 24, 31);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].modeClksRd  = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 21, 23);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 16, 20);
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].isDtr        = TRUE;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].cmdWr        = 0x02;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].enableType   = norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].enableType;
                    norSpiDefines->protos[FLASH_CFG_PROTO_1S_4D_4D].enableSeq    = norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].enableSeq;
                }

                if(NOR_SPI_SFDP_GET_BITFIELD(dtrSupport21, 6, 6))
                {
                    /* 4S-4D-4D read - overlaps with existing FLASH_CFG_PROTO_4S_4D_4D slot.
                     * Update dummy/mode clocks from the dedicated JESD216F fields if non-zero,
                     * as the JESD216B-era DWORD 7 field does not distinguish STR vs DTR timing.
                     */
                    uint8_t  cmd4s4d4d = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 8, 15);
                    uint8_t  mode4s4d4d = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 5, 7);
                    uint16_t dummy4s4d4d = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[22], 0, 4);
                    if(cmd4s4d4d != 0)
                    {
                        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].cmdRd    = cmd4s4d4d;
                        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].modeClksRd  = mode4s4d4d;
                        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].dummyClksRd = dummy4s4d4d;
                    }
                    norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].isDtr = TRUE;
                }
            }

        }
        else
        {
            /* JEDS216B, parsing stops here */
            norSpiDefines->cmdExtType = NOR_SPI_CMD_EXT_TYPE_NONE;
            uint32_t oeType = 0xFF;
            norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].enableType = oeType;
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].enableType = oeType;
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].enableType = oeType;
            norSpiDefines->byteOrder = 0xFF;
        }
    }
    else
    {
        /* JEDS216A, parsing stops here. Set reasonable defaults */
        uint32_t qeType = 0xFF;
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].enableType = qeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].enableType = qeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].enableType = qeType;
        uint32_t oeType = 0xFF;
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].enableType = oeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].enableType = oeType;
        norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].enableType = oeType;
        norSpiDefines->byteOrder = 0xFF;
        norSpiDefines->pageSize = 256;
        norSpiDefines->flashWriteTimeout = 400;
        norSpiDefines->cmdExtType = NOR_SPI_CMD_EXT_TYPE_NONE;
        norSpiDefines->rstType = 16; /* 0x66 to enable reset and 0x99 to do the actual reset */
        norSpiDefines->deviceBusyType = 1;
        norSpiDefines->srWel = 1;
        norSpiDefines->srWip = 0;
    }

    return status;
}

/* Sector Map Parameter Table */
int32_t NorSpi_Sfdp_parseSmpt(NorSpi_SfdpSectorMapParamTable *smpt, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords)
{
    int32_t status = SystemP_SUCCESS;

    /* TODO: Complete region mapping according to sector map selection */

    return status;
}

/* Status, Control and Configuration Registers Table */
int32_t NorSpi_Sfdp_parseSccr(NorSpi_SfdpSCCRParamTable *sccr, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords)
{
    int32_t status = SystemP_SUCCESS;
    uint32_t i;
    uint32_t vRegAddrOffset = sccr->dwords[0];

    uint32_t dw = sccr->dwords[4];
    /* WIP setup in XSPI */
    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        norSpiDefines->xspiWipRdCmd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);

        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            norSpiDefines->xspiWipReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
        }
        else
        {
            norSpiDefines->xspiWipReg = 0xFFFFFFFF;
        }

        norSpiDefines->xspiWipBit = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
    }

    uint32_t protoList[] = {
        FLASH_CFG_PROTO_4S_4S_4S,
        FLASH_CFG_PROTO_4S_4D_4D,
        FLASH_CFG_PROTO_8S_8S_8S,
        FLASH_CFG_PROTO_8D_8D_8D,
    };

    norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].isDtr = TRUE;
    norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].isDtr = TRUE;

    /* Check if variable dummy cycles are supported, and if yes check if it is set through registers */

    dw = sccr->dwords[8];

    for(i = 0; i < 4; i++)
    {
        norSpiDefines->protos[protoList[i]].dummyCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
        norSpiDefines->protos[protoList[i]].dummyCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);
    }

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            for(i = 0; i < 4; i++)
            {
                norSpiDefines->protos[protoList[i]].dummyCfg.isAddrReg = TRUE;
                norSpiDefines->protos[protoList[i]].dummyCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
                uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
                uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
                uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
                norSpiDefines->protos[protoList[i]].dummyCfg.shift = (uint16_t)shift;
                norSpiDefines->protos[protoList[i]].dummyCfg.mask = (uint16_t)mask;
                norSpiDefines->protos[protoList[i]].dummyCfg.cfgRegBitP = NorSpi_Sfdp_getDummyBitPattern(sccr, norSpiDefines->protos[protoList[i]].dummyClksRd);
            }
        }
        else
        {
            if(NOR_SPI_SFDP_GET_BITFIELD(dw, 23, 23) == 1)
            {
                norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyCfg.isAddrReg = FALSE;
                norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 19);
            }
            if(NOR_SPI_SFDP_GET_BITFIELD(dw, 22, 22) == 1)
            {
                norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].dummyCfg.isAddrReg = FALSE;
                norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 19);
            }
            if(NOR_SPI_SFDP_GET_BITFIELD(dw, 21, 21) == 1)
            {
                norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].dummyCfg.isAddrReg = FALSE;
                norSpiDefines->protos[FLASH_CFG_PROTO_4S_4D_4D].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 19);
            }
            if(NOR_SPI_SFDP_GET_BITFIELD(dw, 20, 20) == 1)
            {
                norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].dummyCfg.isAddrReg = FALSE;
                norSpiDefines->protos[FLASH_CFG_PROTO_4S_4S_4S].dummyClksRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 19);
            }
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    /* QPI mode enablement */

    dw = sccr->dwords[13];

    for(i = 0; i < 2; i++)
    {
        norSpiDefines->protos[protoList[i]].protoCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
        norSpiDefines->protos[protoList[i]].protoCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);
    }

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            for(i = 0; i < 2; i++)
            {
                norSpiDefines->protos[protoList[i]].protoCfg.isAddrReg = TRUE;
                norSpiDefines->protos[protoList[i]].protoCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
                uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
                uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
                uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
                norSpiDefines->protos[protoList[i]].protoCfg.shift = (uint16_t)shift;
                norSpiDefines->protos[protoList[i]].protoCfg.mask = (uint16_t)mask;
            }
        }
        else
        {
            for(i = 0; i < 2; i++)
            {
                norSpiDefines->protos[protoList[i]].protoCfg.isAddrReg = FALSE;
            }
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    /* OPI mode enablement */

    dw = sccr->dwords[15];

    for(i = 2; i < 4; i++)
    {
        norSpiDefines->protos[protoList[i]].protoCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
        norSpiDefines->protos[protoList[i]].protoCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);
    }

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            for(i = 2; i < 4; i++)
            {
                norSpiDefines->protos[protoList[i]].protoCfg.isAddrReg = TRUE;
                norSpiDefines->protos[protoList[i]].protoCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
                uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
                uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
                uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
                norSpiDefines->protos[protoList[i]].protoCfg.shift = (uint16_t)shift;
                norSpiDefines->protos[protoList[i]].protoCfg.mask = (uint16_t)mask;
            }
        }
        else
        {
            for(i = 2; i < 4; i++)
            {
                norSpiDefines->protos[protoList[i]].protoCfg.isAddrReg = FALSE;
            }
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    /* STR or DTR mode */
    dw = sccr->dwords[17];

    for(i = 0; i < 4; i++)
    {
        norSpiDefines->protos[protoList[i]].strDtrCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
        norSpiDefines->protos[protoList[i]].strDtrCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);
    }

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        uint32_t i;
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            for(i = 0; i < 4; i++)
            {
                norSpiDefines->protos[protoList[i]].strDtrCfg.isAddrReg = TRUE;
                norSpiDefines->protos[protoList[i]].strDtrCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
                uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
                uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
                uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
                norSpiDefines->protos[protoList[i]].strDtrCfg.shift = (uint16_t)shift;
                norSpiDefines->protos[protoList[i]].strDtrCfg.mask = (uint16_t)mask;
            }
        }
        else
        {
            for(i = 0; i < 4; i++)
            {
                norSpiDefines->protos[protoList[i]].strDtrCfg.isAddrReg = FALSE;
            }
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    /* STR Octal Enable */
    dw = sccr->dwords[19];

    norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
    norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.isAddrReg = TRUE;
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
            uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
            uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
            uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.shift = (uint16_t)shift;
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.mask = (uint16_t)mask;
        }
        else
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_8S_8S_8S].strDtrCfg.isAddrReg = FALSE;
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    /* DTR Octal Enable */
    dw = sccr->dwords[21];

    norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.cmdRegRd = NOR_SPI_SFDP_GET_BITFIELD(dw, 8, 15);
    norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.cmdRegWr = NOR_SPI_SFDP_GET_BITFIELD(dw, 0, 7);

    if(NOR_SPI_SFDP_GET_BITFIELD(dw, 31, 31) == 1)
    {
        if(NOR_SPI_SFDP_GET_BITFIELD(dw, 28, 28) == 1)
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.isAddrReg = TRUE;
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.cfgReg = vRegAddrOffset + NOR_SPI_SFDP_GET_BITFIELD(dw, 16, 23);
            uint32_t numBits = NOR_SPI_SFDP_GET_BITFIELD(dw, 29, 30);
            uint32_t shift = NOR_SPI_SFDP_GET_BITFIELD(dw, 24, 26);
            uint32_t mask = (uint32_t)(1U << (numBits+shift)) - (1U << shift);
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.shift = (uint16_t)shift;
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.mask = (uint16_t)mask;
        }
        else
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].strDtrCfg.isAddrReg = FALSE;
        }
    }
    else
    {
        /* Do nothing, this configuration is not valid */
    }

    return status;
}

/* 4 Byte Addressing Instructions Table */
int32_t NorSpi_Sfdp_parse4bait(NorSpi_Sfdp4ByteAddressingParamTable *fourBait, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords)
{
    int32_t status = SystemP_SUCCESS;

    /* Need to check for 1-1-8, 1-1-4, 1-1-2 modes in 4 byte addressing */
    if(NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[0], 23, 23) == 1)
    {
        /* 1-1-8 page program supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].cmdWr = 0x84;

        /* Check for 1-1-8 fast read */
        if(NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[0], 20, 20) == 1)
        {
            norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_8S].cmdRd = 0x7C;
        }
    }
    if(NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[0], 4, 4) == 1)
    {
        /* 1-1-4 fast read supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].cmdRd = 0x6C;
    }
    if (NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[0], 7, 7) == 1)
    {
        /* 1-1-4 page program supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_4S].cmdWr = 0x34;
    }
    if (NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[0], 2, 2) == 1)
    {
        /* 1-1-2 Fast read supported */
        norSpiDefines->protos[FLASH_CFG_PROTO_1S_1S_2S].cmdRd = 0x3C;
    }

    if(gSectorIdx < 4)
    {
        /* BFPT parsing was done, find out erase cmd for sector in 4B */
        uint32_t start = gSectorIdx << 3;
        norSpiDefines->eraseCfg.cmdSectorErase4B = NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[1], start, start+7);
    }
    if(gBlockIdx < 4)
    {
        /* BFPT parsing was done, find out erase cmd for sector in 4B */
        uint32_t start = gBlockIdx << 3;
        norSpiDefines->eraseCfg.cmdBlockErase4B = NOR_SPI_SFDP_GET_BITFIELD(fourBait->dwords[1], start, start+7);
    }

    return status;
}

/* NOR SPI Profile 1 Table */
int32_t NorSpi_Sfdp_parseXpt1(NorSpi_SfdpProfile1ParamTable *xpt1, NorSpi_SfdpGenericDefines *norSpiDefines, uint32_t numDwords)
{
    int32_t status = SystemP_SUCCESS;

    /* Get the fast read command for 8D-8D-8D mode */
    norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].cmdRd = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[0], 8, 15);
    norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].cmdWr = 0x12;

    /* Dummy Cycles for RDSR and REG READ CMDs */
    if(NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[0], 28, 28) == 1)
    {
        norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyClksCmd = 8;
    }
    else
    {
        norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyClksCmd = 4;
    }

    /* Get dummy cycles for the fastest speed possible */
    uint32_t dummy = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[3], 7, 11); /* 200 MHz */
    uint8_t lc = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[3], 2, 6);

    if(dummy == 0)
    {
        dummy = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[4], 27, 31); /* 166 MHz */
        lc = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[4], 22, 26);
    }

    if(dummy == 0)
    {
        dummy = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[4], 17, 21); /* 133 MHz */
        lc = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[4], 12, 16);
    }

    if(dummy == 0)
    {
        dummy = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[4], 7, 11); /* 100 MHz */
        lc = NOR_SPI_SFDP_GET_BITFIELD(xpt1->dwords[3], 2, 6);
    }

    if(dummy == 0)
    {
        DebugP_logError("No dummy cycle found from NOR SPI Profile 1.0 table !!!\r\n");
    }
    else
    {
        /* Timing can mess up if dummy cycle is odd */
        if(dummy % 2 != 0)
        {
            dummy += 1;
            if(lc < 31)
            {
                lc += 1;
            }
        }
        norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyClksRd = dummy;
        norSpiDefines->protos[FLASH_CFG_PROTO_8D_8D_8D].dummyCfg.cfgRegBitP = lc;
    }

    return status;
}

static void NorSpi_Sfdp_getEraseSizes(NorSpi_SfdpBasicFlashParamTable *bfpt, NorSpi_SfdpGenericDefines *norSpiDefines)
{
    uint32_t i, blk = 0U, sector = 31U;
    uint32_t blkIdx = 4U;
    uint32_t sectorIdx = 4U;
    uint8_t eraseSizeN[4];
    uint8_t eraseCmd[4];

    eraseSizeN[0] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[7], 0, 7);
    eraseSizeN[1] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[7], 16, 23);
    eraseSizeN[2] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[8], 0, 7);
    eraseSizeN[3] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[8], 16, 23);

    eraseCmd[0] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[7], 8, 15);
    eraseCmd[1] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[7], 24, 31);
    eraseCmd[2] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[8], 8, 15);
    eraseCmd[3] = NOR_SPI_SFDP_GET_BITFIELD(bfpt->dwords[8], 24, 31);

    for(i = 0; i < 4; i++)
    {
        if((eraseSizeN[i] != 0) && (blk < eraseSizeN[i]))
        {
            blk = eraseSizeN[i];
            blkIdx = i;
        }
    }

    for(i = 0; i < 4; i++)
    {
        if((eraseSizeN[i] != 0) && (sector > eraseSizeN[i]))
        {
            sector = eraseSizeN[i];
            sectorIdx = i;
        }
    }

    norSpiDefines->eraseCfg.blockSize = (1 << blk);
    norSpiDefines->eraseCfg.sectorSize = (1 << sector);

    if(sectorIdx < 4)
    {
        norSpiDefines->eraseCfg.cmdSectorErase3B = eraseCmd[sectorIdx];
        norSpiDefines->eraseCfg.cmdSectorErase4B = eraseCmd[sectorIdx];
    }
    else
    {
        norSpiDefines->eraseCfg.cmdSectorErase3B = 0;
        norSpiDefines->eraseCfg.cmdSectorErase4B = 0;
    }

    if(blkIdx < 4)
    {
        norSpiDefines->eraseCfg.cmdBlockErase3B = eraseCmd[blkIdx];
        norSpiDefines->eraseCfg.cmdBlockErase4B = eraseCmd[blkIdx];
    }
    else
    {
        norSpiDefines->eraseCfg.cmdBlockErase3B = 0;
        norSpiDefines->eraseCfg.cmdBlockErase4B = 0;
    }

    /* Set some globals to indicate to 4BAIT parser which erase sizes where chosen */
    gSectorIdx = sectorIdx;
    gBlockIdx  = blkIdx;

    /* NOR SPI standard */
    norSpiDefines->cmdChipErase = 0xC7;
}

static uint8_t NorSpi_Sfdp_getDummyBitPattern(NorSpi_SfdpSCCRParamTable *sccr, uint32_t dummyClk)
{
    uint8_t bitPattern = 0xFF;
    uint32_t dword, isDummy, subVal;

    /* Only support even dummy cycles */
    if((dummyClk % 2) != 0)
    {
        dummyClk += 1;
    }

    /* Find the right DWORD in SCCR */
    if((dummyClk) <= 30 && (dummyClk >= 22))
    {
        dword = sccr->dwords[10];
        subVal = 20;
    }
    else if((dummyClk) <= 20 && (dummyClk >= 12))
    {
        dword = sccr->dwords[11];
        subVal = 10;
    }
    else if((dummyClk) <= 10 && (dummyClk >= 2))
    {
        dword = sccr->dwords[12];
        subVal = 0;
    }
    else
    {
        dword = 0xFFFFFFFFU;
    }

    if(dword != 0xFFFFFFFFU)
    {
        uint32_t dummySupportBit = 3*(dummyClk-subVal)+1;
        isDummy = NOR_SPI_SFDP_GET_BITFIELD(dword, dummySupportBit, dummySupportBit);

        if(isDummy)
        {
            bitPattern = NOR_SPI_SFDP_GET_BITFIELD(dword, (dummySupportBit-5), (dummySupportBit-1));
        }
        else
        {
            bitPattern = 0xFF;
        }
    }

    return bitPattern;
}