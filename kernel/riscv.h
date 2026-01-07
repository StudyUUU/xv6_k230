#ifndef RISCV_H
#define RISCV_H

/*
 * xv6-k230 RISC-V 架构定义
 * 
 * 包含：
 * 1. K230/C908 特有扩展（玄铁核心、Sstc 定时器）
 * 2. RISC-V 标准特权级寄存器和位定义
 * 3. Sv39 虚拟内存管理（页表、PTE 标志）
 * 4. CSR 寄存器读写内联函数
 * 5. 常用操作宏（TLB 刷新、中断控制）
 */

// 汇编代码不需要包含 types.h
#ifndef __ASSEMBLER__
#include "types.h"
#endif

// ============================================================================
// K230/C908 特有扩展
// ============================================================================

// --- 时钟配置 ---
#define CLOCK_INTERVAL 270000  // 27MHz 时钟，10ms 间隔

// --- MXSTATUS (0x7C0) - 机器模式扩展状态寄存器 ---
#define CSR_MXSTATUS        0x7C0
#define MXSTATUS_CLINTEE    (1L << 17) // 允许 S-mode 响应 CLINT 中断
#define MXSTATUS_THEADISAEE (1L << 22) // 玄铁扩展指令集使能
#define MXSTATUS_MAEE       (1L << 21) // 扩展 MMU 属性使能（缓存和原子操作）

// --- MENVCFG (0x30A) - 机器环境配置寄存器 ---
#define CSR_MENVCFG         0x30A
#define MENVCFG_STCE        (1ULL << 63) // Sstc 扩展：允许 S-mode 使用 stimecmp

// ============================================================================
// RISC-V 标准特权级寄存器位定义
// ====================================================================

// --- Machine Status (mstatus) ---
#define MSTATUS_MPP_MASK (3L << 11)  // 前一特权级掩码
#define MSTATUS_MPP_M    (3L << 11)  // 机器模式
#define MSTATUS_MPP_S    (1L << 11)  // 监管模式
#define MSTATUS_MPP_U    (0L << 11)  // 用户模式
#define MSTATUS_MIE      (1L << 3)   // 机器中断使能

// --- Supervisor Status (sstatus) ---
#define SSTATUS_SPP      (1L << 8)   // 前一特权级 (0=User, 1=Supervisor)
#define SSTATUS_SPIE     (1L << 5)   // 异常前中断使能位
#define SSTATUS_SIE      (1L << 1)   // 监管模式中断使能

// --- Supervisor Interrupt Enable (sie) ---
#define SIE_SSIE (1L << 1) // 软件中断使能
#define SIE_STIE (1L << 5) // 时钟中断使能
#define SIE_SEIE (1L << 9) // 外部中断使能

// ============================================================================
// Sv39 虚拟内存管理
// ====================================================================

// --- 虚拟地址空间 ---
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1)) // Sv39: 2^38 字节 (256GB)
#define PGSIZE 4096                         // 页面大小 4KB
#define PGSHIFT 12                          // 页面偏移位数

// --- 页面对齐宏 ---
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

// --- 标准 PTE 标志位 ---
#define PTE_V (1L << 0) // Valid - 有效位
#define PTE_R (1L << 1) // Read - 可读
#define PTE_W (1L << 2) // Write - 可写
#define PTE_X (1L << 3) // Execute - 可执行
#define PTE_U (1L << 4) // User - 用户可访问
#define PTE_G (1L << 5) // Global - 全局映射
#define PTE_A (1L << 6) // Accessed - 已访问（K230 不自动设置，必须手动）
#define PTE_D (1L << 7) // Dirty - 已修改（K230 不自动设置，必须手动）

// --- T-Head C908 扩展 PTE 标志位 ---
// K230 的 C908 核心扩展了 PTE 高位（bit 59-63）用于内存属性控制
// 
// Bit 63: Strong Order (SO) - 强序访问，用于 MMIO 设备
// Bit 62: Cacheable (C) - 可缓存
// Bit 61: Bufferable (B) - 可缓冲
// Bit 60: Shareable (S) - 多核共享
// Bit 59: Secondary (Sec) - 次级属性
//
// MAEE (Memory Access Extension Enable) - 启用缓存和原子操作
#define PTE_THEAD_MAEE  ((1L << 62) | (1L << 61) | (1L << 60))

// SO (Strong Order) - 用于设备内存，禁用缓存和乱序
#define PTE_THEAD_SO   (1L << 63)

// PTE_IO - 设备内存专用标志（MMIO 如 UART、PLIC）
// 组合 R + W + SO，确保顺序访问且不缓存
#define PTE_IO         (PTE_R | PTE_W | PTE_THEAD_SO)

// --- PTE 与物理地址转换 ---
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)  // 物理地址 → PTE（[53:10]位）
// [修正] PTE 到 PA：增加掩码，过滤掉 C908 的高位属性 (MAEE)
// 0x3FFFFFFFFFFFFF 是 54 位的掩码 (Sv39 物理地址有效位)
// 这样可以把 Bit 54 以上的脏数据全部清零
#define PTE2PA(pte) ((((pte) >> 10) << 12) & 0x00FFFFFFFFFFFF00L) 
// 或者更严谨的 Sv39 写法 (保留低 56 位物理地址)：
// #define PTE2PA(pte) ((((pte) >> 10) & 0x0FFFFFFFFFFFL) << 12)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)           // 提取低 10 位标志

