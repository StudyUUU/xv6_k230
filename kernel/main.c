#include "defs.h"

void main()
{
    uartinit();

    printf("\n");
    printf("--------------------------------\n");
    printf("xv6 on K230: Hello from S-mode!\n");
    printf("--------------------------------\n");

    kinit();
    printf("kinit success\n");

    kvminit();
    printf("kvminit success\n");

    kvminithart();
    printf("MMU is ON!\n");
    
    cpuinit();
    printf("cpuinit success (hartid=%d)\n", cpuid());
    
    trap_init();
    printf("trap_init success\n");

    timerinit();
    printf("timerinit success. interval = %d\n", CLOCK_INTERVAL);

    intr_on();
    printf("interrupts enabled\n");

    printf("\nSystem ready.\n");

    while (1);
}
