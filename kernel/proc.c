#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
// #include "initcode.h"

/*
 * xv6-k230 进程管理
 * 
 * 核心数据结构：
 * - cpus[NCPU]: 每个 CPU 的状态（当前进程、中断嵌套深度等）
 * - proc[NPROC]: 进程表，每个进程都有独立的状态和上下文
 * 
 * 调度模型：
 * - Round-robin 时间片轮转
 * - scheduler() 在每个 CPU 上运行，循环查找 RUNNABLE 进程
 * - 通过 swtch() 在调度器上下文和进程上下文间切换
 * 
 * 锁规则：
 * - scheduler 持有 p->lock 调用 swtch，进程必须先 release(&p->lock)
 * - 调用 mycpu() 前必须关闭中断，防止进程迁移
 * - 持有多个锁时的顺序：p->lock 必须在 wait_lock 之前获取
 */

struct cpu cpus[NCPU];
struct proc proc[NPROC];
struct proc *initproc;

static struct spinlock pid_lock;
static int nextpid = 1;
struct spinlock wait_lock;

static void forkret(void);
static void freeproc(struct proc *p);


// ============================================================================
//                                CPU 管理
// ============================================================================

/*
 * cpuid - 获取当前 CPU ID
 * 返回 tp 寄存器的值（在启动时设置为 hart ID）
 * 必须在中断关闭的情况下调用，防止进程在读取过程中被移到另一个 CPU
 */
int
cpuid()
{
  int id = r_tp();
  return id;
}

/*
 * mycpu - 返回当前 CPU 的 cpu 结构体指针
 * 中断必须已关闭，否则进程可能迁移导致返回错误的 CPU
 */
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

/*
 * myproc - 获取当前进程
 * 安全版本：临时关闭中断以防止迁移
 */
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

/*
 * cpuinit - 初始化当前 CPU 的状态
 * 清零中断嵌套深度和当前进程指针
 */
void
cpuinit()
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  
  c->noff = 0;      // 嵌套深度清零
  c->intena = 0;    // 中断未启用
  c->proc = 0;      // 当前无运行进程
}

// ============================================================================
//                            进程表初始化和分配
// ============================================================================

/*
 * procinit - 初始化进程表
 * 为每个进程槽初始化锁，设置状态为 UNUSED
 */
void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");

  for(p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
  }
}

/*
 * allocpid - 分配新的进程 ID
 * 使用 pid_lock 保护全局计数器
 */
static int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

/*
 * allocproc - 在进程表中分配一个进程槽
 * 
 * 步骤：
 * 1. 查找 UNUSED 状态的进程槽
 * 2. 分配 PID
 * 3. 分配 trapframe 页
 * 4. 创建用户页表（包含 trampoline 和 trapframe 映射）
 * 5. 设置进程上下文：ra 指向 forkret，sp 指向内核栈顶
 * 
 * 返回时持有 p->lock，调用者负责释放
 */
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // 分配 trapframe 页
  if((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 创建用户页表，包含 trampoline 和 trapframe 映射
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 设置新上下文，从 forkret 开始执行
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

/*
 * freeproc - 释放进程的所有资源
 * 包括 trapframe、页表、进程状态等
 */
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// ============================================================================
//                            调度器和上下文切换
// ============================================================================
// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
/*
 * scheduler - 每个 CPU 的调度器主循环
 * 
 * 永不返回，循环执行：
 * 1. 遍历进程表查找 RUNNABLE 进程
 * 2. 获取 p->lock
 * 3. 设置状态为 RUNNING
 * 4. 调用 swtch 切换到进程上下文
 * 5. 进程返回后检查状态变化
 * 6. 如果没有可运行进程，执行 wfi 等待中断
 * 
 * 关键：持有 p->lock 调用 swtch，进程必须在运行后释放该锁
 */
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;) {
    // 启用中断以避免所有进程都在等待时发生死锁
    // 然后再次关闭中断以避免中断和 wfi 之间的竞态条件
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
        // 进程已运行完毕
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      // 没有可运行的进程，停止在此核心上运行直到有中断
      asm volatile("wfi");
    }
  }
}

/*
 * sched - 切换到调度器
 * 
 * 必须满足的条件：
 * - 持有 p->lock
 * - 已经改变 proc->state
 * - 中断已关闭
 * - 锁嵌套深度为 1（只持有 p->lock）
 * 
 * 保存和恢复 intena，因为 intena 是此内核线程的属性
 */
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

