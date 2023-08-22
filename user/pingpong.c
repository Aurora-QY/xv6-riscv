//xv6-kabs-2020 Labs Utility
//pingpong
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void) {
  char buffer[16];         // 缓冲区
  int parent_to_child[2];  // 父进程到子进程的管道
  int child_to_parent[2];  // 子进程到父进程的管道

  //检查管道创建是否出错
  if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
    fprintf(2, "Error in pingpong: Failed to create pipe.\n");
    exit(1);
  }

  int pid = fork();
  
  //检查进程创建是否出错
  if (pid < 0) {
    fprintf(2, "Error in pingpong: Fork failed.\n");
    exit(1);
  }

  if (pid == 0) {// 子进程
    close(parent_to_child[1]);  // 关闭父进程写端
    close(child_to_parent[0]);  // 关闭子进程读端
    
    read(parent_to_child[0], buffer, strlen("ping"));
    printf("%d: received %s\n", getpid(), buffer);
    write(child_to_parent[1], "pong", strlen("pong"));

    close(parent_to_child[0]);  // 关闭父进程读端
    close(child_to_parent[1]);  // 关闭子进程写端
  } else {
    // 父进程
    close(parent_to_child[0]);  // 关闭父进程读端
    close(child_to_parent[1]);  // 关闭子进程写端

    write(parent_to_child[1], "ping", strlen("ping"));
    wait(0);
    //等到子进程退出
    read(child_to_parent[0], buffer, strlen("pong"));
    printf("%d: received %s\n", getpid(), buffer);

    close(parent_to_child[1]);  // 关闭父进程写端
    close(child_to_parent[0]);  // 关闭子进程读端

    wait(0);  // 等待子进程退出
  }

  exit(0);
}

