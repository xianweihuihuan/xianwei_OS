#ifndef _WAIT_EXIT_H
#define _WAIT_EXIT_H
#include "stdint.h"
#include "thread.h"

pid_t sys_wait(int32_t* status);
void sys_exit(int32_t status);

#endif