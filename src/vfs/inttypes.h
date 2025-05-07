#ifndef UC_VFS_INTTYPES_H
#define UC_VFS_INTTYPES_H

#include "cpu.h"

typedef CPU_INT32S int32_t;

typedef CPU_INT08U uint8_t;
typedef CPU_INT16U uint16_t;
typedef CPU_INT32U uint32_t;
typedef CPU_INT64U uint64_t;
typedef CPU_ADDR uintptr_t;

typedef CPU_SIZE_T size_t;
typedef CPU_INT32S ssize_t;

typedef CPU_INT32U off_t;
typedef int mode_t;
typedef uint32_t ino_t;

typedef uint32_t time_t;

#endif // UC_VFS_INTTYPES_H