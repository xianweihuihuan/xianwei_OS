#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTRE0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTRE0_PORT 0x40
#define COUNTRE0_NO 0
#define COUNTRE_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

uint32_t ticks;

//设置定时器频率
static void frequency_set(uint8_t counter_port,
                          uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t counter_value) {
  outb(PIT_CONTROL_PORT,
       (uint8_t)(COUNTRE0_NO << 6 | rwl << 4 | counter_mode << 1));
  outb(counter_port, (uint8_t)counter_value);
  outb(counter_port, (uint8_t)(counter_value >> 8));
}


static void intr_timer_handler(){
  struct task_struct* cur_thread = running_thread();
  ASSERT(cur_thread->stack_magic == 0x13421342);
  cur_thread->elapsed_ticks++;
  ticks++;
  if(cur_thread->ticks == 0){
    schedule();
  }else{
    cur_thread->ticks--;
  }
}

void timer_init(){
  frequency_set(COUNTRE0_PORT, COUNTRE0_NO, READ_WRITE_LATCH, COUNTRE_MODE,
                COUNTRE0_VALUE);
  register_handler(0x20, intr_timer_handler);
  put_str("  timer_init done\n");
}

