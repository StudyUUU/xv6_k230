#include "defs.h"
#include "param.h"
#include "memlayout.h"

// 这是一个新函数，运行在高位
void main_high() {
	kvm_remove_identity(); // 此时 PC 已经在高位，低位映射没用了，为了安全必须删掉！

	procinit();
	trap_init();
	plicinit();
	plicinithart();
	userinit();
	printf("hart %d starting\n", cpuid());
	scheduler();
}

void main()
{
    if(cpuid() == 0){
		uartinit();
		printfinit();

		printf("\n");
		printf("xv6 kernel is booting\n");
		printf("\n");

		cpuinit();
        kinit();         // 物理页分配器初始化
        kvminit();       // 页表初始化
        
        // B. 开启 MMU (依靠恒等映射存活)
        kvminithart();   
        
		uart_base_addr = UART0; // 切换 UART 驱动到虚拟地址模式
		printf("MMU is enabled. UART is now at virtual address 0x%p\n", uart_base_addr);

        // C. 【爬梯子】绝对跳转到高位虚拟地址
        // main_high 的符号地址是 0xFFFFFFC0...，链接器决定的
        asm volatile("jr %0" : : "r"((uint64)main_high));
    
    } else {
        // 从核的处理逻辑
    }
}