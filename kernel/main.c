#include "defs.h"
#include "param.h"

void test_sd_basic(void)
{
    printf("\n=== SD Card Basic Test ===\n");
    
    // 测试1: 读取扇区0（MBR/引导扇区）
    uint32 sector0[128]; // 512 bytes = 128 * uint32
    printf("[Test 1] Reading Sector 0 (MBR)...\n");
    if (sd_read_sector(0, sector0) == 0) {
        printf("  SUCCESS: Sector 0 read\n");
        printf("  First 16 bytes: ");
        uint8 *p = (uint8*)sector0;
        for(int i = 0; i < 16; i++) {
            printf("%x ", p[i]);
        }
        printf("\n");
    } else {
        printf("  FAILED: Cannot read sector 0\n");
        return;
    }
    
    // 测试2: 写入并读回测试扇区
    uint32 test_sector[128];
    uint32 verify_sector[128];
    
    printf("[Test 2] Write & Read back test (Sector 2097152)...\n");
    // 填充测试数据
    for(int i = 0; i < 128; i++) {
        test_sector[i] = 0xDEADBEEF + i;
    }
    
    // 写入
    if (sd_write_sector(2097152, test_sector) == 0) {
        printf("  Write SUCCESS\n");
    } else {
        printf("  Write FAILED\n");
        return;
    }
    
    // 读回验证
    if (sd_read_sector(2097152, verify_sector) == 0) {
        printf("  Read back SUCCESS\n");
        // 验证数据
        int errors = 0;
        for(int i = 0; i < 128; i++) {
            if(test_sector[i] != verify_sector[i]) {
                errors++;
                if(errors < 5) { // 只打印前几个错误
                    printf("    Mismatch at offset %d: wrote 0x%x, read 0x%x\n", 
                           i*4, test_sector[i], verify_sector[i]);
                }
            }
        }
        if(errors == 0) {
            printf("  VERIFY SUCCESS: All data matches!\n");
        } else {
            printf("  VERIFY FAILED: %d mismatches found\n", errors);
        }
    } else {
        printf("  Read back FAILED\n");
    }
    
    printf("=== SD Card Basic Test Complete ===\n\n");
}

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
		
		// 测试SD卡基础功能
		test_sd_basic();
		
		// 注释掉ramdisk，使用SD卡
		// ramdisk_init();
		
		userinit();

		printf("hart %d starting\n", cpuid());
    }else {
		printf("hart %d starting\n", cpuid());
	}
	scheduler();
}