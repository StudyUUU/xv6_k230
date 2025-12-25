.section .text
.global _entry
# 【优化 1】防止链接器通过 gp 寄存器进行短地址优化
# 在启动早期 gp 未初始化，Relaxation 可能导致地址计算错误
.option norelax 

_entry:
    # ============================================
    # 1. 通用寄存器设置
    # ============================================
    # 【优化 2】将 hartid 提前保存到 tp (Thread Pointer) 寄存器
    # xv6 约定：tp 始终保存 mhartid，便于 C 语言中快速获取 CPU ID
    csrr tp, mhartid

    # ============================================
    # 2. 设置栈 (所有核心都需要)
    # ============================================
    # 逻辑优化：先设栈，再做其他判断，逻辑更统一
    la sp, stack0
    
    # 【优化 3】使用移位指令 (slli) 代替乘法 (mul)
    # 4096 = 2^12。位运算比乘法指令周期更短，且不需要依赖 M 扩展单元
    # (hartid + 1) << 12
    addi t0, tp, 1
    slli t0, t0, 12  
    add sp, sp, t0

    # ============================================
    # 3. BSS 段清零 (仅限 Hart 0)
    # ============================================
    # 如果不是 Hart 0，跳过清零
    bnez tp, skip_bss_clear

    la t1, edata
    la t2, end
    bge t1, t2, skip_bss_clear

clear_loop:
    sd zero, (t1)
    addi t1, t1, 8
    blt t1, t2, clear_loop

    # 【优化 4】内存屏障 (Memory Barrier)
    # 确保 BSS 清零的操作对后续指令（以及其他核心）可见
    fence w, w

skip_bss_clear:
    # ============================================
    # 4. 跳转到 start.c
    # ============================================
    call start
// 如果 start 返回，进入死循环
spin:
    j spin