#ifndef UC_VFS_VFS_H
#define UC_VFS_VFS_H

#include <os.h>

#include <sys/stat.h>

#include "clist.h"
#include "common.h"
#include "inttypes.h"
#include "ramdisk.h"

#define MODULE_FATFS_VFS
#define MODULE_FATFS_VFS_FORMAT
#define CONFIG_FATFS_FORMAT_ALLOC_STATIC 1

#define MODULE_LITTLEFS2

#ifdef MODULE_FATFS_VFS
#include "fatfs/ffconf.h"

#if FF_FS_TINY
#define _FATFS_FILE_CACHE (0)
#else
#define _FATFS_FILE_CACHE FF_MAX_SS
#endif

#if FF_USE_FASTSEEK
#if (__SIZEOF_POINTER__ == 8)
#define _FATFS_FILE_SEEK_PTR (8)
#else
#define _FATFS_FILE_SEEK_PTR (4)
#endif
#else
#define _FATFS_FILE_SEEK_PTR (0)
#endif

#if FF_FS_EXFAT
#define _FATFS_FILE_EXFAT (48)
#define _FATFS_DIR_EXFAT (32)
#else
#define _FATFS_FILE_EXFAT (0)
#define _FATFS_DIR_EXFAT (0)
#endif

#if FF_USE_LFN
#if (__SIZEOF_POINTER__ == 8)
#define _FATFS_DIR_LFN (8)
#else
#define _FATFS_DIR_LFN (4)
#endif
#else
#define _FATFS_DIR_LFN (0)
#endif

#if (__SIZEOF_POINTER__ == 8)
#define FATFS_VFS_DIR_BUFFER_SIZE (64 + _FATFS_DIR_LFN + _FATFS_DIR_EXFAT)
#define FATFS_VFS_FILE_BUFFER_SIZE                                             \
  (64 + VFS_NAME_MAX + _FATFS_FILE_CACHE + _FATFS_FILE_SEEK_PTR +              \
   _FATFS_FILE_EXFAT)
#else
#define FATFS_VFS_DIR_BUFFER_SIZE (44 + _FATFS_DIR_LFN + _FATFS_DIR_EXFAT)
#define FATFS_VFS_FILE_BUFFER_SIZE                                             \
  (44 + VFS_NAME_MAX + _FATFS_FILE_CACHE + _FATFS_FILE_SEEK_PTR +              \
   _FATFS_FILE_EXFAT)
#endif
#else
#define FATFS_VFS_DIR_BUFFER_SIZE (1)
#define FATFS_VFS_FILE_BUFFER_SIZE (1)
#endif

#ifdef MODULE_LITTLEFS2
#if (__SIZEOF_POINTER__ == 8)
#define LITTLEFS2_VFS_DIR_BUFFER_SIZE (56)
#define LITTLEFS2_VFS_FILE_BUFFER_SIZE (104)
#else
#define LITTLEFS2_VFS_DIR_BUFFER_SIZE (52)
#define LITTLEFS2_VFS_FILE_BUFFER_SIZE (84)
#endif
#else
#define LITTLEFS2_VFS_DIR_BUFFER_SIZE (1)
#define LITTLEFS2_VFS_FILE_BUFFER_SIZE (1)
#endif

#ifndef VFS_MAX_OPEN_FILES
#define VFS_MAX_OPEN_FILES (16)
#endif

#ifndef VFS_DIR_BUFFER_SIZE

#define VFS_DIR_BUFFER_SIZE                                                    \
  MAX(FATFS_VFS_DIR_BUFFER_SIZE, LITTLEFS2_VFS_DIR_BUFFER_SIZE)
#endif

#ifndef VFS_FILE_BUFFER_SIZE
#define VFS_FILE_BUFFER_SIZE                                                   \
  MAX(FATFS_VFS_FILE_BUFFER_SIZE, LITTLEFS2_VFS_FILE_BUFFER_SIZE)
#endif

#ifndef VFS_NAME_MAX
#define VFS_NAME_MAX (31)
#endif

#define VFS_ANY_FD (-1)

typedef struct vfs_file_ops vfs_file_ops_t;

typedef struct vfs_dir_ops vfs_dir_ops_t;

typedef struct vfs_file_system_ops vfs_file_system_ops_t;

typedef struct vfs_mount_struct vfs_mount_t;