/*
 * yield - 让出 CPU 给其他进程
 * 获取锁，改变状态为 RUNNABLE，调用 sched() 切换到调度器
 */
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// ============================================================================
//                            进程同步原语
// ============================================================================

/*
 * sleep - 原子地释放锁并睡眠
 * @chan: 等待通道（任意地址，用于唤醒时匹配）
 * @lk: 调用时持有的锁（sleep 返回时会重新获取）
 * 
 * 睡眠在通道 chan 上，释放锁 lk，重新获取 lk 后返回
 * 
 * 关键：释放 lk 和设置状态为 SLEEPING 必须是原子的
 * 否则可能发生：
 * 1. 进程 A 释放 lk
 * 2. 进程 B 获取 lk，修改条件，调用 wakeup(chan)
 * 3. 进程 A 设置状态为 SLEEPING（错过了 wakeup）
 */
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // 必须获取 p->lock 以改变 p->state
  // 并避免与 wakeup 的竞争（wakeup 会锁定 p->lock）
  acquire(&p->lock);

  // 释放传入的锁（通常是保护条件的锁）
  release(lk);

  // 记录睡眠通道并改变状态
  p->chan = chan;
  p->state = SLEEPING;

  // 调用调度器，切换到其他进程
  sched();
  
  // 被唤醒，清除通道
  p->chan = 0;

  // 重新获取原来的锁
  release(&p->lock);
  acquire(lk);
}

/*
 * wakeup - 唤醒所有睡眠在通道 chan 上的进程
 * @chan: 等待通道
 * 
 * 扫描进程表，将所有睡眠在 chan 上的进程设为 RUNNABLE
 * 
 * 注意：wakeup 必须持有每个进程的 p->lock
 * 调用者不需要持有任何特定的锁
 */
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()) {
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// ============================================================================
//                            进程创建和退出
// ============================================================================

/*
 * kfork - 创建当前进程的子进程
 * 
 * 返回值：
 * - 父进程中：返回子进程的 PID
 * - 子进程中：返回 0
 * - 失败：返回 -1
 * 
 * 实现步骤：
 * 1. 调用 allocproc() 分配新进程结构
 * 2. 使用 uvmcopy() 复制父进程的用户内存
 * 3. 复制父进程的 trapframe（保存所有寄存器状态）
 * 4. 设置子进程的返回值为 0（通过修改 trapframe->a0）
 * 5. 复制进程名称
 * 6. 设置父子进程关系
 * 7. 标记子进程为 RUNNABLE
 * 
 * 关键点：
 * - 子进程继承父进程的用户空间内存、寄存器状态
 * - 子进程从 fork 调用点返回，但返回值为 0
 * - 父进程返回子进程 PID
 */
int
kfork(void)
{
  int pid;
  struct proc *np;
  struct proc *p = myproc();

  // 1. 分配进程结构 (分配 PID、内核栈、陷阱帧等)
  if((np = allocproc()) == 0){
    return -1;
  }

  // 2. 复制父进程的用户内存到子进程
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  
// ====================================================
  // [K230/RISC-V 修复] 刷新 I-Cache
  // fork 刚刚通过 uvmcopy (memmove) 写入了新进程的代码段。
  // 这些数据现在可能只存在于 D-Cache 中。
  // 我们必须执行 fence.i，确保 CPU 从 I-Cache 取指时能看到最新的代码。
  // ====================================================
  asm volatile("fence.i");

  np->sz = p->sz;

  // 3. 复制父进程的寄存器状态 (Trapframe)
  // *np->trapframe = *p->trapframe 这种结构体赋值在 C 中是合法的
  *(np->trapframe) = *(p->trapframe);

  // 4. 修改子进程的返回值为 0
  np->trapframe->a0 = 0;

  // 5. 复制文件描述符
  for(int i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  // 6. 复制进程名称用于调试
  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  // 7. 释放 allocproc 时获取的 np->lock
  release(&np->lock);

  // 8. 建立父子关系
  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  // 9. 标记子进程为可运行
  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  // printf("[kfork] parent pid=%d forked child pid=%d\n", p->pid, np->pid);

  return pid;
}

/*
 * reparent - 将进程 p 的所有子进程重新分配给 initproc
 * 
 * 当进程退出时，需要将其子进程转移给 init 进程
 * 必须持有 wait_lock
 */
static void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++) {
    if(pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

/*
 * kexit - 终止当前进程
 * @status: 退出状态码
 * 步骤：
 * 1. 检查是否为 init 进程（不允许退出）
 * 2. 关闭所有打开的文件（触发 pipeclose/iput，解决管道卡死）
 * 3. 释放当前工作目录的引用（iput p->cwd）
 * 4. 将子进程重新分配给 init
 * 5. 唤醒父进程（可能在 wait 中等待）
 * 6. 设置状态为 ZOMBIE
 * 7. 调用 sched() 切换到调度器，永不返回
 * 
 * 注意：内核栈与 proc 结构体等基础资源在父进程调用 wait() 时才会被释放 */
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // 关闭所有打开的文件描述符
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // 将所有子进程交给 init 进程
  reparent(p);

  // 唤醒父进程（可能在 wait() 中睡眠）
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // 跳转到调度器，永不返回
  sched();
  panic("zombie exit");
}

/*
 * kwait - 等待任意子进程退出
 * @addr: 用户空间地址，用于存储子进程退出状态
 * 
 * 返回：
 * - 成功：子进程的 PID
 * - 失败：-1（没有子进程）
 * 
 * 扫描进程表查找已退出的子进程：
 * - 找到 ZOMBIE 子进程：回收资源并返回其 PID
 * - 有子进程但未退出：sleep 等待
 * - 没有子进程：返回 -1
 */
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;) {
    // 检查是否有子进程
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++) {
      if(pp->parent == p) {
        havekids = 1;

        acquire(&pp->lock);

        if(pp->state == ZOMBIE) {
          // 找到已退出的子进程，回收资源
          pid = pp->pid;
          // 如果提供了地址，复制退出状态到用户空间
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate, sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }

        release(&pp->lock);
      }
    }

    // 没有子进程，或者被杀死
    if(!havekids || p->killed) {
      release(&wait_lock);
      return -1;
    }

    // 有子进程但没有退出的，睡眠等待
    sleep(p, &wait_lock);
  }
}

