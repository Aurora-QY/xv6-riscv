// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

//#define NDIRECT 12
#define NDIRECT 11  //减少一个直接块,增加一个双间接块(lab9-1)
#define NINDIRECT (BSIZE / sizeof(uint))  //单间接块存储指针数量 256
#define NDINDIRECT (NINDIRECT * NINDIRECT)  //双间接块存储指针数量  256*256(lab9-1)
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)  //最大文件上限(lab9-1)

// On-disk inode structure

//NDIRECT+1->NDIRECT+2(lab9-1)
struct dinode {
  short type;           // 文件类型 (T_FILE, T_DIR, T_DEV)
  short major;          // 设备类型的 major 设备号
  short minor;          // 设备类型的 minor 设备号
  short nlink;          // 链接计数器
  uint size;            // 文件大小（字节数）
  uint addrs[NDIRECT+2]; // 直接块和间接块的块号
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// 打开符号链接时最大递归深度
#define MAX_SYMLINK_DEPTH 10 //lab9-2

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

