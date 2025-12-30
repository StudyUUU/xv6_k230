        #
        # 在 S 模式（Supervisor）下发生的
        # 中断和异常都会进入这里
        #
        # 保存所有寄存器，调用 kerneltrap()，
        # 恢复寄存器后返回
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        // 为保存寄存器在栈上预留空间
        addi sp, sp, -256

        // 保存所有通用寄存器
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        // 调用 trap.c 中用 C 实现的内核异常处理函数
        call kerneltrap

        // 恢复所有寄存器
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        // 不恢复 tp，防止在处理中发生 CPU 切换
        // ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        // 回收为寄存器保存而分配的栈空间
        addi sp, sp, 256

        // 使用 sret 返回到进入内核前正在执行的地方
        sret
        