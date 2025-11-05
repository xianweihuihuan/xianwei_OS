#ifndef __FS_INODE_H
#define __FS_INODE_H
#include "stdint.h"
#include "list.h"
#include "ide.h"


struct inode{
  uint32_t i_no;//inode号
  uint32_t i_size;//文件大小

  uint32_t open_cnts;//打开次数，该成员在硬盘上无意义
  bool write_deny;//是否正在被写入，该成员在硬盘上无意义

  uint32_t i_sectors[13];//文件占用的block地址
  struct list_elem inode_tag;//在分区中打开inode链表中的tag
  
};
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);
struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_close(struct inode* inode);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_release(struct partition* part, uint32_t inode_no);
#endif