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
uint32 fat32_offset_sector = 0;
uint32 sd_rca = 0;

// 延时函数
void delay(int count)
{
    volatile int i = count * 500;
    while (i > 0)
        i--;
}

// ============================================================================
// SD卡命令传输层
// ============================================================================

int sd_send_cmd(int cmd_idx, uint32 arg, int resp_type, int ignore_err, uint32 *resp_out)
{
    uint32 cmd_reg = 0;
    uint32 time_out = 100000;

    // 等待控制器空闲
    while (SD_READ_REG32(SD_PSTATE_REG) & 0x1)
    {
        if (time_out-- == 0)
        {
            SD_WRITE_REG8(SD_SW_RST_R, 0x2);
            delay(1000);
            if (SD_READ_REG32(SD_PSTATE_REG) & 0x1) {
                if (!ignore_err)
                    printf("sd: CMD%d timeout waiting for controller ready\n", cmd_idx);
                return -1;
            }
        }
    }

    // 清除中断状态
    SD_WRITE_REG32(SD_NORMAL_INT_STAT_R, 0xFFFFFFFF);
    SD_WRITE_REG32(SD_ERROR_INT_STAT_R, 0xFFFFFFFF);

    // 设置传输模式（仅对读写命令）
    if (cmd_idx != CMD17 && cmd_idx != CMD24)
        SD_WRITE_REG16(SD_XFER_MODE_R, 0x0);
    
    // 设置命令参数
    SD_WRITE_REG32(SD_ARGUMENT_R, arg);

    // 构造命令寄存器
    cmd_reg |= (cmd_idx << 8);
    
    // 设置响应类型
    if (resp_type == RESP_136)
        cmd_reg |= 0x1;
    else if (resp_type == RESP_48)
        cmd_reg |= 0x2;
    else if (resp_type == RESP_48_BUSY)
        cmd_reg |= 0x3;
    else if (resp_type == RESP_R3)
        cmd_reg |= 0x2;

    // 设置CRC和索引检查（R3响应除外）
    if (resp_type != RESP_NONE && resp_type != RESP_R3)
        cmd_reg |= CMD_CRC_CHECK;
    if (resp_type != RESP_NONE && resp_type != RESP_R3 && resp_type != RESP_136)
        cmd_reg |= CMD_IDX_CHECK;
    if (cmd_idx == CMD17 || cmd_idx == CMD24)
        cmd_reg |= CMD_DATA_PRESENT;

    // 发送命令
    SD_WRITE_REG16(SD_CMD_R, (uint16)cmd_reg);

    // 等待命令完成
    time_out = 0x500000;
    while (1)
    {
        uint32 status = SD_READ_REG32(SD_NORMAL_INT_STAT_R);
        
        if (status & INT_ERR)
        {
            if (!ignore_err) {
                uint32 err_stat = SD_READ_REG32(SD_ERROR_INT_STAT_R);
                printf("sd: CMD%d error, status=0x%x, error=0x%x\n", 
                       cmd_idx, status, err_stat);
            }
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
        
        if (time_out-- == 0) {
            if (!ignore_err)
                printf("sd: CMD%d timeout waiting for completion\n", cmd_idx);
            return -1;
        }
    }
}

// 读取单个扇区（512字节）
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
        
        if (status & INT_ERR) {
            printf("sd: read sector %d data error\n", sector);
            return -1;
        }
        
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
    
    printf("sd: read sector %d timeout\n", sector);
    return -2;
}

// 写入单个扇区（512字节）
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
        
        if (status & INT_ERR) {
            printf("sd: write sector %d data error\n", sector);
            return -1;
        }
        
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
    
    printf("sd: write sector %d timeout\n", sector);
    return -2;
}

// ============================================================================
// SD卡初始化
// ============================================================================

void sd_reset_and_clock()
{
    // 软复位
    SD_WRITE_REG8(SD_SW_RST_R, 0x6);
    delay(1000);
    while (SD_READ_REG8(SD_SW_RST_R) & 0x6)
        ;
    
    // 设置时钟（400KHz初始化时钟）
    uint16 clk = SD_READ_REG16(SD_CLK_CTRL_R);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk & ~0x4);
    clk &= ~0xFF00;
    clk |= (0x80 << 8);  // 分频系数
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk | 0x1 | 0x4);
    
    // 设置超时和中断
    SD_WRITE_REG8(SD_TOUT_CTRL_R, 0xE);
    SD_WRITE_REG32(SD_NORMAL_INT_STAT_EN_R, 0xFFFFFFFF);
    SD_WRITE_REG32(SD_NORMAL_INT_SIG_EN_R, 0xFFFFFFFF);
    
    delay(100000);
}

