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
    // 将中断向量设置为内核态的 kernelvec
    w_stvec((uint64)kernelvec);
}

// 内核态中断/异常入口程序
void kerneltrap() {
    uint64 sepc = r_sepc(); // 陷阱发生时的程序计数器的保存
    uint64 sstatus = r_sstatus(); // 当前状态的（如中断使能标志）的保存
    uint64 scause = r_scause(); // 陷阱原因
    uint64 stval = r_stval(); // 发生异常的地址或值

    if((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    if(intr_get() != 0)
        panic("kerneltrap: interrupts enabled");

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
            printf("unexpected interrupt: scause=%lx, sepc=%lx\n", scause, sepc);
        }
    } 
    // 检查是否为异常 (scause 最高位为 0)
    else {
        // -------------------------------------------------------
        // 4. 异常处理 (如 Page Fault, Illegal Instruction 等)
        // -------------------------------------------------------
        printf("\n<<<< KERNEL PANIC: EXCEPTION >>>>\n");
        printf("scause: 0x%lx\n", scause);
        printf("sepc:   0x%lx (Instruction address)\n", sepc);
        printf("stval:  0x%lx (Faulting address/value)\n", stval);
        
        // 发生内核异常时，系统无法继续运行，通过看门狗重启系统
        k230_wdt_reboot();
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

    w_stvec((uint64)kernelvec);

    uint64 scause = r_scause();
    uint64 sepc   = r_sepc();
    uint64 stval  = r_stval();

    p->trapframe->epc = sepc;

    if(scause == 8){
        // --- [调试信息] 系统调用 ---
        // 如果看到这条打印，说明 initcode 成功执行到了 ecall
        printf("[usertrap] syscall: pid=%d name=%s epc=0x%lx\n", p->pid, p->name, sepc);

        // if(p->killed)
        //     exit(-1);

        p->trapframe->epc += 4;

        intr_on();
        syscall();
    } 
    else if(scause == 0x8000000000000005L){
        // --- 时钟中断 ---
        set_timer(r_time() + CLOCK_INTERVAL);
        
        // 建议：打印一下，确认 CPU 没有卡死，但只在 PID 1 时打印，避免刷屏
        // if(p->pid == 1) printf("."); 

        // 建议：恢复 yield()，这是多任务调度的基础
        // 如果只有一个 init 进程，它会立即重新被调度回来，这没问题
        yield(); 
    } 
    else if(scause == 0x8000000000000009L){
        // --- 外部中断 (UART 等) ---
        int irq = plic_claim();
        if(irq == UART0_IRQ) {
            uartintr();
        } else if(irq != 0) {
            printf("unexpected external interrupt: irq=%d\n", irq);
        }
        if(irq) {
            plic_complete(irq);
        }
    }
    else {
        // --- [关键调试信息] 异常捕捉 ---
        // 这里会捕获 Page Fault (缺页), Illegal Instruction (非法指令) 等
        printf("\n=== usertrap: unexpected exception ===\n");
        printf("scause = 0x%lx ", scause);
        
        // 解析常见错误原因
        if(scause == 12) printf("(Instruction Page Fault)\n");
        else if(scause == 13) printf("(Load Page Fault)\n");
        else if(scause == 15) printf("(Store/AMO Page Fault)\n");
        else if(scause == 2) printf("(Illegal Instruction)\n");
        else printf("(Unknown)\n");

        printf("pid    = %d (%s)\n", p->pid, p->name);
        printf("sepc   = 0x%lx (Error PC)\n", sepc);
        printf("stval  = 0x%lx (Bad Addr)\n", stval);
        printf("======================================\n");
        
        p->killed = 1;
    }

    // if(p->killed)
    //     exit(-1);

    prepare_return();

    return MAKE_SATP(p->pagetable);
}
