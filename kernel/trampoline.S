        #
        # 处理从用户态进入内核的 trap 的底层代码，
        # 以及从内核返回到用户态的代码。
        #
        # 内核在用户空间和内核空间中，
        # 都将保存本代码的那一页
        # 映射到相同的虚拟地址（TRAMPOLINE），
        # 这样在切换页表时这段代码仍然可以继续执行。
        #
        # kernel.ld 保证这段代码
        # 从一个页边界开始放置。
        #

#include "riscv.h"
#include "memlayout.h"

.section trampsec
.globl trampoline
.globl usertrap
trampoline:
.align 4
.globl uservec
uservec:    
        #
        # trap.c 会把 stvec 设置为这里，
        # 因此来自用户态的 trap 会从这里开始执行，
        # 此时已经处于 S 模式，
        # 但仍然使用的是用户页表。
        #

        # 将用户态的 a0 保存到 sscratch 中，
        # 这样 a0 就可以被用来访问 TRAPFRAME。
        csrw sscratch, a0

        # 每个进程都有自己独立的 p->trapframe 内存区域，
        # 但它在每个进程的用户页表中
        # 都映射到相同的虚拟地址（TRAPFRAME）。
        li a0, TRAPFRAME
        
        # 将用户态的寄存器保存到 TRAPFRAME 中
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

        # 将用户态的 a0 保存到 p->trapframe->a0
        csrr t0, sscratch
        sd t0, 112(a0)

        # 从 p->trapframe->kernel_sp 中
        # 初始化内核栈指针
        ld sp, 8(a0)

        # 从 p->trapframe->kernel_hartid 中
        # 让 tp 保存当前 hartid
        ld tp, 32(a0)

        # 从 p->trapframe->kernel_trap 中
        # 取出 usertrap() 的地址
        ld t0, 16(a0)

        # 从 p->trapframe->kernel_satp 中
        # 取出内核页表地址
        ld t1, 0(a0)

        # 等待之前的所有内存操作完成，
        # 确保它们使用的是用户页表
        sfence.vma zero, zero

        # 安装内核页表
        csrw satp, t1

        # 刷新 TLB 中已经失效的用户态映射
        sfence.vma zero, zero

        # 调用 usertrap()
        jalr t0

.globl userret
userret:
        # usertrap() 返回到这里，
        # 此时 a0 中保存的是用户态的 satp。
        # 从内核返回到用户态。

        # 切换回用户页表
        sfence.vma zero, zero
        csrw satp, a0
        sfence.vma zero, zero

        li a0, TRAPFRAME

        # 从 TRAPFRAME 中恢复除 a0 以外的所有寄存器
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

        # 恢复用户态的 a0
        ld a0, 112(a0)
        
        # 返回到用户态并跳转到用户 PC
        # usertrapret() 已经设置好了 sstatus 和 sepc
        sret
