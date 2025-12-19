#include "types.h"
#include "riscv.h"
#include "param.h"

// PMP 配置位定义 (仅在 start.c 中使用)
#define PMP_R       0x01
#define PMP_W       0x02
#define PMP_X       0x04
#define PMP_A_NAPOT 0x18

__attribute__ ((aligned (16))) char stack0[4096 * NCPU];  // 扩大栈空间

// 外部函数声明
void uart_puts(char *s);
void main();

// 同步变量：标记主核是否完成了共享资源初始化
volatile int started = 0;

void start()
{
    int hartid = r_mhartid();
    
    if (hartid == 0) {
        uart_puts("Hart 0: M-mode start()\n");
    }

    // 1. 设置 M-mode 状态 -> 切到 S-mode
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 2. 异常与中断委托
    w_medeleg(0xffff & ~(1 << 9));
    w_mideleg(0xffff);  

    // 3. 【K230 (C908) 核心特性配置】
    uint64 mxs = r_mxstatus();
    mxs |= MXSTATUS_CLINTEE;
    mxs |= MXSTATUS_THEADISAEE;
    w_mxstatus(mxs);

    uint64 menv = r_menvcfg();
    menv |= MENVCFG_STCE;
    w_menvcfg(menv);

    w_mcounteren(0xffffffff);
    
    // 4. 跳转准备
    w_mepc((uint64)main);
    w_satp(0);

    // 5. 【PMP 物理内存保护配置】
    asm volatile("csrw pmpaddr3, %0" : : "r" (-1ULL));
    uint64 cfg = (PMP_R | PMP_W | PMP_X | PMP_A_NAPOT) << 24;
    w_pmpcfg0(cfg);

    // 6. 设置 tp 为 hartid（在 S-mode 下作为 CPU ID）
    w_tp(hartid);

    if (hartid == 0) {
        uart_puts("Hart 0: mret to S-mode main\n");
    }

    asm volatile("fence.i"); 
    asm volatile("mret"); 
}

// 在 start.c 添加
void start_other_cores()
{
    // K230 使用 SBI 的 HSM 扩展启动其他核心
    // SBI_HSM_START: ecall with a7=0x48534D (HSM), a6=0 (start)
    
    extern void _entry();  // entry.S 的入口
    
    for (int hart = 1; hart < NCPU; hart++) {
        // 通过 SBI HSM 启动核心
        // a7 = 0x48534D (HSM extension)
        // a6 = 0 (start function)
        // a0 = hartid
        // a1 = start_addr (物理地址)
        // a2 = opaque (传递给核心的参数)
        
        register uint64 a7 asm("a7") = 0x48534D;  // SBI_EXT_HSM
        register uint64 a6 asm("a6") = 0;         // SBI_HSM_START
        register uint64 a0 asm("a0") = hart;      // hartid
        register uint64 a1 asm("a1") = (uint64)_entry;  // 入口地址
        register uint64 a2 asm("a2") = 0;         // opaque
        
        asm volatile("ecall" 
            : "+r"(a0) 
            : "r"(a1), "r"(a2), "r"(a6), "r"(a7)
            : "memory");
            
        if (a0 != 0) {
            uart_puts("Failed to start hart ");
            // 打印 hart 号
        }
    }
}