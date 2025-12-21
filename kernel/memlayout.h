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
#define UART0_IRQ 0

// K230 C908 PLIC 地址
// 物理地址在 0xf00000000，但 Sv39 无法直接映射
// 我们将其映射到虚拟地址 0x10000000
#define PLIC_PA             0x0f00000000L  // 物理地址
#define PLIC                0x10000000L    // 虚拟地址（内核访问用这个）

// PLIC 寄存器偏移（相对于 PLIC 虚拟地址）
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 6. 虚拟内存映射
#define KERN_VIRT_BASE 0x80000000L

#endif // MEMLAYOUT_H