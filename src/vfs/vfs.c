/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_vfs
 * @{
 * @file
 * @brief   VFS layer implementation
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#include <fcntl.h>
#include <string.h>

#include "clist.h"
#include "common.h"
#include "errno.h"
#include "logging.h"
#include "mutex.h"
#include "vfs.h"

LOG_MODULE_REGISTER(vfs, LOG_LEVEL_INF);

/**
 * @internal
 * @brief Array of all currently open files
 *
 * This table maps POSIX fd numbers to vfs_file_t instances
 *
 * @attention STDIN, STDOUT, STDERR will use the three first items in this
 * array.
 */
static vfs_file_t _vfs_open_files[VFS_MAX_OPEN_FILES];

/**
 * @internal
 * @brief List handle for list of all currently mounted file systems
 *
 * This singly linked list is used to dispatch vfs calls to the appropriate file
 * system driver.
 */
static clist_node_t _vfs_mounts_list;

/**
 * @internal
 * @brief Find an unused entry in the _vfs_open_files array and mark it as used
 *
 * If the @p fd argument is non-negative, the allocation fails if the
 * corresponding slot in the open files table is already occupied, no iteration
 * is done to find another free number in this case.
 *
 * If the @p fd argument is negative, the algorithm will iterate through the
 * open files table until it find an unused slot and return the number of that
 * slot.
 *
 * @param[in]  fd  Desired fd number, use VFS_ANY_FD for any free fd
 *
 * @return fd on success
 * @return <0 on error
 */
static inline int _allocate_fd(int fd);

/**
 * @internal
 * @brief Mark an allocated entry as unused in the _vfs_open_files array
 *
 * @param[in]  fd     fd to free
 */
static inline void _free_fd(int fd);

/**
 * @internal
 * @brief Initialize an entry in the _vfs_open_files array and mark it as used.
 *
 * @param[in]  fd           desired fd number, passed to _allocate_fd
 * @param[in]  f_op         pointer to file operations table
 * @param[in]  mountp       pointer to mount table entry, may be NULL
 * @param[in]  flags        file flags
 * @param[in]  private_data private_data initial value
 *
 * @return fd on success
 * @return <0 on error
 */
static inline int _init_fd(int fd, const vfs_file_ops_t *f_op,
                           vfs_mount_t *mountp, int flags, void *private_data);

/**
 * @internal
 * @brief Find the file system associated with the file name @p name, and
 * increment the open_files counter
 *
 * A pointer to the vfs_mount_t associated with the found mount will be written
 * to @p mountpp. A pointer to the mount point-relative file name will be
 * written to @p rel_path.
 *
 * @param[out] mountpp   write address of the found mount to this pointer
 * @param[in]  name      absolute path to file
 * @param[out] rel_path  (optional) output pointer for relative path
 *
 * @return mount index on success
 * @return <0 on error
 */
static inline int _find_mount(vfs_mount_t **mountpp, const char *name,
                              const char **rel_path);

/**
 * @internal
 * @brief Check that a given fd number is valid
 *
 * @param[in]  fd    fd to check
 *
 * @return 0 if the fd is valid
 * @return <0 if the fd is not valid
 */
static inline int _fd_is_valid(int fd);

static mutex_t _mount_mutex;
static mutex_t _open_mutex;

int vfs_close(int fd) {
  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (filp->f_op->close != NULL) {
    /* We will invalidate the fd regardless of the outcome of the file
     * system driver close() call below */
    res = filp->f_op->close(filp);
  }
  _free_fd(fd);
  return res;
}

int vfs_fstat(int fd, struct stat *buf) {
  if (buf == NULL) {
    return -EFAULT;
  }
  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (filp->f_op->fstat == NULL) {
    /* driver does not implement fstat() */
    return -EINVAL;
  }
  memset(buf, 0, sizeof(*buf));
  return filp->f_op->fstat(filp, buf);
}

off_t vfs_lseek(int fd, off_t off, int whence) {
  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (filp->f_op->lseek == NULL) {
    /* driver does not implement lseek() */
    /* default seek functionality is naive */
    switch (whence) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      off += filp->pos;
      break;
    case SEEK_END:
      /* we could use fstat here, but most file system drivers will
       * likely already implement lseek in a more efficient fashion */
      return -EINVAL;
    default:
      return -EINVAL;
    }
    if (off < 0) {
      /* the resulting file offset would be negative */
      return -EINVAL;
    }
    filp->pos = off;

    return off;
  }
  return filp->f_op->lseek(filp, off, whence);
}

