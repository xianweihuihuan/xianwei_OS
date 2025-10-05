#include "debug.h"
#include "init.h"
#include "memory.h"
#include "print.h"
#include "thread.h"
#include "timer.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

void k_thread(void* arg);

int main() {
  put_str("This is XM's kernel\n");
  init_all();
  thread_start("k_thread_a", 31, k_thread, " A_");
  thread_start("k_thread_b", 8, k_thread, " B_");
  intr_enable();
  while (1) {
    // console_put_str("Main ");
  }
}

void k_thread(void* arg) {
  char* para = arg;
  while (1) {
    enum intr_status old_status = intr_disable();
    if(!ioq_empty(&kbd_buf)){
      console_put_str(arg);
      char byte = ioq_getchar(&kbd_buf);
      console_put_char(byte);
    }
    intr_set_status(old_status);
  }
}