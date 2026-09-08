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

        .text
        .arm
        .align 2

/* FUNCTION DEF: void HwiP_data_abort_handler(void) */
        .global HwiP_data_abort_handler
        .type HwiP_data_abort_handler,%function
        .section ".text.hwi","ax",%progbits
        .arm
        .align 2
HwiP_data_abort_handler:
        /* Return to the instruction following the interrupted.
         * The instruction that caused the data abort is at R14_ABT – 8 which means
         * that the pointer points two instructions beyond the instruction that caused
         * the abort. (pc - 4) in arm mode and (pc - 6) in thumb mode (2 bytes aligned
         * instruction set) points to the second instruction beyond the address where
         * the exception was generated.
         */

        /*   Push used registers. */
        PUSH    {r0-r4, r12}

        /* Copy LR to R0 to pass to C handler */
        MOV     r0, lr

        /* SPSR has the snapshot of CPSR before data abort. Compare thumb state bit in SPSR */
        MRS     r2, SPSR
        AND     r1, r2, #0x20
        CMP     r1, #0
        SUBNE   lr, lr, #6      /* thumb mode case */
        SUBEQ   lr, lr, #4      /* arm mode case */

        /*   Push the return address and SPSR. */
        PUSH    {lr}
        MRS     lr, SPSR
        PUSH    {lr}

        LDR     r1, HwiP_data_abort_handler_const
        BLX     r1

        POP     {LR}
        MSR     SPSR_cxsf, LR
        POP     {LR}
        POP     {r0-r4, r12}
        MOVS    PC, LR


/* HwiP_irq_handler(), calls the C IRQ handling function. */
.global HwiP_irq_handler
.global __tx_irq_processing_return
.global _tx_thread_context_save
.global _tx_thread_context_restore
.global HwiP_irq_handler_c

.arm
.align 8
.type HwiP_irq_handler, %function
HwiP_irq_handler:

    B _tx_thread_context_save
