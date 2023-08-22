//xv6-kabs-2020 Labs Utility
//primes
//埃氏筛的管道方式实现
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
primes(int read_fd) {//read_fd为上一个进程的读管道文件描述符
  int top;//当前输出的质数(从管道读入的第一个数)
  int cur;//后续读入的数
  int p[2];//管道

  if (read(read_fd, &top, sizeof(int)) > 0) {//如果还有读入
    printf("prime %d\n", top);
    pipe(p);
    int pid = fork();
    if (pid < 0) {//进程创建失败
      fprintf(2, "Error in primes: Fork failed.\n");
      exit(0);
    } else if (pid == 0) {//子进程
      close(p[0]);//子进程不需要读管道
      while (read(read_fd, &cur, sizeof(int)) > 0) {
        if (cur % top != 0) {//筛选并写入管道
          write(p[1], &cur, sizeof(int));
        }
      }
      close(p[1]);
    } else {//父进程
      close(p[1]);//父进程不需要用到写管道
      wait(0);
      primes(p[0]);//将管道读端作为参数传入函数
      close(p[0]);//关闭父进程读管道
    }
  } else {
    close(read_fd);
    exit(0);
  }
}

int
main(void) {
  int p[2];
  pipe(p);

  for (int i = 2; i <= 35; ++i) {
    write(p[1], &i, sizeof(int));
  }
  close(p[1]);
  primes(p[0]);
  close(p[0]);
  exit(0);
}

