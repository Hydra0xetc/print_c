// this asm is linux aarch64

.global _start
.section .text

strlen:
    mov x0, #0
    mov x3, x1
.loop:
    ldrb w2, [x3]
    cbz w2, .done
    add x3, x3, #1
    add x0, x0, #1
    b .loop
.done:
    ret

exit_success:
    // exit(0)
    mov x0, #0      // exit 0 for success
    mov x8, #93     // syscall exit is 93

    svc #0

exit_failure:
    // exit(1)
    mov x0, #1          // exit 1 for failure 
    mov x8, #93

    svc #0

write:
    mov x0, #1      // stdout
    mov x8, #64     // syscall write is 64
    svc #0

    ret             // return x0

_start:
    ////////////////////////////////// 
    // msg1
    adr x1, msg1
    bl strlen
    mov x2, x0
    adr x1, msg1
    bl write

    cmp x0, #0      // if (x0 != 0) exit(1);
    blt exit_failure 
    // end of msg1
    ////////////////////////////////// 
    
    ////////////////////////////////// 
    // msg2
    adr x1, msg2
    bl strlen
    mov x2, x0
    adr x1, msg2
    bl write
    
    cmp x0, #0
    blt exit_failure
    // end of msg2
    //////////////////////////////////

    b exit_success

.section .rodata

msg1:
    .asciz "Hello World\n" // asciz make the string have null terminator

msg2:
    .asciz "Hello Everybody\n"
