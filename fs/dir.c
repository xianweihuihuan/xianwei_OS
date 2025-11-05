#include "dir.h"
#include "debug.h"
#include "file.h"
#include "ide.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

struct dir root_dir;
// 打开根目录
void open_root_dir(struct partition* part) {
  root_dir.inode = inode_open(part, part->sb->root_inode_no);
  root_dir.dir_pos = 0;
}

// 打开对应inode号的目录，创建并返回对应的目录结构
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
  struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
  if (pdir == NULL) {
    printk("sys_malloc for dir failed!\n");
    return NULL;
  }
  pdir->inode = inode_open(part, inode_no);
  pdir->dir_pos = 0;
  return pdir;
}

// 在目录中查找名为name的目录项将对应信息(文件名，文件inode号，文件类型)放入dir_e,返回是否找到结果
bool search_dir_entry(struct partition* part,
                      struct dir* pdir,
                      const char* name,
                      struct dir_entry* dir_e) {
  uint32_t block_cnt = 140;
  uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);
  if (all_blocks == NULL) {
    printk("search_dir_entry: sys_malloc for all blocks");
    return false;
  }

  // 将inode的所有block复制到all_blocks
  uint32_t block_idx = 0;
  while (block_idx < 12) {
    all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    block_idx++;
  }
  block_idx = 0;
  if (pdir->inode->i_sectors[12] != 0) {
    ide_read(part->my_disk, pdir->inode->i_sectors[12], (all_blocks + 12), 1);
  }

  uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
  struct dir_entry* p_de = (struct dir_entry*)buf;
  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;
  // 遍历所有block
  while (block_idx < block_cnt) {
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }
    ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
    uint32_t dir_entry_idx = 0;
    while (dir_entry_idx < dir_entry_cnt) {
      if (!strcmp(name, p_de->filename)) {
        memcpy(dir_e, p_de, dir_entry_size);
        sys_free(buf);
        sys_free(all_blocks);
        return true;
      }
      dir_entry_idx++;
      p_de++;
    }
    block_idx++;
    p_de = (struct dir_entry*)buf;
    memset(buf, 0, SECTOR_SIZE);
  }
  sys_free(buf);
  sys_free(all_blocks);
  return false;
}

// 关闭目录(如果是根目录则不进行任何处理)
void dir_close(struct dir* dir) {
  if (dir == &root_dir) {
    return;
  }
  inode_close(dir->inode);
  sys_free(dir);
}

// 对目录项进行初始化
void create_dir_entry(char* filename,
                      uint32_t inode_no,
                      uint8_t file_type,
                      struct dir_entry* p_de) {
  ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
  memcpy(p_de->filename, filename, strlen(filename));
  p_de->i_no = inode_no;
  p_de->f_type = file_type;
}

// 目录项写入对应的目录的block中，写入磁盘
bool sync_dir_entry(struct dir* parent_dir,
                    struct dir_entry* p_de,
                    void* io_buf) {
  struct inode* dir_inode = parent_dir->inode;
  uint32_t dir_size = dir_inode->i_size;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

  ASSERT(dir_size % dir_entry_size == 0);
  uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
  int32_t block_lba = -1;
  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0};

  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if (dir_inode->i_sectors[12] != 0) {
    ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
  }

  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  int32_t block_bitmap_idx = -1;
  block_idx = 0;
  while (block_idx < 140) {
    block_bitmap_idx = -1;
    // 找到了目录文件中的空洞，申请block并写入目录的inode
    if (all_blocks[block_idx] == 0) {
      block_lba = block_bitmap_alloc(cur_part);
      if (block_lba == -1) {
        printk("alloc block bitmap for sync_dir_entry failed\n");
        return false;
      }
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      ASSERT(block_bitmap_idx != -1);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      block_bitmap_idx = -1;
      if (block_idx < 12) {  // 直接块中创建
        dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
      } else if (block_idx == 12) {  // 创建表和第一个间接块
        dir_inode->i_sectors[12] = block_lba;
        block_lba = -1;
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1) {  // 创建第一个间接块失败，进行回滚
          block_bitmap_idx =
              dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
          bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
          dir_inode->i_sectors[12] = 0;
          printk("alloc block bitmap for sync_dir_entry failed\n");
          return false;
        }
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != -1);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        all_blocks[12] = block_lba;
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                  1);
      } else {  // 创建间接块
        all_blocks[block_idx] = block_lba;
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                  1);
      }
      // 向新创建的block中写入目录项并写入磁盘
      memset(io_buf, 0, 512);
      memcpy(io_buf, p_de, dir_entry_size);
      ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      dir_inode->i_size += dir_entry_size;
      return true;
    }
    // 此时是已经创建过的block，遍历寻找是否有目录项的剩余空间
    ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
    uint8_t dir_entry_idx = 0;
    while (dir_entry_idx < dir_entrys_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
        memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
        ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        dir_inode->i_size += dir_entry_size;
        return true;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }
  printk("directory is full!\n");
  return false;
}

