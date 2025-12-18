#include "types.h"
#include "riscv.h" // 包含读写 CSR 寄存器的宏
#include "defs.h"

extern void kernelvec();

void trap_init(void) {
    // 将 kernelvec 的地址写入 stvec 寄存器
    // 并且模式设为 Direct (末位为0)，所有中断都跳到同一个地址
    w_stvec((uint64)kernelvec);
}

void kerneltrap() {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    // 检查是否为中断 (最高位为1)
    if(scause & 0x8000000000000000L) {
        uint64 which_int = scause & 0xff;
        
        // scause 5 = Supervisor Timer Interrupt
        if(which_int == 5) { 
            // 设置下一次闹钟，如果不加这句，闹钟只会响一次
            set_timer(r_time() + CLOCK_INTERVAL);
            
            // 这里以后会加入 yield() 给进程调度
        } else {
            printf("unexpected interrupt: scause=%p\n", scause);
        }
    } else {
        // 异常处理 (Page Fault 等)
        printf("Panic: Exception scause %p\n", scause);
        printf("sepc=%p stval=%p\n", sepc, r_stval());
        while(1);
    }

    w_sepc(sepc);
    w_sstatus(sstatus);
}