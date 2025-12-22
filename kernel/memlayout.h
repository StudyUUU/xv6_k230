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

#endif // MEMLAYOUT_H