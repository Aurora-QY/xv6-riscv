//xv6-kabs-2020 Labs Utility
//sleep
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
  //检查参数数量，小于2则报错，多于2则忽略后面的
  if (argc < 2) {
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }

  //如果参数小于零则不做任何操作
  int ticks = atoi(argv[1]);
  sleep(ticks);

  exit(0);
}
