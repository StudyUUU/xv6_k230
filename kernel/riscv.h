#ifndef RISCV_H
#define RISCV_H

#include "types.h"

// ====================================================================
// 1. K230 / C908 特有扩展寄存器与常量
// ====================================================================
// 时钟频率 27MHz -> 10ms = 270,000
#define CLOCK_INTERVAL 270000

// MXSTATUS (Machine Extended Status) - 0x7C0
#define CSR_MXSTATUS        0x7C0
#define MXSTATUS_CLINTEE    (1L << 17) // 允许 S-mode 响应 CLINT 中断
#define MXSTATUS_MAEE       (1L << 21) // 扩展 MMU 属性
#define MXSTATUS_THEADISAEE (1L << 22) // 允许玄铁扩展指令集

// MENVCFG (Machine Environment Configuration) - 0x30A
#define CSR_MENVCFG         0x30A
#define MENVCFG_STCE        (1ULL << 63) // Sstc: S-mode Timer Compare Enable

// ====================================================================
// 2. 标准 RISC-V 状态寄存器位定义
// ====================================================================
// mstatus
#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_M    (3L << 11)
#define MSTATUS_MPP_S    (1L << 11)
#define MSTATUS_MPP_U    (0L << 11)
#define MSTATUS_MIE      (1L << 3)

// sstatus
#define SSTATUS_SPP      (1L << 8)
#define SSTATUS_SPIE     (1L << 5)
#define SSTATUS_SIE      (1L << 1)

// sie (Supervisor Interrupt Enable)
#define SIE_SSIE (1L << 1) // 软件中断
#define SIE_STIE (1L << 5) // 时钟中断
#define SIE_SEIE (1L << 9) // 外部中断

// ====================================================================
// 3. 分页与内存宏 (Paging)
// ====================================================================
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define PGSIZE 4096
#define PGSHIFT 12

#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // Valid
#define PTE_R (1L << 1) // Read
#define PTE_W (1L << 2) // Write
#define PTE_X (1L << 3) // Exec
#define PTE_U (1L << 4) // User
#define PTE_A (1L << 6) // Accessed
#define PTE_D (1L << 7) // Dirty

#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

#define PXMASK          0x1FF
#define PXSHIFT(level)  (PGSHIFT + (9*(level)))
#define PX(level, va)   ((((uint64)va) >> PXSHIFT(level)) & PXMASK)

#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// kalloc 辅助类型
typedef uint64 pte_t;
typedef uint64 *pagetable_t;

// ====================================================================
// 4. 内联汇编辅助函数 (CSR Read/Write)
// ====================================================================

// --- K230 特有寄存器操作 ---
static inline uint64 r_mxstatus() {
    uint64 x; asm volatile("csrr %0, %1" : "=r" (x) : "i" (CSR_MXSTATUS)); return x;
}
static inline void w_mxstatus(uint64 x) {
    asm volatile("csrw %0, %1" : : "i" (CSR_MXSTATUS), "r" (x));
}
static inline uint64 r_menvcfg() {
    uint64 x; asm volatile("csrr %0, %1" : "=r" (x) : "i" (CSR_MENVCFG)); return x;
}
static inline void w_menvcfg(uint64 x) {
    asm volatile("csrw %0, %1" : : "i" (CSR_MENVCFG), "r" (x));
}
// Sstc: 直接写 stimecmp，不产生 ecall
static inline void w_stimecmp(uint64 x) {
    asm volatile("csrw 0x14D, %0" : : "r" (x));
}
static inline uint64 r_stimecmp() {
    uint64 x; asm volatile("csrr %0, 0x14D" : "=r" (x)); return x;
}

// --- 标准寄存器操作 ---
static inline uint64 r_mstatus() { uint64 x; asm volatile("csrr %0, mstatus" : "=r" (x) ); return x; }
static inline void w_mstatus(uint64 x) { asm volatile("csrw mstatus, %0" : : "r" (x)); }
static inline void w_mepc(uint64 x) { asm volatile("csrw mepc, %0" : : "r" (x)); }
static inline void w_satp(uint64 x) { asm volatile("csrw satp, %0" : : "r" (x)); }
static inline uint64 r_satp() { uint64 x; asm volatile("csrr %0, satp" : "=r" (x) ); return x; }
static inline void w_medeleg(uint64 x) { asm volatile("csrw medeleg, %0" : : "r" (x)); }
static inline void w_mideleg(uint64 x) { asm volatile("csrw mideleg, %0" : : "r" (x)); }
static inline void w_tp(uint64 x) { asm volatile("mv tp, %0" : : "r" (x)); }
static inline void w_pmpcfg0(uint64 x) { asm volatile("csrw pmpcfg0, %0" : : "r" (x)); }
static inline uint64 r_mhartid() { uint64 x; asm volatile("csrr %0, mhartid" : "=r" (x) ); return x; }
static inline void w_mcounteren(uint64 x) { asm volatile("csrw mcounteren, %0" : : "r" (x)); }

// S-mode Trap 相关
static inline uint64 r_sepc() { uint64 x; asm volatile("csrr %0, sepc" : "=r" (x) ); return x; }
static inline void w_sepc(uint64 x) { asm volatile("csrw sepc, %0" : : "r" (x)); }
static inline uint64 r_sstatus() { uint64 x; asm volatile("csrr %0, sstatus" : "=r" (x) ); return x; }
static inline void w_sstatus(uint64 x) { asm volatile("csrw sstatus, %0" : : "r" (x)); }
static inline uint64 r_scause() { uint64 x; asm volatile("csrr %0, scause" : "=r" (x) ); return x; }
static inline uint64 r_stval() { uint64 x; asm volatile("csrr %0, stval" : "=r" (x) ); return x; }
static inline uint64 r_stvec() { uint64 x; asm volatile("csrr %0, stvec" : "=r" (x) ); return x; }
static inline void w_stvec(uint64 x) { asm volatile("csrw stvec, %0" : : "r" (x)); }

static inline uint64 r_sie() { uint64 x; asm volatile("csrr %0, sie" : "=r" (x)); return x; }
static inline void w_sie(uint64 x) { asm volatile("csrw sie, %0" : : "r" (x)); }

// 杂项
static inline void sfence_vma() { asm volatile("sfence.vma zero, zero"); }
static inline uint64 r_time() { uint64 x; asm volatile("csrr %0, time" : "=r" (x)); return x; }

static inline void intr_on() { w_sstatus(r_sstatus() | SSTATUS_SIE); }
static inline void intr_off() { w_sstatus(r_sstatus() & ~SSTATUS_SIE); }

#endif // RISCV_H