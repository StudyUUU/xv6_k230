#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h" 
#include "fs.h"        
#include "buf.h"      
#include "k230_sd.h"

struct spinlock sd_lock;
uint32 fat32_offset_sector = 0;  // 全局变量定义
uint32 sd_rca = 0;               // 定义全局变量

void delay(int count)
{
    volatile int i = count * 500;
    while (i > 0)
        i--;
}

// --- 核心传输层 (保持不变) ---

int sd_send_cmd(int cmd_idx, uint32 arg, int resp_type, int ignore_err, uint32 *resp_out)
{
    uint32 cmd_reg = 0;
    uint32 time_out = 100000;

    while (SD_READ_REG32(SD_PSTATE_REG) & 0x1)
    {
        if (time_out-- == 0)
        {
            SD_WRITE_REG8(SD_SW_RST_R, 0x2);
            delay(1000);
            if (SD_READ_REG32(SD_PSTATE_REG) & 0x1)
                return -1;
        }
    }

    SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, 0xFFFFFFFF);
    SD_WRITE_REG32(SD_ERROR_INT_STAT_R, 0xFFFFFFFF);

    if (cmd_idx != CMD17 && cmd_idx != CMD24)
        SD_WRITE_REG16(SD_XFER_MODE_R, 0x0);
    SD_WRITE_REG32(SD_ARGUMENT_R, arg);

    cmd_reg |= (cmd_idx << 8);
    if (resp_type == RESP_136)
        cmd_reg |= 0x1;
    else if (resp_type == RESP_48)
        cmd_reg |= 0x2;
    else if (resp_type == RESP_48_BUSY)
        cmd_reg |= 0x3;
    else if (resp_type == RESP_R3)
        cmd_reg |= 0x2;

    if (resp_type != RESP_NONE && resp_type != RESP_R3)
        cmd_reg |= CMD_CRC_CHECK;
    if (resp_type != RESP_NONE && resp_type != RESP_R3 && resp_type != RESP_136)
        cmd_reg |= CMD_IDX_CHECK;
    if (cmd_idx == CMD17 || cmd_idx == CMD24)
        cmd_reg |= CMD_DATA_PRESENT;

    SD_WRITE_REG16(SD_CMD_R, (uint16)cmd_reg);

    time_out = 0x500000;
    while (1)
    {
        uint32 status = SD_READ_REG32(SD_NORMAL_INT_STAT_R);
        if (status & INT_ERR)
        {
            if (!ignore_err)
                printf("k230_sd: CMD%d Error! STAT=0x%x\n", cmd_idx, status);
            SD_WRITE_REG8(SD_SW_RST_R, 0x2);
            return -1;
        }
        if (status & INT_CMD_COMPLETE)
        {
            if (resp_out)
                *resp_out = SD_READ_REG32(SD_RESP01_R);
            SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, INT_CMD_COMPLETE);
            return 0;
        }
        if (time_out-- == 0)
            return -1;
    }
}

// 物理读扇区
int sd_read_sector(uint32 sector, uint32 *dst)
{
    SD_WRITE_REG16(SD_BLOCKSIZE_R, 512);
    SD_WRITE_REG16(SD_BLOCKCOUNT_R, 1);
    SD_WRITE_REG16(SD_XFER_MODE_R, 0x10);
    if (sd_send_cmd(CMD17, sector, RESP_48, 0, 0) < 0)
        return -1;

    uint64 time_out = 0xFFFFFFF;
    while (time_out--)
    {
        uint32 status = SD_READ_REG32(SD_NORMAL_INT_STAT_R);
        if (status & INT_ERR)
            return -1;
        if (status & INT_BUF_READ_RDY)
        {
            for (int i = 0; i < 128; i++)
                *dst++ = SD_READ_REG32(SD_BUF_DATA_R);
            SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, INT_BUF_READ_RDY);
        }
        if (status & INT_XFER_COMPLETE)
        {
            SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, INT_XFER_COMPLETE);
            return 0;
        }
    }
    return -2;
}

