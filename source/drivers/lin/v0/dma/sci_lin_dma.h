/*
 *  Copyright (C) 2025 Texas Instruments Incorporated
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
 *  \file v0/sci_lin_dma.h
 *
 *  \brief This file contains the prototype of LIN DMA mode operation Functions
 */

 #ifndef SCI_LIN_DMA_H_
 #define SCI_LIN_DMA_H_

#include <drivers/lin.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Function to open DMA channels for given LIN Instance
 *
 *  Configures the DMA LIN Instance for Usage
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param dmaChCfg      [IN] #LIN_DmaChConfig
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_dmaOpen(LIN_Handle handle, LIN_DmaChConfig dmaChCfg);

/**
 * \brief Function to Close DMA channels for given LIN Instance
 *
 *  Closes the DMA LIN Instance
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_dmaClose(LIN_Handle handle);

/**
 * \brief Function to Write LIN data Frame in Commander Mode
 *
 *  Writes data along with ID to the buss as Commander
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_WriteLinFrameCommanderDMA(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 * \brief Function to Write LIN data Frame in Responder Mode
 *
 *  Responds with data on ID match as a Responder
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_WriteLinFrameResponderDMA(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 * \brief Function to Read LIN data Frame in Commander Mode
 *
 *  Writes ID and expects a response from responder on the bus
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_ReadLinFrameCommanderDMA(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 * \brief Function to Read LIN data Frame in Responder Mode
 *
 *  Reads ID and data transmitted by some commander on the bus
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_ReadLinFrameResponderDMA(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 * \brief Function to write SCI data Frame
 *
 * Transmits data buffer
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_WriteSciFrameDMA(LIN_Handle handle, LIN_SCI_Frame *frame);

/**
 * \brief Function to reads SCI data Frame
 *
 * Receives data buffer
 *
 * \param handle        [IN] #LIN_Handle returned from #LIN_open()
 * \param frame         [IN] Pointer to the LIN_SCI_Frame
 *
 *  \return #SystemP_SUCCESS if started successfully; else error on failure
 */
int32_t LIN_ReadSciFrameDMA(LIN_Handle handle, LIN_SCI_Frame *frame);


#ifdef __cplusplus
}
#endif

#endif /* SCI_LIN_DMA_H_ */
