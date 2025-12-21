#include "defs.h"
#include "param.h"

void main()
{
    uartinit();

    printf("\n");
    printf("xv6 kernel is booting\n");

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
    
    while(1) {
 
    }
}