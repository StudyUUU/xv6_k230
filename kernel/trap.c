#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern void kernelvec();

uint64 uart_intr_count = 0;
uint64 timer_intr_count = 0;
uint64 last_timer_count = 0;

void trap_init(void) {
    w_stvec((uint64)kernelvec);
    printf("[trap_init] Counters initialized to 0\n");
}

// 设备中断分发
int devintr() {
    uint64 scause = r_scause();
    
    // 判断是否为外部中断 (最高位为1，低位为9)
    if((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        int irq = plic_claim();
        
        if(irq == UART0_IRQ) {
            uartintr();
            uart_intr_count++; 
        } else if (irq != 0) {
            printf("unexpected interrupt irq=%d\n", irq);
        }
        
        if(irq){
            plic_complete(irq);
        }
        
        return 1;

    } else if(scause == 0x8000000000000005L) {
        // Timer 中断
        uint64 now = r_time();
        set_timer(now + CLOCK_INTERVAL); 
        
        timer_intr_count++;
        
        // 每 300 次 Timer 中断 (约 3秒) 打印一次统计
        if(timer_intr_count - last_timer_count >= 300) {
            // 使用 %ld 打印 int64
            printf("[Stats] Timer: %ld | UART IRQs: %ld\n", timer_intr_count, uart_intr_count);
            last_timer_count = timer_intr_count;
        }
        
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
        int which_dev = devintr();
        if(which_dev == 0){
            printf("scause %p\n", scause);
            printf("sepc=%p stval=%p\n", sepc, r_stval());
            panic("kerneltrap: unknown interrupt");
        }
    } else {
        printf("scause %p\n", scause);
        printf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap: exception");
    }

    w_sepc(sepc);
    w_sstatus(sstatus);
}