void sd_mount_partition()
{
    static uint32 buf[128];
    
    // K230 GPT分区：boot分区在LBA 0xf000 (61440)
    fat32_offset_sector = 0xf000;
    
    // 验证超级块
    uint32 sb_sector = fat32_offset_sector + 2;  // Block 1对应扇区2-3
    if (sd_read_sector(sb_sector, buf) == 0) {
        struct superblock *sb = (struct superblock *)buf;
        
        if (sb->magic == 0x10205555 || sb->magic == 0x10203040) {
            printf("sd: mounted xv6 filesystem at LBA 0x%x\n", fat32_offset_sector);
            printf("    %d blocks, %d inodes\n", sb->size, sb->ninodes);
            return;
        } else {
            printf("sd: warning - invalid superblock magic 0x%x at LBA 0x%x\n", 
                   sb->magic, fat32_offset_sector);
        }
    } else {
        printf("sd: error reading superblock at LBA 0x%x\n", fat32_offset_sector);
    }
}

void sd_init(void)
{
    uint32 resp;
    
    initlock(&sd_lock, "sd_card");
    
    // 复位并设置初始化时钟
    sd_reset_and_clock();

    // CMD0: 复位卡片
    sd_send_cmd(CMD0, 0, RESP_NONE, 1, 0);
    delay(2000);

    // CMD8: 检查电压和SD 2.0支持
    if (sd_send_cmd(CMD8, 0x1AA, RESP_48, 0, &resp) != 0) {
        printf("sd: CMD8 failed, card not SD 2.0 compatible\n");
        return;
    }

    // ACMD41: 发送操作条件，等待卡片就绪
    int retry = 5000;
    while (retry--)
    {
        sd_send_cmd(CMD55, 0, RESP_48, 0, 0);
        if (sd_send_cmd(ACMD41, 0x40FF8000, RESP_R3, 0, &resp) == 0 && (resp >> 31))
            break;
        delay(2000);
    }
    if (retry <= 0) {
        printf("sd: ACMD41 timeout, card initialization failed\n");
        return;
    }

    // CMD2: 获取CID
    sd_send_cmd(CMD2, 0, RESP_136, 0, 0);
    
    // CMD3: 获取相对地址（RCA）
    if (sd_send_cmd(CMD3, 0, RESP_48, 0, &resp) == 0)
        sd_rca = resp & 0xFFFF0000;

    // CMD7: 选中卡片
    sd_send_cmd(CMD7, sd_rca, RESP_48_BUSY, 0, 0);

    // ACMD6: 设置4-bit总线宽度
    sd_send_cmd(CMD55, sd_rca, RESP_48, 0, 0);
    sd_send_cmd(ACMD6, 0x2, RESP_48, 0, 0);
    uint8 host_ctrl = SD_READ_REG8(SD_HOST_CTRL1_R);
    SD_WRITE_REG8(SD_HOST_CTRL1_R, host_ctrl | 0x02);

    // 切换到高速时钟（25MHz）
    uint16 clk = SD_READ_REG16(SD_CLK_CTRL_R);
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk & ~0x4);
    clk &= ~0xFF00;
    clk |= (0x02 << 8);  // 25MHz分频
    SD_WRITE_REG16(SD_CLK_CTRL_R, clk | 0x4);
    while (!(SD_READ_REG16(SD_CLK_CTRL_R) & 0x2))
        ;

    // 挂载文件系统分区
    sd_mount_partition();
}

// ============================================================================
// Buffer Cache接口
// ============================================================================

void sd_disk_rw(struct buf *b, int write)
{
    if (BSIZE % 512 != 0)
        panic("sd_disk_rw: BSIZE must be multiple of 512");

    uint32 sector_per_block = BSIZE / 512;
    uint32 start_sector = b->blockno * sector_per_block + fat32_offset_sector;

    acquire(&sd_lock);

    for (int i = 0; i < sector_per_block; i++)
    {
        uint32 current_sector = start_sector + i;
        uint32 *ptr = (uint32 *)b->data + (i * 128);

        int ret = write ? sd_write_sector(current_sector, ptr) 
                        : sd_read_sector(current_sector, ptr);

        if (ret != 0) {
            release(&sd_lock);
            printf("sd_disk_rw: I/O error on block %d sector %d\n", 
                   b->blockno, current_sector);
            panic("sd_disk_rw");
        }
    }

    release(&sd_lock);
}