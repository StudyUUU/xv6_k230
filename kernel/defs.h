#ifndef DEFS_H
#define DEFS_H

#include "types.h"
#include "riscv.h"

/*
 * xv6-k230 内核函数声明
 * 
 * 本文件包含所有内核子系统的公共接口声明
 * 按功能模块分类组织
 */

// ============================================================================
// 前向声明
// ============================================================================
struct spinlock;
struct context;
struct proc;

// ============================================================================
// 字符串和内存操作 (string.c)
// ============================================================================
void* memset(void *dst, int c, uint n);
void* memmove(void *dst, const void *src, uint n);
void* memcpy(void *dst, const void *src, uint n);

// ============================================================================
// 物理内存分配器 (kalloc.c)
// ============================================================================
void kinit(void);           // 初始化空闲页链表
void* kalloc(void);         // 分配一个物理页
void kfree(void *pa);       // 释放一个物理页

// ============================================================================
// 虚拟内存管理 (vm.c)
// ============================================================================

// --- 基础页表操作 ---
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);

// --- 内核虚拟内存 ---
void kvminit(void);         // 初始化内核页表
void kvminithart(void);     // 在当前 hart 上激活内核页表
void kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, uint64 perm);
void kvm_remove_identity(); // 移除内核页表中的恒等映射

// --- 用户虚拟内存 ---
pagetable_t proc_pagetable(struct proc *p);  // 为进程创建用户页表
void proc_freepagetable(pagetable_t pagetable, uint64 sz);  // 释放进程页表
void proc_mapstacks(pagetable_t kpgtbl);     // 映射进程内核栈
void uvminit(pagetable_t pagetable, uchar *src, uint sz);  // 加载初始用户代码
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
void uvmfree(pagetable_t pagetable, uint64 sz);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);

// --- 内核/用户空间数据复制 ---
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);

// ============================================================================
// 进程管理 (proc.c)
// ============================================================================

// --- CPU 管理 ---
int cpuid(void);            // 获取当前 CPU ID
struct cpu* mycpu(void);    // 获取当前 CPU 结构体
void cpuinit(void);         // 初始化当前 CPU
struct proc* myproc(void);  // 获取当前进程

// --- 进程初始化和调度 ---
void procinit(void);        // 初始化进程表
void userinit(void);        // 创建第一个用户进程
void scheduler(void);       // 调度器主循环（永不返回）

// --- 用户态切换辅助 ---
void prepare_return(void);  // 准备返回用户空间

// --- 测试代码 ---
void test_proc_init(void);  // 创建测试进程（仅用于调试）

// ============================================================================
// 同步原语 (spinlock.c)
// ============================================================================
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);  // 检查当前 CPU 是否持有锁
void push_off(void);               // 关闭中断并增加嵌套计数
void pop_off(void);                // 减少嵌套计数并可能重新启用中断

// ============================================================================
// 陷阱和中断处理 (trap.c)
// ============================================================================
void trap_init(void);       // 初始化陷阱处理（设置 stvec）
uint64 usertrap(void);      // 用户态陷阱处理入口
int devintr(void);          // 设备中断分发

// ============================================================================
// 定时器 (timer.c)
// ============================================================================
void timerinit(void);       // 初始化定时器
void set_timer(uint64 stime_value);  // 设置 stimecmp

// ============================================================================
// 平台级中断控制器 (plic.c)
// ============================================================================
void plicinit(void);        // 初始化 PLIC（仅 hart 0）
void plicinithart(void);    // 初始化当前 hart 的 PLIC
int plic_claim(void);       // 声明一个中断
void plic_complete(int irq);// 完成一个中断

// ============================================================================
// UART 串口驱动 (uart.c)
// ============================================================================
void uartinit(void);        // 初始化 UART0
void printfinit(void);      // 初始化 printf 锁
void uart_puts(char *s);    // 输出字符串
void printf(const char *fmt, ...);  // 格式化输出
void panic(const char *s);  // 内核恐慌（打印错误并停机）
int uartgetc(void);         // 获取一个字符（非阻塞）
void uartintr(void);        // UART 中断处理

extern volatile uint64 uart_base_addr; // UART 基地址（物理或虚拟）

// ============================================================================
// K230 硬件外设
// ============================================================================

// --- 看门狗定时器 (k230_wdt.c) ---
void k230_wdt_reboot(void); // 通过看门狗重启系统

// ============================================================================
// 汇编函数
// ============================================================================

// --- 上下文切换 (swtch.S) ---
void swtch(struct context *old, struct context *new);

// --- 用户态/内核态跳板 (trampoline.S) ---
extern char trampoline[];   // trampoline 页的起始地址
extern char uservec[];      // 用户态陷阱入口向量
extern char userret[];      // 返回用户态的代码

#endif // DEFS_H
