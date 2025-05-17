#ifndef UC_VFS_SPIFFS_VFS_H
#define UC_VFS_SPIFFS_VFS_H

#include "mutex.h"
#include "ramdisk.h"

#include "spiffs.h"
#include "spiffs_config.h"
#include "vfs.h"

#define CONFIG_PAGE_SIZE (64)
#define CONFIG_PAGES_PER_SEC (CONFIG_RAM_SEC_SIZE / CONFIG_PAGE_SIZE)

/** Size of the buffer needed for directory */
#define SPIFFS_DIR_SIZE (12)

#if (VFS_DIR_BUFFER_SIZE < SPIFFS_DIR_SIZE)
#error "VFS_DIR_BUFFER_SIZE too small"
#endif

#ifndef SPIFFS_FS_CACHE_SIZE
#if SPIFFS_CACHE || defined(DOXYGEN)
#define SPIFFS_FS_CACHE_SIZE (512)
#else
#define SPIFFS_FS_CACHE_SIZE (0)
#endif /* SPIFFS_CACHE */
#endif /* SPIFFS_FS_CACHE_SIZE */

#ifndef SPIFFS_FS_WORK_SIZE
#define SPIFFS_FS_WORK_SIZE (512)
#endif

#ifndef SPIFFS_FS_FD_SPACE_SIZE
#define SPIFFS_FS_FD_SPACE_SIZE (4 * 32)
#endif

typedef struct spiffs_desc {
  spiffs fs;
  uint8_t work[SPIFFS_FS_WORK_SIZE];
  uint8_t fd_space[SPIFFS_FS_FD_SPACE_SIZE];
#if (SPIFFS_CACHE == 1) || defined(DOXYGEN)
  uint8_t cache[SPIFFS_FS_CACHE_SIZE];
#endif
  spiffs_config config;
  mutex_t lock;
#if (SPIFFS_HAL_CALLBACK_EXTRA == 1) || defined(DOXYGEN)
  ramdisk_t *disk;
#endif
#if (SPIFFS_SINGLETON == 0) || defined(DOXYGEN)
  uint32_t base_addr;
  uint32_t block_count;
#endif
} spiffs_desc_t;

extern const vfs_file_system_t spiffs_file_system;

void spiffs_lock(struct spiffs_t *fs);

void spiffs_unlock(struct spiffs_t *fs);

int spiffs_vfs_init(void);

int spiffs_vfs_desc_init(spiffs_desc_t *desc);

#endif /* UC_VFS_SPIFFS_VFS_H */