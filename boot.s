.global _start
.global _enable_interrupts
.global _disable_interrupts
.global _jump_to_task
.section __TEXT,__text

_start:
    // x0 = Core ID
    // x3 = Total Provisioned RAM (in bytes)
    mov x1, x3
    
    // Switch to EL1h (use SP_EL1) so we take interrupts using the SPx vector
    msr SPSel, #1
    
    // Trust the SP_EL1 injected by hv_daemon
    
    // Set up VBAR_EL1 to point to our vector table
    adr x2, vector_table
    msr vbar_el1, x2

    bl _kmain
    hvc #1

_enable_interrupts:
    msr daifclr, #2
    ret

_disable_interrupts:
    msr daifset, #2
    ret

_jump_to_task:
    // x0 contains the stack pointer of the task to jump to
    mov sp, x0
    // Restore ELR_EL1 and SPSR_EL1
    ldp x0, x1, [sp, #240]
    msr elr_el1, x0
    msr spsr_el1, x1
    
    // Restore all GPRs
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #256]
    add sp, sp, #272 // 34 slots * 8 bytes
    eret

.align 11
vector_table:
    // --- Current EL with SP0 (Exceptions while using SP_EL0) ---
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled

    // --- Current EL with SPx (Exceptions while using SP_EL1) ---
    .align 7
    b _asm_handle_sync
    .align 7
    b _asm_handle_irq
    .align 7
    b unhandled
    .align 7
    b unhandled

    // --- Lower EL ---
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled
    .align 7
    b unhandled

unhandled:
    hvc #2 // Crash VM

_asm_handle_sync:
    sub sp, sp, #272
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #256]

    // Always save ELR and SPSR
    mrs x0, ELR_EL1
    mrs x1, SPSR_EL1
    stp x0, x1, [sp, #240]


    mov x0, sp
    bl _handle_sync
    mov sp, x0

    ldp x0, x1, [sp, #240]
    msr ELR_EL1, x0
    msr SPSR_EL1, x1
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #256]
    add sp, sp, #272
    eret

_asm_handle_irq:
    // 1. Context Save
    sub sp, sp, #272 // 34 * 8 bytes
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    
    // Save ELR_EL1 and SPSR_EL1
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #240]
    
    // Save LR (x30)
    str x30, [sp, #256]
    
    // Tell daemon to deassert IRQ
    hvc #3

    // Call C scheduler!
    mov x0, sp // Pass current task's SP as first argument
    bl _handle_irq
    
    // _schedule returns the next task's SP in x0
    mov sp, x0
    
    // Context Restore
    ldp x0, x1, [sp, #240]
    msr elr_el1, x0
    msr spsr_el1, x1
    
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #256]
    
    add sp, sp, #272
    eret

