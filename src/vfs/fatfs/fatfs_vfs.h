#ifndef UC_VFS_FATFS_VFS_H
#define UC_VFS_FATFS_VFS_H

#include "inttypes.h"
#include "ramdisk.h"
#include "vfs.h"

#include "ff.h"

#include "diskio.h"

#ifndef FATFS_YEAR_OFFSET
/** The year in FatFs timestamps is relative to this offset */
#define FATFS_YEAR_OFFSET (1980)
#endif

/** The epoch offset is used to convert between FatFs and time_t timestamps */
#define EPOCH_YEAR_OFFSET (1970)

/** size needed for volume strings like "n:/" where n is the volume id */
#define FATFS_MAX_VOL_STR_LEN (6)

/** 0:mount on first file access, 1 mount in f_mount() call */
#define FATFS_MOUNT_OPT (1)

/** FAT filesystem type that a file system should be formatted in by
 * vfs_format() */
#ifndef CONFIG_FATFS_FORMAT_TYPE
#if FF_FS_EXFAT
#define CONFIG_FATFS_FORMAT_TYPE FM_EXFAT
#else
#define CONFIG_FATFS_FORMAT_TYPE FM_FAT
#endif
#endif

#define FATFS_MAX_ABS_PATH_SIZE (FATFS_MAX_VOL_STR_LEN + VFS_NAME_MAX + 1)

typedef struct fatfs_desc {
  FATFS fat_fs;
  ramdisk_no dno;
  uint8_t vol_idx;
  char abs_path_str_buff[FATFS_MAX_ABS_PATH_SIZE];
} fatfs_desc_t;

typedef struct fatfs_file_desc {
  FIL file;
  char fname[VFS_NAME_MAX + 1];
} fatfs_file_desc_t;

extern const vfs_file_system_t fatfs_file_system;

int fatfs_vfs_init(void);

#endif /* UC_VFS_FATFS_VFS_H */
