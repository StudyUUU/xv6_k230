#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void plicinit(void)
{
    printf("[plicinit] Start\n");
    
    // 0. 映射检查 (保留此安全检查)
    // 确保 PTE_THEAD_SO 生效，能够读写硬件寄存器
    volatile uint32 *prio_reg = (volatile uint32*)(PLIC_PRIORITY + UART0_IRQ * 4);
    uint32 test_val = 1;
    
    *prio_reg = test_val;
    uint32 read_back = *prio_reg;
    
    if (read_back != test_val) {
        printf("!!!!!! PLIC MAPPING FAIL !!!!!!\n");
        printf("Wrote: 0x%x, Read: 0x%x\n", test_val, read_back);
        panic("PLIC mapping");
    }
    printf("[plicinit] Mapping Check: OK\n");

    // 1. 设置 UART0 的优先级为 1
    // 此时 UART0_IRQ 应在 memlayout.h 中被修改为 16
    *(volatile uint32*)(PLIC_PRIORITY + UART0_IRQ * 4) = 1;
    
    printf("[plicinit] Done\n");
}

void plicinithart(void)
{
    printf("[plicinithart] Start\n");
    int hart = cpuid();

    // 2. 开启当前 Hart S-mode 的中断使能
    // 如果 UART0_IRQ 是 16，(1 << 16) 刚好在第一个寄存器范围内
    *(volatile uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ);

    // 3. 设置当前 Hart S-mode 的优先级阈值为 0
    *(volatile uint32*)PLIC_SPRIORITY(hart) = 0;
    
    printf("[plicinithart] Done\n");
}

int plic_claim(void)
{
    int hart = cpuid();
    int irq = *(volatile uint32*)PLIC_SCLAIM(hart);
    return irq;
}

void plic_complete(int irq)
{
    int hart = cpuid();
    *(volatile uint32*)PLIC_SCLAIM(hart) = irq;
}

// 简单的调试函数，用于确认状态 (可选保留)
void plic_dump_status(void)
{
    printf("\n=== PLIC STATUS ===\n");
    uint32 pending = *(volatile uint32*)PLIC_PENDING;
    printf("Pending [00-31]: 0x%x\n", pending);
    printf("UART IRQ (%d) Pending: %d\n", UART0_IRQ, (pending >> UART0_IRQ) & 1);
    printf("===================\n");
}

int plic_check_pending(void)
{
    return *(volatile uint32*)PLIC_PENDING;
}