int vfs_open(const char *name, int flags, mode_t mode) {
  if (name == NULL) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res = _find_mount(&mountp, name, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_open: no matching mount\n");
    return res;
  }
  mutex_lock(&_open_mutex);
  int fd = _init_fd(VFS_ANY_FD, mountp->fs->f_op, mountp, flags, NULL);
  mutex_unlock(&_open_mutex);
  if (fd < 0) {
    LOG_DBG("vfs_open: _init_fd: ERR %d!\n", fd);
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return fd;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (filp->f_op->open != NULL) {
    res = filp->f_op->open(filp, rel_path, flags, mode);
    if (res < 0) {
      /* something went wrong during open */
      LOG_DBG("vfs_open: open: ERR %d!\n", res);
      /* clean up */
      _free_fd(fd);
      return res;
    }
  }
  return fd;
}

static inline int _prep_read(int fd, const void *dest, vfs_file_t **filp) {
  if (dest == NULL) {
    return -EFAULT;
  }

  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  *filp = &_vfs_open_files[fd];

  if ((((*filp)->flags & O_ACCMODE) != O_RDONLY) &&
      (((*filp)->flags & O_ACCMODE) != O_RDWR)) {
    /* File not open for reading */
    return -EBADF;
  }
  if ((*filp)->f_op->read == NULL) {
    /* driver does not implement read() */
    return -EINVAL;
  }

  return 0;
}

ssize_t vfs_read(int fd, void *dest, size_t count) {
  vfs_file_t *filp = NULL;

  int res = _prep_read(fd, dest, &filp);
  if (res) {
    LOG_DBG("vfs_read: can't open file - %d\n", res);
    return res;
  }

  return filp->f_op->read(filp, dest, count);
}

ssize_t vfs_readline(int fd, char *dst, size_t len_max) {
  vfs_file_t *filp = NULL;

  int res = _prep_read(fd, dst, &filp);
  if (res) {
    LOG_DBG("vfs_readline: can't open file - %d\n", res);
    return res;
  }

  const char *start = dst;
  while (len_max) {
    int res = filp->f_op->read(filp, dst, 1);
    if (res < 0) {
      break;
    }

    if (*dst == '\r' || *dst == '\n' || res == 0) {
      *dst = 0;
      ++dst;
      break;
    } else {
      --len_max;
      ++dst;
    }
  }

  if (len_max == 0) {
    return -E2BIG;
  }

  return dst - start;
}

ssize_t vfs_write(int fd, const void *src, size_t count) {
  if (src == NULL) {
    return -EFAULT;
  }
  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (((filp->flags & O_ACCMODE) != O_WRONLY) &
      ((filp->flags & O_ACCMODE) != O_RDWR)) {
    /* File not open for writing */
    return -EBADF;
  }
  if (filp->f_op->write == NULL) {
    /* driver does not implement write() */
    return -EINVAL;
  }
  return filp->f_op->write(filp, src, count);
}

int vfs_fsync(int fd) {
  int res = _fd_is_valid(fd);
  if (res < 0) {
    return res;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (((filp->flags & O_ACCMODE) != O_WRONLY) &
      ((filp->flags & O_ACCMODE) != O_RDWR)) {
    /* File not open for writing */
    return -EBADF;
  }
  if (filp->f_op->fsync == NULL) {
    /* driver does not implement fsync() */
    return -EINVAL;
  }
  return filp->f_op->fsync(filp);
}

int vfs_opendir(vfs_DIR *dirp, const char *dirname) {
  if ((dirp == NULL) || (dirname == NULL)) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res = _find_mount(&mountp, dirname, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_open: no matching mount\n");
    return res;
  }
  if (rel_path[0] == '\0') {
    /* if the trailing slash is missing we will get an empty string back, to
     * be consistent against the file system drivers we give the relative
     * path "/" instead */
    rel_path = "/";
  }
  if (mountp->fs->d_op == NULL) {
    /* file system driver does not support directories */
    return -EINVAL;
  }
  /* initialize dirp */
  memset(dirp, 0, sizeof(*dirp));
  dirp->mp = mountp;
  dirp->d_op = mountp->fs->d_op;
  if (dirp->d_op->opendir != NULL) {
    int res = dirp->d_op->opendir(dirp, rel_path);
    if (res < 0) {
      /* remember to decrement the open_files count */
      mountp->open_files--;
      return res;
    }
  }
  return 0;
}

int vfs_readdir(vfs_DIR *dirp, vfs_dirent_t *entry) {
  if ((dirp == NULL) || (entry == NULL)) {
    return -EINVAL;
  }
  if (dirp->d_op != NULL) {
    if (dirp->d_op->readdir != NULL) {
      return dirp->d_op->readdir(dirp, entry);
    }
  }
  return -EINVAL;
}

int vfs_closedir(vfs_DIR *dirp) {
  if (dirp == NULL) {
    return -EINVAL;
  }
  vfs_mount_t *mountp = dirp->mp;
  if (mountp == NULL) {
    return -EBADF;
  }
  int res = 0;
  if (dirp->d_op != NULL) {
    if (dirp->d_op->closedir != NULL) {
      res = dirp->d_op->closedir(dirp);
    }
  }
  memset(dirp, 0, sizeof(*dirp));
  mountp->open_files--;
  return res;
}

/**
 * @brief Check if the given mount point is mounted
 *
 * If the mount point is not mounted, _mount_mutex will be locked by this
 * function
 *
 * @param mountp    mount point to check
 * @return 0 on success (mount point is valid and not mounted)
 * @return -EINVAL if mountp is invalid
 * @return -EBUSY if mountp is already mounted
 */
static int check_mount(vfs_mount_t *mountp) {
  if ((mountp == NULL) || (mountp->fs == NULL) ||
      (mountp->mount_point == NULL)) {
    return -EINVAL;
  }
  LOG_DBG("vfs_mount: -> \"%s\" (%p), %p\n", mountp->mount_point,
          (void *)mountp->mount_point, mountp->private_data);
  if (mountp->mount_point[0] != '/') {
    LOG_DBG("vfs: check_mount: not absolute mount_point path\n");
    return -EINVAL;
  }
  mountp->mount_point_len = strlen(mountp->mount_point);
  mutex_lock(&_mount_mutex);
  /* Check for the same mount in the list of mounts to avoid loops */
  clist_node_t *found = clist_find(&_vfs_mounts_list, &mountp->list_entry);
  if (found) {
    /* Same mount is already mounted */
    mutex_unlock(&_mount_mutex);
    LOG_DBG("vfs: check_mount: Already mounted\n");
    return -EBUSY;
  }

  return 0;
}

int vfs_format(vfs_mount_t *mountp) {
  int ret = check_mount(mountp);
  if (ret < 0) {
    return ret;
  }
  mutex_unlock(&_mount_mutex);

  if (mountp->fs->fs_op != NULL) {
    if (mountp->fs->fs_op->format != NULL) {
      return mountp->fs->fs_op->format(mountp);
    }
  }

  /* Format operation not supported */
  return -ENOTSUP;
}

int vfs_mount(vfs_mount_t *mountp) {
  int ret = check_mount(mountp);
  if (ret < 0) {
    return ret;
  }

  if (mountp->fs->fs_op != NULL) {
    if (mountp->fs->fs_op->mount != NULL) {
      /* yes, a file system driver does not need to implement mount/umount */
      int res = mountp->fs->fs_op->mount(mountp);
      if (res < 0) {
        LOG_DBG("vfs_mount: error %d\n", res);
        mutex_unlock(&_mount_mutex);
        return res;
      }
    }
  }
  /* Insert last in list. This property is relied on by vfs_iterate_mount_dirs.
   */
  clist_rpush(&_vfs_mounts_list, &mountp->list_entry);
  mutex_unlock(&_mount_mutex);
  LOG_DBG("vfs_mount: mount done\n");
  return 0;
}

int vfs_umount(vfs_mount_t *mountp, bool force) {
  int ret = check_mount(mountp);
  switch (ret) {
  case 0:
    LOG_DBG("vfs_umount: not mounted\n");
    mutex_unlock(&_mount_mutex);
    return -EINVAL;
  case -EBUSY:
    /* -EBUSY returned when fs is mounted, just continue */
    mutex_lock(&_mount_mutex);
    break;
  default:
    LOG_DBG("vfs_umount: invalid fs\n");
    return -EINVAL;
  }

  if (mountp->open_files > 0 && !force) {
    mutex_unlock(&_mount_mutex);
    return -EBUSY;
  }
  if (mountp->fs->fs_op != NULL) {
    if (mountp->fs->fs_op->umount != NULL) {
      int res = mountp->fs->fs_op->umount(mountp);
      if (res < 0) {
        /* umount failed */
        LOG_DBG("vfs_umount: ERR %d!\n", res);
        mutex_unlock(&_mount_mutex);
        return res;
      }
    }
  }
  /* find mountp in the list and remove it */
  clist_node_t *node = clist_remove(&_vfs_mounts_list, &mountp->list_entry);
  if (node == NULL) {
    /* not found */
    LOG_DBG("vfs_umount: ERR not mounted!\n");
    mutex_unlock(&_mount_mutex);
    return -EINVAL;
  }

  mutex_unlock(&_mount_mutex);
  return 0;
}

int vfs_rename(const char *from_path, const char *to_path) {
  if ((from_path == NULL) || (to_path == NULL)) {
    return -EINVAL;
  }
  const char *rel_from;
  vfs_mount_t *mountp;
  int res = _find_mount(&mountp, from_path, &rel_from);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_rename: from: no matching mount\n");
    return res;
  }
  if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->rename == NULL)) {
    /* rename not supported */
    LOG_DBG("vfs_rename: rename not supported by fs!\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return -EROFS;
  }
  const char *rel_to;
  vfs_mount_t *mountp_to;
  res = _find_mount(&mountp_to, to_path, &rel_to);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_rename: to: no matching mount\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return res;
  }
  if (mountp_to != mountp) {
    /* The paths are on different file systems */
    LOG_DBG("vfs_rename: from_path and to_path are on different mounts\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    mountp_to->open_files--;
    return -EXDEV;
  }
  res = mountp->fs->fs_op->rename(mountp, rel_from, rel_to);
  LOG_DBG("vfs_rename: rename %p, \"%s\" -> \"%s\"", (void *)mountp, rel_from,
          rel_to);
  if (res < 0) {
    /* something went wrong during rename */
    LOG_DBG(": ERR %d!\n", res);
  } else {
    LOG_DBG("\n");
  }
  /* remember to decrement the open_files count */
  mountp->open_files--;
  mountp_to->open_files--;
  return res;
}

