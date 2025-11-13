

 /* This is the stack that is used by code running within main()
  * In case of NORTOS,
  * - This means all the code outside of ISR uses this stack
  * In case of FreeRTOS
  * - This means all the code until vTaskStartScheduler() is called in main()
  *   uses this stack.
  * - After vTaskStartScheduler() each task created in FreeRTOS has its own stack
  */

 --stack_size=16384
/* This is the heap size for malloc() API in NORTOS and FreeRTOS
* This is also the heap used by pvPortMalloc in FreeRTOS
*/
 --heap_size=32768
-e_vectors  /* This is the entry of the application, _vector MUST be placed starting address 0x0 */

/* This is the size of stack when R5 is in IRQ mode
 * In NORTOS,
 * - Here interrupt nesting is enabled
 * - This is the stack used by ISRs registered as type IRQ
 * In FreeRTOS,
 * - Here interrupt nesting is enabled
 * - This is stack that is used initally when a IRQ is received
 * - But then the mode is switched to SVC mode and SVC stack is used for all user ISR callbacks
 * - Hence in FreeRTOS, IRQ stack size is less and SVC stack size is more
 */
__IRQ_STACK_SIZE = 4096;
/* This is the size of stack when R5 is in IRQ mode
 * - In both NORTOS and FreeRTOS nesting is disabled for FIQ
 */
__FIQ_STACK_SIZE = 256;
__SVC_STACK_SIZE = 4096; /* This is the size of stack when R5 is in SVC mode */
__ABORT_STACK_SIZE = 256;  /* This is the size of stack when R5 is in ABORT mode */
__UNDEFINED_STACK_SIZE = 256;  /* This is the size of stack when R5 is in UNDEF mode */



SECTIONS
{
    .sbl_init_code  : {
        *(.vectors). = align(8);
    } load= MSRAM_VECS , run= R5F_VECS , align(8) 


    GROUP  :   {
    .text : {
    } palign(8)
    .text.hwi : {
    } palign(8)
    .text.cache : {
    } palign(8)
    .text.mpu : {
    } palign(8)
    .text.boot : {
    } palign(8)
    .data : {
    } palign(8)
    .rodata : {
    } palign(8)
    } > MSRAM_0  

    .bss  : {
    } > MSRAM_0   , palign(8) 
    RUN_START(__BSS_START)
    RUN_END(__BSS_END)

    .sysmem  : {
    } > MSRAM_0   , palign(8) 

    .stack  : {
    } > MSRAM_0   , palign(8) 


    GROUP  :   {
    .irqstack : {
        . = . + __IRQ_STACK_SIZE;
    } align(8)
    RUN_START(__IRQ_STACK_START)
    RUN_END(__IRQ_STACK_END)
    .fiqstack : {
        . = . + __FIQ_STACK_SIZE;
    } align(8)
    RUN_START(__FIQ_STACK_START)
    RUN_END(__FIQ_STACK_END)
    .svcstack : {
        . = . + __SVC_STACK_SIZE;
    } align(8)
    RUN_START(__SVC_STACK_START)
    RUN_END(__SVC_STACK_END)
    .abortstack : {
        . = . + __ABORT_STACK_SIZE;
    } align(8)
    RUN_START(__ABORT_STACK_START)
    RUN_END(__ABORT_STACK_END)
    .undefinedstack : {
        . = . + __UNDEFINED_STACK_SIZE;
    } align(8)
    RUN_START(__UNDEFINED_STACK_START)
    RUN_END(__UNDEFINED_STACK_END)
    } > MSRAM_0  

    .bss.filebuf (NOLOAD) : {
    } > MSRAM_1    

    .rodata.hsmrt  : {
    } > MSRAM_HSMRT   , align(8) 


}


MEMORY
{
    R5F_VECS   : ORIGIN = 0x0 , LENGTH = 0x100 
    R5F_TCMA   : ORIGIN = 0x100 , LENGTH = 0x7F00 
    R5F_TCMB0   : ORIGIN = 0x80000 , LENGTH = 0x8000 
    MSRAM_VECS   : ORIGIN = 0x70002000 , LENGTH = 0x100 
    MSRAM_0   : ORIGIN = 0x70002100 , LENGTH = 0x44F00 
    MSRAM_1   : ORIGIN = 0x70087000 , LENGTH = 0x1C0000 
    MSRAM_HSMRT   : ORIGIN = 0x70047000 , LENGTH = 0x40000 

    /* For memory Regions not defined in this core but shared by other cores with the current core */


}
