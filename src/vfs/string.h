#ifndef UC_VFS_STRING_H
#define UC_VFS_STRING_H

#include <lib_str.h>

#include "inttypes.h"
#include "lib_mem.h"

static inline size_t strlen(const char *s) { return Str_Len(s); }

static inline char *strcpy(char *dst, const char *src) {
  return Str_Copy(dst, src);
}

static inline char *strncpy(char *dst, const char *src, size_t n) {
  return Str_Copy_N(dst, src, n);
}

static inline int strcmp(const char *s1, const char *s2) {
  return Str_Cmp(s1, s2);
}

static inline int strncmp(const char *s1, const char *s2, size_t n) {
  return Str_Cmp_N(s1, s1, n);
}

static inline void memset(void *mem, char c, size_t n) { Mem_Set(mem, c, n); }

static inline void memcpy(void *dst, const void *src, size_t n) {
  Mem_Copy(dst, src, n);
}

#endif // UC_VFS_STRING_H