extern const vfs_file_ops_t mtd_vfs_ops;

#define VFS_FS_FLAG_WANT_ABS_PATH (1 << 0)

typedef struct {
  const vfs_file_ops_t *f_op;
  const vfs_dir_ops_t *d_op;
  const vfs_file_system_ops_t *fs_op;
  const uint32_t flags;
} vfs_file_system_t;

struct vfs_mount_struct {
  clist_node_t list_entry;
  const vfs_file_system_t *fs;
  const char *mount_point;
  size_t mount_point_len;
  uint16_t open_files;
  ramdisk_no dno;
  void *private_data;
};

typedef struct {
  const vfs_file_ops_t *f_op;
  vfs_mount_t *mp;
  int flags;
  off_t pos;
  OS_TCB *p_tcb;
  union {
    void *ptr;
    int value;
    uint8_t buffer[VFS_FILE_BUFFER_SIZE];
  } private_data;
} vfs_file_t;

typedef struct {
  const vfs_dir_ops_t *d_op;
  vfs_mount_t *mp;
  union {
    void *ptr;
    int value;
    uint8_t buffer[VFS_DIR_BUFFER_SIZE];
  } private_data;
} vfs_DIR;

typedef struct {
  ino_t d_ino;
  char d_name[VFS_NAME_MAX + 1];
} vfs_dirent_t;

struct vfs_file_ops {
  int (*open)(vfs_file_t *filp, const char *name, int flags, mode_t mode);
  int (*close)(vfs_file_t *filp);
  int (*fcntl)(vfs_file_t *filp, int cmd, int arg);
  int (*fstat)(vfs_file_t *filp, struct stat *buf);
  off_t (*lseek)(vfs_file_t *filp, off_t off, int whence);
  ssize_t (*read)(vfs_file_t *filp, void *dest, size_t nbytes);
  ssize_t (*write)(vfs_file_t *filp, const void *src, size_t nbytes);
  int (*fsync)(vfs_file_t *filp);
};

struct vfs_dir_ops {
  int (*opendir)(vfs_DIR *dirp, const char *dirname);
  int (*readdir)(vfs_DIR *dirp, vfs_dirent_t *entry);
  int (*closedir)(vfs_DIR *dirp);
};

struct vfs_file_system_ops {
  int (*format)(vfs_mount_t *mountp);
  int (*mount)(vfs_mount_t *mountp);
  int (*umount)(vfs_mount_t *mountp);
  int (*rename)(vfs_mount_t *mountp, const char *from_path,
                const char *to_path);
  int (*unlink)(vfs_mount_t *mountp, const char *name);
  int (*mkdir)(vfs_mount_t *mountp, const char *name, mode_t mode);
  int (*rmdir)(vfs_mount_t *mountp, const char *name);
  int (*stat)(vfs_mount_t *mountp, const char *restrict path,
              struct stat *restrict buf);
};

int vfs_open(const char *name, int flags, mode_t mode);
int vfs_close(int fd);
int vfs_fcntl(int fd, int cmd, int arg);
int vfs_fstat(int fd, struct stat *buf);
off_t vfs_lseek(int fd, off_t off, int whence);
ssize_t vfs_read(int fd, void *dest, size_t count);
ssize_t vfs_write(int fd, const void *src, size_t count);
int vfs_fsync(int fd);
int vfs_opendir(vfs_DIR *dirp, const char *dirname);
int vfs_readdir(vfs_DIR *dirp, vfs_dirent_t *entry);
int vfs_closedir(vfs_DIR *dirp);
int vfs_format(vfs_mount_t *mountp);
int vfs_mount(vfs_mount_t *mountp);
int vfs_rename(const char *from_path, const char *to_path);
int vfs_umount(vfs_mount_t *mountp, bool force);
int vfs_unlink(const char *name);
int vfs_mkdir(const char *name, mode_t mode);
int vfs_rmdir(const char *name);
int vfs_stat(const char *restrict path, struct stat *restrict buf);

int vfs_normalize_path(char *buf, const char *path, size_t buflen);
ssize_t vfs_readline(int fd, char *dest, size_t count);

const vfs_file_t *vfs_file_get(int fd);

int vfs_sysop_stat_from_fstat(vfs_mount_t *mountp, const char *restrict path,
                              struct stat *restrict buf);

int vfs_init(void);

#endif /* VFS_H */

/** @} */