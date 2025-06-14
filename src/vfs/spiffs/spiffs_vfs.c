#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#include "common.h"
#include "logging.h"
#include "mutex.h"
#include "ramdisk.h"

#include "spiffs_vfs.h"

LOG_MODULE_REGISTER(spiffs, LOG_LEVEL_INF);

static int spiffs_err_to_errno(s32_t err);

static int32_t _dev_read(struct spiffs_t *fs, u32_t addr, u32_t size,
                         u8_t *dst) {
  ramdisk_t *disk = (ramdisk_t *)fs->user_data;

  ramdisk_read_addr(disk, dst, addr, size);
  return 0;
}

static int32_t _dev_write(struct spiffs_t *fs, u32_t addr, u32_t size,
                          u8_t *src) {
  ramdisk_t *disk = (ramdisk_t *)fs->user_data;

  ramdisk_write_addr(disk, src, addr, size);
  return 0;
}

static int32_t _dev_erase(struct spiffs_t *fs, u32_t addr, u32_t size) {
  ramdisk_t *disk = (ramdisk_t *)fs->user_data;

  ramdisk_erase_addr(disk, addr, size);
  return 0;
}

void spiffs_lock(struct spiffs_t *fs) {
  spiffs_desc_t *fs_desc = CONTAINER_OF(fs, spiffs_desc_t, fs);

  mutex_lock(&fs_desc->lock);
}

void spiffs_unlock(struct spiffs_t *fs) {
  spiffs_desc_t *fs_desc = CONTAINER_OF(fs, spiffs_desc_t, fs);

  mutex_unlock(&fs_desc->lock);
}

static int prepare(spiffs_desc_t *fs_desc) {
  ramdisk_t *dev = fs_desc->disk;
  fs_desc->fs.user_data = dev;

  fs_desc->config.hal_read_f = _dev_read;
  fs_desc->config.hal_write_f = _dev_write;
  fs_desc->config.hal_erase_f = _dev_erase;

  // fs_desc->config.phys_size = CONFIG_RAM_SEC_SIZE * CONFIG_RAM_N_SECS;
  // fs_desc->config.phys_addr = 0;
  // fs_desc->config.phys_erase_block = CONFIG_PAGE_SIZE * CONFIG_PAGES_PER_SEC;
  // fs_desc->config.log_block_size = CONFIG_PAGE_SIZE * CONFIG_PAGES_PER_SEC;
  // fs_desc->config.log_page_size = CONFIG_PAGE_SIZE;

  fs_desc->config.phys_size = CONFIG_RAM_SEC_SIZE * CONFIG_RAM_N_SECS;
  fs_desc->config.phys_addr = 0;
  fs_desc->config.phys_erase_block = 4096;
  fs_desc->config.log_block_size = 4096;
  fs_desc->config.log_page_size = 256;

  return 0;
}

static int _format(vfs_mount_t *mountp) {
  spiffs_desc_t *fs_desc = mountp->private_data;
  LOG_DBG("spiffs: format: private_data = %p\n", mountp->private_data);

  ramdisk_t *disk = ramdisk_open(mountp->dno);
  if (!disk) {
    return -ENODEV;
  }
  fs_desc->disk = disk;

  int res = prepare(fs_desc);
  if (res) {
    return -ENODEV;
  }

  s32_t ret = SPIFFS_mount(&fs_desc->fs, &fs_desc->config, fs_desc->work,
                           fs_desc->fd_space, SPIFFS_FS_FD_SPACE_SIZE,
                           fs_desc->cache, SPIFFS_FS_CACHE_SIZE, NULL);

  if (ret == 0) {
    LOG_DBG("spiffs: format: unmount fs\n");
    SPIFFS_unmount(&fs_desc->fs);
  }
  LOG_DBG("spiffs: format: formatting fs\n");
  ret = SPIFFS_format(&fs_desc->fs);
  return spiffs_err_to_errno(ret);
}

static int _mount(vfs_mount_t *mountp) {
  spiffs_desc_t *fs_desc = mountp->private_data;
  LOG_DBG("spiffs: mount: private_data = %p\n", mountp->private_data);

  ramdisk_t *disk = ramdisk_open(mountp->dno);
  if (!disk) {
    return -ENODEV;
  }
  fs_desc->disk = disk;

  int res = prepare(fs_desc);
  if (res) {
    return -ENODEV;
  }

  s32_t ret = SPIFFS_mount(&fs_desc->fs, &fs_desc->config, fs_desc->work,
                           fs_desc->fd_space, SPIFFS_FS_FD_SPACE_SIZE,
                           fs_desc->cache, SPIFFS_FS_CACHE_SIZE, NULL);

  return spiffs_err_to_errno(ret);
}

static int _umount(vfs_mount_t *mountp) {
  spiffs_desc_t *fs_desc = mountp->private_data;

  SPIFFS_unmount(&fs_desc->fs);

  return 0;
}

