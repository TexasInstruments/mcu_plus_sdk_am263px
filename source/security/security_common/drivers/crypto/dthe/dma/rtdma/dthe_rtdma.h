/*
 *  Copyright (C) 2026 Texas Instruments Incorporated
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

#ifndef DTHE_RTDMA_
#define DTHE_RTDMA_

/* ========================================================================== */
/*                             Include Files                                  */
/* ========================================================================== */

#include <security_common/drivers/crypto/dthe/dma.h>
#include <rtdma.h>

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

/** Global RTDMA function pointer array */
extern DMA_Fxns gRtdmaFxns;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 *  \brief Initialize the RTDMA controller.
 *
 *  This function performs one-time initialization of the RTDMA controller
 *  including hard reset, emulation mode, priority configuration, and locking.
 *  This should be called once during system initialization before any DMA
 *  channel operations.
 *
 *  \return None
 */
void RTDMA_init(void);

/**
 *  \brief This RTDMA api implemented to configure RTDMA config TX channel.
 *
 *  \param handle    [IN] DMA driver handle from \ref DMA_open
 *  \param srcAddress [IN] Source address for RTDMA transfer.
 *  \param dstAddress [IN] Destination address for RTDMA transfer.
 *  \param numBlocks [IN] Number of block to transfer.
 *  \param blockSize [IN] Block size for sha config.
 *  \param operationType  [IN] for selecting Aes Or SHA cfg.
 *
 *  \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_Config_TxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, uint16_t blockSize, int32_t operationType);

/**
 * \brief This RTDMA api implemented to enable DMA TX transfer region.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_enableTxTransferRegion(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to wait TX transfer.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_WaitForTxTransfer(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to start DMA TX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_startTxChannel(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to disable DMA TX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_disableTxChannel(DMA_Handle handle);

/**
 *  \brief This RTDMA api implemented to configure RTDMA config RX channel.
 *
 *  \param handle    [IN] DMA driver handle from \ref DMA_open
 *  \param srcAddress [IN] Source address for RTDMA transfer.
 *  \param dstAddress [IN] Destination address for RTDMA transfer.
 *  \param numBlocks [IN] Number of block to transfer.
 *  \param operationType [IN] Operation type (DMA_AES_ENABLE or DMA_SM4_ENABLE).
 *
 *  \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_Config_RxChannel(DMA_Handle handle, uint32_t *srcAddress, uint32_t *dstAddress, uint16_t numBlocks, int32_t operationType);

/**
 * \brief This RTDMA api implemented to enable Rx Transfer Region.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_enableRxTransferRegion(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to wait RX transfer.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_WaitForRxTransfer(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to start RTDMA RX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_startRxChannel(DMA_Handle handle);

/**
 * \brief This RTDMA api implemented to disable RTDMA RX channel.
 *
 * \param handle    [IN] DMA driver handle from \ref DMA_open
 *
 * \return SystemP_SUCCESS on success or SystemP_FAILURE on Failure.
 */
int32_t RTDMA_disableRxChannel(DMA_Handle handle);

/* ========================================================================== */
/*                       Static Function Definitions                          */
/* ========================================================================== */

/* None */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef DTHE_RTDMA_ */

/** @} */
