#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include <stdarg.h>

// ====================================================================
// Part 1: SBI System Reset 接口 (用于优雅退出/重启)
// ====================================================================

// SBI Extension ID for System Reset
#define SBI_EXT_SRST        0x53525354

// SBI Function ID
#define SBI_SRST_FID_RESET  0

// Reset Types
#define SBI_SRST_TYPE_SHUTDOWN    0
#define SBI_SRST_TYPE_COLD_REBOOT 1
#define SBI_SRST_TYPE_WARM_REBOOT 2

// Reset Reasons
#define SBI_SRST_REASON_NONE      0
#define SBI_SRST_REASON_SYSTEM_FAILURE 1

struct sbiret {
    long error;
    long value;
};

// 执行 SBI 调用 (内联汇编)
static struct sbiret sbi_call(int ext, int fid, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    struct sbiret ret;
    register unsigned long a0 asm ("a0") = (unsigned long)(arg0);
    register unsigned long a1 asm ("a1") = (unsigned long)(arg1);
    register unsigned long a2 asm ("a2") = (unsigned long)(arg2);
    register unsigned long a6 asm ("a6") = (unsigned long)(fid);
    register unsigned long a7 asm ("a7") = (unsigned long)(ext);

    asm volatile (
        "ecall"
        : "+r" (a0), "+r" (a1)
        : "r" (a2), "r" (a6), "r" (a7)
        : "memory"
    );
    ret.error = a0;
    ret.value = a1;
    return ret;
}

// 重启
void sbi_reboot(void) {
    printf("\n[SBI] System Rebooting...\n");
    sbi_call(SBI_EXT_SRST, SBI_SRST_FID_RESET, SBI_SRST_TYPE_COLD_REBOOT, SBI_SRST_REASON_NONE, 0);
    while(1); // Should not reach here
}

// ====================================================================
// Part 2: UART 硬件寄存器定义
// ====================================================================

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

// ====================================================================
// Part 3: UART 驱动逻辑
// ====================================================================

// 并发控制
static struct spinlock uart_tx_lock;
static struct spinlock uart_rx_lock;
volatile int panicking = 0;   

// 环形缓冲区 (Ring Buffer)
#define UART_RX_BUF_SIZE 32
static char uart_rx_buf[UART_RX_BUF_SIZE];
static uint64 uart_rx_w = 0; // 写索引
static uint64 uart_rx_r = 0; // 读索引 (新增)

void uartinit(void)
{
    initlock(&uart_tx_lock, "uart");
    initlock(&uart_rx_lock, "uart_rx");
    
    // 1. 关闭中断
    WriteReg(IER, 0x00);
    
    // 2. 配置 8n1
    WriteReg(LCR, LCR_EIGHT_BITS);
    
    // 3. 复位 FIFO
    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
    
    // 4. 打开 OUT2
    WriteReg(MCR, MCR_OUT2 | MCR_RTS | MCR_DTR);

    // 5. 仅开启 RX 中断
    WriteReg(IER, IER_RX_ENABLE);
}

// --- 底层输出 ---
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

// --- 底层输入 (Polling) ---
static int uart_getc_nowait(void)
{
    if (ReadReg(LSR) & LSR_RX_READY) {
        return ReadReg(RHR) & 0xFF;
    }
    return -1;
}

// --- 高层输入 (Consumer, 供 console 使用) ---
// 临时版本：由于 process/sleep 尚未实现，使用“忙等待”替代
int uartgetc(void)
{
    while(1) {
        acquire(&uart_rx_lock);
        
        // 检查缓冲区是否有数据
        if(uart_rx_r != uart_rx_w){
            int c = uart_rx_buf[uart_rx_r % UART_RX_BUF_SIZE];
            uart_rx_r++;
            release(&uart_rx_lock);
            return c;
        }
        
        // 缓冲区为空，释放锁让中断可以发生
        release(&uart_rx_lock);
        
        // [TODO] 将来这里应该用 sleep(&uart_rx_r, &uart_rx_lock);
        // 现在暂时空转等待
        for(volatile int i=0; i<1000; i++); 
    }
}

// ====================================================================
// Part 4: Printf & Panic
// ====================================================================

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

// 简单的 panic，死循环
// 在未来可以修改为调用 sbi_shutdown() 或 sbi_reboot()
void panic(const char *s) {
    panicking = 1; 
    printf("panic: %s\n", s); 
    // 遇到 panic 自动重启，避免必须手动硬复位
    sbi_reboot(); 
    // while(1);
}

// ====================================================================
// Part 5: 中断处理 (Producer)
// ====================================================================

#define CTRL_X 0x18 // Ctrl+X

void uartintr(void)
{
    while(1) {
        uint32 iir = ReadReg(2); 
        
        // IIR Bit 0: 0=Pending, 1=No Interrupt
        if (iir & 1) {
            // [Ghost Interrupt Check]
            if (ReadReg(LSR) & LSR_RX_READY) {
                int c = ReadReg(RHR) & 0xFF;
                
                // --- 紧急按键检查 ---
                if (c == CTRL_X) sbi_reboot();
                // ------------------

                uart_putc(c); 
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                // [TODO] wakeup(&uart_rx_r); // 暂未实现进程，注释掉
                release(&uart_rx_lock);
                continue; 
            }
            // 清除 Busy Detect
            volatile uint32 usr = ReadReg(USR);
            (void)usr;
            break; 
        }

        int id = iir & 0x0F;
        
        if (id == 4 || id == 12) { // RX Data
            int c = uart_getc_nowait();
            if(c != -1) {
                // --- 紧急按键检查 ---
                // Ctrl+X (0x18) -> 重启
                if (c == CTRL_X) {
                    sbi_reboot();
                }
                // ------------------

                uart_putc(c); // 回显
                
                acquire(&uart_rx_lock);
                uart_rx_buf[uart_rx_w % UART_RX_BUF_SIZE] = c;
                uart_rx_w++;
                // [TODO] wakeup(&uart_rx_r); // 暂未实现进程，注释掉
                release(&uart_rx_lock);
            }
        } else {
            volatile uint32 lsr = ReadReg(LSR);
            volatile uint32 msr = ReadReg(MSR);
            (void)lsr; (void)msr;
        }
    }
}