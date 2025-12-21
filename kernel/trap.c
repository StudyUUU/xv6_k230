#include "types.h"
#include "riscv.h"
#include "defs.h"

extern void kernelvec();

void trap_init(void) {
    w_stvec((uint64)kernelvec);
}

// 设备中断分发
int devintr() {
    uint64 scause = r_scause();
    
    if((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        // 外部中断 (PLIC)
        int irq = plic_claim();
        printf("devintr: irq=%d\n", irq);
        if(irq == 0) {
            uartintr();
        } else if(irq) {
            printf("unexpected interrupt irq=%d\n", irq);
        }
        
        if(irq)
            plic_complete(irq);
        
        return 1;
    } else if(scause == 0x8000000000000005L) {
        // 定时器中断
        set_timer(r_time() + CLOCK_INTERVAL);
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
        // 中断
        devintr();
    } else {
        // 异常
        printf("scause %p\n", scause);
        printf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("kerneltrap");
    }

    w_sepc(sepc);
    w_sstatus(sstatus);
}