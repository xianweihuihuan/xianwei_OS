#include "fs.h"
#include "console.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "ide.h"
#include "inode.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "list.h"
#include "pipe.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

// 当前挂载的分区
struct partition* cur_part;

// 挂载分区
static bool mount_partition(struct list_elem* pelem, int arg) {
  char* part_name = (char*)arg;
  struct partition* part = elem2entry(struct partition, part_tag, pelem);
  if (!strcmp(part_name, part->name)) {  // 是要挂载的分区
    cur_part = part;
    struct disk* hd = cur_part->my_disk;
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
    if (cur_part->sb == NULL || sb_buf == NULL) {
      PANIC("alloc memort failed!");
    }
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);
    memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

    // block_bitmap
    cur_part->block_bitmap.bits =
        (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
    if (cur_part->block_bitmap.bits == NULL) {
      PANIC("alloc memort failed!");
    }
    cur_part->block_bitmap.btmp_bytes_len =
        sb_buf->block_bitmap_sects * SECTOR_SIZE;
    ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits,
             sb_buf->block_bitmap_sects);

    // inode_bitmap
    cur_part->inode_bitmap.bits =
        (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);

    if (cur_part->inode_bitmap.bits == NULL) {
      PANIC("alloc memort failed!");
    }
    cur_part->inode_bitmap.btmp_bytes_len =
        sb_buf->inode_bitmap_sects * SECTOR_SIZE;
    ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits,
             sb_buf->inode_bitmap_sects);

    list_init(&cur_part->open_inodes);
    printk("  mount %s done!\n", part->name);
    printk("  sdb1's block_bitmap_lba: %x\n", sb_buf->block_bitmap_lba);
    printk("  sdb1's inode_bitmap_lba: %x\n", sb_buf->inode_bitmap_lba);
    printk("  sdb1's inode_table_lba: %x\n", sb_buf->inode_table_lba);
    printk("  sdb1's data_start_lba: %x\n", sb_buf->data_start_lba);
    sys_free(sb_buf);
    return true;
  }
  return false;
}

// 初始化分区，其实就是为分区的超级块填充该分区的数据和魔数
static void partition_format(struct partition* part) {
  uint32_t boot_sector_sects = 1;
  uint32_t super_block_sects = 1;

  uint32_t inode_bitmap_sects =
      DIV_ROUND_UP(MAX_FILES_PER_PART, BIT_PER_SECTOR);

  uint32_t inode_table_sects =
      DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);

  uint32_t used_sects = boot_sector_sects + super_block_sects +
                        inode_bitmap_sects + inode_table_sects;
  uint32_t free_sects = part->sec_cnt - used_sects;

  uint32_t now_total_free_sects = free_sects;  // 定义一个现在总的可用扇区数
  uint32_t prev_block_bitmap_sects = 0;        // 之前的块位图扇区数
  uint32_t block_bitmap_sects =
      DIV_ROUND_UP(now_total_free_sects, BIT_PER_SECTOR);  // 初始估算
  uint32_t block_bitmap_bit_len;

  while (block_bitmap_sects != prev_block_bitmap_sects) {
    prev_block_bitmap_sects = block_bitmap_sects;
    /* block_bitmap_bit_len是位图中位的长度,也是可用块的数量 */
    block_bitmap_bit_len = now_total_free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BIT_PER_SECTOR);
  }

  struct super_block sb;
  sb.magic = 0x20060127;
  sb.sec_cnt = part->sec_cnt;
  sb.inode_cnt = MAX_FILES_PER_PART;
  sb.part_lba_base = part->start_lba;

  sb.block_bitmap_lba = sb.part_lba_base + 2;
  sb.block_bitmap_sects = block_bitmap_sects;

  sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
  sb.inode_bitmap_sects = inode_bitmap_sects;

  sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
  sb.inode_table_sects = inode_table_sects;

  sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;

  sb.root_inode_no = 0;
  sb.dir_entry_size = sizeof(struct dir_entry);

  printk("  %s info:\n", part->name);
  printk(
      "     magic:0x%x\n     part_lba_base:0x%x\n     all_sectors:0x%x\n     "
      "inode_cnt:0x%x\n     block_bitmap_lba:0x%x\n     "
      "block_bitmap_sectors:0x%x\n     inode_bitmap_lba:0x%x\n     "
      "inode_bitmap_sectors:0x%x\n     inode_table_lba:0x%x\n     "
      "inode_table_sectors:0x%x\n     data_start_lba:0x%x\n",
      sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba,
      sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects,
      sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);
  struct disk* hd = part->my_disk;
  ide_write(hd, part->start_lba + 1, &sb, 1);
  printk("    super_block_lba: 0x%x\n", part->start_lba + 1);
  uint32_t buf_size =
      (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects
                                                      : sb.inode_bitmap_sects);
  buf_size =
      (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) *
      SECTOR_SIZE;
  uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

  // block_bitmap
  buf[0] |= 0x01;
  uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
  uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
  uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
  memset(&buf[block_bitmap_last_byte], 0xff, last_size);
  uint8_t bit_idx = 0;
  while (bit_idx < block_bitmap_last_bit) {
    buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
  }
  ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
  // inode_bitmap
  memset(buf, 0, buf_size);
  buf[0] |= 0x1;
  ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);
  // inode_table
  memset(buf, 0, buf_size);
  struct inode* i = (struct inode*)buf;
  i->i_size = sb.dir_entry_size * 2;
  i->i_no = 0;
  i->i_sectors[0] = sb.data_start_lba;
  ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);
  // 根目录
  memset(buf, 0, buf_size);
  struct dir_entry* p_de = (struct dir_entry*)buf;

  memcpy(p_de->filename, ".", 1);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY;
  p_de++;

  memcpy(p_de->filename, "..", 2);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY;

  ide_write(hd, sb.data_start_lba, buf, 1);
  printk("    root_dir_lba: 0x%x\n", sb.data_start_lba);
  printk("  %s format done\n", part->name);
  sys_free(buf);
}

