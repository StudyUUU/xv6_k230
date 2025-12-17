// uart.c
#include <stdint.h>
#include <stdarg.h>

// K230 UART0 物理地址
#define UART0_BASE 0x91400000L

// 寄存器偏移 (DesignWare 8250 标准)
// K230 的寄存器步进可能是 4字节 (32bit)
#define RHR 0    // Receive Holding Register (read mode)
#define THR 0    // Transmit Holding Register (write mode)
#define LSR 5    // Line Status Register
#define LSR_THRE (1 << 5) // 发送持有寄存器为空标志

// 指针宏
#define Reg(reg) ((volatile uint32_t *)(UART0_BASE + reg * 4))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

static void uart_putc(char c) {
    // 1. 处理换行
    if(c == '\n') {
        // 等待直到硬件 FIFO 有空位
        while((ReadReg(LSR) & LSR_THRE) == 0); 
        WriteReg(THR, '\r');
    }

    // 2. 发送当前字符前，必须等待 FIFO 有空位
    // 这一步如果不加，由于 printf 跑得太快，连续发送会导致后面字符挤掉前面字符
    while((ReadReg(LSR) & LSR_THRE) == 0); 
    
    WriteReg(THR, c);
}

void uart_puts(char *s) {
    while(*s){
        uart_putc(*s++);
    }
}
static int uart_getc() {
    // 等待接收数据
    while((ReadReg(LSR) & 0x01) == 0);
    return ReadReg(RHR) & 0xFF;
}

// 辅助：将无符号整数转为十六进制字符串（小端存储，需反转）
static void print_hex(uint64_t x, int uppercase) {
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
    // 反转输出
    while (i > 0) uart_putc(buf[--i]);
}

// 辅助：将有符号整数转为十进制字符串
static void print_dec(int64_t x) {
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
    while (i > 0) uart_putc(buf[--i]);
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++; // 跳过 '%'
        switch (*p) {
            case 'd': // 十进制整数
                print_dec(va_arg(ap, int));
                break;
            case 'l': // 支持 %ld (long)
                if (*(p+1) == 'd') {
                    print_dec(va_arg(ap, int64_t));
                    p++;
                }
                break;
            case 'x': // 十六进制（小写）
                print_hex(va_arg(ap, uint64_t), 0);
                break;
            case 'X': // 十六进制（大写）
                print_hex(va_arg(ap, uint64_t), 1);
                break;
            case 'p': { // 指针
                uart_puts("0x");
                uint64_t ptr = (uint64_t)va_arg(ap, void*);
                print_hex(ptr, 0);
                break;
            }
            case 's': { // 字符串
                char *s = va_arg(ap, char*);
                uart_puts(s ? s : "(null)");
                break;
            }
            case 'c': // 单字符
                uart_putc((char)va_arg(ap, int));
                break;
            case '%': // 转义 %%
                uart_putc('%');
                break;
            default:
                uart_putc('%');
                uart_putc(*p);
        }
    }
    va_end(ap);
}

// 读取一行，支持退格，遇到回车返回
// buf: 缓冲区, n: 最大长度, 返回实际读取字节数
int uart_getline(char *buf, int n) {
    int i = 0;
    while (i < n - 1) {
        char c = uart_getc();
        
        if (c == '\r' || c == '\n') {  // 回车结束
            uart_puts("\n");
            break;
        } else if (c == 127 || c == 8) {  // 退格键 (DEL/BS)
            if (i > 0) {
                i--;
                uart_puts("\b \b");  // 回显：退格+空格+退格
            }
        } else if (c >= 32 && c < 127) {  // 可打印字符
            buf[i++] = c;
            uart_putc(c);  // 回显
        }
    }
    buf[i] = '\0';
    return i;
}
void panic(const char *s) {
    printf("panic: ");
    printf((char *)s);
    printf("\n");
    while(1);
}