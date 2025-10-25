#include "string.h"
#include "debug.h"
#include "global.h"

//将内存区域设置为value
void memset(void* dst_, uint8_t value, uint32_t size) {
  ASSERT(dst_ != NULL);
  uint8_t* dst = (uint8_t*)dst_;
  while (size-- > 0) {
    *dst = value;
  }
}

//将src处的size个字节复制到dst
void memcpy(void* dst_, const void* src_, uint32_t size) {
  ASSERT(dst_ != NULL && src_ != NULL);
  uint8_t* dst = (uint8_t*)dst_;
  uint8_t* src = (uint8_t*)src_;
  while (size-- > 0) {
    *dst++ = *src++;
  }
}

//比较a与b出的值
int memcmp(const void* a_, const void* b_, uint32_t size) {
  const char* a = a_;
  const char* b = b_;
  ASSERT(a != NULL && b != NULL);
  while (size-- > 0) {
    if (*a != *b) {
      return *a > *b ? 1 : -1;
    }
    a++;
    b++;
  }
  return 0;
}


char* strcpy(char* dst_, const char* src_) {
  ASSERT(src_ != NULL && dst_ != NULL);
  char* ret = dst_;
  while (*dst_++ = *src_++) {
  }
  return ret;
}

uint32_t strlen(const char* str) {
  ASSERT(str != NULL);
  const char* p = str;
  while (*p++)
    ;
  return p - str - 1;
}

int8_t strcmp(const char* a, const char* b) {
  ASSERT(a != NULL && b != NULL);
  while (*a != 0 && *a == *b) {
    a++;
    b++;
  }
  return *a < *b ? -1 : *a > *b;
}

char* strchr(const char* string, const uint8_t ch) {
  ASSERT(string != NULL);
  while (*string != 0) {
    if (*string == ch) {
      return (char*)string;
    }
    string++;
  }
  return NULL;
}

char* strrchr(const char* string, const uint8_t ch) {
  ASSERT(string != NULL);
  const char* last_char = NULL;
  while (*string != 0) {
    if (*string == ch) {
      last_char = string;
    }
    string++;
  }
  return (char*)last_char;
}

char* strcat(char* dst_, const char* src_){
  ASSERT(dst_ != NULL && src_ != NULL);
  char* str = dst_;
  while(*str++)
    ;
  --str;
  while((*str++ = *src_++))
    ;
  return dst_;
}

uint32_t strchrs(const char* filename, uint8_t ch){
  ASSERT(filename != NULL);
  uint32_t ret = 0;
  const char* p = filename;
  while(*p != 0){
    if(*p == ch){
      ret++;
    }
    p++;
  }
  return ret;
}