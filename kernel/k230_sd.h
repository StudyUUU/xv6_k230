#ifndef K230_SD_H
#define K230_SD_H

#include "types.h"
#include "memlayout.h"

/*
 * K230 SD 卡驱动头文件
 * 
 * 本驱动基于 SDHCI 标准接口，支持 SD 2.0 协议
 * 采用 PIO 模式进行数据传输（非 DMA）
 * 
 * 关键功能：
 * - SD 卡初始化（CMD0/CMD2/CMD3/CMD7/CMD8/ACMD41/ACMD6）
 * - 单块读写（CMD17/CMD24，512 字节扇区）
 * - GPT 分区表解析（支持 K230 标准镜像格式）
 * - 与 xv6 buffer cache 集成（通过 sd_disk_rw）
 */

// ============================================================================
// SDHCI 标准寄存器偏移量定义
// ============================================================================

// --- 数据传输寄存器 ---
#define SD_BLOCKSIZE_R 0x04          // 块大小：通常设为 512 字节
#define SD_BLOCKCOUNT_R 0x06         // 块数量：单块传输设为 1
#define SD_ARGUMENT_R 0x08           // 命令参数：存放在 CMD 线上发送的 32 位参数
#define SD_XFER_MODE_R 0x0C          // 传输模式：设置读/写、单/多块、DMA 使能等
#define SD_BUF_DATA_R 0x20           // 缓冲区数据：PIO 模式下 CPU 读写数据的窗口

// --- 命令与响应寄存器 ---
#define SD_CMD_R 0x0E                // 命令寄存器：写入此寄存器会立即触发命令发送
#define SD_RESP01_R 0x10             // 响应寄存器：存放卡片返回的 48/136 位响应

// --- 控制与状态寄存器 ---
#define SD_PSTATE_REG 0x24           // 状态寄存器：查看总线忙闲、是否有卡等
#define SD_HOST_CTRL1_R 0x28         // 主机控制：设置 1/4 bit 位宽、电平逻辑
#define SD_PWR_CTRL_R 0x29           // 电源控制
#define SD_CLK_CTRL_R 0x2C           // 时钟控制：分频、内部时钟使能
#define SD_TOUT_CTRL_R 0x2E          // 超时控制
#define SD_SW_RST_R 0x2F             // 软件复位：复位 CMD 或 DAT 状态机
#define SD_CAPABILITIES1_R 0x40      // 控制器能力寄存器

// --- 中断寄存器 ---
#define SD_NORMAL_INT_STAT_R 0x30    // 中断状态：命令完成、传输完成等标志
#define SD_ERROR_INT_STAT_R 0x32     // 错误状态：存放 CRC、超时等具体错误
#define SD_NORMAL_INT_STAT_EN_R 0x34 // 中断状态使能
#define SD_NORMAL_INT_SIG_EN_R 0x38  // 中断信号使能

// ============================================================================
// SD 协议定义
// ============================================================================

// --- 响应类型 ---
#define RESP_NONE 0x00    // 无响应
#define RESP_136 0x01     // 长响应 (R2)，用于 CID, CSD
#define RESP_48 0x02      // 普通 48 位响应 (R1, R3, R7)
#define RESP_48_BUSY 0x03 // 带有 Busy 信号的 R1b
#define RESP_R3 0x04      // R3 响应 (ACMD41 专用，无 CRC)

// --- 命令标志位 ---
#define CMD_CRC_CHECK 0x08    // 硬件自动校验响应的 CRC
#define CMD_IDX_CHECK 0x10    // 硬件自动检查响应中的命令序号
#define CMD_DATA_PRESENT 0x20 // 告知控制器：命令后紧随数据包

// --- 常用 SD 命令索引 ---
#define CMD0 0    // GO_IDLE_STATE: 卡软复位
#define CMD2 2    // ALL_SEND_CID: 获取卡唯一 ID
#define CMD3 3    // SEND_RELATIVE_ADDR: 获取相对地址 RCA
#define CMD7 7    // SELECT_CARD: 选中卡片进入传输状态
#define CMD8 8    // SEND_IF_COND: 校验接口电平和 SD 2.0 支持
#define CMD17 17  // READ_SINGLE_BLOCK: 读单扇区
#define CMD24 24  // WRITE_SINGLE_BLOCK: 写单扇区
#define CMD55 55  // APP_CMD: 发送 ACMD 的前导码
#define ACMD41 41 // SD_SEND_OP_COND: 发送操作条件并触发上电
#define ACMD6 6   // SET_BUS_WIDTH: 切换 1/4 bit