// 物理写扇区
int sd_write_sector(uint32 sector, uint32 *src)
{
    SD_WRITE_REG16(SD_BLOCKSIZE_R, 512);
    SD_WRITE_REG16(SD_BLOCKCOUNT_R, 1);
    SD_WRITE_REG16(SD_XFER_MODE_R, 0x00);
    if (sd_send_cmd(CMD24, sector, RESP_48, 0, 0) < 0)
        return -1;

    uint64 time_out = 0xFFFFFFF;
    while (time_out--)
    {
        uint32 status = SD_READ_REG32(SD_NORMAL_INT_STAT_R);
        if (status & INT_ERR)
            return -1;
        if (status & INT_BUF_WRITE_RDY)
        {
            for (int i = 0; i < 128; i++)
                SD_WRITE_REG32(SD_BUF_DATA_R, *src++);
            SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, INT_BUF_WRITE_RDY);
        }
        if (status & INT_XFER_COMPLETE)
        {
            SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, INT_XFER_COMPLETE);
            return 0;
        }
    }
    return -2;
}

// --- 初始化与挂载逻辑 ---

void sd_reset_and_clock()
{
    SD_WRITE_REG8(SD_SW_RST_R, 0x6);
    delay(1000);
    while (SD_READ_REG8(SD_SW_RST_R) & 0x6)
        ;
    uint16 clk = SD_READ_REG16(SD_CLK_CTRL_R);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk & ~0x4);
    clk &= ~0xFF00;
    clk |= (0x80 << 8);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk | 0x1 | 0x4);
    SD_WRITE_REG8(SD_TOUT_CTRL_R, 0xE);
    SD_WRITE_REG32(SD_NORMAL_INT_STAT_EN_R, 0xFFFFFFFF);
    SD_WRITE_REG32(SD_NORMAL_INT_SIG_EN_R, 0xFFFFFFFF);
    delay(100000);
}

// 自动寻找最大的 FAT32 分区并设置为挂载点
void sd_mount_largest_partition()
{
    static uint32 buf[128];
    
    printf("\n=== SD Card GPT Partition Detection ===\n");
    
    // GPT boot分区在LBA 0xf000 (61440)
    fat32_offset_sector = 0xf000;
    
    printf("[SD] Using boot partition at LBA 0x%x (%d)\n", 
           fat32_offset_sector, fat32_offset_sector);
    
    // 验证超级块
    uint32 sb_sector = fat32_offset_sector + 2;  // Block 1 = LBA + 2
    if (sd_read_sector(sb_sector, buf) == 0) {
        struct superblock {
            uint magic;
            uint size;
            uint nblocks;
            uint ninodes;
            uint nlog;
            uint logstart;
            uint inodestart;
            uint bmapstart;
        } *sb = (struct superblock *)buf;
        
        printf("  Superblock verification:\n");
        printf("    Magic     : 0x%x ", sb->magic);
        
        if (sb->magic == 0x10205555 || sb->magic == 0x10203040) {
            printf("✓\n");
            printf("    Size      : %d blocks\n", sb->size);
            printf("    Data blks : %d\n", sb->nblocks);
            printf("    Ninodes   : %d\n", sb->ninodes);
            printf("    Log start : %d\n", sb->logstart);
        } else {
            printf("✗ (got 0x%x)\n", sb->magic);
        }
    }
    
    printf("===================================\n\n");
}

void sd_init(void)
{

            printf("DEBUG: BSIZE in bio.c is %d\n", BSIZE);
    uint32 resp;
    initlock(&sd_lock, "sd_card");


            printf("sd driver init...\n");
    sd_reset_and_clock();

    sd_send_cmd(CMD0, 0, RESP_NONE, 1, 0);
    delay(2000);

    if (sd_send_cmd(CMD8, 0x1AA, RESP_48, 0, &resp) != 0)
        return;

    int retry = 5000;
    while (retry--)
    {
        sd_send_cmd(CMD55, 0, RESP_48, 0, 0);
        if (sd_send_cmd(ACMD41, 0x40FF8000, RESP_R3, 0, &resp) == 0 && (resp >> 31))
            break;
        delay(2000);
    }

    sd_send_cmd(CMD2, 0, RESP_136, 0, 0);
    if (sd_send_cmd(CMD3, 0, RESP_48, 0, &resp) == 0)
        sd_rca = resp & 0xFFFF0000;

    sd_send_cmd(CMD7, sd_rca, RESP_48_BUSY, 0, 0);

    sd_send_cmd(CMD55, sd_rca, RESP_48, 0, 0);
    sd_send_cmd(ACMD6, 0x2, RESP_48, 0, 0);
    uint8 host_ctrl = SD_READ_REG8(SD_HOST_CTRL1_R);
    SD_WRITE_REG8(SD_HOST_CTRL1_R, host_ctrl | 0x02);

    uint16 clk = SD_READ_REG16(SD_CLK_CTRL_R);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk & ~0x4);
    clk &= ~0xFF00;
    clk |= (0x02 << 8);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk | 0x4);
    while (!(SD_READ_REG16(SD_CLK_CTRL_R) & 0x2))
        ;

    // 自动挂载 (找到那个 60GB 的大分区)
    sd_mount_largest_partition();

    // [重要] 去掉了 panic，驱动初始化完就返回，让内核继续启动！
}

