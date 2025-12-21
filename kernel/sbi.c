#include "types.h"
#include "sbi.h"
#include "defs.h"

// ====================================================================
// RISC-V SBI (Supervisor Binary Interface) 实现
// ====================================================================

// 执行 SBI ecall 调用
struct sbiret sbi_call(int ext, int fid,
                       unsigned long arg0,
                       unsigned long arg1,
                       unsigned long arg2,
                       unsigned long arg3,
                       unsigned long arg4,
                       unsigned long arg5)
{
    struct sbiret ret;
    register unsigned long a0 asm("a0") = arg0;
    register unsigned long a1 asm("a1") = arg1;
    register unsigned long a2 asm("a2") = arg2;
    register unsigned long a3 asm("a3") = arg3;
    register unsigned long a4 asm("a4") = arg4;
    register unsigned long a5 asm("a5") = arg5;
    register unsigned long a6 asm("a6") = (unsigned long)fid;
    register unsigned long a7 asm("a7") = (unsigned long)ext;

    asm volatile(
        "ecall"
        : "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
        : "memory"
    );
    
    ret.error = a0;
    ret.value = a1;
    return ret;
}

// ====================================================================
// 系统重置接口
// ====================================================================

// 关机
void sbi_shutdown(void)
{
    printf("\n[SBI] System Shutting Down...\n");
    sbi_call(SBI_EXT_SRST, SBI_SRST_FID_RESET,
             SBI_SRST_TYPE_SHUTDOWN,
             SBI_SRST_REASON_NONE,
             0, 0, 0, 0);
    
    // 不应该执行到这里
    while (1);
}

// 重启
void sbi_reboot(void)
{
    printf("\n[SBI] System Rebooting...\n");
    sbi_call(SBI_EXT_SRST, SBI_SRST_FID_RESET,
             SBI_SRST_TYPE_COLD_REBOOT,
             SBI_SRST_REASON_NONE,
             0, 0, 0, 0);
    
    // 不应该执行到这里
    while (1);
}

// ====================================================================
// 基础信息查询（可选，用于调试）
// ====================================================================

// 获取 SBI 规范版本
long sbi_get_spec_version(void)
{
    struct sbiret ret = sbi_call(SBI_EXT_BASE, 0, 0, 0, 0, 0, 0, 0);
    if (ret.error == 0) {
        return ret.value;
    }
    return -1;
}

// 获取 SBI 实现 ID
long sbi_get_impl_id(void)
{
    struct sbiret ret = sbi_call(SBI_EXT_BASE, 1, 0, 0, 0, 0, 0, 0);
    if (ret.error == 0) {
        return ret.value;
    }
    return -1;
}