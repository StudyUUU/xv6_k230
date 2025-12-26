#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * xv6-k230 内存布局 (Linux 风格线性映射版)
 * * 策略：
 * 1. 用户空间：占据低 256GB (0x0000000000 -> 0x003FFFFFFFFF)
 * 2. 内核空间：占据高 256GB (0xFFFFFFC000... -> Top)
 * 3. 映射方式：所有物理地址(RAM & IO) 均通过线性偏移映射到内核空间
 */

// ============================================================================
// 1. 核心转换宏 (The Magic Offset)
// ============================================================================

// Sv39 高半区起始地址 (也就是内核空间的起点)
// Offset = 0xFFFFFFC000000000
#define KERN_VIRT_BASE   0xFFFFFFC000000000L

// 物理转虚拟 (PA -> VA)
#define P2V(pa)          ((uint64)(pa) + KERN_VIRT_BASE)

// 虚拟转物理 (VA -> PA)
#define V2P(va)          ((uint64)(va) - KERN_VIRT_BASE)

// ============================================================================
// 2. 物理内存定义
// ============================================================================

// 物理基地址
#define KERNBASE_PA      0x00200000L // DDR从0x00000000L开始，但预留2MB给 底层的 OpenSBI

// 物理大小 (1GB)
#define PHYSTOP_PA       (KERNBASE_PA + 1022*1024*1024) // 1GB 内存上限，但是前面预留了 2MB 给OpenSBI，所以是 1022MB 给 xv6 使用

// 内核代码的虚拟地址 (给代码中的链接符号用)
#define KERNBASE         P2V(KERNBASE_PA)
#define PHYSTOP          P2V(PHYSTOP_PA)

// 页面大小
#define PGSIZE           4096

// ============================================================================
// 3. 外设 MMIO (通过 P2V 映射到高位)
// ============================================================================
// 这种方式下，内核通过高位虚拟地址访问设备，彻底避开用户低位空间

// --- UART0 ---
#define UART0_PA         0x91400000L
#define UART0            P2V(UART0_PA)  // VA = 0xFFFFFFC091400000
#define UART0_IRQ        16

// --- PLIC ---
// K230 PLIC 物理地址: 0x0F00000000 (约 60GB)
// Sv39 内核窗口大小: 256GB
// 60GB < 256GB，所以直接线性映射是安全的，不会溢出！
#define PLIC_PA          0x0f00000000L
#define PLIC             P2V(PLIC_PA)   // VA = 0xFFFFFFCF00000000 只在内核空间（高半区）的四分之一处

// PLIC 寄存器 (基于高位虚拟地址计算)
#define PLIC_CTRL        (PLIC + 0x01FFFFCL)
#define PLIC_PRIORITY    (PLIC + 0x0)
#define PLIC_PENDING     (PLIC + 0x1000)
#define PLIC_SENABLE(h)  (PLIC + 0x2080 + (h)*0x100)
#define PLIC_SPRIORITY(h)(PLIC + 0x201000 + (h)*0x2000)
#define PLIC_SCLAIM(h)   (PLIC + 0x201004 + (h)*0x2000)

// --- WDT0 ---
#define WDT0_BASE_PA          0x91106000L
#define WDT0_BASE        P2V(WDT0_BASE_PA)

// --- WDT0 寄存器偏移
#define WDT_CR_OFFSET     0x00    // Control Register
#define WDT_TORR_OFFSET   0x04    // Timeout Range Register
#define WDT_CCVR_OFFSET   0x08    // Current Counter Value Register
#define WDT_CRR_OFFSET    0x0c    // Counter Restart Register

// --- WDT0 寄存器访问宏 ---
#define WDT_CR           ((volatile uint32 *)(WDT0_BASE + WDT_CR_OFFSET))
#define WDT_TORR         ((volatile uint32 *)(WDT0_BASE + WDT_TORR_OFFSET))
#define WDT_CCVR         ((volatile uint32 *)(WDT0_BASE + WDT_CCVR_OFFSET))
#define WDT_CRR          ((volatile uint32 *)(WDT0_BASE + WDT_CRR_OFFSET))

// --- WDT0 控制寄存器位定义（参考手册 2.7.6 Register Description）---
#define WDT_CR_ENABLE    (1 << 0)   // WDT_EN: 1=启用, 0=禁用
#define WDT_CR_RMOD_RST  (0 << 1)   // RMOD: 0=系统复位, 1=中断
#define WDT_CR_RPL_16    (0x3 << 2) // RPL: 复位脉冲长度（默认 16 时钟周期）

// --- WDT0 魔术值 ---
#define WDT_CRR_MAGIC    0x76       // 必须写入 0x76 才能重启/激活计数器

// ============================================================================
// 4. 特殊页面 (保持在虚拟空间的最顶端)
// ============================================================================
#define TRAMPOLINE       (MAXVA - PGSIZE)
#define TRAPFRAME        (TRAMPOLINE - PGSIZE)

// --- 内核栈布局 ---
// 每个进程的内核栈映射在 trampoline 下方
// 每个栈大小 PGSIZE，下方有 guard page（未映射）用于检测栈溢出
// 
// 布局示例（从高到低）：
// TRAMPOLINE (MAXVA - PGSIZE)
// TRAPFRAME  (MAXVA - 2*PGSIZE)
// [guard]    (MAXVA - 3*PGSIZE) - 未映射
// kstack[0]  (MAXVA - 4*PGSIZE)
// [guard]    (MAXVA - 5*PGSIZE) - 未映射
// kstack[1]  (MAXVA - 6*PGSIZE)
// ...
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

#endif // MEMLAYOUT_H