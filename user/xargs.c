//xv6=labs-2020 Labs Utilities
//xargs
//(eXtended ARGuments)用于组合多个命令(将管道或标准输入转换为命令行参数/捕获输出等功能)
//允许向一个命令中传输多个参数,以xargstest.sh运行为例,使用find找到的./a/b  ./c/b  ./b(换行符隔开)分行传给grep(每次只接受一个参数,共运行三次)(read时会将上述内容全部读入而不是逐行读入)
//read不会因为读到换行符停止,而是读到文件末尾或指定长度
#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define BUFSIZ 1024 //最大缓存

int
main(int argc, char *argv[]) {
  char buf[BUFSIZ]; //用于存放读入数据的缓存(包括换行符,可能是多行)
  char *cmd[MAXARG];//MAXARG为定义在kernel/param.h中的常量,表示一个进程的最大命令行参数数量(值为32)
  int cmd_idx = 0; //除去xargs和指令名称以外的参数数量
  if (argc < 2) {
    fprintf(2, "Usage: xargs <command>\n");
    exit(1);
  }
  
  for (int i = 1; i < argc; ++i) { //从第二个参数开始读取(argv[0]是xargs,忽略;argv[1]是要运行的指令名称)
    cmd[cmd_idx++] = argv[i];
  }
  
  int n = 0;//读入长度
  while((n = read(0, buf, BUFSIZ)) > 0){//未结束读入则循环
    char *p = buf; //指向每次'/n'后一字符位置的指针(初始指向buf开头)
    for(int i = 0; i < n; ++i){
      if(buf[i] == '\n'){//对本次读入逐字搜索,若找到换行符,则运行指令
        buf[i] = '\0';
        cmd[cmd_idx] = p;
        p = buf + i + 1;
        
        int pid = fork();
         if (pid < 0) {
          fprintf(2, "Error in xargs: Failed to fork\n");
          exit(1);
        } else if (pid == 0) {
          // 子进程中执行命令
          exec(argv[1], cmd);
          fprintf(2, "Error in xargs: Failed to exec\n");
          exit(1);
        } else {
          // 父进程中等待子进程结束
          wait(0);
        }
      }
    }
  }
  
  exit(0);
}
// Lab Xv6 and Unix utilities
// xargs.c

/*#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXN 1024

int
main(int argc, char *argv[])
{
    // 如果参数个数小于 2
    if (argc < 2) {
        // 打印参数错误提示
        fprintf(2, "usage: xargs command\n");
        // 异常退出
        exit(1);
    }
    // 存放子进程 exec 的参数
    char * argvs[MAXARG];
    // 索引
    int index = 0;
    // 略去 xargs ，用来保存命令行参数
    for (int i = 1; i < argc; ++i) {
        argvs[index++] = argv[i];
    }
    // 缓冲区存放从管道读出的数据
    char buf[MAXN] = {"\0"};
    
    int n;
	// 0 代表的是管道的 0，也就是从管道循环读取数据
    while((n = read(0, buf, MAXN)) > 0 ) {
        printf("%s\n",buf);
        // 临时缓冲区存放追加的参数
		char temp[MAXN] = {"\0"};
        // xargs 命令的参数后面再追加参数
        argvs[index] = temp;
        // 内循环获取追加的参数并创建子进程执行命令
        for(int i = 0; i < n; ++i) {
            // 读取单个输入行，当遇到换行符时，创建子线程
            if(buf[i] == '\n') {
                // 创建子线程执行命令
                if (fork() == 0) {
                printf("%s 8 ",argv[1]);
                for(int i = 0; i <= index; ++i) {
                 printf("%s ",argvs[i]);
                }
                printf("\n");
                    exec(argv[1], argvs);
                }
                // 等待子线程执行完毕
                wait(0);
            } else {
                // 否则，读取管道的输出作为输入
                temp[i] = buf[i];
            }
        }
    }
    // 正常退出
    exit(0);
}*/


