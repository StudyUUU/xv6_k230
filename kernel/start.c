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

__attribute__ ((aligned (16))) char stack0[4096 * NCPU]; // CPU0 内核栈， 每个 CPU 4KB

// 外部函数声明
void uart_puts(char *s);
void main();

void start()
{
    // uart_puts("we are in M-mode start()\n");

    // 1. 设置 M-mode 状态 -> 切到 S-mode
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 2. 异常与中断委托
    w_medeleg(0xffff & ~(1 << 9)); //Environment call from S-mode	M-mode (保留，用于 SBI)
    w_mideleg(0xffff);  
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // 3. 【K230 (C908) 核心特性配置】
    uint64 mxs = r_mxstatus(); 
    mxs |= MXSTATUS_CLINTEE; // 允许缓存一致性指令
    mxs |= MXSTATUS_THEADISAEE; // 允许 T-Head 自定义指令 (如 MMU 的 MAEE)
    w_mxstatus(mxs);

    // 开启 MENVCFG.STCE
    uint64 menv = r_menvcfg();
    menv |= MENVCFG_STCE; // 允许 S-mode 下直接操作 stimecmp 寄存器，用于控制定时器中断
    w_menvcfg(menv);

    w_mcounteren(0xffffffff);
    
    // 4. 解锁并验证 PLIC_CTRL (必须在 M-Mode 且无 Cache 干扰时进行)
    // 由于默认 PLIC_CTRL 寄存器在 S-mode 下是锁定的，必须写入 1 解锁 S-mode 访问
    volatile uint32 *plic_ctrl_pa = (uint32*)(PLIC + 0x01FFFFC);
    *plic_ctrl_pa = 1;
    
    // // 简单验证
    // if (*plic_ctrl_pa == 1) {
    //      uart_puts("PLIC: Control unlocked SUCCESS\n");
    // } else {
    //      uart_puts("PLIC: Control unlock FAILED (Is this real hardware?)\n");
    // }

    // 5. 跳转准备
    w_mepc((uint64)main); // 设置返回地址到 main 函数
    w_satp(0); // 清空当前页表，使用空页表切换到 S-mode

    // 6. PMP 配置
    // 如果 M-mode 没有显式配置 PMP 授权 S-mode 访问内存，那么 S-mode 访问任何物理地址都会触发 Access Fault
    asm volatile("csrw pmpaddr3, %0" : : "r" (-1ULL));
    uint64 cfg = (PMP_R | PMP_W | PMP_X | PMP_A_NAPOT) << 24;
    w_pmpcfg0(cfg);

	// 7.初始化定时器, 切换到 S-mode 后由 S-mode 定时器中断处理程序维护
	timerinit();

    // 8. 切换到 S-mode
    int id = r_mhartid();
    w_tp(id);

    // uart_puts("mret to S-mode main\n"); 

    asm volatile("fence.i"); 
    asm volatile("mret"); // mret 指令实现的是从异常中返回，恢复到 mepc 指向的地址，并切换到 mstatus.MPP 中指定的特权级别
}