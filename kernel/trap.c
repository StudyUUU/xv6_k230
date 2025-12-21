#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern void kernelvec();

// 全局中断统计计数器
// 由于已经在 entry.S 中实现了 BSS 清零，这些变量在启动时会自动为 0
uint64 uart_intr_count;
uint64 timer_intr_count;
static uint64 last_timer_count;

void trap_init(void) {
    // 设置内核态的中断向量入口
    w_stvec((uint64)kernelvec);
    
    // 初始化统计变量
    uart_intr_count = 0;
    timer_intr_count = 0;
    last_timer_count = 0;
}

// 内核态中断/异常入口程序
void kerneltrap() {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    // 检查是否为中断 (scause 最高位为 1)
    if(scause & 0x8000000000000000L) {
        uint64 which_int = scause & 0xff;
        
        // -------------------------------------------------------
        // 1. 处理 Supervisor Timer Interrupt (时钟中断，scause = 5)
        // -------------------------------------------------------
        if(which_int == 5) { 
            // 重新设置下一次时钟中断的时间 (K230 STIMECMP)
            set_timer(r_time() + CLOCK_INTERVAL);
            timer_intr_count++;

            // 定期打印中断统计信息（约每 3 秒一次）
            if(timer_intr_count - last_timer_count >= 300) {
                printf("[Stats] Timer: %ld | UART IRQs: %ld\n", timer_intr_count, uart_intr_count);
                last_timer_count = timer_intr_count;
            }
            
            // 以后在此处可以添加 yield() 进行进程调度
        } 
        // -------------------------------------------------------
        // 2. 处理 Supervisor External Interrupt (外部中断，scause = 9)
        // -------------------------------------------------------
        else if(which_int == 9) {
            // 从 PLIC 获取当前触发的中断号
            int irq = plic_claim();
            
            if(irq == UART0_IRQ) {
                // 如果是 UART0 (IRQ 16)
                uartintr();
                uart_intr_count++;
            } 
            else if(irq != 0) {
                // 处理非预期的其他硬件中断
                printf("unexpected external interrupt: irq=%d\n", irq);
            }
            
            // 告诉 PLIC 该中断已处理完成
            if(irq) {
                plic_complete(irq);
            }
        } 
        // -------------------------------------------------------
        // 3. 其他类型的中断
        // -------------------------------------------------------
        else {
            printf("unexpected interrupt: scause=%p, sepc=%p\n", scause, sepc);
        }
    } 
    // 检查是否为异常 (scause 最高位为 0)
    else {
        // -------------------------------------------------------
        // 4. 异常处理 (如 Page Fault, Illegal Instruction 等)
        // -------------------------------------------------------
        printf("\n<<<< KERNEL PANIC: EXCEPTION >>>>\n");
        printf("scause: %p\n", scause);
        printf("sepc:   %p (Instruction address)\n", sepc);
        printf("stval:  %p (Faulting address/value)\n", r_stval());
        
        // 发生内核异常时，系统无法继续运行，进入死循环
        while(1);
    }

    // 恢复进入陷阱前的状态
    w_sepc(sepc);
    w_sstatus(sstatus);
}