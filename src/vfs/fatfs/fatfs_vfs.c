#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "inttypes.h"
#include "logging.h"
#include "mutex.h"
#include "printf.h"
#include "ramdisk.h"
#include "vfs.h"

#include "fatfs_vfs.h"

LOG_MODULE_REGISTER(fatfs, LOG_LEVEL_INF);

static mutex_t work_mtx;

#define TEST_FATFS_MAX_VOL_STR_LEN 14 /* "-2147483648:/\0" */

static int fatfs_err_to_errno(int32_t err);
static void _fatfs_time_to_timespec(WORD fdate, WORD ftime, time_t *time);

ramdisk_t *fat_disk;

static void _build_abs_path(fatfs_desc_t *fs_desc, const char *name) {
  snprintf(fs_desc->abs_path_str_buff, FATFS_MAX_ABS_PATH_SIZE, "/%s", name);
}

static int _init(vfs_mount_t *mountp) {
  fat_disk = ramdisk_open(mountp->dno);
  if (fat_disk) {
    return 0;
  }

  return -EINVAL;
}

#ifdef MODULE_FATFS_VFS_FORMAT
static int _format(vfs_mount_t *mountp) {
  fatfs_desc_t *fs_desc = mountp->private_data;
  char volume_str[TEST_FATFS_MAX_VOL_STR_LEN];

  static BYTE work[FF_MAX_SS];
  mutex_lock(&work_mtx);

  /* make sure the volume has been initialized */
  if (_init(mountp)) {
    return -EINVAL;
  }

  const MKFS_PARM param = {
      .fmt = CONFIG_FATFS_FORMAT_TYPE,
  };

  snprintf(volume_str, sizeof(volume_str), "%u:/", fs_desc->vol_idx);

  FRESULT res = f_mkfs(volume_str, &param, work, FF_MAX_SS);

  mutex_unlock(&work_mtx);

  return fatfs_err_to_errno(res);
}
#endif

static int _mount(vfs_mount_t *mountp) {

  fatfs_desc_t *fs_desc = (fatfs_desc_t *)mountp->private_data;

  if (_init(mountp)) {
    LOG_DBG("can't find free slot in fatfs_mtd_devs\n");
    return -ENOMEM;
  }

  _build_abs_path(fs_desc, "");

  memset(&fs_desc->fat_fs, 0, sizeof(fs_desc->fat_fs));

  FRESULT res = f_mount(&fs_desc->fat_fs, fs_desc->abs_path_str_buff, 1);

  return fatfs_err_to_errno(res);
}

static int _umount(vfs_mount_t *mountp) {
  fatfs_desc_t *fs_desc = mountp->private_data;

  _build_abs_path(fs_desc, "");

  FRESULT res = f_unmount(fs_desc->abs_path_str_buff);

  if (res == FR_OK) {
    memset(&fs_desc->fat_fs, 0, sizeof(fs_desc->fat_fs));
  }

  return fatfs_err_to_errno(res);
}

static int _unlink(vfs_mount_t *mountp, const char *name) {
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)mountp->private_data;

  _build_abs_path(fs_desc, name);

  return fatfs_err_to_errno(f_unlink(fs_desc->abs_path_str_buff));
}

static int _rename(vfs_mount_t *mountp, const char *from_path,
                   const char *to_path) {
  char fatfs_abs_path_to[FATFS_MAX_ABS_PATH_SIZE];
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)mountp->private_data;

  _build_abs_path(fs_desc, from_path);

  snprintf(fatfs_abs_path_to, sizeof(fatfs_abs_path_to), "%u:/%s",
           fs_desc->vol_idx, to_path);

  return fatfs_err_to_errno(
      f_rename(fs_desc->abs_path_str_buff, fatfs_abs_path_to));
}

static fatfs_file_desc_t *_get_fatfs_file_desc(vfs_file_t *f) {
  /* the private buffer is part of a union that also contains a
   * void pointer, hence, it is naturally aligned */
  return (fatfs_file_desc_t *)(uintptr_t)f->private_data.buffer;
}

