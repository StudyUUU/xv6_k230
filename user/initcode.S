#include "kernel/syscall.h"

# -------------------------------------------------------------------
# K230 File System Verification (Phase 1)
# Goal: Open "/README" -> Read -> Write to Console
# -------------------------------------------------------------------

.globl start
start:
    # ----------------------------------------------------------
    # 1. open("/README", O_RDONLY)
    # ----------------------------------------------------------
    la a0, file_path    # 参数0: 路径字符串地址
    li a1, 0            # 参数1: flags (O_RDONLY = 0)
    li a7, SYS_open     # 系统调用号: 15
    ecall
    
    # 检查返回值 (a0 是 fd)
    # 如果 a0 < 0，跳转到错误处理
    bltz a0, panic_open
    
    # 保存 fd 到 s0 寄存器 (callee-saved)
    mv s0, a0

    # ----------------------------------------------------------
    # 2. read(fd, buf, 128)
    # ----------------------------------------------------------
    mv a0, s0           # 参数0: fd
    la a1, buf          # 参数1: 缓冲区地址
    li a2, 128          # 参数2: 读取字节数
    li a7, SYS_read     # 系统调用号: 5
    ecall

    # 检查返回值 (a0 是读取的字节数)
    # 如果 a0 < 0，跳转到错误处理
    bltz a0, panic_read
    
    # 保存读取字节数到 s1
    mv s1, a0

    # ----------------------------------------------------------
    # 3. write(1, buf, count)  -> 打印读取到的内容
    # ----------------------------------------------------------
    li a0, 1            # 参数0: stdout (fd 1)
    la a1, buf          # 参数1: 缓冲区地址
    mv a2, s1           # 参数2: 刚才读取的长度
    li a7, SYS_write    # 系统调用号: 16
    ecall

    # ----------------------------------------------------------
    # 4. Success Marker (打印一个换行，表示结束)
    # ----------------------------------------------------------
    li a0, 1
    la a1, newline
    li a2, 1
    li a7, SYS_write
    ecall

    # ----------------------------------------------------------
    # 5. exit(0)
    # ----------------------------------------------------------
    li a0, 0
    li a7, SYS_exit
    ecall

# ----------------------------------------------------------
# 错误处理分支
# ----------------------------------------------------------
panic_open:
    li a0, 1
    la a1, msg_open_err
    li a2, 14           # 字符串长度
    li a7, SYS_write
    ecall
    j exit_fail

panic_read:
    li a0, 1
    la a1, msg_read_err
    li a2, 14
    li a7, SYS_write
    ecall
    j exit_fail

exit_fail:
    li a0, 1
    li a7, SYS_exit
    ecall

# ----------------------------------------------------------
# 数据段
# ----------------------------------------------------------
.data
.align 4

file_path:
    .string "/README\0"   # 尝试绝对路径

msg_open_err:
    .string "Open Failed!\n\0"

msg_read_err:
    .string "Read Failed!\n\0"

newline:
    .string "\n\0"

# 读取缓冲区 (放在最后)
buf:
    .space 128