#ifndef __FS_FILE_H
#define __FS_FILE_H
#include "dir.h"
#include "inode.h"
#include "stdint.h"
#define MAX_FILES_OPEN 32

//file_table 的表项，记录文件的指针，文件的打开方式，以及inode的指针，
//该结构每打开一次文件就会创建一次，对同一文件，不同的进程进行打开也会创建多次，也就是说不同的进程对同一文件的指针是独立的
struct file {
  uint32_t fd_pos;//读写位置
  uint32_t fd_flag;//打开方式
  struct inode* fd_inode;//inode指针
};

//标准输入，标准输出，标准错误
enum std_fd { stdin_no, stdout_no, stderr_no };

//位图标记，inode位图和block位图
enum bitmap_type { INODE_BITMAP, BLOCK_BITMAP };

//全局file_table,记录全局被打开的文件
extern struct file file_table[MAX_FILES_OPEN];

int32_t get_free_slot_in_global();
int32_t pcb_fd_install(int32_t globa_fd_idx);
int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);
int32_t file_open(uint32_t inode_no, uint8_t flag);
int32_t file_close(struct file* file);
int32_t file_write(struct file* file, const void* buf, uint32_t count);
int32_t file_read(struct file* file, void* buf, uint32_t count);
#endif