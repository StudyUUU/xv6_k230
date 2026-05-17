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

		ramdisk_init(); //使用内置的ramdisk作为根文件系统，模拟SD卡的文件系统
		// sd_init(); // 初始化SD卡

		userinit();

		printf("hart %d starting\n", cpuid());
    }else {
		printf("hart %d starting\n", cpuid());
	}
	scheduler();
}