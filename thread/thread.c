#include "thread.h"
#include "debug.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "interrupt.h"
#include "print.h"
#include "process.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "sync.h"

#define PG_SIZE 4096

struct task_struct* main_thread;
struct list thread_ready_list;
struct list thread_all_list;
static struct list_elem* thread_tag;
struct lock pid_lock;
struct task_struct* idle_thread;

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init();

uint8_t pid_bitmap_bits[128] = {0};

struct pid_pool {
  struct bitmap pid_bitmap;
  uint32_t pid_start;
  struct lock pid_lock;
} pid_pool;

static pid_t allocate_pid() {
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
  lock_release(&pid_pool.pid_lock);
  return (bit_idx + pid_pool.pid_start);
}

void release_pid(pid_t pid) {
  lock_acquire(&pid_pool.pid_lock);
  int32_t bit_idx = pid - pid_pool.pid_start;
  bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
  lock_release(&pid_pool.pid_lock);
}

pid_t fork_pid(void) {
  return allocate_pid();
}

struct task_struct* running_thread() {
  uint32_t esp;
  asm("mov %%esp, %0" : "=g"(esp));
  return (struct task_struct*)(esp & 0xfffff000);
}

static void kernel_thread(thread_func* function, void* func_arg) {
  intr_enable();
  function(func_arg);
}

void thread_create(struct task_struct* pthread,
                   thread_func function,
                   void* func_arg) {
  pthread->self_kstack -= sizeof(struct intr_stack);
  pthread->self_kstack -= sizeof(struct thread_stack);
  struct thread_stack* kthread_stack =
      (struct thread_stack*)pthread->self_kstack;
  kthread_stack->eip = kernel_thread;
  kthread_stack->function = function;
  kthread_stack->func_arg = func_arg;
  kthread_stack->ebp = kthread_stack->ebx = kthread_stack->edi =
      kthread_stack->esi = 0;
}

void init_thread(struct task_struct* pthread, char* name, int prio) {
  pthread->self_kstack =
      (uint32_t*)((int)(pthread->self_kstack) - sizeof(struct thread_stack));
  memset(pthread, 0, sizeof(*pthread));
  pthread->pid = allocate_pid();
  strcpy(pthread->name, name);
  if (pthread == main_thread) {
    pthread->status = TASK_RUNNING;
  } else {
    pthread->status = TASK_READY;
  }
  pthread->priority = prio;
  pthread->ticks = prio;
  pthread->elapsed_ticks = 0;
  pthread->pgdir = NULL;
  pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
  pthread->fd_table[0] = 0;
  pthread->fd_table[1] = 1;
  pthread->fd_table[2] = 2;
  uint8_t fd_index = 3;
  while (fd_index < MAX_FILES_OPEN_PER_PROC) {
    pthread->fd_table[fd_index] = -1;
    fd_index++;
  }
  pthread->cwd_inode_nr = 0;
  pthread->parent_pid = -1;
  pthread->stack_magic = 0x13421342;
}

struct task_struct* thread_start(char* name,
                                 int prio,
                                 thread_func function,
                                 void* func_arg) {
  struct task_struct* thread = get_kernel_pages(1);
  init_thread(thread, name, prio);
  thread_create(thread, function, func_arg);
  ASSERT(!(elem_find(&thread_ready_list, &thread->general_tag)));
  list_append(&thread_ready_list, &thread->general_tag);
  ASSERT(!(elem_find(&thread_all_list, &thread->all_list_tag)));
  list_append(&thread_all_list, &thread->all_list_tag);
  return thread;
}

