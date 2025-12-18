#include "defs.h"

void main()
{
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
    
    trap_init();
    printf("trap_init success\n");

    timerinit();
    printf("timerinit success. interval = %d\n", CLOCK_INTERVAL);

    intr_on();
    printf("interrupts enabled\n");

    while (1);
}