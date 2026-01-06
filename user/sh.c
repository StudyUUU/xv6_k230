// user/sh.c
#include "kernel/types.h"

// 简单声明
int write(int, const void*, int);
int read(int, void*, int);

int main(void)
{
    char buf[64];
    
    // 打印欢迎语
    write(1, "\nWelcome to xv6-k230!\n", 22);
    
    while(1) {
        // 打印提示符
        write(1, "$ ", 2);
        
        // 读取输入（阻塞等待）
        // 这会测试你的 console read 和 UART 中断逻辑
        int n = read(0, buf, sizeof(buf));
        
        if(n > 0){
             // 简单回显，证明读写都通了
             write(1, "You typed: ", 11);
             write(1, buf, n);
        }
    }
    return 0;
}