/* TODO: Share code between vfs_unlink, vfs_mkdir, vfs_rmdir since they are
 * almost identical */

int vfs_unlink(const char *name) {
  LOG_DBG("vfs_unlink: \"%s\"\n", name);
  if (name == NULL) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res;
  res = _find_mount(&mountp, name, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_unlink: no matching mount\n");
    return res;
  }
  if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->unlink == NULL)) {
    /* unlink not supported */
    LOG_DBG("vfs_unlink: unlink not supported by fs!\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return -EROFS;
  }
  res = mountp->fs->fs_op->unlink(mountp, rel_path);
  LOG_DBG("vfs_unlink: unlink %p, \"%s\"", (void *)mountp, rel_path);
  if (res < 0) {
    /* something went wrong during unlink */
    LOG_DBG(": ERR %d!\n", res);
  } else {
    LOG_DBG("\n");
  }
  /* remember to decrement the open_files count */
  mountp->open_files--;
  return res;
}

int vfs_mkdir(const char *name, mode_t mode) {
  if (name == NULL) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res;
  res = _find_mount(&mountp, name, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_mkdir: no matching mount\n");
    return res;
  }
  if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->mkdir == NULL)) {
    /* mkdir not supported */
    LOG_DBG("vfs_mkdir: mkdir not supported by fs!\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return -ENOTSUP;
  }
  res = mountp->fs->fs_op->mkdir(mountp, rel_path, mode);
  LOG_DBG("vfs_mkdir: mkdir %p, \"%s\"", (void *)mountp, rel_path);
  if (res < 0) {
    /* something went wrong during mkdir */
    LOG_DBG(": ERR %d!\n", res);
  } else {
    LOG_DBG("\n");
  }
  /* remember to decrement the open_files count */
  mountp->open_files--;
  return res;
}

