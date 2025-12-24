#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

// 1. 内核加载的物理基地址
#define KERNBASE 0x00200000L

// 2. 物理内存结束地址
#define PHYSTOP (KERNBASE + 128*1024*1024) 

// 3. 页大小
#define PGSIZE 4096

// 4. 串口地址
#define UART0 0x91400000L

// [修改] K230 手册中 UART0 的 Interrupt Bit 是 0
// RISC-V PLIC 中，Source ID 0 保留，因此 ID = Bit + 1
#define UART0_IRQ 16

// K230 C908 PLIC 地址配置
// 物理地址在 0x0f00000000，Sv39 无法直接映射
// 我们将其映射到虚拟地址 0x10000000
#define PLIC_PA             0x0f00000000L  // 物理基地址
#define PLIC                0x10000000L    // 虚拟基地址（内核访问用这个）

// [新增] K230 特有的 PLIC 权限控制寄存器
// 物理偏移 0x01FFFFC -> 虚拟地址偏移相同
#define PLIC_CTRL           (PLIC + 0x01FFFFCL)

// PLIC 寄存器偏移（相对于 PLIC 虚拟地址）
// 优先级寄存器：Source 1 对应偏移 0x4
#define PLIC_PRIORITY       (PLIC + 0x0) 
#define PLIC_PENDING        (PLIC + 0x1000)

// S-Mode Enable (Context 1)
// C908: M-mode Enable @ 0x2000, 步进 0x80 -> S-mode @ 0x2080
#define PLIC_SENABLE(hart)  (PLIC + 0x2080 + (hart)*0x100)

// S-Mode Threshold & Claim (Context 1)
// C908 手册明确指出的偏移
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart)*0x2000)

// 6. 虚拟内存映射
#define KERN_VIRT_BASE 0x80000000L

// 7. 用户栈和陷阱帧位置
#define TRAMPOLINE (MAXVA - PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// 8. 看门狗定时器寄存器地址
// K230 WDT0 寄存器基地址 (参考你提供的地址映射表)
#define WDT0_BASE         0x91106000L

// 寄存器偏移量定义 (参考手册 2.7.5 Register Summary)
#define WDT_CR_OFFSET     0x00    // Control Register
#define WDT_TORR_OFFSET   0x04    // Timeout Range Register
#define WDT_CCVR_OFFSET   0x08    // Current Counter Value Register
#define WDT_CRR_OFFSET    0x0c    // Counter Restart Register

// 将偏移量转换为可以直接访问的 volatile 指针
#define WDT_CR           ((volatile uint32 *)(WDT0_BASE + WDT_CR_OFFSET))
#define WDT_TORR         ((volatile uint32 *)(WDT0_BASE + WDT_TORR_OFFSET))
#define WDT_CCVR         ((volatile uint32 *)(WDT0_BASE + WDT_CCVR_OFFSET))
#define WDT_CRR          ((volatile uint32 *)(WDT0_BASE + WDT_CRR_OFFSET))

// 关键位与常量定义 (参考手册 2.7.6 Register Description)
#define WDT_CR_ENABLE    (1 << 0)   // WDT_EN: 1=开启, 0=关闭
#define WDT_CR_RMOD_RST  (0 << 1)   // RMOD: 0=System Reset, 1=Interrupt
#define WDT_CR_RPL_16    (0x3 << 2) // RPL: Reset Pulse Length (默认16个时钟)

#define WDT_CRR_MAGIC    0x76       // 必须写入 0x76 才能重启/激活计数器


#endif // MEMLAYOUT_H