//xv6=labs-2020 Labs Utilities
//find
//参考ls
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"//文件系统相关头文件,使用相关结构体

char*
fmtname(char *path)//ls中的函数(更改过),将文件转化为文件名,(例如将"./echo"转化为"echo",find的path就是前者
{
  char *p;
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}

void
find(char *path, char *fileName) {//path为当前目录,fileName为文件名
  int fd;
  struct dirent de;//目录项结构体,包括文件名(大小为DIRSIZ=14的char数组)与inum(inode号,类型为ushort)
  struct stat st;//文件信息结构体(包括文件类型/大小等)
  char buf[512];
  char *p;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "Error in find: Failed to open %s.\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "Error in find: Failed to stat %s.\n", path);
    close(fd);
    return;
  }//读取文件及其信息

  switch (st.type) {//判断文件类型
    case T_FILE://如果是文件,则比较
      if (!strcmp(fileName, fmtname(path))) {
        printf("%s\n", path);
      }
      break;

    case T_DIR://如果是目录,则进入下级目录搜索
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        fprintf(2, "Error in find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';//把当前目录复制到buf中并且末尾添加'/'

      // 读取目录中的每一项
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (!de.inum || !strcmp(de.name, ".") || !strcmp(de.name, "..")) {//不递归"."和".."(inum=0是无效目录项)
          continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        find(buf, fileName);
      }
      break;
  }

  close(fd);
}

int
main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(2, "Usage: find <path> <fileName>\n");
    exit(1);
  } else if(argc == 2){
    find(".", argv[1]);
  } else{
    find(argv[1], argv[2]);
  }
  
  exit(0);
}

