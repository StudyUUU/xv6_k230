#ifndef SBI_H
#define SBI_H

#include "types.h"

// ====================================================================
// RISC-V SBI (Supervisor Binary Interface) 定义
// ====================================================================

// SBI 调用返回结构
struct sbiret {
    long error;
    long value;
};

// ====================================================================
// SBI Extension IDs
// ====================================================================

#define SBI_EXT_BASE            0x10        // Base Extension
#define SBI_EXT_TIME            0x54494D45  // Timer Extension
#define SBI_EXT_IPI             0x735049    // IPI Extension
#define SBI_EXT_RFENCE          0x52464E43  // Remote Fence Extension
#define SBI_EXT_HSM             0x48534D    // Hart State Management
#define SBI_EXT_SRST            0x53525354  // System Reset Extension
#define SBI_EXT_PMU             0x504D55    // Performance Monitoring Unit

// ====================================================================
// System Reset Extension (0x53525354)
// ====================================================================

#define SBI_SRST_FID_RESET      0

// Reset Types
#define SBI_SRST_TYPE_SHUTDOWN      0
#define SBI_SRST_TYPE_COLD_REBOOT   1
#define SBI_SRST_TYPE_WARM_REBOOT   2

// Reset Reasons
#define SBI_SRST_REASON_NONE            0
#define SBI_SRST_REASON_SYSTEM_FAILURE  1

// ====================================================================
// SBI 函数接口
// ====================================================================

// 通用 SBI 调用
struct sbiret sbi_call(int ext, int fid, 
                       unsigned long arg0,
                       unsigned long arg1, 
                       unsigned long arg2,
                       unsigned long arg3,
                       unsigned long arg4,
                       unsigned long arg5);

// 系统控制
void sbi_shutdown(void);
void sbi_reboot(void);

// 基础信息查询
long sbi_get_spec_version(void);
long sbi_get_impl_id(void);

#endif // SBI_H