// 文件系统初始化
void filesys_init() {
  uint8_t channel_no = 0, dev_no = 0, part_index = 0;
  struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
  if (sb_buf == NULL) {
    PANIC("alloc memory failed!");
  }
  printk("  searching filesystem......\n");
  while (channel_no < channel_cnt) {
    dev_no = 0;

    while (dev_no < 2) {
      if (dev_no == 0) {
        dev_no++;
        continue;  // 跳过hd60M.img
      }
      struct disk* hd = &channels[channel_no].devices[dev_no];
      struct partition* part = hd->prim_parts;
      part_index = 0;
      while (part_index < 12) {
        if (part_index == 4) {
          part = hd->logic_parts;
        }
        if (part->sec_cnt != 0) {
          memset(sb_buf, 0, SECTOR_SIZE);
          ide_read(hd, part->start_lba + 1, sb_buf, 1);
          if (sb_buf->magic == 0x20060127) {  // 识别到了自定义的文件系统
            printk("  %s has filesystem\n", part->name);

          } else {  // 不是支持的，进行文件系统初始化
            printk("  formatting %s's partition %s......\n", hd->name,
                   part->name);
            partition_format(part);
          }
        }
        part_index++;
        part++;
      }
      dev_no++;
    }
    channel_no++;
  }
  sys_free(sb_buf);
  char default_part[8] = "sdb1";
  list_traversal(&partition_list, mount_partition, (int)default_part);
  open_root_dir(cur_part);
  uint32_t fd_idx = 0;
  while (fd_idx < MAX_FILES_OPEN) {
    file_table[fd_idx++].fd_inode = NULL;
  }
}

// 分割文件名并放入name_store，返回下一次要进行分割的文件路径，有点像strtok
char* path_parse(char* pathname, char* name_store) {
  while ((*pathname) == '/') {
    pathname++;
  }
  while (*pathname != '/' && *pathname != '\0') {
    *name_store++ = *pathname++;
  }
  if (pathname[0] == 0) {
    return NULL;
  }
  return pathname;
}

// 获取文件路径的深度
int32_t path_depth_cnt(char* pathname) {
  ASSERT(pathname != NULL);
  char* p = pathname;
  char name[MAX_FILE_NAME_LEN];
  uint32_t depth = 0;

  p = path_parse(p, name);
  while (name[0]) {
    depth++;
    memset(name, 0, MAX_FILE_NAME_LEN);
    if (p) {
      p = path_parse(p, name);
    }
  }
  return depth;
}