// --- 操作系统接口 (Buffer Cache Bridge) ---

// kernel/k230_sd.c

void sd_disk_rw(struct buf *b, int write)
{
    // [调试点 1] 打印入口请求
    // 观察 b->blockno 是多少。
    // 如果一直是 0 或其他奇怪的数字，说明上层调用就有问题。
    // 如果是 1，说明终于开始读超级块了。

    // printf("[DEBUG] sd_disk_rw: Request Block=%d, Mode=%s\n",
    //     b->blockno, write ? "WRITE" : "READ");

    if (BSIZE % 512 != 0)
        panic("sd_disk_rw: BSIZE must be multiple of 512");

    uint32 sector_per_block = BSIZE / 512;
    uint32 start_sector = b->blockno * sector_per_block + fat32_offset_sector;

    // [调试点 2] 打印地址映射计算
    // 检查 start_sector 是否等于你 HxD 里看到的那个偏移量 (2097152 + ...)

    // printf("        --> Mapping: Block %d = Sectors [%d, %d] (Offset: %d)\n",
    //     b->blockno, start_sector, start_sector + sector_per_block - 1, fat32_offset_sector);

    acquire(&sd_lock);

    for (int i = 0; i < sector_per_block; i++)
    {
        uint32 current_sector = start_sector + i;

        // 计算当前扇区对应 buffer 中的内存地址偏移
        // 512 字节 = 128 个 uint32
        uint32 *ptr = (uint32 *)b->data + (i * 128);

        int ret = 0;
        if (write)
        {
            ret = sd_write_sector(current_sector, ptr);
        }
        else
        {
            ret = sd_read_sector(current_sector, ptr);
        }

        if (ret != 0)
        {
            printf("[ERROR] sd_disk_rw: Hardware IO Failed at Sector %d\n", current_sector);
        }
    }

    release(&sd_lock);
    // [新增] 专门抓捕 Block 46 (位图块) 的读取结果
    if (!write && b->blockno == 46)
    {

        // printf("\n[DEBUG] === Bitmap Block 46 Inspection ===\n");
        uint8 *p = (uint8 *)b->data;

        // // 打印前 64 个字节
        // // 我们期望看到：FF FF ... (约49个) ... 然后变成 00 00
        // // 如果全是 FF，那就是读错了！
        // for (int k = 0; k < 64; k++)
        // {
        //     printf("%x ", p[k]);
        //     if ((k + 1) % 16 == 0)
        //             printf("\n");
        // }

        //     printf("==========================================\n\n");
    }
    // [调试点 3] 超级块核查 (最关键的一步！)
    // 如果读的是 Block 1，我们必须把它的内容打印出来看看。
    // 这能直接判定是“读错位置”还是“卡里没数据”。
    if (!write && b->blockno == 1)
    {
        struct superblock *sb = (struct superblock *)b->data;


        // printf("\n[DEBUG] === Superblock Inspection (Block 1) ===\n");
        // printf("        Magic Number : 0x%x (Expect: 0x10205555 or 0x10203040)\n", sb->magic);
        // printf("        Size (blocks): %d\n", sb->size);
        // printf("        Ninodes      : %d\n", sb->ninodes);
        // printf("===========================================\n\n");

        // 严重错误预警：如果读出来全是 0
        if (sb->magic == 0)
        {
            printf("[FATAL] Superblock is ZERO! We are reading an empty sector.\n");
            printf("        Possible causes:\n");
            printf("        1. 'fat32_offset_sector' variable is WRONG.\n");
            printf("        2. The SD card write failed (data not saved).\n");
        }
    }
}