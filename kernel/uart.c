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
    // 处理换行符：遇到 \n 先发一个 \r
    if(c == '\n') {
        while((ReadReg(LSR) & (1 << 5)) == 0); // 稍微等一下 FIFO
        WriteReg(THR, '\r');
    }

    // 等待发送空闲（为了防止发太快丢包，最好加上这个检查，之前注释掉是为了调试死锁）
    // 如果之前加这个导致卡死，说明 LSR 定义不对。
    // 但现在我们可以先简单粗暴地直接写，只要加个 \r 就行
    // while((ReadReg(LSR) & (1 << 5)) == 0); 
    
    WriteReg(THR, c);
}

void uart_puts(char *s) {
    while(*s){
        uart_putc(*s++);
    }
}
int uart_getc() {
    // 等待接收数据
    while((ReadReg(LSR) & 0x01) == 0);
    return ReadReg(RHR) & 0xFF;
}