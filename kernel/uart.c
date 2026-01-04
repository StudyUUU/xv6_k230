#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include <stdarg.h>

// ====================================================================
// DW8250 UART 驱动 (K230 UART0)
// ====================================================================

// ====================================================================
// 1. 硬件寄存器定义
// ====================================================================

// K230 UART0 寄存器访问宏 (32位对齐，4字节步进)
#define Reg(reg) ((volatile uint32 *)(UART0 + (reg) * 4))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// DW8250 标准寄存器偏移
#define RHR 0                 // Receive Holding Register (Read)
#define THR 0                 // Transmit Holding Register (Write)
#define IER 1                 // Interrupt Enable Register
#define IER_RX_ENABLE (1<<0)  // 接收中断使能
#define IER_TX_ENABLE (1<<1)  // 发送中断使能

#define FCR 2                 // FIFO Control Register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // 清空 RX & TX FIFO

#define ISR 2                 // Interrupt Status Register (Read, same address as FCR)

#define LCR 3                 // Line Control Register
#define LCR_EIGHT_BITS (3<<0) // 8 数据位
#define LCR_BAUD_LATCH (1<<7) // 波特率除数锁存访问

#define MCR 4                 // Modem Control Register
#define MCR_DTR  (1<<0)       // Data Terminal Ready
#define MCR_RTS  (1<<1)       // Request To Send
#define MCR_OUT2 (1<<3)       // OUT2: UART 中断总开关

#define LSR 5                 // Line Status Register
#define LSR_RX_READY (1<<0)   // 接收数据就绪
#define LSR_TX_IDLE (1<<5)    // 发送器空闲

#define MSR 6                 // Modem Status Register

#define USR 31                // UART Status Register (DesignWare 扩展)

// ====================================================================
// 2. 软件状态
// ====================================================================

// 并发控制
static struct spinlock uart_tx_lock;
static struct spinlock uart_rx_lock;

// Panic 标志（panic 时跳过锁机制）
volatile int panicking = 0;

// 接收环形缓冲区
#define UART_RX_BUF_SIZE 32
static char uart_rx_buf[UART_RX_BUF_SIZE];
static uint64 uart_rx_w = 0;  // 写索引 (Producer)
static uint64 uart_rx_r = 0;  // 读索引 (Consumer)

static struct {
  struct spinlock lock;
} pr;

// ====================================================================
// 3. 初始化
// ====================================================================

void uartinit(void)
{
    initlock(&uart_tx_lock, "uart_tx");
    initlock(&uart_rx_lock, "uart_rx");
    
    // 1. 关闭所有中断
    WriteReg(IER, 0x00);
    
    // 2. 配置串口参数：8n1 (8 数据位，无校验，1 停止位)
    WriteReg(LCR, LCR_EIGHT_BITS);
    
    // 3. 复位并使能 FIFO
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
    
    // 4. 使能调制解调器控制信号和中断输出
    WriteReg(MCR, MCR_OUT2 | MCR_RTS | MCR_DTR);

    // 5. 仅开启接收中断（发送采用轮询方式）
    WriteReg(IER, IER_RX_ENABLE);
}

// ====================================================================
// 4. 底层输出（字符级）
// ====================================================================

// 发送单个字符（自动转换 \n 为 \r\n）
static void uart_putc(char c)
{
    // Panic 时跳过锁，避免死锁
    if (panicking == 0) {
        acquire(&uart_tx_lock);
    }
    
    // 自动插入回车符
    if (c == '\n') {
        while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
        WriteReg(THR, '\r');
    }
    
    // 等待发送器空闲
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
    WriteReg(THR, c);
    
    if (panicking == 0) {
        release(&uart_tx_lock);
    }
}