static int _open(vfs_file_t *filp, const char *name, int flags, mode_t mode) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)filp->mp->private_data;
  _build_abs_path(fs_desc, name);

  (void)mode; /* fatfs can't use mode param with f_open*/
  LOG_DBG("fatfs_vfs.c: _open: private_data = %p, name = %s; flags = 0x%x\n",
          filp->mp->private_data, name, flags);

  strncpy(fd->fname, fs_desc->abs_path_str_buff, VFS_NAME_MAX);

  uint8_t fatfs_flags = 0;

  if ((flags & O_ACCMODE) == O_RDONLY) {
    fatfs_flags |= FA_READ;
  }
  if ((flags & O_ACCMODE) == O_WRONLY) {
    fatfs_flags |= FA_WRITE;
  }
  if ((flags & O_ACCMODE) == O_RDWR) {
    fatfs_flags |= (FA_READ | FA_WRITE);
  }
  if ((flags & O_APPEND) == O_APPEND) {
    fatfs_flags |= FA_OPEN_APPEND;
  }
  if ((flags & O_TRUNC) == O_TRUNC) {
    fatfs_flags |= FA_CREATE_ALWAYS;
  }
  if ((flags & O_CREAT) == O_CREAT) {
    if ((flags & O_EXCL) == O_EXCL) {
      fatfs_flags |= FA_CREATE_NEW;
    } else {
      fatfs_flags |= FA_OPEN_ALWAYS;
    }
  } else {
    fatfs_flags |= FA_OPEN_EXISTING;
  }

  FRESULT open_resu =
      f_open(&fd->file, fs_desc->abs_path_str_buff, fatfs_flags);

  return fatfs_err_to_errno(open_resu);
}

static int _close(vfs_file_t *filp) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);

  FRESULT res = f_close(&fd->file);

  return fatfs_err_to_errno(res);
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);

  UINT bw;

  FRESULT res = f_write(&fd->file, src, nbytes, &bw);

  if (res != FR_OK) {
    return fatfs_err_to_errno(res);
  }

  return (ssize_t)bw;
}

static int _fsync(vfs_file_t *filp) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);

  FRESULT res = f_sync(&fd->file);

  if (res != FR_OK) {
    return fatfs_err_to_errno(res);
  }

  return 0;
}

static ssize_t _read(vfs_file_t *filp, void *dest, size_t nbytes) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);

  UINT br;

  FRESULT res = f_read(&fd->file, dest, nbytes, &br);

  if (res != FR_OK) {
    return fatfs_err_to_errno(res);
  }

  return (ssize_t)br;
}

static off_t _lseek(vfs_file_t *filp, off_t off, int whence) {
  fatfs_file_desc_t *fd = _get_fatfs_file_desc(filp);
  FRESULT res;
  off_t new_pos = 0;

  if (whence == SEEK_SET) {
    new_pos = off;
  } else if (whence == SEEK_CUR) {
    new_pos = f_tell(&fd->file) + off;
  } else if (whence == SEEK_END) {
    new_pos = f_size(&fd->file) + off;
  } else {
    return fatfs_err_to_errno(FR_INVALID_PARAMETER);
  }

  res = f_lseek(&fd->file, new_pos);

  if (res == FR_OK) {
    return new_pos;
  }

  return fatfs_err_to_errno(res);
}

static int _fstat(vfs_file_t *filp, struct stat *buf) {
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)filp->mp->private_data;
  FILINFO fi;
  FRESULT res;

  res = f_stat(fs_desc->abs_path_str_buff, &fi);

  if (res != FR_OK) {
    return fatfs_err_to_errno(res);
  }

  buf->st_size = fi.fsize;

  /* set last modification timestamp */
#ifdef SYS_STAT_H
  _fatfs_time_to_timespec(fi.fdate, fi.ftime, &(buf->st_mtim.tv_sec));
#else
  _fatfs_time_to_timespec(fi.fdate, fi.ftime, &(buf->st_mtime));
#endif

  if (fi.fattrib & AM_DIR) {
    buf->st_mode = S_IFDIR; /**< it's a directory */
  } else {
    buf->st_mode = S_IFREG; /**< it's a regular file */
  }

  /** always grant read access */
  buf->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH);

  if (fi.fattrib & AM_RDO) {
    /** grant write access if file isn't RO*/
    buf->st_mode ^= (S_IWUSR | S_IWGRP | S_IWOTH);
  }

  return fatfs_err_to_errno(res);
}

static inline DIR *_get_DIR(vfs_DIR *d) {
  /* the private buffer is part of a union that also contains a
   * void pointer, hence, it is naturally aligned */
  return (DIR *)(uintptr_t)d->private_data.buffer;
}

static int _opendir(vfs_DIR *dirp, const char *dirname) {
  DIR *dir = _get_DIR(dirp);
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)dirp->mp->private_data;

  _build_abs_path(fs_desc, dirname);

  return fatfs_err_to_errno(f_opendir(dir, fs_desc->abs_path_str_buff));
}

static int _readdir(vfs_DIR *dirp, vfs_dirent_t *entry) {
  DIR *dir = _get_DIR(dirp);
  FILINFO fi;

  FRESULT res = f_readdir(dir, &fi);

  if (res == FR_OK) {
    if (fi.fname[0] == 0) {
      return 0; /**< end of dir reached */
    } else {
      entry->d_ino = 0; // TODO: set this properly
      strncpy(entry->d_name, fi.fname, VFS_NAME_MAX);
      return 1;
    }
  }

  return fatfs_err_to_errno(res);
}

