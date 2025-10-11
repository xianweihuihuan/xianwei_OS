#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"
//向对应端口写入一个字节
static inline void outb(uint16_t port, uint8_t data) {
  asm volatile("outb %b0, %w1" ::"a"(data), "Nd"(port));
}
//向对应端口写入多个字节
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
  asm volatile("cld;rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
}
//从对应端口读一个字节
static inline uint8_t inb(uint16_t port) {
  uint8_t data;
  asm volatile("inb %w1,%b0" : "=a"(data) : "Nd"(port));
}
//从对应端口读多个字节
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
  asm volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
}

#endif