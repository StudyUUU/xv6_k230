#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * xv6-k230 内存布局定义
 * 
 * 包含：
 * 1. 物理内存布局（KERNBASE、PHYSTOP）
 * 2. 外设 MMIO 地址（UART0、PLIC、WDT）
 * 3. 虚拟地址空间布局（TRAMPOLINE、TRAPFRAME、KSTACK）
 * 4. 设备寄存器偏移和常量
 */

// ============================================================================
// 物理内存布局
// ============================================================================

// 内核加载的物理基地址（由 U-Boot 加载到此地址）
#define KERNBASE 0x00200000L

// 物理内存结束地址（内核管理 128MB RAM）
#define PHYSTOP (KERNBASE + 128*1024*1024) 

// 页面大小
#define PGSIZE 4096

// ============================================================================
// UART0 - DW8250 串口
// ============================================================================

// UART0 基地址（物理地址，identity 映射）
#define UART0 0x91400000L

// UART0 中断号
// K230 手册中 UART0 的 Interrupt Bit 是 0
// RISC-V PLIC 中，Source ID 0 保留，因此实际 IRQ = Bit + 1
#define UART0_IRQ 16

// ============================================================================
// PLIC - 平台级中断控制器
// ============================================================================

// --- PLIC 地址映射 ---
// K230 PLIC 物理地址在 0x0f00000000（36 位地址）
// Sv39 虚拟地址空间只有 39 位，无法直接映射
// 因此映射到虚拟地址 0x10000000（256MB 处）
#define PLIC_PA             0x0f00000000L  // 物理基地址
#define PLIC                0x10000000L    // 虚拟基地址（内核访问）

// --- K230 特有 PLIC 控制寄存器 ---
// K230 PLIC 在 M-mode 下默认锁定，必须在 M-mode 解锁后 S-mode 才能访问
// 控制寄存器物理偏移 0x01FFFFC
#define PLIC_CTRL           (PLIC + 0x01FFFFCL)

// --- PLIC 标准寄存器 ---
// 优先级寄存器：Source N 对应偏移 0x4 * N
#define PLIC_PRIORITY       (PLIC + 0x0) 

// 挂起寄存器：Source 1-31 对应 bit 1-31
#define PLIC_PENDING        (PLIC + 0x1000)

// S-Mode Enable 寄存器（Context 1）
// C908 手册：M-mode Enable @ 0x2000，步进 0x80
// S-mode Context 1 @ 0x2080
#define PLIC_SENABLE(hart)  (PLIC + 0x2080 + (hart)*0x100)

// S-Mode Threshold & Claim 寄存器（Context 1）
// C908 手册明确指出的偏移
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart)*0x2000)

// ============================================================================
// WDT0 - 看门狗定时器
// ============================================================================

// --- WDT0 基地址 ---
#define WDT0_BASE         0x91106000L

// --- WDT0 寄存器偏移（参考手册 2.7.5 Register Summary）---
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
// 虚拟地址空间布局
// ============================================================================

// --- 内核虚拟基地址（预留，暂未使用）---
#define KERN_VIRT_BASE 0x80000000L

// --- 用户地址空间高端映射 ---
// MAXVA 定义在 riscv.h 中（Sv39 最大虚拟地址）

// Trampoline 页（用户/内核共享代码，用于陷阱入口）
#define TRAMPOLINE (MAXVA - PGSIZE)

// Trapframe 页（每个进程的陷阱帧，存储用户寄存器）
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

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