static int _closedir(vfs_DIR *dirp) {
  DIR *dir = _get_DIR(dirp);

  return fatfs_err_to_errno(f_closedir(dir));
}

static int _mkdir(vfs_mount_t *mountp, const char *name, mode_t mode) {
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)mountp->private_data;
  (void)mode;

  _build_abs_path(fs_desc, name);

  return fatfs_err_to_errno(f_mkdir(fs_desc->abs_path_str_buff));
}

static int _rmdir(vfs_mount_t *mountp, const char *name) {
  fatfs_desc_t *fs_desc = (fatfs_desc_t *)mountp->private_data;

  _build_abs_path(fs_desc, name);

  return fatfs_err_to_errno(f_unlink(fs_desc->abs_path_str_buff));
}

static void _fatfs_time_to_timespec(WORD fdate, WORD ftime, time_t *time) {
  const uint16_t days_for_month[12] = {0,   31,  59,  90,  120, 151,
                                       181, 212, 243, 273, 304, 334};

  int year = ((fdate >> 9) & 0x7F) + FATFS_YEAR_OFFSET - EPOCH_YEAR_OFFSET;
  int month = ((fdate >> 5) & 0x0F) - 1; /**< 0..11 */
  int day = (fdate & 0x1F) - 1;          /**< 0..31 */

  int hour = (ftime >> 11) & 0x1F; /**< 0..23 */
  int min = (ftime >> 5) & 0x3F;   /**< 0..59 */
  int sec = (ftime & 0x1F) * 2;    /**< 0..58 */

  /* leap years since 1970 */
  int leap_years = ((year - 1 - EPOCH_YEAR_OFFSET - 2) / 4) -
                   ((year - 1 - 1900) / 100) + ((year - 1 - 1600) / 400);

  long days_since_epoch = (year - EPOCH_YEAR_OFFSET) * 365 +
                          days_for_month[month] + leap_years + day;

  if ((month > 2) &&
      ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))) {
    days_since_epoch++; /* add one day if this is a leap year */
  }

  *time = ((((days_since_epoch * 24) + hour) * 60) + min) * 60 + sec;
}

static int fatfs_err_to_errno(int32_t err) {
  switch (err) {
  case FR_OK:
    return 0;
  case FR_DISK_ERR:
    return -EIO;
  case FR_INT_ERR:
    return -EIO;
  case FR_NOT_READY:
    return -ENODEV;
  case FR_NO_FILE:
    return -ENOENT;
  case FR_NO_PATH:
    return -ENOENT;
  case FR_INVALID_NAME:
    return -ENOENT;
  case FR_DENIED:
    return -EACCES;
  case FR_EXIST:
    return -EEXIST;
  case FR_INVALID_OBJECT:
#ifdef EBADFD
    return -EBADFD;
#else
    return -EINVAL;
#endif
  case FR_WRITE_PROTECTED:
    return -EACCES;
  case FR_INVALID_DRIVE:
    return -ENXIO;
  case FR_NOT_ENABLED:
    return -ENODEV;
  case FR_NO_FILESYSTEM:
    return -ENODEV;
  case FR_MKFS_ABORTED:
    return -EINVAL;
  case FR_TIMEOUT:
    return -EBUSY;
  case FR_LOCKED:
    return -EACCES;
  case FR_NOT_ENOUGH_CORE:
    return -ENOMEM;
  case FR_TOO_MANY_OPEN_FILES:
    return -ENFILE;
  case FR_INVALID_PARAMETER:
    return -EINVAL;
  }

  return (int)err;
}

int fatfs_vfs_init() {
  int ret = 0;

  if ((ret = mutex_init(&work_mtx))) {
    return ret;
  }
  return ret;
}

static const vfs_file_system_ops_t fatfs_fs_ops = {
#ifdef MODULE_FATFS_VFS_FORMAT
    .format = _format,
#endif
    .mount = _mount,
    .umount = _umount,
    .rename = _rename,
    .unlink = _unlink,
    .mkdir = _mkdir,
    .rmdir = _rmdir,
    .stat = vfs_sysop_stat_from_fstat,
};

static const vfs_file_ops_t fatfs_file_ops = {
    .open = _open,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .fstat = _fstat,
    .fsync = _fsync,
};

static const vfs_dir_ops_t fatfs_dir_ops = {
    .opendir = _opendir,
    .readdir = _readdir,
    .closedir = _closedir,
};

const vfs_file_system_t fatfs_file_system = {
    .fs_op = &fatfs_fs_ops,
    .f_op = &fatfs_file_ops,
    .d_op = &fatfs_dir_ops,
};