__tx_irq_processing_return:

    LDR     r1, =_tx_thread_current_ptr     @ Pickup address of current thread ptr
    LDR     r0, [r1]                        @ Pickup current thread pointer
    LDR     r2, [r0, #144]                  @ Pickup the VFP enabled flag
    CMP     r2, #0                          @ Is the VFP enabled?

/* Save the floating point context, if any. */
    FMRXNE  R1,  FPSCR
    VPUSHNE {D0-D15}                        @ Save D0-D15
    PUSHNE  {R1}                            @ Save FPSCR
#ifdef HWIP_NESTED_INTERRUPTS_IRQ_ENABLE
    BL      _tx_thread_irq_nesting_start
#endif

    bl HwiP_irq_handler_c

#ifdef HWIP_NESTED_INTERRUPTS_IRQ_ENABLE
    BL      _tx_thread_irq_nesting_end
#endif

    LDR     r1, =_tx_thread_current_ptr     @ Pickup address of current thread ptr
    LDR     r0, [r1]                        @ Pickup current thread pointer
    LDR     r1, [r0, #144]                  @ Pickup the VFP enabled flag
    CMP     r1, #0                          @ Is the VFP enabled?

/* Restore the floating point context, if any. */
    POPNE   {R0}
    VPOPNE  {D0-D15}                        @ Restore D0 to D15
    VMSRNE  FPSCR, R0                       @ Restore FPSCR

    B _tx_thread_context_restore

    /* Should not get here. */
    B .

/* Unused SVC handler, should not be reached. */
.global HwiP_svc_handler
.arm
.align 8
.type HwiP_svc_handler, %function
HwiP_svc_handler:
    NOP
    B .
    NOP
    BX lr

/* ThreadX Low Level initialization call, unused in this implementation. */
.global _tx_initialize_low_level
.arm
.align 8
.type _tx_initialize_low_level, %function
_tx_initialize_low_level:
    NOP
    BX lr


/* FUNCTION DEF: void HwiP_prefetch_abort_handler(void) */
        .global HwiP_prefetch_abort_handler
        .type HwiP_prefetch_abort_handler,%function
        .section ".text.hwi","ax",%progbits
        .align 2
HwiP_prefetch_abort_handler:
        /* In case of abort from user/system mode, 
         * Return to the instruction following the one which prefetched the invalid instruction.
         * So use SPSR to determine the state of the processor and use R14_USER to return. 
         * R14_USER register is shared for both user & system mode and can be loaded using `^` operator
         * with STM instruction from abort mode.
         * Else this handler returns to the instruction following that caused the abort.
         * Hence the C implementation should make sure to not return if not intended.
         * The instruction that caused the data abort is at `R14_ABT - 4` which means
         * that the pointer already points one instruction beyond the instruction that caused
         * the abort.
         * Refer https://developer.arm.com/documentation/ddi0460/d/Programmers-Model/Exceptions/Exception-entry-and-exit-summary?lang=en
         * for related information.
         */

        /*   Push used registers. */
        PUSH    {r0-r4, r12}

        /* Copy LR to R0 to pass to C handler */
        MOV     r0, lr

        /*   Push the LR and SPSR. */
        PUSH    {lr}                                                                    
        MRS     lr, SPSR
        PUSH    {lr}

        LDR     r1, HwiP_prefetch_abort_handler_const
        BLX     r1

        /*   Get the return address and SPSR. */
        POP     {r0}              /* Pop SPSR to R0  */
        AND     r1, r0, #0x1F     /* Get the mode bits in SPSR                              */                                                                      
        TEQ     r1, #0x10         /* Compare with user mode (0x10)                          */                                                                      
        TEQNE   r1, #0x1F         /* If not, Compare with system mode (0x1F)                */                                                                         
        STMEQ   SP, {r14}^        /* If in user/system mode, store R14_USER onto the stack  */  
        POP     {lr}              /* Pop return address (R14_USER/R14_ABT) to LR            */
        ANDS    r1, lr, #1        /* Determine if return code was compiled in Thumb mode    */
        CMP     r1, #1            /* If the LSB is 1, the code is in Thumb mode             */
        ORREQ   r0, #0x20         /* If Thumb, Set the thumb bit (5) before return          */
        MSR     SPSR_cxsf, r0

        /*   Pop used registers. */
        POP     {r0-r4, r12}

        MOVS    PC, LR

/* FUNCTION DEF: void HwiP_undefined_handler(void) */
        .global HwiP_undefined_handler
        .type HwiP_undefined_handler,%function
        .section ".text.hwi","ax",%progbits
        .align 2
HwiP_undefined_handler:
        /* Return to the instruction following the interrupted.
         * The instruction that caused the undefined exception is at `R14_ABT - 4` in arm mode
         * or `R14_ABT - 2` in thumb mode. This means that the pointer already points to one instruction
         * beyond the instruction that caused the abort.
         * Refer https://developer.arm.com/documentation/ddi0460/d/Programmers-Model/Exceptions/Exception-entry-and-exit-summary?lang=en
         * for related information.
         */

        PUSH    {r0-r4, r12}

        /* Copy LR to R0 to pass to C handler */
        MOV     r0, lr
        
        /*   Push the return address and SPSR. */
        PUSH    {lr}
        MRS     lr, SPSR
        PUSH    {lr}

        LDR     r1, HwiP_undefined_handler_const
        BLX     r1

        POP     {LR}
        MSR     SPSR_cxsf, LR
        POP     {LR}
        POP     {r0-r4, r12}
        MOVS    PC,LR

/*==============================================================================
*   Get Data Fault Status Register (DFSR) value
==============================================================================*/
        .global GET_DFSR
GET_DFSR:
        MRC p15, #0, r0, c5, c0, #0
        BX      lr

/*==============================================================================
*   Get Data Fault Status Register (ADFSR) value
==============================================================================*/
        .global GET_ADFSR
GET_ADFSR:
        MRC p15, #0, r0, c5, c1, #0
        BX      lr

/*==============================================================================
*   Get Data Fault Address Register (DFAR) value
==============================================================================*/
        .global GET_DFAR
GET_DFAR:
        MRC p15, #0, r0, c6, c0, #0
        BX      lr



/*==============================================================================
*   Get Instruction Fault Status Register Register (IFSR) value
==============================================================================*/
        .global GET_IFSR
GET_IFSR:
        MRC p15, #0, r0, c5, c0, #1
        BX      lr

/*==============================================================================
*   Get Instruction Fault Status Register Register (AIFSR) value
==============================================================================*/
        .global GET_AIFSR
GET_AIFSR:
        MRC p15, #0, r0, c5, c1, #1
        BX      lr

/*==============================================================================
*   Get Instruction Fault Address Register Register (IFAR) value
==============================================================================*/
        .global GET_IFAR
GET_IFAR:
        MRC p15, #0, r0, c6, c0, #2
        BX      lr



/*==============================================================================
*   Get Saved Program Status Register (SPSR) value
==============================================================================*/
        .global GET_SPSR
GET_SPSR:
        MRS r0,SPSR
        BX      lr

HwiP_data_abort_handler_const: .word HwiP_data_abort_handler_c
HwiP_prefetch_abort_handler_const: .word HwiP_prefetch_abort_handler_c
HwiP_undefined_handler_const: .word HwiP_undefined_handler_c

.end
