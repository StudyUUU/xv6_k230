//
// DW8250 UART 驱动 for K230
// 基于标准 xv6 uart.c 重构，适配 K230/C908 硬件特性
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// ====================================================================
// 硬件寄存器定义
// ====================================================================

// K230 UART0 寄存器访问宏 (32位对齐，4字节步进)
#define Reg(reg) ((volatile uint32 *)(UART0 + (reg) * 4))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// DW8250 寄存器偏移
#define RHR 0                 // Receive Holding Register (Read)
#define THR 0                 // Transmit Holding Register (Write)
#define IER 1                 // Interrupt Enable Register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO Control Register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1)
#define ISR 2                 // Interrupt Status Register (Read)
#define LCR 3                 // Line Control Register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7)
#define MCR 4                 // Modem Control Register
#define MCR_DTR  (1<<0)
#define MCR_RTS  (1<<1)
#define MCR_OUT2 (1<<3)       // K230: UART 中断总开关
#define LSR 5                 // Line Status Register
#define LSR_RX_READY (1<<0)
#define LSR_TX_IDLE (1<<5)

// ====================================================================
// 软件状态
// ====================================================================

// 发送同步（用于中断驱动的发送）
static struct spinlock uart_tx_lock;
static int uart_tx_busy;      // UART 是否正在发送
static int uart_tx_chan;      // 等待通道（地址）

// 用于 panic 时的无锁输出
extern volatile int panicking;
extern volatile int panicked;

void
uartinit(void)
{
  // disable interrupts.
  WriteReg(IER, 0x00);

  // K230 DW8250 不支持设置波特率（固定硬件配置）
  // 标准 16550a 在这里会设置 LCR_BAUD_LATCH

  // set word length to 8 bits, no parity.
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable modem control and interrupt output (K230 specific)
  WriteReg(MCR, MCR_OUT2 | MCR_RTS | MCR_DTR);

  // enable transmit and receive interrupts.
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// 中断驱动的批量发送
// 当 UART 忙时会 sleep 等待，不能在中断中调用
void
uartwrite(char buf[], int n)
{
  acquire(&uart_tx_lock);

  int i = 0;
  while(i < n){ 
    while(uart_tx_busy != 0){
      // 等待 UART 发送完成中断将 uart_tx_busy 置 0
      sleep(&uart_tx_chan, &uart_tx_lock);
    }   
      
    WriteReg(THR, buf[i]);
    i += 1;
    uart_tx_busy = 1;
  }

  release(&uart_tx_lock);
}

// 同步发送单个字符（用于 printf 和 echo）
// 轮询方式，可在中断中调用
void
uartputc_sync(int c)
{
  if(panicking == 0)
    push_off();

  if(panicked){
    for(;;)
      ;
  }

  // wait for UART to set Transmit Holding Empty in LSR.
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  if(panicking == 0)
    pop_off();
}

// 同步输出字符串（用于早期启动，M-mode）
// 轮询方式，不依赖锁和中断
void
uart_puts(char *s)
{
  while(*s){
    // 等待 UART 空闲
    while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
      ;
    WriteReg(THR, *s);
    s++;
  }
}

// 尝试读取一个字符（非阻塞）
// 返回 -1 表示没有数据
int
uartgetc(void)
{
  if(ReadReg(LSR) & LSR_RX_READY){
    // input data is ready.
    return ReadReg(RHR) & 0xFF;
  } else {
    return -1;
  }
}

// UART 中断处理
// 处理接收中断和发送完成中断
void
uartintr(void)
{
  // K230: 读取 ISR 确认中断（可能包含中断 ID）
  ReadReg(ISR);

  // 处理发送完成中断
  acquire(&uart_tx_lock);
  if(ReadReg(LSR) & LSR_TX_IDLE){
    // UART finished transmitting; wake up sending thread.
    uart_tx_busy = 0;
    wakeup(&uart_tx_chan);
  }
  release(&uart_tx_lock);

  // 处理接收中断
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);  // 交给 console 处理（行编辑、回显等）
  }
}