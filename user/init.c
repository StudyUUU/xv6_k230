// user/init.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"

// 简单的声明，防止你缺少 user.h
int open(const char*, int);
int mknod(const char*, short, short);
int dup(int);
int fork(void);
int exec(char*, char**);
int wait(int*);
int printf(const char*, ...);
int exit(int);

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  // 1. 尝试打开控制台
  if(open("console", O_RDWR) < 0){
    // 如果失败，尝试创建设备节点
    // major=1, minor=1 是 xv6 console 的标准设备号
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }

  // 2. 复制文件描述符，确保 0, 1, 2 都是控制台
  dup(0);  // stdout
  dup(0);  // stderr

  printf("init: starting sh\n");

  // 3. 主循环：负责重启 Shell
  for(;;){
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // 子进程：执行 shell
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    // 父进程：等待 shell 退出（通常 shell 不会退出，除非崩溃）
    while((wpid=wait(0)) >= 0 && wpid != pid){
      // printf("zombie!\n");
    }
  }
}