// 删除目录中对应inode号的目录项
bool delete_dir_entry(struct partition* part,
                      struct dir* pdir,
                      uint32_t inode_no,
                      void* io_buf) {
  struct inode* dir_inode = pdir->inode;

  uint32_t block_idx = 0, all_blocks[140] = {0};

  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if (dir_inode->i_sectors[12] != 0) {
    ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
  }

  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  struct dir_entry* dir_entry_found = NULL;
  uint8_t dir_entry_idx, dir_entry_cnt;
  bool is_dir_first_block = false;

  block_idx = 0;
  while (block_idx < 140) {
    is_dir_first_block = false;
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }
    dir_entry_idx = dir_entry_cnt = 0;
    memset(io_buf, 0, SECTOR_SIZE);
    ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);
    // 遍历block寻找目录项
    while (dir_entry_idx < dir_entrys_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
        if (!strcmp((dir_e + dir_entry_idx)->filename, ".")) {
          // 此时是第一个block，该block不能被释放
          is_dir_first_block = true;
        } else if (strcmp((dir_e + dir_entry_idx)->filename, ".") &&
                   strcmp((dir_e + dir_entry_idx)->filename, "..")) {
          // 寻找到不为空的目录项，记录
          dir_entry_cnt++;
          if ((dir_e + dir_entry_idx)->i_no == inode_no) {
            ASSERT(dir_entry_found == NULL);
            // 找到了目标目录项，但仍需遍历完该块以统计该block中的目录项的个数
            dir_entry_found = dir_e + dir_entry_idx;
          }
        }
      }
      dir_entry_idx++;
    }

    // 未找到目标，继续进行循环
    if (dir_entry_found == NULL) {
      block_idx++;
      continue;
    }

    // 运行到这里，说明已经找到了目标
    ASSERT(dir_entry_cnt >= 1);
    // 不是第一个block并且只含有要删除的目录项，需要释放block
    if (dir_entry_cnt == 1 && !is_dir_first_block) {
      uint32_t block_bitmap_idx =
          all_blocks[block_idx] - part->sb->data_start_lba;
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      if (block_idx < 12) {  // 是直接块
        dir_inode->i_sectors[block_idx] = 0;
      } else {
        uint32_t indirect_blocks = 0;
        uint32_t indirect_block_idx = 12;
        // 统计间接块的个数
        while (indirect_block_idx < 140) {
          if (all_blocks[indirect_block_idx] != 0) {
            indirect_blocks++;
          }
          indirect_block_idx++;
        }
        ASSERT(indirect_blocks >= 1);
        // 间接块多于一个，只需要释放对应的间接块
        if (indirect_blocks > 1) {
          all_blocks[block_idx] = 0;
          ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                    1);
        } else {  // 间接块只有一个，释放间接块表的block
          block_bitmap_idx =
              dir_inode->i_sectors[12] - part->sb->data_start_lba;
          bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
          bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
          dir_inode->i_sectors[12] = 0;
        }
      }
    } else {  // 将对应的目录项置0并写入磁盘
      memset(dir_entry_found, 0, dir_entry_size);
      ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
    }

    ASSERT(dir_inode->i_size >= dir_entry_size);
    dir_inode->i_size -= dir_entry_size;
    memset(io_buf, 0, SECTOR_SIZE * 2);
    // 将新的inode信息更新到磁盘
    inode_sync(part, dir_inode, io_buf);
    return true;
  }
  return false;
}

// 读取目录中的目录项，需要循环调用，根据dir->dir_pos判断已经读取了多少目录项，如果读取结束则返回NULL
struct dir_entry* dir_read(struct dir* dir) {
  struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
  struct inode* dir_inode = dir->inode;

  uint32_t all_blocks[140] = {0}, block_cnt = 12;
  uint32_t block_idx = 0, dir_entry_idx = 0;
  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if (dir_inode->i_sectors[12] != 0) {
    ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;
  }
  block_idx = 0;

  uint32_t cur_dir_entry_pos = 0;//记录已经读取的字节数
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
  while (block_idx < block_cnt) {
    if (dir->dir_pos >= dir_inode->i_size) {
      return NULL;
    }
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }
    memset(dir_e, 0, SECTOR_SIZE);
    ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);
    dir_entry_idx = 0;
    //开是遍历对应的block
    while (dir_entry_idx < dir_entrys_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type) {
        if (cur_dir_entry_pos < dir->dir_pos) {
          cur_dir_entry_pos += dir_entry_size;
          dir_entry_idx++;
          continue;
        }
        // 找到了本次该读取的目录项位置，更新dir->dir_pos，并返回目录项
        ASSERT(cur_dir_entry_pos == dir->dir_pos);
        dir->dir_pos += dir_entry_size;
        return dir_e + dir_entry_idx;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }
  return NULL;
}

//空目录只有.和..两个目录项
bool dir_is_empty(struct dir* dir) {
  struct inode* dir_inode = dir->inode;
  return (dir_inode->i_size == 2 * cur_part->sb->dir_entry_size);
}

//删除空目录
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir) {
  struct inode* child_dir_inode = child_dir->inode;
  int32_t block_idx = 1;
  while (block_idx < 13) {
    ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
    block_idx++;
  }
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    printk("dir_remove: malloc for io_buf failed!\n");
    return -1;
  }
  //删除符目录中的对应目录项
  delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);
  // 释放该目录的inode和block
  inode_release(cur_part, child_dir_inode->i_no);
  sys_free(io_buf);
  return 0;
}
