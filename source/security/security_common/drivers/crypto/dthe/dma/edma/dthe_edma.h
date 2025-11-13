/*
 *  Copyright (C) 2024 Texas Instruments Incorporated
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

#ifndef DTHE_EDMA_
#define DTHE_EDMA_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <drivers/edma.h>
#include <security_common/drivers/crypto/dthe/dma.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                         Structure Declarations                             */
/* ========================================================================== */

/* None */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/** Global edma function pointer array */
extern DMA_Fxns gEdmaFxns;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 *  \brief This EDMA api implemented to configure EDMA config TX channel.
 *
 *  \param handle    [IN] DMA driver handle from \ref DMA_open
 *  \param srcAddress [IN] Source address for EDMA transfer.
 *  \param dstAddress [IN] Destination address for EDMA transfer.
 *  \param numBlocks [IN] Number of block to transfer.
 *  \param blockSize [IN] Block size for sha config.
 *  \param operationType  [IN] for selecting Aes Or SHA cfg.
 *
 *  \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_Config_TxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, uint16_t blockSize, int32_t operationType);

/**
 * \brief This EDMA api implemented to enable DMA TX transfer region.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_enableTxTransferRegion(DMA_Handle handle);

/**
 * \brief This EDMA api implemented to wait TX transfer.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 * 
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_WaitForTxTransfer(DMA_Handle handle);

/**
 * \brief This EDMA api implemented to disable DMA TX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_disableTxChannel(DMA_Handle handle);

/**
 *  \brief This EDMA api implemented to configure EDMA config TX channel.
 *
 *  \param handle    [IN] DMA driver handle from \ref DMA_open
 *  \param srcAddress [IN] Source address for EDMA transfer.
 *  \param dstAddress [IN] Destination address for EDMA transfer.
 *  \param numBlocks [IN] Number of block to transfer.
 *  
 *  \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_Config_RxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks);

/**
 * \brief This EDMA api implemented to enable Rx Transfer Region.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_enableRxTransferRegion(DMA_Handle handle);

/**
 * \brief This EDMA api implemented to wait RX transfer.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 * 
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_WaitForRxTransfer(DMA_Handle handle);

/**
 * \brief This EDMA api implemented to disable EDMA RX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_SUCCESS_FAILURE on Failure.
 */
int32_t EDMA_disableRxChannel(DMA_Handle handle);

/* ========================================================================== */
/*                       Static Function Definitions                          */
/* ========================================================================== */

/* None */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef DTHE_EDMA_ */

/** @} */
