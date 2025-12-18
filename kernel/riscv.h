#ifndef RISCV_H
#define RISCV_H

#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

// 1. 模式与状态寄存器 (mstatus / sstatus)
// mstatus 寄存器的位定义
#define MSTATUS_MPP_MASK (3L << 11) // 之前的模式
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // M-mode 中断使能

// sstatus 寄存器的位定义 (S-mode 状态)
#define SSTATUS_SPP (1L << 8)  // 之前的模式 (1=S, 0=U)
#define SSTATUS_SPIE (1L << 5) // 之前的模式中断使能
#define SSTATUS_SIE (1L << 1)  // S-mode 中断使能

//2. 页表映射相关的宏 (为 kvminit 做准备)
#define PGSIZE 4096 // 4KB 每页
#define PGSHIFT 12  // 2^12 = 4096

// 将地址向下对齐到页边界
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
// 将地址向上对齐到页边界
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

// 页表项 (PTE) 权限位
#define PTE_V (1L << 0) // 有效 (Valid)
#define PTE_R (1L << 1) // 可读
#define PTE_W (1L << 2) // 可写
#define PTE_X (1L << 3) // 可执行
#define PTE_U (1L << 4) // 用户可访问

// 新增下面两个定义
#define PTE_A (1L << 6) // Accessed (已访问)
#define PTE_D (1L << 7) // Dirty (已脏)

// 将物理地址转换为页表项中的物理页号 (PPN)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
// 从页表项中提取物理地址
#define PTE2PA(pte) (((pte) >> 10) << 12)

// 提取页表项中的标志位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 从虚拟地址中提取三级索引 (Sv39 模式: 9+9+9 位)
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT + (9*(level)))
#define PX(level, va)   ((((uint64)va) >> PXSHIFT(level)) & PXMASK)

// satp 寄存器：控制分页
// 8L << 60 代表使用 Sv39 模式
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// 3.读取/写入 CSR 寄存器的内联函数
static inline uint64 r_mstatus() {
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void w_mstatus(uint64 x) {
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64 r_satp() {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline void w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r" (x));
}

// 刷新 TLB (虚拟地址转换缓存)
static inline void sfence_vma() {
  asm volatile("sfence.vma zero, zero");
}

// 4. 给 kalloc.c 用的辅助定义
// 物理地址类型定义
typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 页表其实就是一个页表项数组

// 
static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}
static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}
// Supervisor Trap Cause
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}
// Supervisor Trap Value
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}
static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}
// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}
static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}
#endif // RISCV_H