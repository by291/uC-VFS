#include "disk.h"
#include "errno.h"
#include "inttypes.h"
#include "logging.h"
#include "mem.h"
#include "string.h"

#include "littlefs_vfs.h"

mem_pool_t cache_pool;

static int littlefs_err_to_errno(ssize_t err) {
  switch (err) {
  case LFS_ERR_OK:
    return 0;
  case LFS_ERR_IO:
    return -EIO;
  case LFS_ERR_CORRUPT:
    return -ENODEV;
  case LFS_ERR_NOENT:
    return -ENOENT;
  case LFS_ERR_EXIST:
    return -EEXIST;
  case LFS_ERR_NOTDIR:
    return -ENOTDIR;
  case LFS_ERR_ISDIR:
    return -EISDIR;
  case LFS_ERR_NOTEMPTY:
    return -ENOTEMPTY;
  case LFS_ERR_BADF:
    return -EBADF;
  case LFS_ERR_INVAL:
    return -EINVAL;
  case LFS_ERR_NOSPC:
    return -ENOSPC;
  case LFS_ERR_NOMEM:
    return -ENOMEM;
  default:
    return err;
  }
}

void *_cache_alloc() { return mem_pool_alloc(&cache_pool); }

void _cache_free(void *blk) { mem_pool_free(&cache_pool, blk); }

static int _dev_read(const struct lfs_config *c, lfs_block_t block,
                     lfs_off_t off, void *buffer, lfs_size_t size) {
  littlefs2_desc_t *fs = c->context;
  vdisk_t *disk = fs->dev;

  uint32_t start_sec = (fs->base_addr + block) * fs->sectors_per_block;
  return vdisk_read(disk, buffer, start_sec, off, size);
}

static int _dev_write(const struct lfs_config *c, lfs_block_t block,
                      lfs_off_t off, const void *buffer, lfs_size_t size) {
  littlefs2_desc_t *fs = c->context;
  vdisk_t *disk = fs->dev;

  uint32_t start_sec = (fs->base_addr + block) * fs->sectors_per_block;
  return vdisk_write(disk, buffer, start_sec, off, size);
}

static int _dev_erase(const struct lfs_config *c, lfs_block_t block) {
  return 0;
}

static int _dev_sync(const struct lfs_config *c) {
  (void)c;

  return 0;
}

static int prepare(littlefs2_desc_t *fs, vdisk_no dno) {
  mutex_init(&fs->lock);
  mutex_lock(&fs->lock);

  vdisk_t *disk = vdisk_open(dno);

  if (!disk) {
    return -EINVAL;
  }

  memset(&fs->fs, 0, sizeof(fs->fs));

  size_t block_size = CONFIG_RAM_SEC_SIZE * CONFIG_SECTORS_PER_BLOCK;

  fs->sectors_per_block = CONFIG_SECTORS_PER_BLOCK;
  size_t block_count = CONFIG_RAM_N_SECS / fs->sectors_per_block;

  if (!fs->config.block_size) {
    fs->config.block_size = block_size;
  }
  if (!fs->config.block_count) {
    fs->config.block_count = block_count - fs->base_addr;
  }
  if (!fs->config.prog_size) {
    fs->config.prog_size = CONFIG_PAGE_SIZE;
  }
  if (!fs->config.read_size) {
    fs->config.read_size = CONFIG_PAGE_SIZE;
  }
  if (!fs->config.cache_size) {
    fs->config.cache_size = CONFIG_CACHE_SIZE;
  }
  if (!fs->config.block_cycles) {
    fs->config.block_cycles = CONFIG_LITTLEFS2_BLOCK_CYCLES;
  }
  fs->config.lookahead_size = CONFIG_LITTLEFS2_LOOKAHEAD_SIZE;
  fs->config.lookahead_buffer = fs->lookahead_buf;
  fs->config.context = fs;
  fs->config.read = _dev_read;
  fs->config.prog = _dev_write;
  fs->config.erase = _dev_erase;
  fs->config.sync = _dev_sync;
#if CONFIG_LITTLEFS2_FILE_BUFFER_SIZE
  fs->config.file_buffer = fs->file_buf;
#endif
#if CONFIG_LITTLEFS2_READ_BUFFER_SIZE
  fs->config.read_buffer = fs->read_buf;
#endif
#if CONFIG_LITTLEFS2_PROG_BUFFER_SIZE
  fs->config.prog_buffer = fs->prog_buf;
#endif

  return 0;
}

