// this asm is linux aarch64

.global _start
.section .text

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
    adr x1, msg1
    mov x2, #12     // TODO: I think a good thing if i can implement strlen
    bl write
    
    adr x1, msg2
    mov x2, #16
    bl write

    cmp x0, #0      // if (x0 != 0) return 1;
    blt exit_failure

    b exit_success

.section .rodata

msg1:
    .ascii "Hello World\n"

msg2:
    .ascii "Hello Everybody\n"
