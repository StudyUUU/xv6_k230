.globl _start
_start:
    # fork
    li a7, 1          # SYS_fork
    ecall

exit:
    li a0, 0
    li a7, 2          # SYS_exit
    ecall
1:
    j 1b
