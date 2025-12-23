#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;

extern void kernelvec();

void trap_init(void) {
    initlock(&tickslock, "time");
    // 设置内核态的中断向量入口
    w_stvec((uint64)kernelvec);
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

uint64
usertrap(void)
{
    struct proc *p = myproc();
    
    if((r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user");

    // 在处理用户陷阱时，将中断向量切换回内核模式的 kernelvec
    w_stvec((uint64)kernelvec);

    uint64 scause = r_scause();
    uint64 sepc   = r_sepc();
    uint64 stval  = r_stval();

    // 保存用户程序的 pc，以便后续恢复或修改
    p->trapframe->epc = sepc;

    if(scause == 8){
        printf("[usertrap] System call from user mode\n");
        // 系统调用 (ecall from U-mode)
        
        // // 检查进程是否已被杀死 (可选)
        // if(p->killed)
        //     exit(-1);

        // sepc 指向的是 ecall 指令，返回后需要执行下一条指令
        p->trapframe->epc += 4;

        // 这里以后可以调用 syscall() 处理具体的系统调用
        // syscall();
    } else if(scause == 0x8000000000000005L){
        // 时钟中断 (Supervisor timer interrupt)
        // K230 使用 Sstc，需要重置 stimecmp 以清除中断并预设下一次中断
        set_timer(r_time() + CLOCK_INTERVAL);
        // 可以在这里调用 yield() 让出 CPU 给其他进程
        // yield();
    } else {
        printf("\n[usertrap] Unexpected scause %p\n", scause);
        printf("[usertrap] sepc=%p stval=%p\n", sepc, stval);
        panic("usertrap");
    }

    prepare_return();

    return MAKE_SATP(p->pagetable);
}