static int _unlink(vfs_mount_t *mountp, const char *name) {
  spiffs_desc_t *fs_desc = mountp->private_data;

  return spiffs_err_to_errno(SPIFFS_remove(&fs_desc->fs, name));
}

static int _rename(vfs_mount_t *mountp, const char *from_path,
                   const char *to_path) {
  spiffs_desc_t *fs_desc = mountp->private_data;

  return spiffs_err_to_errno(SPIFFS_rename(&fs_desc->fs, from_path, to_path));
}

static int _open(vfs_file_t *filp, const char *name, int flags, mode_t mode) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;
  LOG_DBG("spiffs: open: private_data = %p\n", filp->mp->private_data);

  spiffs_flags s_flags = 0;
  if ((flags & O_ACCMODE) == O_RDONLY) {
    s_flags |= SPIFFS_O_RDONLY;
  }
  if ((flags & O_APPEND) == O_APPEND) {
    s_flags |= SPIFFS_O_APPEND;
  }
  if ((flags & O_TRUNC) == O_TRUNC) {
    s_flags |= SPIFFS_O_TRUNC;
  }
  if ((flags & O_CREAT) == O_CREAT) {
    s_flags |= SPIFFS_O_CREAT;
  }
  if ((flags & O_ACCMODE) == O_WRONLY) {
    s_flags |= SPIFFS_O_WRONLY;
  }
  if ((flags & O_ACCMODE) == O_RDWR) {
    s_flags |= SPIFFS_O_RDWR;
  }
  if ((flags & O_EXCL) == O_EXCL) {
    s_flags |= SPIFFS_O_EXCL;
  }

  LOG_DBG("spiffs: open: %s, flags: 0x%x, mode: %d\n", name, (int)s_flags,
          (int)mode);

  s32_t ret = SPIFFS_open(&fs_desc->fs, name, s_flags, mode);
  if (ret >= 0) {
    filp->private_data.value = ret;
    return ret;
  } else {
    return spiffs_err_to_errno(ret);
  }
}

static int _close(vfs_file_t *filp) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;

  return spiffs_err_to_errno(
      SPIFFS_close(&fs_desc->fs, filp->private_data.value));
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;

  return spiffs_err_to_errno(SPIFFS_write(
      &fs_desc->fs, filp->private_data.value, (void *)src, nbytes));
}

static ssize_t _read(vfs_file_t *filp, void *dest, size_t nbytes) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;

  return spiffs_err_to_errno(
      SPIFFS_read(&fs_desc->fs, filp->private_data.value, dest, nbytes));
}

static off_t _lseek(vfs_file_t *filp, off_t off, int whence) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;

  int s_whence = 0;
  if (whence == SEEK_SET) {
    s_whence = SPIFFS_SEEK_SET;
  } else if (whence == SEEK_CUR) {
    s_whence = SPIFFS_SEEK_CUR;
  } else if (whence == SEEK_END) {
    s_whence = SPIFFS_SEEK_END;
  }

  return spiffs_err_to_errno(
      SPIFFS_lseek(&fs_desc->fs, filp->private_data.value, off, s_whence));
}

static int _fsync(vfs_file_t *filp) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;

  int ret = SPIFFS_fflush(&fs_desc->fs, filp->private_data.value);

  return spiffs_err_to_errno(ret);
}

static int _fstat(vfs_file_t *filp, struct stat *buf) {
  spiffs_desc_t *fs_desc = filp->mp->private_data;
  spiffs_stat stat;
  s32_t ret;

  ret = SPIFFS_fstat(&fs_desc->fs, filp->private_data.value, &stat);

  if (ret < 0) {
    return ret;
  }
  /* stat.name; */
  buf->st_ino = stat.obj_id;
  /* stat.pix; */
  buf->st_size = stat.size;
  /* stat.type;*/
  buf->st_mode = S_IFREG;

  return spiffs_err_to_errno(ret);
}

static spiffs_DIR *_get_spifs_dir(vfs_DIR *dirp) {
  /* the private buffer is part of a union that also contains a
   * void pointer, hence, it is naturally aligned */
  return (spiffs_DIR *)(uintptr_t)&dirp->private_data.buffer[0];
}

static int _opendir(vfs_DIR *dirp, const char *dirname) {
  spiffs_desc_t *fs_desc = dirp->mp->private_data;
  spiffs_DIR *d = _get_spifs_dir(dirp);

  spiffs_DIR *res = SPIFFS_opendir(&fs_desc->fs, dirname, d);
  if (res == NULL) {
    return -ENOENT;
  }

  return 0;
}

static int _readdir(vfs_DIR *dirp, vfs_dirent_t *entry) {
  spiffs_DIR *d = _get_spifs_dir(dirp);
  struct spiffs_dirent e;
  struct spiffs_dirent *ret;

  ret = SPIFFS_readdir(d, &e);
  if (ret == NULL) {
    s32_t err = SPIFFS_errno(d->fs);
    if (err != SPIFFS_OK && err > SPIFFS_ERR_INTERNAL) {
      LOG_DBG("spiffs: readdir: err=%d", err);
      return -EIO;
    }
  }

  if (ret) {
    entry->d_ino = e.obj_id;
    // +1 to skip the leading `/`
    strncpy(entry->d_name, (char *)e.name + 1, VFS_NAME_MAX);
    return 1;
  } else {
    return 0;
  }
}