// --- Sv39 三级页表索引计算 ---
// 虚拟地址划分：[38:30]=L2, [29:21]=L1, [20:12]=L0, [11:0]=offset
#define PXMASK          0x1FF                    // 9 位索引掩码（2^9=512 项）
#define PXSHIFT(level)  (PGSHIFT + (9*(level)))  // 各级页表索引偏移
#define PX(level, va)   ((((uint64)va) >> PXSHIFT(level)) & PXMASK)

// --- SATP 寄存器配置 ---
#define SATP_SV39 (8L << 60)                     // Sv39 模式标志
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// ============================================================================
// C 语言专用部分
// ============================================================================
#ifndef __ASSEMBLER__

// --- 类型定义 ---
typedef uint64 pte_t;        // 页表项类型
typedef uint64 *pagetable_t; // 页表指针类型

// ============================================================================
// CSR 寄存器读写内联函数
// ============================================================================

// --- K230/C908 扩展寄存器 ---

// MXSTATUS - 机器模式扩展状态
static inline uint64 r_mxstatus() {
    uint64 x; asm volatile("csrr %0, %1" : "=r" (x) : "i" (CSR_MXSTATUS)); return x;
}
static inline void w_mxstatus(uint64 x) {
    asm volatile("csrw %0, %1" : : "i" (CSR_MXSTATUS), "r" (x));
}

// MENVCFG - 机器环境配置
static inline uint64 r_menvcfg() {
    uint64 x; asm volatile("csrr %0, %1" : "=r" (x) : "i" (CSR_MENVCFG)); return x;
}
static inline void w_menvcfg(uint64 x) {
    asm volatile("csrw %0, %1" : : "i" (CSR_MENVCFG), "r" (x));
}

// Sstc 扩展 - S-mode 定时器比较寄存器
// K230 支持 Sstc，可直接写 stimecmp (0x14D) 而无需 ecall
static inline void w_stimecmp(uint64 x) {
    asm volatile("csrw 0x14D, %0" : : "r" (x));
}
static inline uint64 r_stimecmp() {
    uint64 x; asm volatile("csrr %0, 0x14D" : "=r" (x)); return x;
}

// --- Machine 模式寄存器 ---

static inline uint64 r_mstatus() {
    uint64 x; asm volatile("csrr %0, mstatus" : "=r" (x)); return x;
}
static inline void w_mstatus(uint64 x) {
    asm volatile("csrw mstatus, %0" : : "r" (x));
}

static inline uint64 r_mhartid() {
    uint64 x; asm volatile("csrr %0, mhartid" : "=r" (x)); return x;
}

static inline void w_mepc(uint64 x) {
    asm volatile("csrw mepc, %0" : : "r" (x));
}

static inline void w_medeleg(uint64 x) {
    asm volatile("csrw medeleg, %0" : : "r" (x));
}

static inline void w_mideleg(uint64 x) {
    asm volatile("csrw mideleg, %0" : : "r" (x));
}

static inline void w_mcounteren(uint64 x) {
    asm volatile("csrw mcounteren, %0" : : "r" (x));
}

// PMP (物理内存保护)
static inline void w_pmpaddr0(uint64 x) {
    asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

static inline void w_pmpcfg0(uint64 x) {
    asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

// --- Supervisor 模式寄存器 ---

static inline uint64 r_sstatus() {
    uint64 x; asm volatile("csrr %0, sstatus" : "=r" (x)); return x;
}
static inline void w_sstatus(uint64 x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

static inline uint64 r_sepc() {
    uint64 x; asm volatile("csrr %0, sepc" : "=r" (x)); return x;
}
static inline void w_sepc(uint64 x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64 r_scause() {
    uint64 x; asm volatile("csrr %0, scause" : "=r" (x)); return x;
}

static inline uint64 r_stval() {
    uint64 x; asm volatile("csrr %0, stval" : "=r" (x)); return x;
}

static inline uint64 r_stvec() {
    uint64 x; asm volatile("csrr %0, stvec" : "=r" (x)); return x;
}
static inline void w_stvec(uint64 x) {
    asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64 r_sie() {
    uint64 x; asm volatile("csrr %0, sie" : "=r" (x)); return x;
}
static inline void w_sie(uint64 x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

static inline uint64 r_sip() {
    uint64 x; asm volatile("csrr %0, sip" : "=r" (x)); return x;
}
static inline void w_sip(uint64 x) {
    asm volatile("csrw sip, %0" : : "r" (x));
}

// SATP (地址翻译与保护)
static inline uint64 r_satp() {
    uint64 x; asm volatile("csrr %0, satp" : "=r" (x)); return x;
}
static inline void w_satp(uint64 x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

// --- 其他寄存器 ---

// 线程指针 (tp) - 用于存储 hart ID
static inline uint64 r_tp() {
    uint64 x; asm volatile("mv %0, tp" : "=r" (x)); return x;
}
static inline uint64 r_sp(){
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}
static inline void w_tp(uint64 x) {
    asm volatile("mv tp, %0" : : "r" (x));
}

// 时间计数器 (time) - 读取当前时间
static inline uint64 r_time() {
    uint64 x; asm volatile("csrr %0, time" : "=r" (x)); return x;
}

// ============================================================================
// 常用操作宏
// ============================================================================

// --- TLB 刷新 ---
// 刷新所有地址空间的 TLB 条目
static inline void sfence_vma() {
    asm volatile("sfence.vma zero, zero");
}

// --- 中断控制 ---

// 启用中断
static inline void intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 关闭中断
static inline void intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// 查询中断是否启用
static inline int intr_get() {
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

#endif // __ASSEMBLER__

#endif // RISCV_H