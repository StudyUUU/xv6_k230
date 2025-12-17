// start.c
#include <stdint.h>

#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_S    (1L << 11)

static inline uint64_t r_mstatus() { uint64_t x; asm volatile("csrr %0, mstatus" : "=r" (x) ); return x; }
static inline void w_mstatus(uint64_t x) { asm volatile("csrw mstatus, %0" : : "r" (x)); }
static inline void w_mepc(uint64_t x) { asm volatile("csrw mepc, %0" : : "r" (x)); }
static inline void w_satp(uint64_t x) { asm volatile("csrw satp, %0" : : "r" (x)); }
static inline void w_pmpcfg0(uint64_t x) { asm volatile("csrw pmpcfg0, %0" : : "r" (x)); }
// 我们不需要 w_pmpaddr0 了，我们需要 pmpaddr3
static inline void w_tp(uint64_t x) { asm volatile("mv tp, %0" : : "r" (x)); }
static inline uint64_t r_mhartid() { uint64_t x; asm volatile("csrr %0, mhartid" : "=r" (x) ); return x; }

__attribute__ ((aligned (16))) char stack0[4096];

void uart_puts(char *s);
void main();

void start()
{
  uart_puts("we are in M-mode start()\n");

  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  w_mepc((uint64_t)main);
  w_satp(0);

  // ----------------------------------------------------------------
  // 【关键修复】 PMP 配置
  // ----------------------------------------------------------------
  // 之前的 Entry 0 (pmpaddr0) 被 OpenSBI 锁住了，导致 S-mode 无权执行。
  // 我们改用 Entry 3。
  
  // 设置 pmpaddr3 为 -1 (全 1)，在 NAPOT 模式下表示整个 64位地址空间
  asm volatile("csrw pmpaddr3, %0" : : "r" (-1ULL));

  // 设置 pmpcfg0
  // Entry 3 在 bits 24-31。
  // 0x1f = Binary 0001 1111 
  //        Bit 0,1,2 (R,W,X) = 1
  //        Bit 3,4   (A)     = 11 (NAPOT Mode)
  //        Bit 7     (L)     = 0  (Not Locked)
  uint64_t cfg = 0x1fULL << 24;
  w_pmpcfg0(cfg);
  // ----------------------------------------------------------------

  int id = r_mhartid();
  w_tp(id);

  uart_puts("mret to S-mode main\n"); 

  asm volatile("fence.i"); 
  asm volatile("mret"); 
}