// 搜索目标文件，pathname一定是“/”根目录开始的文件路径，若是中途有路径不存在，则会将不存在的路径存入searhed_record
static int search_file(const char* pathname,
                       struct path_search_record* searched_record) {
  // 是根目录，直接返回根目录的inode号即0
  if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") ||
      !strcmp(pathname, "/..")) {
    searched_record->parent_dir = &root_dir;
    searched_record->searched_path[0] = 0;
    searched_record->file_type = FT_DIRECTORY;
    return 0;
  }

  uint32_t path_len = strlen(pathname);
  ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
  char* sub_path = (char*)pathname;
  struct dir* parent_dir = &root_dir;
  struct dir_entry dir_e;

  char name[MAX_FILE_NAME_LEN] = {0};

  searched_record->parent_dir = parent_dir;
  searched_record->file_type = FT_UNKNOWN;
  uint32_t parent_inode_no = 0;
  sub_path = path_parse(sub_path, name);
  while (name[0]) {
    ASSERT(strlen(searched_record->searched_path) < 512);

    strcat(searched_record->searched_path, "/");
    strcat(searched_record->searched_path, name);

    if (search_dir_entry(cur_part, parent_dir, name, &dir_e)) {
      memset(name, 0, MAX_FILE_NAME_LEN);
      if (sub_path) {
        sub_path = path_parse(sub_path, name);
      }
      if (dir_e.f_type == FT_DIRECTORY) {
        parent_inode_no = parent_dir->inode->i_no;
        dir_close(parent_dir);
        parent_dir = dir_open(cur_part, dir_e.i_no);
        searched_record->parent_dir = parent_dir;
        continue;
      } else if (dir_e.f_type == FT_REGULAR) {
        searched_record->file_type = FT_REGULAR;
        return dir_e.i_no;
      }
    } else {
      return -1;
    }
  }
  // 走到这，说明找到的是一个目录文件，需要进行特殊处理
  dir_close(searched_record->parent_dir);
  searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
  searched_record->file_type = FT_DIRECTORY;
  return dir_e.i_no;
}

// 以对应方式打开文件
int32_t sys_open(const char* pathname, uint8_t flags) {
  if (pathname[strlen(pathname) - 1] == '/') {
    printk("can't open a directory %s\n", pathname);
    return -1;
  }
  ASSERT(flags <= 7);
  int32_t fd = -1;

  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));

  uint32_t pathname_depth = path_depth_cnt((char*)pathname);

  int inode_no = search_file(pathname, &searched_record);
  bool found = inode_no != -1 ? true : false;

  if (searched_record.file_type == FT_DIRECTORY) {
    printk("can't open a direcotry with open(), use opendir() to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

  // 说明pathname中有不存在的目录
  if (path_searched_depth != pathname_depth) {
    printk("can't access %s: Not a directory, subpath %s is't exist\n",
           pathname, searched_record.searched_path);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  if (!found && !(flags & O_CREAT)) {  // 文件不存在且不允许创建
    printk("in path %s,file %s isn't exist\n", searched_record.searched_path,
           (strrchr(searched_record.searched_path, '/') + 1));
    dir_close(searched_record.parent_dir);
    return -1;
  } else if (found && flags & O_CREAT) {  // 文件存在但仍要创建
    printk("%s has already exist!\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  switch (flags & O_CREAT) {
    case O_CREAT:
      printk("creating file\n");
      fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1),
                       flags);
      dir_close(searched_record.parent_dir);
      break;
    default:
      fd = file_open(inode_no, flags);
  }
  return fd;
}

// 将对应的fd转换为file_table的下标
uint32_t fd_local2global(uint32_t local_fd) {
  struct task_struct* cur = running_thread();
  int32_t global_fd = cur->fd_table[local_fd];
  ASSERT(global_fd >= 0 && global_fd < MAX_FILES_OPEN);
  return (uint32_t)global_fd;
}

// 关闭fd对应的文件
int32_t sys_close(int32_t fd) {
  int32_t ret = -1;  // 返回值默认为-1,即失败
  if (fd > 2) {
    uint32_t global_fd = fd_local2global(fd);
    if (is_pipe(fd)) {
      /* 如果此管道上的描述符都被关闭,释放管道的环形缓冲区 */
      if (--file_table[global_fd].fd_pos == 0) {
        mfree_page(PF_KERNEL, file_table[global_fd].fd_inode, 1);
        file_table[global_fd].fd_inode = NULL;
      }
      ret = 0;
    } else {
      ret = file_close(&file_table[global_fd]);
    }
    running_thread()->fd_table[fd] = -1;  // 使该文件描述符位可用
  }
  return ret;
}

// 文件写入
int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
  if (fd < 0) {
    printk("sys_wirte: fd error\n");
    return -1;
  }
  if (fd == stdout_no || fd == stderr_no) {
    char tmp_buf[1024] = {0};
    memcpy(tmp_buf, buf, count);
    console_put_str(tmp_buf);
    return count;
  } else if (is_pipe(fd)) {
    return pipe_write(fd, buf, count);
  } else {
    uint32_t _fd = fd_local2global(fd);
    struct file* wr_file = &file_table[_fd];
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWD) {
      uint32_t bytes_written = file_write(wr_file, buf, count);
      return bytes_written;
    } else {
      console_put_str(
          "sys_write: not allowed to write file without flag O_RDWR or "
          "O_WRONLY\n");
      return -1;
    }
  }
}

