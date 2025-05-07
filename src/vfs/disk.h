#ifndef UC_VFS_DISKIO_H
#define UC_VFS_DISKIO_H

#include "inttypes.h"

#define CONFIG_RAM_SEC_SIZE 512
#define CONFIG_RAM_N_SECS 1024

typedef uint8_t vdisk_no;

typedef struct vdisk_struct vdisk_t;

vdisk_t *vdisk_open(vdisk_no no);

int vdisk_read(vdisk_t *disk, void *buf, uint32_t start_sec, off_t off,
               size_t sz);

int vdisk_write(vdisk_t *disk, const void *buf, uint32_t start_sec, off_t off,
                size_t sz);

#endif