#include "types.h"
#include "riscv.h" // 包含读写 CSR 寄存器的宏
#include "defs.h"

extern void kernelvec();

void trap_init(void) {
    // 将 kernelvec 的地址写入 stvec 寄存器
    // 并且模式设为 Direct (末位为0)，所有中断都跳到同一个地址
    w_stvec((uint64)kernelvec);
}

// 所有的内核中断/异常都会跳到这里
void kerneltrap(void) {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    if((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");

    // 检查 scause 最高位，1=中断，0=异常
    if((scause & 0x8000000000000000L) && (scause & 0xff) == 5) {
        // Code 5 = Supervisor Timer Interrupt (时钟中断)
        // 这一步在第二阶段实现，现在先留空或者打印个点
        // printf("."); 
        
        // 重要：如果不处理时钟，需要暂时清除 Pending 位，否则会死循环
        // 但最简单的办法是先别开时钟中断
    } 
    else {
        // 异常处理 (Exception)
        printf("Panic: scause %p pid %d\n", scause, 0);
        printf("sepc=%p stval=%p\n", sepc, r_stval());
        panic("Exception");
    }

    // 恢复 sepc 和 sstatus
    // 因为 kerneltrap 内部如果发生了中断，这两个寄存器可能会被覆盖
    // 所以需要在软件层面保存/恢复它们
    w_sepc(sepc);
    w_sstatus(sstatus);
}