// 读取文件
int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
  ASSERT(buf != NULL);
  int32_t ret = -1;
  uint32_t global_fd = 0;
  if (fd < 0 || fd == stdout_no || fd == stderr_no) {
    printk("sys_read: fd error\n");
  } else if (fd == stdin_no) {
    /* 标准输入有可能被重定向为管道缓冲区, 因此要判断 */
    if (is_pipe(fd)) {
      ret = pipe_read(fd, buf, count);
    } else {
      char* buffer = buf;
      uint32_t bytes_read = 0;
      while (bytes_read < count) {
        *buffer = ioq_getchar(&kbd_buf);
        bytes_read++;
        buffer++;
      }
      ret = (bytes_read == 0 ? -1 : (int32_t)bytes_read);
    }
  } else if (is_pipe(fd)) { /* 若是管道就调用管道的方法 */
    ret = pipe_read(fd, buf, count);
  } else {
    global_fd = fd_local2global(fd);
    ret = file_read(&file_table[global_fd], buf, count);
  }
  return ret;
}

// 改变文件读写指针的位置
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
  if (fd < 0) {
    printk("sys_lseek: fd error\n");
    return -1;
  }
  ASSERT(whence > 0 && whence < 4);
  uint32_t _fd = fd_local2global(fd);
  struct file* file = &file_table[_fd];
  int32_t new_pos = 0;
  int32_t file_size = (int32_t)file->fd_inode->i_size;
  switch (whence) {
    case SEEK_SET:
      new_pos = offset;
      break;
    case SEEK_CUR:
      new_pos = (int32_t)file->fd_pos + offset;
      break;
    case SEEK_END:
      new_pos = file_size + offset;
      break;
  }
  if (new_pos < 0 || new_pos > (file_size - 1)) {
    return -1;
  }
  file->fd_pos = new_pos;
  return file->fd_pos;
}

// 删除文件
int32_t sys_unlink(const char* pathname) {
  ASSERT(strlen(pathname) < MAX_PATH_LEN);

  struct path_search_record searched_record;
  int inode_no = search_file(pathname, &searched_record);
  ASSERT(inode_no != 0);
  // 未发现文件
  if (inode_no == -1) {
    printk("file %s not found!\n", pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }
  // 文件是一个目录文件
  if (searched_record.file_type == FT_DIRECTORY) {
    printk("can't delete a directory with unlink(),use rmdir() to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  uint32_t file_idx = 0;
  while (file_idx < MAX_FILES_OPEN) {
    if (file_table[file_idx].fd_inode != NULL &&
        (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no) {
      break;
    }
    file_idx++;
  }
  // 文件正在被打开
  if (file_idx < MAX_FILES_OPEN) {
    dir_close(searched_record.parent_dir);
    printk("file %s is in use, not allow to delete!\n", pathname);
    return -1;
  }
  ASSERT(file_idx == MAX_FILES_OPEN);
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    dir_close(searched_record.parent_dir);
    printk("sys_unlink: malloc for io_buf failed!\n");
    return -1;
  }

  struct dir* parent_dir = searched_record.parent_dir;
  delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_release(cur_part, inode_no);
  sys_free(io_buf);
  dir_close(searched_record.parent_dir);
  return 0;
}

// 创建目录
int32_t sys_mkdir(const char* pathname) {
  uint8_t rollback_step = 0;
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    printk("sys_mkdir: sys_malloc for io_buf failed\n");
    return -1;
  }

  // 查找要创建的目录是否存在
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = -1;
  inode_no = search_file(pathname, &searched_record);
  if (inode_no != -1) {
    printk("sys_mkdir: file or directory %s exist!\n", pathname);
    rollback_step = 1;
    goto rollback;
  } else {
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);
    uint32_t path_search_depth = path_depth_cnt(searched_record.searched_path);
    if (pathname_depth != path_search_depth) {  // 中间有目录不存在
      printk("sys_mkdir: can't access %s, subpath %s is't exist\n", pathname,
             searched_record.searched_path);
      rollback_step = 1;
      goto rollback;
    }
  }

  struct dir* parent_dir = searched_record.parent_dir;
  char* dirname = strrchr(searched_record.searched_path, '/') + 1;

  // 获取新inode号
  inode_no = inode_bitmap_alloc(cur_part);
  if (inode_no == -1) {
    printk("sys_mkdir: alloc inode failed!\n");
    rollback_step = 1;
    goto rollback;
  }

  struct inode new_dir_inode;
  inode_init(inode_no, &new_dir_inode);

  // 创建新的块存放 .. 和 .
  uint32_t block_bitmap_idx;
  int32_t block_lba = -1;
  block_lba = block_bitmap_alloc(cur_part);
  if (block_lba == -1) {
    printk("sys_mkdir: alloc block failed!\n");
    rollback_step = 2;
    goto rollback;
  }
  block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
  ASSERT(block_bitmap_idx > 0);
  bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  new_dir_inode.i_sectors[0] = block_lba;
  memset(io_buf, 0, SECTOR_SIZE * 2);
  struct dir_entry* p_de = (struct dir_entry*)io_buf;
  memcpy(p_de->filename, ".", 1);
  p_de->i_no = inode_no;
  p_de->f_type = FT_DIRECTORY;

  p_de++;
  memcpy(p_de->filename, "..", 2);
  p_de->i_no = parent_dir->inode->i_no;
  p_de->f_type = FT_DIRECTORY;
  ide_write(cur_part->my_disk, block_lba, io_buf, 1);

  new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

  // 将目录项写入父目录
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));
  create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
  memset(io_buf, 0, SECTOR_SIZE * 2);
  if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
    printk("sys_mkdir: sync_dir_entry to disk failed!\n");
    rollback_step = 2;
    goto rollback;
  }

  // 更新父目录 inode
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, parent_dir->inode, io_buf);

  // 更新新目录 inode
  memset(io_buf, 0, SECTOR_SIZE * 2);
  inode_sync(cur_part, &new_dir_inode, io_buf);

  // 更新inode_bitmap
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);

  sys_free(io_buf);

  dir_close(parent_dir);
  return 0;

