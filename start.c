// start.c
#include <stdint.h>

// --------------------------------------------------
// 1. 补全必要的 RISC-V 寄存器定义 (如果你没有 riscv.h)
// --------------------------------------------------
#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_S    (1L << 11)
#define MSTATUS_MIE      (1L << 3)
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

static inline uint64_t r_mstatus() { uint64_t x; asm volatile("csrr %0, mstatus" : "=r" (x) ); return x; }
static inline void w_mstatus(uint64_t x) { asm volatile("csrw mstatus, %0" : : "r" (x)); }
static inline void w_mepc(uint64_t x) { asm volatile("csrw mepc, %0" : : "r" (x)); }
static inline void w_satp(uint64_t x) { asm volatile("csrw satp, %0" : : "r" (x)); }
static inline void w_medeleg(uint64_t x) { asm volatile("csrw medeleg, %0" : : "r" (x)); }
static inline void w_mideleg(uint64_t x) { asm volatile("csrw mideleg, %0" : : "r" (x)); }
static inline void w_sie(uint64_t x) { asm volatile("csrw sie, %0" : : "r" (x)); }
static inline uint64_t r_sie() { uint64_t x; asm volatile("csrr %0, sie" : "=r" (x) ); return x; }
static inline void w_pmpcfg0(uint64_t x) { asm volatile("csrw pmpcfg0, %0" : : "r" (x)); }
static inline void w_pmpaddr0(uint64_t x) { asm volatile("csrw pmpaddr0, %0" : : "r" (x)); }
static inline void w_tp(uint64_t x) { asm volatile("mv tp, %0" : : "r" (x)); }
static inline uint64_t r_mhartid() { uint64_t x; asm volatile("csrr %0, mhartid" : "=r" (x) ); return x; }

// --------------------------------------------------
// 2. 定义栈空间 (entry.S 需要这个)
// --------------------------------------------------
__attribute__ ((aligned (16))) char stack0[4096]; // 单核先给 4K

// 声明外部的 main 函数
void main();
void uart_puts(char *s);

// entry.S jumps here in machine mode on stack0.
void start()
{
  uart_puts("we are in S-mode start\n"); // indicate we are in S-mode start

  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  w_mepc((uint64_t)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  
  // 【修改点 2】暂时不要开中断，因为我们还没设置 S-mode 的中断向量表 (stvec)
  // 如果这里开了中断，一旦有干扰，系统就崩了
  // w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 【修改点 3】必须配置 PMP (物理内存保护)
  // 告诉硬件：允许 S-mode 访问所有内存 (0 ~ 0x3fffffffffffffull)
  // 如果不写这几行，mret 进入 S-mode 后，读取第一条指令就会报错
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf); // 0xf = TOR, R, W, X (全权限)

  // 【修改点 4】注释掉 timerinit
  // ask for clock interrupts.
  // timerinit(); // K230 的时钟控制器不是 CLINT，这个函数会导致崩溃

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  uart_puts("mret to S-mode main\n"); 
  asm volatile("mret");
}