static void make_main_thread() {
  main_thread = running_thread();
  init_thread(main_thread, "main", 31);
  ASSERT(!(elem_find(&thread_all_list, &main_thread->all_list_tag)));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

void thread_yield() {
  struct task_struct* cur = running_thread();
  enum intr_status old_status = intr_disable();
  ASSERT(!(elem_find(&thread_ready_list, &cur->general_tag)));
  cur->status = TASK_READY;
  list_append(&thread_ready_list, &cur->general_tag);
  schedule();
  intr_set_status(old_status);
}

static void idle(void* arg UNUSED) {
  while (1) {
    thread_block(TASK_BLOCKED);
    asm volatile("sti;hlt" ::: "memory");
  }
}

static void pid_pool_init() {
  pid_pool.pid_start = 1;
  pid_pool.pid_bitmap.bits = pid_bitmap_bits;
  pid_pool.pid_bitmap.btmp_bytes_len = 128;
  bitmap_init(&pid_pool.pid_bitmap);
  lock_init(&pid_pool.pid_lock);
}

void schedule() {
  ASSERT(intr_get_status() == INTR_OFF)
  struct task_struct* cur = running_thread();
  if (cur->status == TASK_RUNNING) {
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->ticks = cur->priority;
    cur->status = TASK_READY;
  } else {
  }
  if (list_empty(&thread_ready_list)) {
    thread_unblock(idle_thread);
  }
  thread_tag = NULL;
  thread_tag = list_pop(&thread_ready_list);
  struct task_struct* next =
      elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;
  process_activate(next);
  switch_to(cur, next);
}

void thread_init() {
  put_str("  thread_init start\n");
  list_init(&thread_all_list);
  list_init(&thread_ready_list);
  pid_pool_init();
  process_execute(init, "init");
  make_main_thread();
  idle_thread = thread_start("idle", 10, idle, NULL);
  put_str("  thread_init_done\n");
}

void thread_block(enum task_status stat) {
  enum intr_status old_status = intr_disable();
  ASSERT(stat == TASK_BLOCKED || stat == TASK_HANGING || TASK_WAITING);
  struct task_struct* cur_thread = running_thread();
  cur_thread->status = stat;
  schedule();
  intr_set_status(old_status);
}

void thread_unblock(struct task_struct* pthread) {
  enum intr_status old_status = intr_disable();
  ASSERT((pthread->status == TASK_BLOCKED) ||
         (pthread->status == TASK_HANGING) ||
         (pthread->status == TASK_WAITING));
  if (pthread->status != TASK_READY) {
    ASSERT(!(elem_find(&thread_ready_list, &pthread->general_tag)));
    if (elem_find(&thread_ready_list, &pthread->general_tag)) {
      PANIC("thread_unblock:block thread in ready_list\n");
    }
    list_push(&thread_ready_list, &pthread->general_tag);
    pthread->status = TASK_READY;
  }
  intr_set_status(old_status);
}

static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
  memset(buf, 0, buf_len);
  uint8_t out_pad_0idx = 0;
  switch (format) {
    case 's':
      out_pad_0idx = sprintf(buf, "%s", ptr);
      break;
    case 'd':
      out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
      break;
    case 'x':
      out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
      break;
  }
  while (out_pad_0idx < buf_len) {
    buf[out_pad_0idx] = ' ';
    out_pad_0idx++;
  }
  sys_write(stdout_no, buf, buf_len - 1);
}

static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
  struct task_struct* pthread =
      elem2entry(struct task_struct, all_list_tag, pelem);
  char out_pad[16] = {0};

  pad_print(out_pad, 16, &pthread->pid, 'd');

  if (pthread->parent_pid == -1) {
    pad_print(out_pad, 16, "NULL", 's');
  } else {
    pad_print(out_pad, 16, &pthread->parent_pid, 'd');
  }

  switch (pthread->status) {
    case 0:
      pad_print(out_pad, 16, "RUNNING", 's');
      break;
    case 1:
      pad_print(out_pad, 16, "READY", 's');
      break;
    case 2:
      pad_print(out_pad, 16, "BLOCKED", 's');
      break;
    case 3:
      pad_print(out_pad, 16, "WAITING", 's');
      break;
    case 4:
      pad_print(out_pad, 16, "HANGING", 's');
      break;
    case 5:
      pad_print(out_pad, 16, "DIED", 's');
  }
  pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

  memset(out_pad, 0, 16);
  ASSERT(strlen(pthread->name) < 17);
  memcpy(out_pad, pthread->name, strlen(pthread->name));
  strcat(out_pad, "\n");
  sys_write(stdout_no, out_pad, strlen(out_pad));
  return false;
}

void sys_ps() {
  char* ps_title =
      "PID            PPID           STAT           TICKS          COMMAND\n";
  sys_write(stdout_no, ps_title, strlen(ps_title));
  list_traversal(&thread_all_list, elem2thread_info, 0);
}

void thread_exit(struct task_struct* thread_over, bool need_schedule) {
  intr_disable();
  thread_over->status = TASK_DIED;

  if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
    list_remove(&thread_over->general_tag);
  }
  if (thread_over->pgdir) {
    mfree_page(PF_KERNEL, thread_over->pgdir, 1);
  }

  list_remove(&thread_over->all_list_tag);
  if (thread_over != main_thread) {
    mfree_page(PF_KERNEL, thread_over, 1);
  }
  release_pid(thread_over->pid);
  if (need_schedule) {
    schedule();
    PANIC("thread_exit: should not be here\n");
  }
}

static bool pid_check(struct list_elem* pelem,int32_t pid){
  struct task_struct* pthread =
      elem2entry(struct task_struct, all_list_tag, pelem);
  if(pthread->pid == pid){
    return true;
  }
  return false;
}

struct task_struct* pid2thread(int32_t pid){
  struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
  if(pelem == NULL){
    return NULL;
  }
  struct task_struct* thread =
      elem2entry(struct task_struct, all_list_tag, pelem);
  return thread;
}