rollback:
  switch (rollback_step) {
    case 2:
      bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
    case 1:
      dir_close(searched_record.parent_dir);
      break;
  }
  sys_free(io_buf);
  return -1;
}

// 打开目录
struct dir* sys_opendir(const char* name) {
  ASSERT(strlen(name) < MAX_PATH_LEN);
  if (name[0] == '/' && (name[1] == 0 || name[0] == '.')) {
    return &root_dir;
  }
  struct path_search_record searched_record;
  int inode_no = search_file(name, &searched_record);
  struct dir* ret = NULL;
  if (inode_no == -1) {
    printk("In %s, sub_path %s not exist!\n", name,
           searched_record.searched_path);
  } else {
    if (searched_record.file_type == FT_REGULAR) {
      printk("%s is regular file!\n", name);
    } else if (searched_record.file_type == FT_DIRECTORY) {
      ret = dir_open(cur_part, inode_no);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

// 关闭目录
int32_t sys_closedir(struct dir* dir) {
  int32_t ret = -1;
  if (dir != NULL) {
    dir_close(dir);
    ret = 0;
  }
  return ret;
}

// 读取目录
struct dir_entry* sys_readdir(struct dir* dir) {
  ASSERT(dir != NULL);
  return dir_read(dir);
}

// 重置目录读指针
void sys_rewinddir(struct dir* dir) {
  ASSERT(dir != NULL);
  dir->dir_pos = 0;
}

// 删除空目录
int32_t sys_rmdir(const char* pathname) {
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(pathname, &searched_record);
  ASSERT(inode_no != 0);
  int ret = -1;
  if (inode_no == -1) {
    printk("In %s, sub path %s not exist\n", pathname,
           searched_record.searched_path);
  } else {
    if (searched_record.file_type == FT_REGULAR) {
      printk("%s is regular file!\n", pathname);
    } else {
      struct dir* dir = dir_open(cur_part, inode_no);
      if (!dir_is_empty(dir)) {
        printk(
            "dir %s is not empty, it is not allowed to delete a nonempty "
            "directory\n",
            pathname);
      } else {
        if (!dir_remove(searched_record.parent_dir, dir)) {
          ret = 0;
        }
      }
      dir_close(dir);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

// 获取对应文件的父目录的inode号
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void* io_buf) {
  struct inode* child_inode = inode_open(cur_part, child_inode_nr);
  uint32_t block_lba = child_inode->i_sectors[0];
  ASSERT(block_lba >= cur_part->sb->data_start_lba);
  inode_close(child_inode);
  ide_read(cur_part->my_disk, block_lba, io_buf, 1);
  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);
  return dir_e[1].i_no;
}

// 获取c_inode_nr对应的文件名并拼接到path
static int get_child_dir_name(uint32_t p_inode_nr,
                              uint32_t c_inode_nr,
                              char* path,
                              void* io_buf) {
  struct inode* parent_dir_inode = inode_open(cur_part, p_inode_nr);
  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0}, block_cnt = 12;
  while (block_idx < 12) {
    all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if (parent_dir_inode->i_sectors[12] != 0) {
    ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[12],
             all_blocks + 12, 1);
    block_cnt = 140;
  }
  inode_close(parent_dir_inode);

  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = (512 / dir_entry_size);
  block_idx = 0;
  while (block_idx < block_cnt) {
    if (all_blocks[block_idx] != 0) {
      ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      uint8_t dir_e_idx = 0;
      while (dir_e_idx < dir_entrys_per_sec) {
        if ((dir_e + dir_e_idx)->i_no == c_inode_nr) {
          strcat(path, "/");
          strcat(path, (dir_e + dir_e_idx)->filename);
          return 0;
        }
        dir_e_idx++;
      }
    }
    block_idx++;
  }
  return -1;
}

// 获取当前进程工作目录
char* sys_getcwd(char* buf, uint32_t size) {
  ASSERT(buf != NULL);
  void* io_buf = sys_malloc(SECTOR_SIZE);
  if (io_buf == NULL) {
    printk("sys_getcwd: malloc for io_buf fail!\n");
    return NULL;
  }

  struct task_struct* cur = running_thread();
  int32_t parent_inode_nr = 0;
  int32_t child_inode_nr = cur->cwd_inode_nr;
  ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);

  if (child_inode_nr == 0) {
    buf[0] = '/';
    buf[1] = 0;
    sys_free(io_buf);
    return buf;
  }

  memset(buf, 0, size);
  char full_path_reverse[MAX_PATH_LEN] = {0};
  while ((child_inode_nr)) {
    parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
    if (get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse,
                           io_buf) == -1) {
      sys_free(io_buf);
      return NULL;
    }
    child_inode_nr = parent_inode_nr;
  }
  ASSERT(strlen(full_path_reverse) <= size);

  char* last_slash;
  while ((last_slash = strrchr(full_path_reverse, '/'))) {
    uint16_t len = strlen(buf);
    strcpy(buf + len, last_slash);
    *last_slash = 0;
  }
  sys_free(io_buf);
  return buf;
}

// 改变当前进程工作目录
int32_t sys_chdir(const char* path) {
  int32_t ret = -1;
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(struct path_search_record));
  int inode_no = search_file(path, &searched_record);
  if (inode_no != -1) {
    if (searched_record.file_type == FT_DIRECTORY) {
      running_thread()->cwd_inode_nr = inode_no;
      ret = 0;
    } else {
      printk("sys_chdir: %s is regular file or other!\n", path);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

// 获取对用文件的属性并写入buf
int32_t sys_stat(const char* path, struct stat* buf) {
  if (!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")) {
    buf->st_filetype = FT_DIRECTORY;
    buf->st_ino = 0;
    buf->st_size = root_dir.inode->i_size;
    return 0;
  }

  int32_t ret = -1;
  struct path_search_record searched_record;
  memset(&searched_record, 0, sizeof(searched_record));

  int inode_no = search_file(path, &searched_record);
  if (inode_no != -1) {
    struct inode* obj_inode = inode_open(cur_part, inode_no);
    buf->st_size = obj_inode->i_size;
    inode_close(obj_inode);
    buf->st_filetype = searched_record.file_type;
    buf->st_ino = inode_no;
    ret = 0;
  } else {
    printk("sys_stat: %s not found\n", path);
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

void sys_putchar(char char_asci) {
  console_put_char(char_asci);
}

/* 显示系统支持的内部命令 */
void sys_help(void) {
  printk(
      "\
 buildin commands:\n\
       ls: show directory or file information\n\
       cd: change current work directory\n\
       mkdir: create a directory\n\
       rmdir: remove a empty directory\n\
       rm: remove a regular file\n\
       pwd: show current work directory\n\
       ps: show process information\n\
       clear: clear screen\n\
 shortcut key:\n\
       ctrl+l: clear screen\n\
       ctrl+u: clear input\n\n");
};
