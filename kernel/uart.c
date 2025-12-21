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
#define MCR 4                 // Modem Control Register
#define MCR_DTR  (1<<0)
#define MCR_RTS  (1<<1)
#define MCR_OUT2 (1<<3)       // OUT2: 中断总开关
#define LSR 5                 // Line Status Register
#define LSR_RX_READY (1<<0)   
#define LSR_TX_IDLE (1<<5)    
#define MSR 6                 // Modem Status Register
#define USR 31                // UART Status Register (DesignWare extension)

// 并发控制
static struct spinlock uart_tx_lock;
static struct spinlock uart_rx_lock;
volatile int panicking = 0;   

// ==================== 初始化 ====================
void uartinit(void)
{
    initlock(&uart_tx_lock, "uart");
    initlock(&uart_rx_lock, "uart_rx");
    
    // 1. 关闭中断
    WriteReg(IER, 0x00);
    
    // 2. 配置 8n1
    WriteReg(LCR, LCR_EIGHT_BITS);
    
    // 3. 复位 FIFO，设置触发深度为 1 (Bit 7-6 = 00)
    // 这一点很重要，如果触发深度太高，可能导致有数据但不报 RX 中断
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
    
    // 4. 打开 OUT2
    WriteReg(MCR, MCR_OUT2 | MCR_RTS | MCR_DTR);

    // 5. 仅开启 RX 中断
    WriteReg(IER, IER_RX_ENABLE);
}

// ==================== 底层字符输出 ====================
static void uart_putc(char c)
{
    if (panicking == 0) acquire(&uart_tx_lock);
    
    if (c == '\n') {
        while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
        WriteReg(THR, '\r');
    }
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
    WriteReg(THR, c);
    
    if (panicking == 0) release(&uart_tx_lock);
}

void uart_puts(char *s)
{
    while (*s) {
        if (*s == '\n') {
            while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
            WriteReg(THR, '\r');
        }
        while ((ReadReg(LSR) & LSR_TX_IDLE) == 0);
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

// static int uart_getc(void)
// {
//     int c;
//     while ((c = uart_getc_nowait()) == -1);
//     return c;
// }

// ... printf 相关代码保持不变 ...
static void print_hex(uint64 x, int uppercase) {
    char buf[17]; int i = 0;
    if (x == 0) { uart_putc('0'); return; }
    while (x) {
        int digit = x & 0xF;
        buf[i++] = digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10;
        x >>= 4;
    }
    while (i > 0) uart_putc(buf[--i]);
}
static void print_dec(int64 x) {
    char buf[20]; int i = 0;
    if (x < 0) { uart_putc('-'); x = -x; }
    if (x == 0) { uart_putc('0'); return; }
    while (x) { buf[i++] = '0' + (x % 10); x /= 10; }
    while (i > 0) uart_putc(buf[--i]);
}
void printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { uart_putc(*p); continue; }
        p++;
        switch (*p) {
            case 'd': print_dec(va_arg(ap, int)); break;
            case 'l': if (*(p + 1) == 'd') { print_dec(va_arg(ap, int64)); p++; } break;
            case 'x': print_hex(va_arg(ap, uint64), 0); break;
            case 'X': print_hex(va_arg(ap, uint64), 1); break;
            case 'p': uart_puts("0x"); print_hex((uint64)va_arg(ap, void *), 0); break;
            case 's': { char *s = va_arg(ap, char *); if (!s) s="(null)"; while(*s) uart_putc(*s++); break; }
            case 'c': uart_putc((char)va_arg(ap, int)); break;
            case '%': uart_putc('%'); break;
            default: uart_putc('%'); uart_putc(*p);
        }
    }
    va_end(ap);
}
int uart_getline(char *buf, int n) { return 0; }
void panic(const char *s) {
    panicking = 1; printf("panic: %s\n", s); while(1);
}

// ==================== 中断处理 ====================

#define UART_RX_BUF_SIZE 32
static char uart_rx_buf[UART_RX_BUF_SIZE];
static uint64 uart_rx_w;

void uartintr(void)
{
    while(1) {
        uint32 iir = ReadReg(2); 
        
        // IIR Bit 0: 0=Pending, 1=No Interrupt
        if (iir & 1) {
            // [诊断关键] 检查 LSR 是否有数据残留 (Ghost Interrupt)
            // 某些情况下，IIR 说无中断，但 LSR 仍显示 Data Ready
            // 必须手动清除，否则中断线一直拉高
            if (ReadReg(LSR) & LSR_RX_READY) {
                int c = ReadReg(RHR) & 0xFF;
                
                // [关键] 恢复回显
                uart_putc(c); 
                
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                release(&uart_rx_lock);
                
                continue; // 继续检查
            }
            
            // 尝试读取 USR (DesignWare 特有) 以清除 Busy Detect
            volatile uint32 usr = ReadReg(USR);
            (void)usr;

            break; // 真的没事了，退出
        }

        // 正常 IIR 处理
        int id = iir & 0x0F;
        
        if (id == 4 || id == 12) { // RX Data
            int c = uart_getc_nowait();
            if(c != -1) {
                // [关键] 恢复回显
                uart_putc(c); 
                
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                release(&uart_rx_lock);
            }
        } else {
            // 其他中断，读状态寄存器清除
            volatile uint32 lsr = ReadReg(LSR);
            volatile uint32 msr = ReadReg(MSR);
            (void)lsr; (void)msr;
        }
    }
}