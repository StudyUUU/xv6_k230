#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// PMP 配置位定义 (仅在 start.c 中使用)
#define PMP_R       0x01
#define PMP_W       0x02
#define PMP_X       0x04
#define PMP_A_NAPOT 0x18

__attribute__ ((aligned (16))) char stack0[4096];

// 外部函数声明
void uart_puts(char *s);
void main();

void start()
{
    uart_puts("we are in M-mode start()\n");

    // 1. 设置 M-mode 状态 -> 切到 S-mode
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 2. 异常与中断委托
    w_medeleg(0xffff & ~(1 << 9));
    w_mideleg(0xffff);  
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // 3. 【K230 (C908) 核心特性配置】
    uint64 mxs = r_mxstatus();
    mxs |= MXSTATUS_CLINTEE;
    mxs |= MXSTATUS_THEADISAEE; // 允许玄铁扩展指令
    w_mxstatus(mxs);

    // 开启 MENVCFG.STCE
    uint64 menv = r_menvcfg();
    menv |= MENVCFG_STCE;
    w_menvcfg(menv);

    w_mcounteren(0xffffffff);
    
    // 4. [关键步骤] 解锁并验证 PLIC_CTRL (必须在 M-Mode 且无 Cache 干扰时进行)
    volatile uint32 *plic_ctrl_pa = (uint32*)(PLIC_PA + 0x01FFFFC);
    *plic_ctrl_pa = 1;
    
    // // 简单验证
    // if (*plic_ctrl_pa == 1) {
    //      uart_puts("PLIC: Control unlocked SUCCESS\n");
    // } else {
    //      uart_puts("PLIC: Control unlock FAILED (Is this real hardware?)\n");
    // }

    // 5. 跳转准备
    w_mepc((uint64)main);
    w_satp(0);

    // 6. PMP 配置
    asm volatile("csrw pmpaddr3, %0" : : "r" (-1ULL));
    uint64 cfg = (PMP_R | PMP_W | PMP_X | PMP_A_NAPOT) << 24;
    w_pmpcfg0(cfg);

	// 7.初始化定时器, 切换到 S-mode 后由 S-mode 定时器中断处理程序维护
	timerinit();

    // 8. 切换到 S-mode
    int id = r_mhartid();
    w_tp(id);

    uart_puts("mret to S-mode main\n"); 

    asm volatile("fence.i"); 
    asm volatile("mret"); 
}