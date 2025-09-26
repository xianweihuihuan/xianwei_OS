#include "debug.h"
#include "init.h"
#include "print.h"
#include "timer.h"

int main() {
  put_str("This is XM's kernel\n");
  init_all();
  ASSERT(1 == 2);
  asm volatile("sti");
  while (1) {
  }
}