// --- 中断标志位掩码 ---
#define INT_CMD_COMPLETE 0x01  // 命令响应已收到
#define INT_XFER_COMPLETE 0x02 // 数据传输任务已完成
#define INT_BUF_WRITE_RDY 0x10 // 缓冲区已空，可以写入 (PIO)
#define INT_BUF_READ_RDY 0x20  // 缓冲区已满，可以读取 (PIO)
#define INT_ERR 0x8000         // 发生任何错误

// ============================================================================
// 寄存器访问宏（直接物理地址操作）
// ============================================================================

#define SD_READ_REG32(offset) (*(volatile uint32 *)(K230_SD1 + (offset)))
#define SD_READ_REG16(offset) (*(volatile uint16 *)(K230_SD1 + (offset)))
#define SD_READ_REG8(offset) (*(volatile uint8 *)(K230_SD1 + (offset)))
#define SD_WRITE_REG32(offset, val) (*(volatile uint32 *)(K230_SD1 + (offset)) = (val))
#define SD_WRITE_REG16(offset, val) (*(volatile uint16 *)(K230_SD1 + (offset)) = (val))
#define SD_WRITE_REG8(offset, val) (*(volatile uint8 *)(K230_SD1 + (offset)) = (val))

// ============================================================================
// 全局变量声明
// ============================================================================

// 文件系统分区起始扇区（LBA）
// K230 标准镜像中 boot 分区位于 LBA 0xf000 (61440)
extern uint32 fat32_offset_sector;

// SD 卡相对地址（RCA）
// 通过 CMD3 从卡片获取，后续命令需要使用此地址
extern uint32 sd_rca;

// ============================================================================
// 分区表相关结构体（保留，用于未来 MBR/GPT 解析）
// ============================================================================

/*
 * MBR 分区表项结构体（标准定义，16 字节）
 * 使用 __attribute__((packed)) 防止编译器进行字节对齐填充
 */
struct partition_entry
{
    uint8 status;        // 启动标志（0x80 = 可启动）
    uint8 chs_start[3];  // CHS 起始地址（已废弃，现代系统使用 LBA）
    uint8 type;          // 分区类型（0x0C = FAT32 LBA）
    uint8 chs_end[3];    // CHS 结束地址（已废弃）
    uint32 lba_start;    // LBA 起始扇区
    uint32 sector_count; // 扇区数量
} __attribute__((packed));

/*
 * FAT32 引导扇区 (BPB) 结构
 * 注意：xv6 不使用 FAT32，此结构仅用于调试/分析 K230 镜像格式
 */
struct fat32_bpb
{
    uint8 jmp_boot[3];
    uint8 oem_name[8];
    uint16 bytes_per_sector;   // 每扇区字节数（通常 512）
    uint8 sectors_per_cluster; // 每簇扇区数
    uint16 reserved_sectors;   // 保留扇区数
    uint8 num_fats;            // FAT 表数量（通常 2）
    uint16 root_entry_count;   // （FAT32 不使用，为 0）
    uint16 total_sectors_16;   // （FAT32 不使用，为 0）
    uint8 media;
    uint16 fat_sz_16;          // （FAT32 不使用，为 0）
    uint16 sectors_per_track;
    uint16 num_heads;
    uint32 hidden_sectors;
    uint32 total_sectors_32;   // 总扇区数
    uint32 fat_sz_32;          // 一个 FAT 表占用的扇区数
    uint16 ext_flags;
    uint16 fs_version;
    uint32 root_cluster;       // 根目录所在的簇号
    uint16 fs_info;
    uint16 backup_boot_sec;
    uint8 reserved[12];
    uint8 drive_number;
    uint8 reserved1;
    uint8 boot_signature;
    uint32 vol_id;
    uint8 vol_label[11];
    uint8 fs_type[8];          // "FAT32   " 字符串
} __attribute__((packed));

/*
 * FAT32 短文件名目录项（32 字节）
 * 注意：xv6 不使用 FAT32，此结构仅用于调试/分析 K230 镜像格式
 */
struct dir_entry
{
    uint8 name[8];       // 文件名（8 字节）
    uint8 ext[3];        // 扩展名（3 字节）
    uint8 attr;          // 属性（0x10=目录, 0x20=归档, 0x0F=长文件名）
    uint8 reserved;
    uint8 create_time_tenth;
    uint16 create_time;
    uint16 create_date;
    uint16 access_date;
    uint16 cluster_high; // 簇号高 16 位
    uint16 write_time;
    uint16 write_date;
    uint16 cluster_low;  // 簇号低 16 位
    uint32 file_size;
} __attribute__((packed));

#endif // K230_SD_H