static int _closedir(vfs_DIR *dirp) {
  spiffs_DIR *d = _get_spifs_dir(dirp);

  return spiffs_err_to_errno(SPIFFS_closedir(d));
}

static int spiffs_err_to_errno(s32_t err) {
  if (err >= 0) {
    return (int)err;
  }

  LOG_DBG("spiffs: error=%d", err);

  switch (err) {
  case SPIFFS_OK:
    return 0;
  case SPIFFS_ERR_NOT_MOUNTED:
    return -EINVAL;
  case SPIFFS_ERR_FULL:
    return -ENOSPC;
  case SPIFFS_ERR_NOT_FOUND:
    return -ENOENT;
  case SPIFFS_ERR_END_OF_OBJECT:
    return 0;
  case SPIFFS_ERR_DELETED:
    return -ENOENT;
  case SPIFFS_ERR_MOUNTED:
    return -EBUSY;
  case SPIFFS_ERR_ERASE_FAIL:
    return -EIO;
  case SPIFFS_ERR_MAGIC_NOT_POSSIBLE:
    return -ENOSPC;
  case SPIFFS_ERR_NO_DELETED_BLOCKS:
    return 0;
  case SPIFFS_ERR_FILE_EXISTS:
    return -EEXIST;
  case SPIFFS_ERR_NOT_A_FILE:
    return -ENOENT;
  case SPIFFS_ERR_RO_NOT_IMPL:
    return -EROFS;
  case SPIFFS_ERR_RO_ABORTED_OPERATION:
    return -SPIFFS_ERR_RO_ABORTED_OPERATION;
  case SPIFFS_ERR_PROBE_TOO_FEW_BLOCKS:
    return -ENOSPC;
  case SPIFFS_ERR_PROBE_NOT_A_FS:
    return -ENODEV;
  case SPIFFS_ERR_NAME_TOO_LONG:
    return -ENAMETOOLONG;
  case SPIFFS_ERR_NOT_FINALIZED:
    return -ENODEV;
  case SPIFFS_ERR_NOT_INDEX:
    return -ENODEV;
  case SPIFFS_ERR_OUT_OF_FILE_DESCS:
    return -ENFILE;
  case SPIFFS_ERR_FILE_CLOSED:
    return -ENOENT;
  case SPIFFS_ERR_FILE_DELETED:
    return -ENOENT;
  case SPIFFS_ERR_BAD_DESCRIPTOR:
    return -EBADF;
  case SPIFFS_ERR_IS_INDEX:
    return -ENOENT;
  case SPIFFS_ERR_IS_FREE:
    return -ENOENT;
  case SPIFFS_ERR_INDEX_SPAN_MISMATCH:
    return -EIO;
  case SPIFFS_ERR_DATA_SPAN_MISMATCH:
    return -EIO;
  case SPIFFS_ERR_INDEX_REF_FREE:
    return -EIO;
  case SPIFFS_ERR_INDEX_REF_LU:
    return -EIO;
  case SPIFFS_ERR_INDEX_REF_INVALID:
    return -EIO;
  case SPIFFS_ERR_INDEX_FREE:
    return -EIO;
  case SPIFFS_ERR_INDEX_LU:
    return -EIO;
  case SPIFFS_ERR_INDEX_INVALID:
    return -EIO;
  case SPIFFS_ERR_NOT_WRITABLE:
    return -EACCES;
  case SPIFFS_ERR_NOT_READABLE:
    return -EACCES;
  case SPIFFS_ERR_CONFLICTING_NAME:
    return -EEXIST;
  case SPIFFS_ERR_NOT_CONFIGURED:
    return -ENODEV;
  case SPIFFS_ERR_NOT_A_FS:
    return -ENODEV;
  }

  return (int)err;
}

int spiffs_vfs_init(void) { return 0; }

int spiffs_vfs_desc_init(spiffs_desc_t *desc) {
  int ret = 0;

  if ((ret = mutex_init(&desc->lock))) {
    return ret;
  }
  return 0;
}

static const vfs_file_system_ops_t spiffs_fs_ops = {
    .format = _format,
    .mount = _mount,
    .umount = _umount,
    .unlink = _unlink,
    .rename = _rename,
    .stat = vfs_sysop_stat_from_fstat,
};

static const vfs_file_ops_t spiffs_file_ops = {
    .open = _open,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .fstat = _fstat,
    .fsync = _fsync,
};

static const vfs_dir_ops_t spiffs_dir_ops = {
    .opendir = _opendir,
    .readdir = _readdir,
    .closedir = _closedir,
};

const vfs_file_system_t spiffs_file_system = {
    .fs_op = &spiffs_fs_ops,
    .f_op = &spiffs_file_ops,
    .d_op = &spiffs_dir_ops,
};