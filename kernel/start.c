#include "types.h"
#include "riscv.h" // 包含所有寄存器宏和函数

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
    // -----------------------------------------------------------
    // 委托所有异常，但【排除】S-mode Ecall (bit 9)。
    // 即使我们用了 Sstc，也防止意外的 ecall 导致死循环。
    w_medeleg(0xffff & ~(1 << 9));
    
    // 委托所有中断 (软中断、时钟中断、外部中断)
    w_mideleg(0xffff);  

    // 3. 【K230 (C908) 核心特性配置】
    // -----------------------------------------------------------
    // 开启 MXSTATUS.CLINTEE (Bit 17): 允许 S-mode 响应 CLINT 中断
    uint64 mxs = r_mxstatus();
    mxs |= MXSTATUS_CLINTEE;
    mxs |= MXSTATUS_THEADISAEE; // 允许玄铁扩展指令
    w_mxstatus(mxs);

    // 开启 MENVCFG.STCE (Bit 63): Sstc 扩展
    // 允许 S-mode 直接写 stimecmp 寄存器，无需 SBI 调用
    uint64 menv = r_menvcfg();
    menv |= MENVCFG_STCE;
    w_menvcfg(menv);

    // 允许 S-mode 访问所有硬件计数器 (time, cycle...)
    w_mcounteren(0xffffffff);
    
    // 4. 跳转准备
    w_mepc((uint64)main);
    w_satp(0); // 暂时禁用 MMU

    // 5. 【PMP 物理内存保护配置】
    // -----------------------------------------------------------
    // K230 OpenSBI 锁定了 PMP Entry 0，我们使用 Entry 3 覆盖全内存
    
    // pmpaddr3 = -1 (覆盖所有 64位地址空间)
    asm volatile("csrw pmpaddr3, %0" : : "r" (-1ULL));

    // pmpcfg0: 配置 Entry 3 (Bits 24-31)
    // R/W/X 权限 + NAPOT 模式
    uint64 cfg = (PMP_R | PMP_W | PMP_X | PMP_A_NAPOT) << 24;
    w_pmpcfg0(cfg);

    // 6. 切换到 S-mode
    int id = r_mhartid();
    w_tp(id);

    uart_puts("mret to S-mode main\n"); 

    asm volatile("fence.i"); 
    asm volatile("mret"); 
}