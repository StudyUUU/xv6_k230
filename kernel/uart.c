// uart.c - K230 UART driver with xv6-style improvements
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include <stdarg.h>

// K230 UART0 寄存器 (32位对齐)
#define Reg(reg) ((volatile uint32 *)(UART0 + (reg) * 4))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// DW8250 寄存器偏移
#define RHR 0                 // Receive Holding Register
#define THR 0                 // Transmit Holding Register
#define IER 1                 // Interrupt Enable Register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO Control Register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1)
#define ISR 2                 // Interrupt Status Register
#define LCR 3                 // Line Control Register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7)
#define LSR 5                 // Line Status Register
#define LSR_RX_READY (1<<0)   // 接收数据就绪
#define LSR_TX_IDLE (1<<5)    // 发送空闲

// 并发控制
static struct spinlock uart_tx_lock;
static struct spinlock uart_rx_lock;
volatile int panicking = 0;   // 在 panic 时设为 1

// ==================== 初始化 ====================
void uartinit(void)
{
    // 初始化锁
    initlock(&uart_tx_lock, "uart");
    initlock(&uart_rx_lock, "uart_rx");
    
    // 配置前先禁用中断
    WriteReg(IER, 0x00);
    
    // K230 的 UART 波特率可能由 bootloader 配置好了
    // 但为了保险，这里显式设置（如果需要的话）
    // 注释掉是因为 K230 时钟可能不是标准的，波特率计算需要查手册
    /*
    WriteReg(LCR, LCR_BAUD_LATCH);
    WriteReg(0, 0x03);  // LSB for baud rate
    WriteReg(1, 0x00);  // MSB for baud rate
    */
    
    // 设置 8 位数据位，无校验
    WriteReg(LCR, LCR_EIGHT_BITS);
    
    // 启用 FIFO 并清空
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
    
    WriteReg(IER, IER_RX_ENABLE | IER_TX_ENABLE);
}

// ==================== 底层字符输出（带并发保护）====================
static void uart_putc(char c)
{
    if (panicking == 0)
        acquire(&uart_tx_lock);
    
    // 处理换行
    if (c == '\n') {
        while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
            ;
        WriteReg(THR, '\r');
    }
    
    // 等待发送空闲
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
        ;
    WriteReg(THR, c);
    
    if (panicking == 0)
        release(&uart_tx_lock);
}

// ==================== 字符串输出（M-mode 可用）====================
void uart_puts(char *s)
{
    // 这个函数在 M-mode 使用，不能用锁
    while (*s) {
        if (*s == '\n') {
            while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
                ;
            WriteReg(THR, '\r');
        }
        while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
            ;
        WriteReg(THR, *s++);
    }
}

// ==================== 字符输入 ====================
static int uart_getc_nowait(void)
{
    if (ReadReg(LSR) & LSR_RX_READY) {
        return ReadReg(RHR) & 0xFF;
    }
    return -1;
}

static int uart_getc(void)
{
    int c;
    while ((c = uart_getc_nowait()) == -1)
        ;
    return c;
}

// ==================== printf 格式化输出 ====================
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
    while (i > 0)
        uart_putc(buf[--i]);
}

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
    while (i > 0)
        uart_putc(buf[--i]);
}

void printf(const char *fmt, ...)
{
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
            case 'p': {
                uart_puts("0x");
                uint64 ptr = (uint64)va_arg(ap, void *);
                print_hex(ptr, 0);
                break;
            }
            case 's': {
                char *s = va_arg(ap, char *);
                if (s == 0)
                    s = "(null)";
                while (*s)
                    uart_putc(*s++);
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
}

// ==================== 读取一行 ====================
int uart_getline(char *buf, int n)
{
    int i = 0;
    while (i < n - 1) {
        char c = uart_getc();
        
        if (c == '\r' || c == '\n') {
            uart_puts("\n");
            break;
        } else if (c == 127 || c == 8) {  // 退格
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {  // 可打印字符
            buf[i++] = c;
            uart_putc(c);
        }
    }
    buf[i] = '\0';
    return i;
}

// ==================== panic ====================
void panic(const char *s)
{
    panicking = 1;  // 禁用锁，防止死锁
    
    printf("panic: %s\n", s);
    printf("hart %d\n", cpuid());
    
    // 停止所有核心
    while (1)
        ;
}

// ==================== UART 中断处理 ====================

#define UART_TX_BUF_SIZE 32
#define UART_RX_BUF_SIZE 32

// 发送缓冲区
// static char uart_tx_buf[UART_TX_BUF_SIZE];
// static uint64 uart_tx_w; // 写指针
// static uint64 uart_tx_r; // 读指针

// 接收缓冲区
static char uart_rx_buf[UART_RX_BUF_SIZE];
static uint64 uart_rx_w;
// static uint64 uart_rx_r;

// UART 中断处理函数
void uartintr(void)
{
    // 处理接收中断
    while(1) {
        int c = uart_getc_nowait();
        if(c == -1)
            break;
        
        // 简单回显
        uart_putc(c);
        
        acquire(&uart_rx_lock);
        uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
        uart_rx_w++;
        release(&uart_rx_lock);
    }
}