int vfs_rmdir(const char *name) {
  LOG_DBG("vfs_rmdir: \"%s\"\n", name);
  if (name == NULL) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res;
  res = _find_mount(&mountp, name, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_rmdir: no matching mount\n");
    return res;
  }
  if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->rmdir == NULL)) {
    /* rmdir not supported */
    LOG_DBG("vfs_rmdir: rmdir not supported by fs!\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return -ENOTSUP;
  }
  res = mountp->fs->fs_op->rmdir(mountp, rel_path);
  LOG_DBG("vfs_rmdir: rmdir %p, \"%s\"", (void *)mountp, rel_path);
  if (res < 0) {
    /* something went wrong during rmdir */
    LOG_DBG(": ERR %d!\n", res);
  } else {
    LOG_DBG("\n");
  }
  /* remember to decrement the open_files count */
  mountp->open_files--;
  return res;
}

int vfs_stat(const char *restrict path, struct stat *restrict buf) {
  LOG_DBG("vfs_stat: \"%s\", %p\n", path, (void *)buf);
  if (path == NULL || buf == NULL) {
    return -EINVAL;
  }
  const char *rel_path;
  vfs_mount_t *mountp;
  int res;
  res = _find_mount(&mountp, path, &rel_path);
  /* _find_mount implicitly increments the open_files count on success */
  if (res < 0) {
    /* No mount point maps to the requested file name */
    LOG_DBG("vfs_stat: no matching mount\n");
    return res;
  }
  if ((mountp->fs->fs_op == NULL) || (mountp->fs->fs_op->stat == NULL)) {
    /* stat not supported */
    LOG_DBG("vfs_stat: stat not supported by fs!\n");
    /* remember to decrement the open_files count */
    mountp->open_files--;
    return -EPERM;
  }
  memset(buf, 0, sizeof(*buf));
  res = mountp->fs->fs_op->stat(mountp, rel_path, buf);
  /* remember to decrement the open_files count */
  mountp->open_files--;
  return res;
}

