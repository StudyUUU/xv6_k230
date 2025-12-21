#include "defs.h"
#include "memlayout.h"

// ============ kernel/main.c 添加调试信息 ============
void main()
{
    uartinit();

    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");

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

    plicinit();
    printf("plicinit success\n");

    plicinithart();
    printf("plicinithart success\n");

    timerinit();
    printf("timerinit success. interval = %d\n", CLOCK_INTERVAL);

    intr_on();

    printf("hart %d starting\n", cpuid());

    while(1)
        ;
}
