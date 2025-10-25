#include "stdio-kernel.h"
#include "stdio.h"
#include "console.h"
#include "global.h"

#define va_start(ap, v) ap = (va_list) & v
#define va_arg(ap, t) *((t*)(ap += 4))
#define va_end(ap) ap = NULL



void printk(const char* format,...){
  va_list ap;
  va_start(ap, format);
  char buf[1024] = {0};
  vsprintf(buf, format, ap);
  va_end(ap);
  console_put_str(buf);
}