int vfs_normalize_path(char *buf, const char *path, size_t buflen) {
  size_t len = 0;
  int npathcomp = 0;
  const char *path_end =
      path + strlen(path); /* Find the terminating null byte */
  if (len >= buflen) {
    return -ENAMETOOLONG;
  }

  while (path <= path_end) {
    LOG_DBG("vfs_normalize_path: + %d \"%.*s\" <- \"%s\" (%p)\n", npathcomp,
            (int)len, buf, path, (void *)path);
    if (path[0] == '\0') {
      break;
    }
    while (path[0] == '/') {
      /* skip extra slashes */
      ++path;
    }
    if (path[0] == '.') {
      ++path;
      if (path[0] == '/' || path[0] == '\0') {
        /* skip /./ components */
        LOG_DBG("vfs_normalize_path: skip .\n");
        continue;
      }
      if (path[0] == '.' && (path[1] == '/' || path[1] == '\0')) {
        LOG_DBG("vfs_normalize_path: reduce ../\n");
        if (len == 0) {
          /* outside root */
          return -EINVAL;
        }
        ++path;
        /* delete the last component of the path */
        while (len > 0 && buf[--len] != '/') {
        }
        --npathcomp;
        continue;
      }
    }
    buf[len++] = '/';
    if (len >= buflen) {
      return -ENAMETOOLONG;
    }
    if (path[0] == '\0') {
      /* trailing slash in original path, don't increment npathcomp */
      break;
    }
    ++npathcomp;
    /* copy the path component */
    while (len < buflen && path[0] != '/' && path[0] != '\0') {
      buf[len++] = path[0];
      ++path;
    }
    if (len >= buflen) {
      return -ENAMETOOLONG;
    }
  }
  /* special case for "/": (otherwise it will be zero) */
  if (len == 1) {
    npathcomp = 1;
  }
  buf[len] = '\0';
  LOG_DBG("vfs_normalize_path: = %d, \"%s\"\n", npathcomp, buf);
  return npathcomp;
}

const vfs_file_t *vfs_file_get(int fd) {
  if (_fd_is_valid(fd) == 0) {
    return &_vfs_open_files[fd];
  } else {
    return NULL;
  }
}

static inline int _allocate_fd(int fd) {
  if (fd < 0) {
    for (fd = 0; fd < VFS_MAX_OPEN_FILES; ++fd) {
      if (_vfs_open_files[fd].p_tcb == NULL) {
        break;
      }
    }
  }
  if (fd >= VFS_MAX_OPEN_FILES) {
    /* The _vfs_open_files array is full */
    return -ENFILE;
  } else if (_vfs_open_files[fd].p_tcb != NULL) {
    /* The desired fd is already in use */
    return -EEXIST;
  }

  _vfs_open_files[fd].p_tcb = OSTCBCurPtr;
  return fd;
}

static inline void _free_fd(int fd) {
  if (_vfs_open_files[fd].mp != NULL) {
    _vfs_open_files[fd].mp->open_files--;
  }
  _vfs_open_files[fd].p_tcb = NULL;
}

