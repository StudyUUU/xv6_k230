.section .text
.global _entry
_entry:
    # ============================================
    # 1. BSS 段清零 (仅限 Hart 0)
    # ============================================
    # 读取当前核心 ID
    csrr t0, mhartid
    # 如果不是 Hart 0，跳过清零步骤，直接去设置栈
    bnez t0, skip_bss_clear

    # 获取链接器脚本中定义的 BSS 起始 (edata) 和结束 (end) 地址
    la t1, edata
    la t2, end
    # 如果 BSS 为空 (edata >= end)，则跳过
    bge t1, t2, skip_bss_clear

clear_loop:
    sd zero, (t1)       # 将当前地址写入 0 (64位)
    addi t1, t1, 8      # 指针后移 8 字节
    blt t1, t2, clear_loop # 如果还没到结尾，继续循环

skip_bss_clear:
    # ============================================
    # 2. 设置 C 语言运行栈
    # ============================================
    # stack0 在 start.c 中声明
    # 也就是: sp = stack0 + (hartid + 1) * 4096
    
    la sp, stack0
    li a0, 1024*4       # 4096 字节
    
    csrr a1, mhartid    # 读取 hartid
    
    addi a1, a1, 1      # a1 = hartid + 1
    mul a0, a0, a1      # a0 = 4096 * (hartid + 1)
    add sp, sp, a0      # sp = stack0 + 偏移量
    
    # ============================================
    # 3. 跳转到 start.c
    # ============================================
    call start

spin:
    j spin