#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern void kernelvec();

void trap_init(void) {
    w_stvec((uint64)kernelvec);
}

// 设备中断分发
int devintr() {
    uint64 scause = r_scause();
    
    // 判断是否为外部中断 (最高位为1，低位为9)
    if((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        // 1. 获取中断号
        int irq = plic_claim();
        
        // 2. 根据中断号分发处理
        if(irq == UART0_IRQ) {
            // 如果是 1，说明是串口中断
            uartintr();
            printf("[uartintr] Handled UART0 interrupt\n");
        } else if (irq != 0) {
            // 其他未预期的中断
            printf("unexpected interrupt irq=%d\n", irq);
        }
        
        // 3. 告诉 PLIC 处理完成 (Complete)
        if(irq){
            plic_complete(irq);
            printf("[plic_complete] Completed IRQ %d\n", irq);
        }

        
        // 如果 claim 返回 0，说明没有中断需要处理，但仍然返回 1 表示外部中断路径已执行
        return 1;

    } else if(scause == 0x8000000000000005L) {
        // 重新设置下一次中断时间 (这里需要适配 K230 的 STIMECMP 或 SBI)
        uint64 now = r_time();
        set_timer(now + CLOCK_INTERVAL); 
        
        return 2;
    } else {
        return 0;
    }
}

void kerneltrap() {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    if(scause & 0x8000000000000000L) {
        // 尝试处理设备中断
        int which_dev = devintr();
        
        if(which_dev == 0){
            // 未知中断类型
            printf("scause %p\n", scause);
            printf("sepc=%p stval=%p\n", sepc, r_stval());
            panic("kerneltrap: unknown interrupt");
        }
    } else {
        // 异常
        printf("scause %p\n", scause);
        printf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap: exception");
    }

    w_sepc(sepc);
    w_sstatus(sstatus);
}