#ifndef UC_VFS_DISKIO_H
#define UC_VFS_DISKIO_H

#include <unistd.h>

#include "inttypes.h"

#define CONFIG_RAM_SEC_SIZE 512
#define CONFIG_RAM_N_SECS 1024

typedef uint8_t ramdisk_no;

typedef struct ramdisk_struct ramdisk_t;

int ramdisk_init(void);

ramdisk_t *ramdisk_open(ramdisk_no no);

int ramdisk_read(ramdisk_t *disk, void *buf, uint32_t start_sec, off_t off,
                 size_t sz);

int ramdisk_read_addr(ramdisk_t *disk, void *buf, size_t addr, size_t sz);

int ramdisk_write(ramdisk_t *disk, const void *buf, uint32_t start_sec,
                  off_t off, size_t sz);

int ramdisk_write_addr(ramdisk_t *disk, const void *buf, size_t addr,
                       size_t sz);

int ramdisk_erase_addr(ramdisk_t *disk, size_t addr, size_t sz);

#endif