// ============================================================================
//                            进程终止信号
// ============================================================================

/*
 * kkill - 向进程发送终止信号
 * @pid: 目标进程 ID
 * 
 * 返回：0 成功，-1 失败（进程不存在）
 * 
 * 注意：kill 只是设置 killed 标志，不会立即终止进程
 * 进程会在下次从内核返回用户空间或 sleep 时检查该标志并退出
 */
int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->pid == pid) {
      p->killed = 1;
      
      // 如果进程在睡眠，唤醒它以便能检查 killed 标志
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

/*
 * setkilled - 标记进程为已被终止
 * 
 * 设置 p->killed 为 1，表示该进程应退出
 */
void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

/*
 * killed - 检查当前进程是否被标记为终止
 * 
 * 返回：1 表示已被 kill，0 表示正常
 * 
 * 用于在关键点检查进程是否应该退出
 * 例如：系统调用返回前、长时间循环中等
 */
int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// ============================================================================
//                            用户进程初始化
// ============================================================================

/*
 * prepare_return - 准备从内核返回到用户空间
 * 
 * 设置 trapframe 中的内核信息（satp、sp、trap handler）
 * 配置 stvec 指向 trampoline 中的 uservec
 * 设置 sstatus 为用户模式并启用中断
 */
void
prepare_return(void)
{
  struct proc *p = myproc();

  // 即将切换 trap 目标从 kerneltrap() 到 usertrap()
  // 因为从内核代码 trap 到 usertrap 会是灾难性的，所以关闭中断
  intr_off();

  // 将 syscalls、中断和异常发送到 trampoline.S 中的 uservec
  // 通过 trampoline 在内核和用户空间虚拟地址都一致的情况，去设置 stvec
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // 设置 trapframe 值，供下次 trap 到内核时 uservec 使用
  p->trapframe->kernel_satp = r_satp();         // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程的内核栈
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // cpuid() 用的 hartid

  // 设置 trampoline.S 的 sret 将使用的寄存器，以进入用户空间
  
  // 设置 S Previous Privilege 模式为 User
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清除 SPP 为 0（用户模式）
  x |= SSTATUS_SPIE; // 在用户模式下启用中断
  w_sstatus(x);

  // 设置 S Exception Program Counter 为保存的用户 pc
  w_sepc(p->trapframe->epc);
}

/*
 * forkret - fork 子进程的第一次调度入口
 * 
 * 由 scheduler() 首次调度时 swtch 到此
 * 释放 scheduler 持有的 p->lock
 * 执行首次初始化（如果需要）
 * 准备返回用户空间
 */
static void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // 仍然持有来自 scheduler 的 p->lock
  release(&p->lock);

  if(first) {
    // 此处可添加文件系统初始化等操作

    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);
    
    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }
  
  // 返回到用户空间，模仿 usertrap() 的返回
  prepare_return();
  
  // 跳转到 trampoline.S 中的 userret 以返回用户态
  // 切换页表，并且切换到用户态
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

