#include "defs.h"
#include "memlayout.h"
#include "riscv.h" 

extern void plic_dump_status(void);
extern int plic_check_pending(void);

// UART 寄存器
#define UART_REG(off) ((volatile uint32 *)(UART0 + (off)*4))
// IIR: Interrupt Identity Register (Offset 2, Read Only)
// Bit 0: 0=Pending, 1=None
// Bit 3..0: ID (4=RX Data, 2=TX Empty, 6=Line Status)
#define UART_IIR_PTR UART_REG(2)
#define UART_LSR_PTR UART_REG(5)

void main()
{
    uartinit();

    printf("\n");
    printf("xv6 kernel is booting (DEEP DIAGNOSTIC MODE)\n");

    kinit();
    kvminit();
    kvminithart();
    cpuinit();
    trap_init();
    plicinit();
    plicinithart();
    timerinit();

    intr_on();

    printf("hart %d starting\n", cpuid());
    plic_dump_status();

    printf("Waiting for inputs... (Checking IIR Register)\n");

    uint64 loop_count = 0;
    
    while(1) {
        for(volatile int i=0; i<20000000; i++); 

        uint64 sip = r_sip();
        
        uint32 lsr = *UART_LSR_PTR;
        uint32 iir = *UART_IIR_PTR; // 关键！
        
        int has_data = (lsr & 1);
        // IIR Bit 0: 0 if interrupt pending
        int uart_int_pending = ((iir & 1) == 0);
        // IIR ID: Bits 3:0
        int iir_id = iir & 0xF;

        uint32 plic_pend_val = plic_check_pending();
        int plic_sees_irq1 = (plic_pend_val >> 1) & 1;
        int cpu_sees_irq = (sip >> 9) & 1;

        int interesting = has_data || uart_int_pending || plic_pend_val;

        if (interesting) {
            printf("\n>>> STATUS <<<\n");
            // 1. UART 内部状态
            printf("[UART] DataReady=%d | IIR=0x%x (IntPending=%s, ID=%d)\n", 
                   has_data, iir, uart_int_pending ? "YES" : "NO", iir_id);
            
            // 2. PLIC 状态
            printf("[PLIC] PendingRaw=0x%x | IRQ1_Pend=%d\n", 
                   plic_pend_val, plic_sees_irq1);
            
            // 3. CPU 状态
            printf("[CPU ] SIP.SEIP=%d\n", cpu_sees_irq);

            // 诊断建议
            if (has_data && !uart_int_pending) 
                printf("  -> WARN: UART has data but IIR says NO INT. Check IER/MCR.\n");
            if (uart_int_pending && !plic_sees_irq1 && plic_pend_val == 0) 
                printf("  -> WARN: UART triggering INT, but PLIC sees nothing. Check Wire/IRQ Number.\n");
            if (plic_pend_val & 0x10000)
                printf("  -> INFO: IRQ 16 is pending! Is UART actually IRQ 16?\n");

            // 手动读取数据，防止死循环
            if (has_data) {
                 uint32 val = *UART_REG(0); 
                 printf("  [POLLED]: '%c'\n", val);
            }
        } else {
            char *temp = ".";
            if (loop_count % 10 == 0) uart_puts(temp);
        }
        loop_count++;
    }
}