#include <Source/fs_dev.h>
#include <Source/fs_err.h>
#include <fs_dev_ramdisk.h>

#include "common.h"
#include "disk.h"
#include "errno.h"
#include "inttypes.h"
#include "logging.h"
#include "string.h"

LOG_MODULE_REGISTER(diskio, LOG_LEVEL_DBG);

struct vdisk_struct {
  char *dev_name;
};

uint8_t DISK_RAM_AREAS[2][CONFIG_RAM_SEC_SIZE * CONFIG_RAM_N_SECS];
FS_DEV_RAM_CFG DISK_RAM_CONFIGS[2] = {
    {
        .SecSize = CONFIG_RAM_SEC_SIZE,
        .Size = CONFIG_RAM_N_SECS,
        .DiskPtr = DISK_RAM_AREAS[0],
    },
    {
        .SecSize = CONFIG_RAM_SEC_SIZE,
        .Size = CONFIG_RAM_N_SECS,
        .DiskPtr = DISK_RAM_AREAS[1],
    },
};
vdisk_t DISKS[2] = {{.dev_name = "ram:0:"}, {.dev_name = "ram:1:"}};

vdisk_t *vdisk_open(vdisk_no no) {
  FS_ERR err = FS_ERR_NONE;

  if (no > 1) {
    LOG_DBG("disk_no=%d", no);
    return NULL;
  }

  FS_DEV_RAM_CFG *cfg = &DISK_RAM_CONFIGS[no];
  vdisk_t *disk = &DISKS[no];
  FSDev_Open(disk->dev_name, cfg, &err);
  if (err != FS_ERR_NONE) {
    LOG_DBG("Dev_Open=%d", err);
    return NULL;
  }
  return disk;
}

int vdisk_read(vdisk_t *disk, void *buf, uint32_t start_sec, off_t off,
               size_t sz) {
  FS_ERR err = FS_ERR_NONE;

  size_t start_addr = start_sec * CONFIG_RAM_SEC_SIZE + off;
  size_t end_addr = start_addr + sz;
  uint32_t first_sec =
      ROUND_DOWN(start_addr, CONFIG_RAM_SEC_SIZE) / CONFIG_RAM_SEC_SIZE;
  uint32_t last_sec =
      ROUND_DOWN(end_addr, CONFIG_RAM_SEC_SIZE) / CONFIG_RAM_SEC_SIZE;

  off_t first_off = start_addr % CONFIG_RAM_SEC_SIZE;
  size_t first_sz = MIN(CONFIG_RAM_SEC_SIZE - first_off, sz);
  size_t last_sz = end_addr % CONFIG_RAM_SEC_SIZE ?: CONFIG_RAM_SEC_SIZE;
  size_t middle_secs = last_sec > start_sec ? last_sec - start_sec - 1 : 0;

  char sec_buf[CONFIG_RAM_SEC_SIZE];

  // first sec
  FSDev_Rd(disk->dev_name, sec_buf, first_sec, 1, &err);
  if (err != FS_ERR_NONE) {
    LOG_DBG("Dev_Rd=%d", err);
    return -EIO;
  }
  memcpy(buf, sec_buf + first_off, first_sz);
  buf += first_sz;

  // middle secs
  if (middle_secs > 0) {
    FSDev_Rd(disk->dev_name, buf, first_sec + 1, middle_secs, &err);
    if (err != FS_ERR_NONE) {
      LOG_DBG("Dev_Rd=%d", err);
      return -EIO;
    }
    buf += (middle_secs * CONFIG_RAM_SEC_SIZE);
  }

  // last sec
  if (last_sec != first_sec) {
    FSDev_Rd(disk->dev_name, sec_buf, last_sec, 1, &err);
    if (err != FS_ERR_NONE) {
      LOG_DBG("Dev_Rd=%d", err);
      return -EIO;
    }
    memcpy(buf, sec_buf, last_sz);
  }
  return sz;
}

int vdisk_write(vdisk_t *disk, const void *buf, uint32_t start_sec, off_t off,
                size_t sz) {
  FS_ERR err = FS_ERR_NONE;

  size_t start_addr = start_sec * CONFIG_RAM_SEC_SIZE + off;
  size_t end_addr = start_addr + sz;
  uint32_t first_sec =
      ROUND_DOWN(start_addr, CONFIG_RAM_SEC_SIZE) / CONFIG_RAM_SEC_SIZE;
  uint32_t last_sec =
      ROUND_DOWN(end_addr, CONFIG_RAM_SEC_SIZE) / CONFIG_RAM_SEC_SIZE;

  off_t first_off = start_addr % CONFIG_RAM_SEC_SIZE;
  size_t first_sz = MIN(CONFIG_RAM_SEC_SIZE - first_off, sz);
  size_t last_sz = end_addr % CONFIG_RAM_SEC_SIZE ?: CONFIG_RAM_SEC_SIZE;
  size_t middle_secs = last_sec > start_sec ? last_sec - start_sec - 1 : 0;

  char sec_buf[CONFIG_RAM_SEC_SIZE];

  // first sec
  FSDev_Rd(disk->dev_name, sec_buf, first_sec, 1, &err);
  if (err != FS_ERR_NONE) {
    LOG_DBG("Dev_Rd=%d", err);
    return -EIO;
  }
  memcpy(sec_buf + first_off, buf, first_sz);
  FSDev_Wr(disk->dev_name, sec_buf, first_sec, 1, &err);
  if (err != FS_ERR_NONE) {
    LOG_DBG("Dev_Wr=%d", err);
    return -EIO;
  }
  buf += first_sz;

  // middle secs
  if (middle_secs > 0) {
    FSDev_Wr(disk->dev_name, buf, first_sec + 1, middle_secs, &err);
    if (err != FS_ERR_NONE) {
      LOG_DBG("Dev_Wr=%d", err);
      return -EIO;
    }
  }
  buf += middle_secs * CONFIG_RAM_SEC_SIZE;

  // last sec
  if (last_sec != first_sec) {
    FSDev_Rd(disk->dev_name, sec_buf, last_sec, 1, &err);
    if (err != FS_ERR_NONE) {
      LOG_DBG("Dev_Rd=%d", err);
      return -EIO;
    }
    memcpy(sec_buf, buf, last_sz);
    FSDev_Wr(disk->dev_name, sec_buf, last_sec, 1, &err);
    if (err != FS_ERR_NONE) {
      LOG_DBG("Dev_Wr=%d", err);
      return -EIO;
    }
  }
  return sz;
}