/*
 * userinit - 设置第一个用户进程
 * 
 * 步骤：
 * 1. 调用 allocproc 分配进程结构
 * 2. 使用 uvminit 将 initcode 复制到用户地址 0
 * 3. 设置 trapframe 中的 epc 和 sp
 * 4. 标记为 RUNNABLE
 * 
 * 这是系统中第一个用户进程，之后所有进程都通过 fork 创建
 */
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // 1. 设置进程名
  safestrcpy(p->name, "init", sizeof(p->name)); 

  // 2. 设置工作目录 (必须！)
  p->cwd = namei("/"); 

  // 3. 这里的内存分配 (uvminit) 可以删掉了
  // 因为 kexec 会负责分配新的内存页表并加载 ELF。
  // allocproc 已经为我们分配了一个空的页表 (包含 trampoline 和 trapframe 映射)，这就足够了。
  p->sz = 0; 

  // 4. Trapframe 设置也可以简化
  // kexec 会重置 epc (入口点) 和 sp (栈指针)
  // 所以这里不需要设 epc 和 sp
  
  // 5. 不要在这里打开文件描述符 (FD)
  // 让 /init 程序自己去 open("console")，这样符合 UNIX 标准行为。

  // 6. 就绪
  p->state = RUNNABLE;

  release(&p->lock);
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}


// ============================================================================
//                            测试代码
// ============================================================================
/*
 * 以下函数仅用于内核线程协作式多任务测试
 * 生产环境不使用
 */

/*
 * test_func_a - 测试函数 A，循环打印 'A'
 * 
 * 关键点：
 * - 第一行必须 release(&p->lock)，因为 scheduler 持锁调用 swtch
 * - 通过 yield() 主动让出 CPU，实现协作式多任务
 */
void
test_func_a(void)
{
  struct proc *p = myproc();
  // 【关键】释放 scheduler 移交过来的锁
  // 当 scheduler 首次 swtch 到此进程时，它持有 p->lock
  release(&p->lock);
  
  for(;;) {
    printf("A");
    // 模拟耗时工作
    for(volatile int i = 0; i < 1000000; i++);
    yield(); // 主动让出 CPU
  }
}

/*
 * test_func_b - 测试函数 B，循环打印 'B'
 * 逻辑同 test_func_a
 */
void
test_func_b(void)
{
  struct proc *p = myproc();
  // 【关键】释放 scheduler 移交过来的锁
  release(&p->lock);
  
  for(;;) {
    printf("B");
    for(volatile int i = 0; i < 1000000; i++);
    yield();
  }
}

/*
 * test_proc_init - 手动创建测试进程
 * 
 * 为两个测试函数分别创建进程，手动设置上下文：
 * - context.ra 设为函数入口（swtch 返回时跳转到此）
 * - context.sp 设为内核栈顶
 * - state 设为 RUNNABLE
 * 
 * 注意：这是简化的测试代码，生产环境应使用 allocproc
 */
void
test_proc_init(void)
{
  struct proc *p;
  char *sp;

  // === 创建进程 A ===
  p = &proc[0];
  p->state = RUNNABLE;
  p->pid = 1;
  
  // 分配内核栈
  p->kstack = (uint64)kalloc(); 
  if(p->kstack == 0) 
    panic("kalloc failed");
  
  // 设置上下文：ra 指向函数入口，sp 指向栈顶
  sp = (char *)(p->kstack + PGSIZE);
  p->context.ra = (uint64)test_func_a;
  p->context.sp = (uint64)sp;

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