static inline int _init_fd(int fd, const vfs_file_ops_t *f_op,
                           vfs_mount_t *mountp, int flags, void *private_data) {
  fd = _allocate_fd(fd);
  if (fd < 0) {
    return fd;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  filp->mp = mountp;
  filp->f_op = f_op;
  filp->flags = flags;
  filp->pos = 0;
  filp->private_data.ptr = private_data;
  return fd;
}

static inline int _find_mount(vfs_mount_t **mountpp, const char *name,
                              const char **rel_path) {
  size_t longest_match = 0;
  size_t name_len = strlen(name);
  mutex_lock(&_mount_mutex);

  clist_node_t *node = _vfs_mounts_list.next;
  if (node == NULL) {
    /* list empty */
    mutex_unlock(&_mount_mutex);
    return -ENOENT;
  }

  vfs_mount_t *mountp = NULL;
  do {
    node = node->next;
    vfs_mount_t *it = CONTAINER_OF(node, vfs_mount_t, list_entry);
    size_t len = it->mount_point_len;
    if (len < longest_match) {
      /* Already found a longer prefix */
      continue;
    }
    if (len > name_len) {
      /* path name is shorter than the mount point name */
      continue;
    }
    if ((len > 1) && (name[len] != '/') && (name[len] != '\0')) {
      /* name does not have a directory separator where mount point name ends */
      continue;
    }
    if (strncmp(name, it->mount_point, len) == 0) {
      /* mount_point is a prefix of name */
      /* special check for mount_point == "/" */
      if (len > 1) {
        longest_match = len;
      }
      mountp = it;
    }
  } while (node != _vfs_mounts_list.next);

  if (mountp == NULL) {
    /* not found */
    mutex_unlock(&_mount_mutex);
    return -ENOENT;
  }
  /* Increment open files counter for this mount */
  mountp->open_files++;
  /* We cannot use assume() here, an overflow could occur in absence of
   * any bugs and should also be checked for in production code. We use
   * expect() here, which was actually written for unit tests but works
   * here as well */
  mutex_unlock(&_mount_mutex);
  *mountpp = mountp;

  if (rel_path != NULL) {
    if (mountp->fs->flags & VFS_FS_FLAG_WANT_ABS_PATH) {
      *rel_path = name;
    } else {
      *rel_path = name + longest_match;
    }
  }
  return 0;
}

static inline int _fd_is_valid(int fd) {
  if ((unsigned int)fd >= VFS_MAX_OPEN_FILES) {
    return -EBADF;
  }
  vfs_file_t *filp = &_vfs_open_files[fd];
  if (filp->p_tcb == NULL) {
    return -EBADF;
  }
  if (filp->f_op == NULL) {
    return -EBADF;
  }
  return 0;
}

static bool _is_dir(vfs_mount_t *mountp, vfs_DIR *dir,
                    const char *restrict path) {
  const vfs_dir_ops_t *ops = mountp->fs->d_op;
  if (!ops->opendir) {
    return false;
  }

  dir->d_op = ops;
  dir->mp = mountp;

  int res = ops->opendir(dir, path);
  if (res < 0) {
    return false;
  }

  ops->closedir(dir);
  return true;
}

int vfs_sysop_stat_from_fstat(vfs_mount_t *mountp, const char *restrict path,
                              struct stat *restrict buf) {
  const vfs_file_ops_t *f_op = mountp->fs->f_op;

  union {
    vfs_file_t file;
    vfs_DIR dir;
  } filedir = {
      .file =
          {
              .mp = mountp,
              /* As per definition of the `vfsfile_ops::open` field */
              .f_op = f_op,
              .private_data = {.ptr = NULL},
              .pos = 0,
          },
  };

  int err = f_op->open(&filedir.file, path, 0, 0);
  if (err < 0) {
    if (_is_dir(mountp, &filedir.dir, path)) {
      buf->st_mode = S_IFDIR;
      return 0;
    }
    return err;
  }
  err = f_op->fstat(&filedir.file, buf);
  f_op->close(&filedir.file);
  return err;
}

int vfs_init() {
  int ret = 0;

  if ((ret = mutex_init(&_mount_mutex))) {
    return ret;
  }
  if ((ret = mutex_init(&_open_mutex))) {
    return ret;
  }
  if ((ret = ramdisk_init())) {
    return ret;
  }
  return ret;
}

/** @} */