// 发送字符串
void uart_puts(char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

// ====================================================================
// 5. 底层输入（非阻塞）
// ====================================================================

// 非阻塞读取一个字符（内部使用）
static int uart_getc_nowait(void)
{
    if (ReadReg(LSR) & LSR_RX_READY) {
        return ReadReg(RHR) & 0xFF;
    }
    return -1;
}

// ====================================================================
// 6. 高层输入（阻塞，供上层调用）
// ====================================================================

// 阻塞读取一个字符（从缓冲区）
// TODO: 将来用 sleep/wakeup 替代忙等待
int uartgetc(void)
{
    while (1) {
        acquire(&uart_rx_lock);
        
        // 检查缓冲区是否有数据
        if (uart_rx_r != uart_rx_w) {
            int c = uart_rx_buf[uart_rx_r % UART_RX_BUF_SIZE];
            uart_rx_r++;
            release(&uart_rx_lock);
            return c;
        }
        
        // 缓冲区为空，释放锁后继续轮询
        release(&uart_rx_lock);
        
        // TODO: 将来这里应该用 sleep(&uart_rx_r, &uart_rx_lock);
        // 现在暂时空转等待
        for (volatile int i = 0; i < 1000; i++);
    }
}

// ====================================================================
// 7. 格式化输出
// ====================================================================

// 打印十六进制数
static void print_hex(uint64 x, int uppercase)
{
    char buf[17];
    int i = 0;
    
    if (x == 0) {
        uart_putc('0');
        return;
    }
    
    while (x) {
        int digit = x & 0xF;
        buf[i++] = digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10;
        x >>= 4;
    }
    
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

// 打印十进制数
static void print_dec(int64 x)
{
    char buf[20];
    int i = 0;
    
    if (x < 0) {
        uart_putc('-');
        x = -x;
    }
    
    if (x == 0) {
        uart_putc('0');
        return;
    }
    
    while (x) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

// 简化版 printf
// 支持格式：%d %ld %x %X %p %s %c %%
void printf(const char *fmt, ...)
{
    if(panicking == 0)
        acquire(&pr.lock);

    va_list ap;
    va_start(ap, fmt);
    
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        
        p++;
        switch (*p) {
            case 'd':
                print_dec(va_arg(ap, int));
                break;
            case 'l':
                if (*(p + 1) == 'd') {
                    print_dec(va_arg(ap, int64));
                    p++;
                }
                break;
            case 'x':
                print_hex(va_arg(ap, uint64), 0);
                break;
            case 'X':
                print_hex(va_arg(ap, uint64), 1);
                break;
            case 'p':
                uart_puts("0x");
                print_hex((uint64)va_arg(ap, void *), 0);
                break;
            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                while (*s) uart_putc(*s++);
                break;
            }
            case 'c':
                uart_putc((char)va_arg(ap, int));
                break;
            case '%':
                uart_putc('%');
                break;
            default:
                uart_putc('%');
                uart_putc(*p);
        }
    }
    
    va_end(ap);

    if(panicking == 0)
        release(&pr.lock);
}

// ====================================================================
// 8. Panic 处理
// ====================================================================

void panic(const char *s)
{
    panicking = 1;
    printf("\n=== KERNEL PANIC ===\n");
    printf("panic: %s\n", s);
    printf("====================\n");
    
    // 自动重启系统
    k230_wdt_reboot();

    while (1);
}

// ====================================================================
// 9. 中断处理（Producer）
// ====================================================================

#define CTRL_X 0x18  // Ctrl+X: 紧急重启

// UART 中断服务例程
void uartintr(void)
{
    while (1) {
        uint32 iir = ReadReg(ISR);
        
        // IIR Bit 0: 0=有中断待处理, 1=无中断
        if (iir & 1) {
            // Ghost Interrupt Check: 有时 IIR 显示无中断，但 LSR 显示有数据
            if (ReadReg(LSR) & LSR_RX_READY) {
                int c = ReadReg(RHR) & 0xFF;
                
                // 紧急按键检查
                if (c == CTRL_X) {
                    k230_wdt_reboot();
                }
                
                // 回显
                uart_putc(c);
                
                // 放入缓冲区
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                // TODO: wakeup(&uart_rx_r);
                release(&uart_rx_lock);
                
                continue;
            }
            
            // 清除 Busy Detect 状态（读取 USR 寄存器）
            volatile uint32 usr = ReadReg(USR);
            (void)usr;
            break;
        }

        // 解析中断类型
        int id = iir & 0x0F;
        
        // RX Data Available (4) 或 Character Timeout (12)
        if (id == 4 || id == 12) {
            int c = uart_getc_nowait();
            if (c != -1) {
                // 紧急按键检查
                if (c == CTRL_X) {
                    k230_wdt_reboot();
                }
                
                // 回显
                uart_putc(c);
                
                // 放入缓冲区
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                // TODO: wakeup(&uart_rx_r);
                release(&uart_rx_lock);
            }
        } else {
            // 其他中断类型：读取状态寄存器以清除
            volatile uint32 lsr = ReadReg(LSR);
            volatile uint32 msr = ReadReg(MSR);
            (void)lsr;
            (void)msr;
        }
    }
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
}