.syntax unified
.arch armv7e-m
.thumb

.section .text.Reset_Handler,"ax",%progbits
.align 2
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    cpsid i
    ldr r0, =0xe000ed08
    ldr r1, =__isr_vector
    str r1, [r0]
    ldr r2, [r1]
    msr msp, r2
    bl SystemInit

    ldr r1, =__etext
    ldr r2, =__data_start__
    ldr r3, =__data_end__
1:
    cmp r2, r3
    bge 2f
    ldr r0, [r1], #4
    str r0, [r2], #4
    b 1b
2:
    ldr r1, =__bss_start__
    ldr r2, =__bss_end__
    movs r0, #0
3:
    cmp r1, r2
    bge 4f
    str r0, [r1], #4
    b 3b
4:
    b firmware_main
.size Reset_Handler, . - Reset_Handler
