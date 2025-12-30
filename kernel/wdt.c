#include "defs.h"
#include "memlayout.h"

void k230_wdt_reboot(void)
{
    // 1. 设置超时时间 (Timeout Range Register)
    // 写入 0x0 对应最短的 64K Clocks
    *WDT_TORR = 0x0;

    // 2. 配置并使能看门狗 (Control Register)
    // 注意：一定要确保 RMOD 为 0 (System Reset)
    *WDT_CR = WDT_CR_ENABLE | WDT_CR_RMOD_RST | WDT_CR_RPL_16;

    // 3. 激活计数器 (Kick the dog)
    // 手册强调：这是安全机制，必须写 0x76
    *WDT_CRR = WDT_CRR_MAGIC;

    printf("\n[WDT] Reboot command sent. Waiting for hardware...\n");

    // 4. 调试验证：读取当前计数值 (CCVR)
    // 如果 CCVR 在变小，说明 WDT 正在倒计时，复位即将发生
    uint32 start_val = *WDT_CCVR;
    for(int i = 0; i < 1000; i++) {
        if(*WDT_CCVR < start_val) {
            // 计数器在工作，可以放心等待复位
            break; 
        }
    }

    // 强制同步内存操作
    __sync_synchronize();

    // 停机等待硬件拉低 Reset 线
    while(1);
}