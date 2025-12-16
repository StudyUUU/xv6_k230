// uart.c
#include <stdint.h>

// K230 UART0 物理地址
#define UART0_BASE 0x91400000L

// 寄存器偏移 (DesignWare 8250 标准)
// K230 的寄存器步进可能是 4字节 (32bit)
#define RHR 0    // Receive Holding Register (read mode)
#define THR 0    // Transmit Holding Register (write mode)
#define LSR 5    // Line Status Register

// 指针宏
#define Reg(reg) ((volatile uint32_t *)(UART0_BASE + reg * 4))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

void uart_putc(char c) {
    // 1. 等待发送缓冲区为空 (LSR 的第5位 THRE)
    // 防止发送太快乱码
    while((ReadReg(LSR) & (1 << 5)) == 0)
        ;
    // 2. 写入字符
    WriteReg(THR, c);
}

void uart_puts(char *s) {
    while(*s){
        uart_putc(*s++);
    }
}