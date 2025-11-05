#ifndef __FS_FS_H
#define __FS_FS_H
#include "stdint.h"
#define MAX_FILES_PER_PART 4096
#define BIT_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE

#define MAX_PATH_LEN 512

// 文件类型
enum file_types { FT_UNKNOWN, FT_REGULAR, FT_DIRECTORY };

// 文件打开方式
enum oflags { O_RDONLY, O_WRONLY, O_RDWD, O_CREAT = 4 };

// 文件指针调整的标志
enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };

// 文件的属性
struct stat {
  uint32_t st_ino;              // inode号
  uint32_t st_size;             // 文件大小
  enum file_types st_filetype;  // 文件类型
};

struct path_search_record {
  char searched_path
      [MAX_PATH_LEN];  // 文件的路径，若某目录不存在，则不存在的目录将出现在这里
  struct dir* parent_dir;  // 父目录，因为操作一个文件，经常会操作他的父目录
  enum file_types file_type;  // 文件类型
};
char* path_parse(char* pathname, char* name_store);
int32_t path_depth_cnt(char* pathname);
int32_t sys_open(const char* pathname, uint8_t flags);
extern struct partition* cur_part;
void filesys_init(void);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char* pathname);
int32_t sys_mkdir(const char* pathname);
struct dir* sys_opendir(const char* name);
int32_t sys_closedir(struct dir* dir);
struct dir_entry* sys_readdir(struct dir* dir);
void sys_rewinddir(struct dir* dir);
int32_t sys_rmdir(const char* pathname);
int32_t sys_chdir(const char* path);
char* sys_getcwd(char* buf, uint32_t size);
int32_t sys_stat(const char* path, struct stat* buf);
void sys_putchar(char char_asci);
uint32_t fd_local2global(uint32_t local_fd);
void sys_help(void);
#endif