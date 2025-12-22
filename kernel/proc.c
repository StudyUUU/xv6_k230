#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];
struct proc *initproc;

// 必须在中断关闭的情况下调用，
// 防止进程在读取过程中被移到另一个 CPU
int
cpuid()
{
  int id = r_tp();
  return id;
}

// 返回当前 CPU 的 cpu 结构体指针
// 中断必须已关闭
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// 初始化当前 CPU 的状态
void cpuinit()
{
    int id = cpuid();
    struct cpu *c = &cpus[id];
    
    c->noff = 0;      // 嵌套深度清零
    c->intena = 0;    // 中断未启用
    c->proc = 0;      // 当前无运行进程
}

// 初始化进程表
void procinit(void) {
  struct proc *p;
  // 初始化每个进程的锁
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
  }
}

// 获取当前进程
struct proc* myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

// 调度器：每个 CPU 调用一次，永不返回
// 循环查找可运行的进程并执行
// 调度器在持有 p->lock 的情况下调用 swtch
// 然后进程负责在返回前释放和重新获取该锁
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // 最近运行的进程可能关闭了中断；
    // 启用中断以避免所有进程都在等待时发生死锁。
    // 然后再次关闭中断以避免中断和 wfi 之间可能的竞态条件。
    intr_on();
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // 切换到选定的进程
        // 进程的工作是释放其锁，然后在跳回调度器之前重新获取锁
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // 进程现在运行完毕
        // 它应该在返回之前已经改变了 p->state
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      // 没有可运行的进程；停止在此核心上运行，直到有中断
      asm volatile("wfi");
    }
  }
}

// 切换到调度器
// 必须持有 p->lock，并且已经改变 proc->state
// 保存和恢复 intena，因为 intena 是此内核线程的属性，而不是此 CPU 的属性
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// 让出 CPU 给其他进程
// 获取锁，改变状态，调用 sched() 切换到调度器
void yield(void) {
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// 测试函数 A - 循环打印 'A'
void test_func_a(void) {
  struct proc *p = myproc();
  // 【关键修复】释放 scheduler 移交过来的锁
  // 当 scheduler 首次 swtch 到此进程时，它持有 p->lock
  // 需要在进入正常执行前释放该锁
  release(&p->lock);
  
  for(;;) {
    printf("A");
    // 模拟耗时工作
    for(volatile int i=0; i<1000000; i++); 
    yield(); // 主动让出 CPU
  }
}

// 测试函数 B - 循环打印 'B'
void test_func_b(void) {
  struct proc *p = myproc();
  // 【关键修复】释放 scheduler 移交过来的锁
  // 理由同 test_func_a
  release(&p->lock);
  
  for(;;) {
    printf("B");
    for(volatile int i=0; i<1000000; i++); 
    yield();
  }
}

// 手动创建测试进程
// 为两个测试函数分别创建进程，设置其上下文使其能被调度执行
void test_proc_init(void) {
    struct proc *p;
    char *sp;

    // === 创建进程 A ===
    p = &proc[0];
    p->state = RUNNABLE;  // 设为可运行状态
    p->pid = 1;
    
    // 分配内核栈（假设 kalloc 已初始化）
    p->kstack = (uint64)kalloc(); 
    if(p->kstack == 0) 
        panic("kalloc failed");
    
    // 【关键】伪造进程上下文
    // 当 scheduler 第一次 swtch 到此进程时：
    // - swtch 会恢复 ra 寄存器（返回地址）
    // - swtch 的 ret 指令会跳转到 ra（即 test_func_a）
    // - sp 指向进程的内核栈顶
    sp = (char *)(p->kstack + PGSIZE);   // 栈顶地址
    p->context.ra = (uint64)test_func_a; // 返回地址 = 函数入口
    p->context.sp = (uint64)sp;          // 栈指针

    // === 创建进程 B ===
    p = &proc[1];
    p->state = RUNNABLE;
    p->pid = 2;
    
    p->kstack = (uint64)kalloc();
    if(p->kstack == 0) 
        panic("kalloc failed");

    sp = (char *)(p->kstack + PGSIZE);
    p->context.ra = (uint64)test_func_b;
    p->context.sp = (uint64)sp;
}