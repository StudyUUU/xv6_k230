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

// -------------------------------------------------------------------
// 内核态中断/异常入口程序
// -------------------------------------------------------------------
void kerneltrap() {
	int which_dev = 0;
	uint64 sepc = r_sepc(); 
	uint64 sstatus = r_sstatus(); 
	uint64 scause = r_scause(); 
	
	// 检查是否非法进入
	if((sstatus & SSTATUS_SPP) == 0)
			panic("kerneltrap: not from supervisor mode");
	if(intr_get() != 0)
			panic("kerneltrap: interrupts enabled");

	// 判断是中断还是异常
	if(scause & 0x8000000000000000L) {
			uint64 which_int = scause & 0xff;
			
			// --- 1. 时钟中断 (Timer) ---
			if(which_int == 5) { 
				set_timer(r_time() + CLOCK_INTERVAL);
				which_dev = 2; // 标记为时钟中断
			} 
			// --- 2. 外部中断 (External/PLIC) ---
			else if(which_int == 9) {
					int irq = plic_claim();
					if(irq == UART0_IRQ) {
						uartintr();
					} else if(irq != 0) {
						printf("kerneltrap: unexpected irq=%d\n", irq);
					}
					if(irq) plic_complete(irq);
					which_dev = 1;
			} 
			else {
				printf("kerneltrap: unexpected interrupt scause=%lx\n", scause);
			}
	} else {
		// --- 3. 内核异常 (Kernel Exception) ---
		// 内核自己犯错（如空指针），必须 Panic
		printf("\n<<<< KERNEL PANIC: EXCEPTION >>>>\n");
		printf("scause: 0x%lx\n", scause);
		printf("sepc:   0x%lx\n", sepc);
		printf("stval:  0x%lx\n", r_stval());
		k230_wdt_reboot();
		while(1);
	}

	// 【新增】内核抢占逻辑
	// 如果是时钟中断，且当前有进程在运行，尝试让出 CPU
	if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING) {
		yield();
	}

	// 恢复状态
	w_sepc(sepc);
	w_sstatus(sstatus);
}

// -------------------------------------------------------------------
// 用户态中断/异常入口程序
// -------------------------------------------------------------------
uint64
usertrap(void)
{   
	int which_dev = 0;
	struct proc *p = myproc();
	
	if((r_sstatus() & SSTATUS_SPP) != 0)
			panic("usertrap: not from user");

	// 进入内核后，必须将中断向量切换回 kernelvec
	w_stvec((uint64)kernelvec);

	uint64 scause = r_scause();

	// 保存用户程序计数器
	p->trapframe->epc = r_sepc();

	if(scause == 8){
		// --- 1. 系统调用 (Syscall) ---
		
		// 【新增】如果进程已被 kill，不要执行系统调用
		if(p->killed)
				kexit(-1);

		// 跳过 ecall 指令
		p->trapframe->epc += 4;

		// 开启中断，允许 syscall 执行期间响应中断
		intr_on();
		
		syscall();
	} 
	else if(scause == 0x8000000000000005L){
		// --- 2. 时钟中断 ---
		set_timer(r_time() + CLOCK_INTERVAL);
		which_dev = 2;
	} 
	else if(scause == 0x8000000000000009L){
		// --- 3. 外部中断 ---
		int irq = plic_claim();
		if(irq == UART0_IRQ) {
				uartintr();
		} else if(irq != 0) {
				printf("usertrap: unexpected irq=%d\n", irq);
		}
		if(irq) plic_complete(irq);
		which_dev = 1;
	} 
	else {
		// --- 4. 用户态异常 (User Exception) ---
		// 【修改】不要 panic，而是杀死当前进程
		printf("usertrap(): unexpected scause 0x%lx pid=%d\n", scause, p->pid);
		printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
		p->killed = 1;
	}

	// 【新增】如果进程已被 kill，退出
	if(p->killed)
		kexit(-1);

	// 【新增】如果是时钟中断，让出 CPU (抢占)
	if(which_dev == 2)
		yield();


	prepare_return();

	// the user page table to switch to, for trampoline.S
	uint64 satp = MAKE_SATP(p->pagetable);

	// return to trampoline.S; satp value in a0.
	return satp;
}