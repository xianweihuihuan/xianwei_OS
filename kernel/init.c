#include "init.h"
#include "timer.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
//进行必要的初始化
void init_all() {
  put_str("init all\n");
  idt_init();//有关中断额的初始化
  mem_init();//初始化内存池
  thread_init();
  timer_init();//初始化时钟中断的频率
  console_init();
  keyboard_init();
  tss_init();
  put_str("init all done\n");
}