static int _format(vfs_mount_t *mountp) {
  littlefs2_desc_t *fs = mountp->private_data;

  int ret = prepare(fs, mountp->dno);
  if (ret) {
    return -ENODEV;
  }

  ret = lfs_format(&fs->fs, &fs->config);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _mount(vfs_mount_t *mountp) {
  /* if one of the lines below fail to compile you probably need to adjust
     vfs buffer sizes ;) */
  littlefs2_desc_t *fs = mountp->private_data;

  int ret = prepare(fs, mountp->dno);
  if (ret) {
    return -ENODEV;
  }

  ret = lfs_mount(&fs->fs, &fs->config);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _umount(vfs_mount_t *mountp) {
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  int ret = lfs_unmount(&fs->fs);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _unlink(vfs_mount_t *mountp, const char *name) {
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  int ret = lfs_remove(&fs->fs, name);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _rename(vfs_mount_t *mountp, const char *from_path,
                   const char *to_path) {
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  int ret = lfs_rename(&fs->fs, from_path, to_path);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _mkdir(vfs_mount_t *mountp, const char *name, mode_t mode) {
  (void)mode;
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  int ret = lfs_mkdir(&fs->fs, name);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _rmdir(vfs_mount_t *mountp, const char *name) {
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  int ret = lfs_remove(&fs->fs, name);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static inline lfs_file_t *_get_lfs_file(vfs_file_t *f) {
  /* The buffer in `private_data` is part of a union that also contains a
   * pointer, so the alignment is fine. Adding an intermediate cast to
   * uintptr_t to silence -Wcast-align
   */
  return (lfs_file_t *)(uintptr_t)f->private_data.buffer;
}

static int _open(vfs_file_t *filp, const char *name, int flags, mode_t mode) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);
  (void)mode;

  mutex_lock(&fs->lock);

  int l_flags = 0;
  if ((flags & O_ACCMODE) == O_RDONLY) {
    l_flags |= LFS_O_RDONLY;
  }
  if ((flags & O_APPEND) == O_APPEND) {
    l_flags |= LFS_O_APPEND;
  }
  if ((flags & O_TRUNC) == O_TRUNC) {
    l_flags |= LFS_O_TRUNC;
  }
  if ((flags & O_CREAT) == O_CREAT) {
    l_flags |= LFS_O_CREAT;
  }
  if ((flags & O_ACCMODE) == O_WRONLY) {
    l_flags |= LFS_O_WRONLY;
  }
  if ((flags & O_ACCMODE) == O_RDWR) {
    l_flags |= LFS_O_RDWR;
  }
  if ((flags & O_EXCL) == O_EXCL) {
    l_flags |= LFS_O_EXCL;
  }

  void *buffer = _cache_alloc();
  if (!buffer) {
    return -ENOMEM;
  }

  struct lfs_file_config cfg = {.buffer = buffer};
  int ret = lfs_file_opencfg(&fs->fs, fp, name, l_flags, &cfg);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _close(vfs_file_t *filp) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);

  mutex_lock(&fs->lock);

  int ret = lfs_file_close(&fs->fs, fp);
  _cache_free(fp->cfg->buffer);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);

  mutex_lock(&fs->lock);

  ssize_t ret = lfs_file_write(&fs->fs, fp, src, nbytes);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static ssize_t _read(vfs_file_t *filp, void *dest, size_t nbytes) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);

  mutex_lock(&fs->lock);

  ssize_t ret = lfs_file_read(&fs->fs, fp, dest, nbytes);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static off_t _lseek(vfs_file_t *filp, off_t off, int whence) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);

  mutex_lock(&fs->lock);

  int ret = lfs_file_seek(&fs->fs, fp, off, whence);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _fsync(vfs_file_t *filp) {
  littlefs2_desc_t *fs = filp->mp->private_data;
  lfs_file_t *fp = _get_lfs_file(filp);

  mutex_lock(&fs->lock);

  int ret = lfs_file_sync(&fs->fs, fp);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _stat(vfs_mount_t *mountp, const char *restrict path,
                 struct stat *restrict buf) {
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  struct lfs_info info;
  int ret = lfs_stat(&fs->fs, path, &info);
  mutex_unlock(&fs->lock);
  /* info.name */
  buf->st_size = info.size;
  switch (info.type) {
  case LFS_TYPE_REG:
    buf->st_mode = S_IFREG;
    break;
  case LFS_TYPE_DIR:
    buf->st_mode = S_IFDIR;
    break;
  }

  return littlefs_err_to_errno(ret);
}

static int _traverse_cb(void *param, lfs_block_t block) {
  (void)block;
  unsigned long *nb_blocks = param;
  (*nb_blocks)++;

  return 0;
}

static int _statvfs(vfs_mount_t *mountp, const char *restrict path,
                    struct statvfs *restrict buf) {
  (void)path;
  littlefs2_desc_t *fs = mountp->private_data;

  mutex_lock(&fs->lock);

  unsigned long nb_blocks = 0;
  int ret = lfs_fs_traverse(&fs->fs, _traverse_cb, &nb_blocks);
  mutex_unlock(&fs->lock);

  buf->f_bsize = fs->fs.cfg->block_size; /* block size */
  buf->f_frsize =
      CONFIG_PAGE_SIZE * CONFIG_PAGES_PER_SEC; /* fundamental block size */
  buf->f_blocks = fs->fs.cfg->block_count;     /* Blocks total */
  buf->f_bfree = buf->f_blocks - nb_blocks;    /* Blocks free */
  buf->f_bavail = buf->f_blocks -
                  nb_blocks; /* Blocks available to non-privileged processes */
  buf->f_flag = ST_NOSUID;
  buf->f_namemax = LFS_NAME_MAX;

  return littlefs_err_to_errno(ret);
}

static inline lfs_dir_t *_get_lfs_dir(vfs_DIR *dirp) {
  /* The buffer in `private_data` is part of a union that also contains a
   * pointer, so the alignment is fine. Adding an intermediate cast to
   * uintptr_t to silence -Wcast-align
   */
  return (lfs_dir_t *)(uintptr_t)dirp->private_data.buffer;
}

static int _opendir(vfs_DIR *dirp, const char *dirname) {
  littlefs2_desc_t *fs = dirp->mp->private_data;
  lfs_dir_t *dir = _get_lfs_dir(dirp);

  mutex_lock(&fs->lock);

  int ret = lfs_dir_open(&fs->fs, dir, dirname);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _readdir(vfs_DIR *dirp, vfs_dirent_t *entry) {
  littlefs2_desc_t *fs = dirp->mp->private_data;
  lfs_dir_t *dir = _get_lfs_dir(dirp);

  mutex_lock(&fs->lock);

  struct lfs_info info;
  int ret = lfs_dir_read(&fs->fs, dir, &info);
  if (ret >= 0) {
    entry->d_ino = info.type;
    strncpy(entry->d_name, info.name, VFS_NAME_MAX - 1);
  }

  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static int _closedir(vfs_DIR *dirp) {
  littlefs2_desc_t *fs = dirp->mp->private_data;
  lfs_dir_t *dir = _get_lfs_dir(dirp);

  mutex_lock(&fs->lock);

  int ret = lfs_dir_close(&fs->fs, dir);
  mutex_unlock(&fs->lock);

  return littlefs_err_to_errno(ret);
}

static const vfs_file_system_ops_t littlefs_fs_ops = {
    .format = _format,
    .mount = _mount,
    .umount = _umount,
    .unlink = _unlink,
    .mkdir = _mkdir,
    .rmdir = _rmdir,
    .rename = _rename,
    .stat = _stat,
    .statvfs = _statvfs,
};

static const vfs_file_ops_t littlefs_file_ops = {
    .open = _open,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .fsync = _fsync,
};

static const vfs_dir_ops_t littlefs_dir_ops = {
    .opendir = _opendir,
    .readdir = _readdir,
    .closedir = _closedir,
};

const vfs_file_system_t littlefs2_file_system = {
    .fs_op = &littlefs_fs_ops,
    .f_op = &littlefs_file_ops,
    .d_op = &littlefs_dir_ops,
};