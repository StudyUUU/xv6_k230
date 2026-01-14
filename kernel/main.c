#include "defs.h"
#include "param.h"

void main()
{
	if(cpuid() == 0){
		consoleinit();

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

		binit();         // buffer cache
		iinit();         // inode table
		fileinit();      // file table
		
		// 初始化SD卡驱动
		sd_init();
		
		// 注释掉ramdisk，使用SD卡
		// ramdisk_init();
		
		userinit();

		printf("hart %d starting\n", cpuid());
    }else {
		printf("hart %d starting\n", cpuid());
	}
	scheduler();
}