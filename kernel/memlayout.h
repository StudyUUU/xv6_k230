#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * xv6-k230 内存布局定义
 * * 对应架构图：[image_b2c52e.png]
 * * 核心设计哲学：
 * 1. 低端内存 (Identity Mapping): VA = PA。用于内核自身的代码、数据运行，以及驱动访问硬件。
 * 2. 高端内存 (High Mapping):    VA != PA。用于进程隔离机制（Trampoline, Trapframe, Kernel Stacks）。
 */

// ============================================================================
// 1. 物理内存布局 (对应图中右侧 Physical Address 区域)
// ============================================================================

// [图中右下角] 内核加载基地址
// U-Boot 将 kernel.bin 加载到物理内存的 0x00200000 处
// 内核的第一条指令 _entry 就在这里
#define KERNBASE 0x00200000L

// [图中左侧中部文字] 物理内存结束地址
// 除去为opensbi预留2MB，K230拥有约 1022MB 可用内存，从 KERNBASE 延伸到 PHYSTOP
#define PHYSTOP (KERNBASE + 1022*1024*1024) 

// 页面大小 (4KB)
#define PGSIZE 4096

// ============================================================================
// 2. 外设 MMIO (Identity Mapping 区)
//    这些地址既是物理地址，也是内核页表中映射的虚拟地址
// ============================================================================

// [图中中部] UART0 - DW8250 串口
// 物理地址：0x91400000
// 映射属性：PTE_R | PTE_W | PTE_IO (强顺序，无缓存)
#define UART0 0x91400000L

// UART0 中断号 (PLIC Source ID)
#define UART0_IRQ 16

// [图中中部] PLIC - 平台级中断控制器
// 物理地址：0x0f00000000 (36位地址)
// 注意：Sv39 虚拟地址空间有 39 位 (512GB)，足以覆盖 36 位物理地址。
//      因此我们可以直接做 Identity Mapping。
#define PLIC        0x0f00000000L

// --- PLIC 寄存器偏移定义 ---
// K230 特有：在 M-mode 下必须解锁此寄存器，S-mode 才能访问 PLIC
#define PLIC_CTRL           (PLIC + 0x01FFFFCL)

// 标准 PLIC 寄存器
#define PLIC_PRIORITY       (PLIC + 0x0) 
#define PLIC_PENDING        (PLIC + 0x1000)

// 上下文 Context 1 (对应 S-mode Hart 0)
// 偏移量基于 K230/C908 手册
#define PLIC_SENABLE(hart)  (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart)*0x2000)

// [图中中部] WDT0 - 看门狗
// 物理地址：0x91106000
#define WDT0_BASE 0x91106000L

// WDT 寄存器偏移
#define WDT_CR_OFFSET     0x00    
#define WDT_TORR_OFFSET   0x04    
#define WDT_CCVR_OFFSET   0x08    
#define WDT_CRR_OFFSET    0x0c    

// 访问宏
#define WDT_CR           ((volatile uint32 *)(WDT0_BASE + WDT_CR_OFFSET))
#define WDT_TORR         ((volatile uint32 *)(WDT0_BASE + WDT_TORR_OFFSET))
#define WDT_CCVR         ((volatile uint32 *)(WDT0_BASE + WDT_CCVR_OFFSET))
#define WDT_CRR          ((volatile uint32 *)(WDT0_BASE + WDT_CRR_OFFSET))

// WDT 配置常量
#define WDT_CR_ENABLE    (1 << 0)   
#define WDT_CR_RMOD_RST  (0 << 1)   
#define WDT_CR_RPL_16    (0x3 << 2) 
#define WDT_CRR_MAGIC    0x76       

// SD卡相关
#define K230_SD1 0x91581000L // SD控制器物理基地址

// ============================================================================
// 3. 虚拟地址空间高端映射 (对应图中左上角 Virtual Address 区域)
//    这些地址仅存在于虚拟空间，物理上映射到 kernel_heap 或 text 段
// ============================================================================

// MAXVA 定义在 riscv.h (Sv39 = 1L << 38)

// [图中左上角] Trampoline 页
// 虚拟地址：最高页 (MAXVA - PGSIZE)
// 物理来源：对应图中右下箭头 -> 指向 .text 段中的 trampoline 代码
// 作用：用户态/内核态切换的“跳板”，必须在固定位置
#define TRAMPOLINE (MAXVA - PGSIZE)

// [图中左上角] Trapframe 页
// 虚拟地址：TRAMPOLINE 下方
// 物理来源：对应图中右上箭头 -> 指向 kernel_heap 中动态分配的页
// 作用：保存进程进入内核时的用户寄存器
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// [图中左上角] 内核栈 (Kernel Stack)
// 虚拟地址：TRAMPOLINE 向下生长
// 物理来源：对应图中右侧箭头 -> 指向 kernel_heap 中动态分配的页
// 
// 关键结构：Guard Page (图中的 Guard)
// 每个栈之间有一个未映射的页 (PTE_V = 0)。
// 如果内核栈溢出，会访问到 Guard Page 触发 Page Fault，从而通过 Panic 保护系统。
// 
// 计算公式：
// p=0: Top = MAXVA - 2*PGSIZE, Stack = [Top-PGSIZE, Top]
// p=1: ... 向下偏移 2*PGSIZE (一个栈页 + 一个 Guard 页)
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

#endif // MEMLAYOUT_H