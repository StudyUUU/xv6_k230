#include "kernel/syscall.h"

# -------------------------------------------------------------------
# K230 Phase 2: Exec "/init"
# Goal: Load and run the C-based init program
# -------------------------------------------------------------------

.globl start
start:
    # exec("/init", argv)
    # a0 = path
    # a1 = argv array
    
    la a0, init_path
    la a1, argv
    li a7, SYS_exec
    ecall

    # ---------------------------------------------------------------
    # 如果 exec 成功，CPU 会跳转到 /init 的入口点，
    # 永远不会执行到这里。
    # ---------------------------------------------------------------

    # 如果代码走到这，说明 exec 失败了 (比如找不到文件，或者格式不对)
    li a0, 1
    la a1, exec_fail_msg
    li a2, 12
    li a7, SYS_write
    ecall

exit_loop:
    li a0, 0
    li a7, SYS_exit
    ecall
    j exit_loop

# ----------------------------------------------------------
# Data Section
# ----------------------------------------------------------
.data
.align 4

init_path:
    .string "/init\0"

exec_fail_msg:
    .string "Exec Fail!\n\0"

# argv 数组必须以 NULL (0) 结尾
# 在 64 位系统上，指针是 8 字节 (.quad)
.align 4
argv:
    .quad init_path   # argv[0] = "/init"
    .quad 0           # argv[1] = NULL