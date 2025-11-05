#include "inode.h"
#include "debug.h"
#include "file.h"
#include "ide.h"
#include "interrupt.h"
#include "string.h"
#include "super_block.h"

// 该结构体描述的是inode的位置信息，包括inode位置位于哪个扇区的哪个字节，是否跨扇区
struct inode_position {
  bool two_sec;  // 是否跨扇区
  uint32_t sec_lba;
  uint32_t off_size;
};

// 获取对应inode的位置信息
static void inode_locate(struct partition* part,
                         uint32_t inode_no,
                         struct inode_position* inode_pos) {
  ASSERT(inode_no < 4096);
  uint32_t inode_table_lba = part->sb->inode_table_lba;

  uint32_t inode_size = sizeof(struct inode);
  uint32_t off_size = inode_no * inode_size;

  uint32_t off_sec = off_size / 512;
  uint32_t off_size_in_sec = off_size % 512;

  uint32_t left_in_sec = 512 - off_size_in_sec;
  if (left_in_sec < inode_size) {
    inode_pos->two_sec = true;
  } else {
    inode_pos->two_sec = false;
  }
  inode_pos->sec_lba = inode_table_lba + off_sec;
  inode_pos->off_size = off_size_in_sec;
}

// 将inode的信息同步入磁盘
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
  // 获取inode位置信息
  uint8_t inode_no = inode->i_no;
  struct inode_position inode_pos;
  inode_locate(part, inode_no, &inode_pos);

  ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

  // 将磁盘上不应存在的东西清空
  struct inode pure_inode;
  memcpy(&pure_inode, inode, sizeof(struct inode));

  pure_inode.open_cnts = 0;
  pure_inode.write_deny = false;
  pure_inode.inode_tag.next = pure_inode.inode_tag.prev = NULL;

  // 写入磁盘
  char* inode_buf = (char*)io_buf;
  if (inode_pos.two_sec) {
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
  } else {
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
  }
}

// 打开对应编号的inode
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
  struct list_elem* elem = part->open_inodes.head.next;
  struct inode* inode_found;
  // 若发现该inode已经打开，即能在open_inode中找到，则直接返回
  while (elem != &part->open_inodes.tail) {
    inode_found = elem2entry(struct inode, inode_tag, elem);
    if (inode_found->i_no == inode_no) {
      inode_found->open_cnts++;
      return inode_found;
    }
    elem = elem->next;
  }

  // 获取inode位置信息
  struct inode_position inode_pos;
  inode_locate(part, inode_no, &inode_pos);

  // 此处短暂的将用户进程提升至内核态，因为为inode创建的空间应该是所有进程全部可见的，所以应该申请的是内核空间的堆内存
  struct task_struct* cur = running_thread();
  uint32_t* cur_pagedir_bak = cur->pgdir;
  cur->pgdir = NULL;
  inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
  cur->pgdir = cur_pagedir_bak;

  // 读取inode
  char* inode_buf;
  if (inode_pos.two_sec) {
    inode_buf = (char*)sys_malloc(1024);
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
  } else {
    inode_buf = (char*)sys_malloc(512);
    ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
  }
  memcpy(inode_found, (inode_buf + inode_pos.off_size), sizeof(struct inode));
  // 将inode加入open_inode
  list_push(&part->open_inodes, &inode_found->inode_tag);
  inode_found->open_cnts = 1;
  sys_free(inode_buf);
  return inode_found;
}

//关闭inode
void inode_close(struct inode* inode) {
  enum intr_status old_status = intr_disable();
  //有点像shared_ptr
  if (--inode->open_cnts == 0) {
    list_remove(&inode->inode_tag);
    struct task_struct* cur = running_thread();
    //同样需要提升至内核态，防止释放内存出现错误
    uint32_t* cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    sys_free(inode);
    cur->pgdir = cur_pagedir_bak;
  }
  intr_set_status(old_status);
}

//初始化inode
void inode_init(uint32_t inode_no, struct inode* new_inode) {
  new_inode->i_no = inode_no;
  new_inode->i_size = 0;
  new_inode->open_cnts = 0;
  new_inode->write_deny = false;
  uint8_t sec_idx = 0;
  while (sec_idx < 13) {
    new_inode->i_sectors[sec_idx] = 0;
    sec_idx++;
  }
}

//删除inode，即将硬盘上的inode_table的对应inode位置置0,但这步实际上是不需要的，因为对应的inode是否可用取决于inode_bitmap
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf) {
  ASSERT(inode_no < 4096);
  struct inode_position inode_pos;
  inode_locate(part, inode_no, &inode_pos);
  ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));
  char* inode_buf = (char*)io_buf;

  if (inode_pos.two_sec) {
    ide_read(part->my_disk, inode_pos.sec_lba, io_buf, 2);
    memset((io_buf + inode_pos.off_size), 0, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba, io_buf, 2);
  } else {
    ide_read(part->my_disk, inode_pos.sec_lba, io_buf, 1);
    memset((io_buf + inode_pos.off_size), 0, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba, io_buf, 1);
  }
}

//释放inode，是删除一个文件的步骤之一
void inode_release(struct partition* part, uint32_t inode_no) {
  struct inode* inode_to_del = inode_open(part, inode_no);
  ASSERT(inode_to_del->i_no == inode_no);

  uint8_t block_idx = 0, block_cnt = 12;
  uint32_t block_bitmap_idx;
  uint32_t all_blocks[140] = {0};

  while (block_idx < 12) {
    all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
    block_idx++;
  }

  if (inode_to_del->i_sectors[12] != 0) {
    ide_read(part->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;

    block_bitmap_idx = inode_to_del->i_sectors[12] - part->sb->data_start_lba;
    ASSERT(block_bitmap_idx > 0);
    // 释放间接块表
    bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }
  block_idx = 0;
  //释放所有块
  while (block_idx < block_cnt) {
    if (all_blocks[block_idx] != 0) {
      block_bitmap_idx = 0;
      block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
      ASSERT(block_bitmap_idx > 0);
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }
    block_idx++;
  }

  //回收对应的inode
  bitmap_set(&part->inode_bitmap, inode_no, 0);
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);

  void* io_buf = sys_malloc(BLOCK_SIZE * 2);
  inode_delete(part, inode_no, io_buf);
  sys_free(io_buf);
  inode_close(inode_to_del);
}
