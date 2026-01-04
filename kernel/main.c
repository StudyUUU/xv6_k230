#include "defs.h"
#include "param.h"

void main()
{
	if(cpuid() == 0){
		consoleinit();
		printfinit();

		printf("\n");
		printf("xv6 kernel is booting\n");
		printf("\n");

		cpuinit();
		kinit();
		kvminit();
		kvminithart();
		procinit();
		trap_init();
		plicinit();
		plicinithart();
		userinit();

		printf("hart %d starting\n", cpuid());
    }else {
		printf("hart %d starting\n", cpuid());
	}
	scheduler();
}