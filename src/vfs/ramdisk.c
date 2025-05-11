#include <string.h>

#include "common.h"
#include "errno.h"
#include "inttypes.h"
#include "logging.h"
#include "ramdisk.h"
#include "string.h"

LOG_MODULE_REGISTER(diskio, LOG_LEVEL_DBG);

#define RAMDISK_MAX_SIZE (CONFIG_RAM_SEC_SIZE * CONFIG_RAM_N_SECS)

struct ramdisk_struct {
  char mem[CONFIG_RAM_SEC_SIZE * CONFIG_RAM_N_SECS];
};

ramdisk_t disks[2] = {};

int ramdisk_init() {
  memset(disks[0].mem, 0, RAMDISK_MAX_SIZE);
  memset(disks[1].mem, 0xFF, RAMDISK_MAX_SIZE);
  return 0;
}

ramdisk_t *ramdisk_open(ramdisk_no no) {
  if (no > 1) {
    LOG_DBG("disk_no=%d", no);
    return NULL;
  }

  ramdisk_t *disk = &disks[no];
  return disk;
}

int ramdisk_read(ramdisk_t *disk, void *buf, uint32_t start_sec, off_t off,
                 size_t sz) {

  if (!disk || !buf) {
    LOG_ERR("invalid disk or buf");
    return -EINVAL;
  }
  if (sz == 0) {
    return 0;
  }

  if (start_sec >= CONFIG_RAM_N_SECS || off < 0) {
    LOG_ERR("invalid start_sec or off");
    return -EINVAL;
  }

  size_t start_addr = start_sec * CONFIG_RAM_SEC_SIZE + off;
  size_t end_addr = start_addr + sz;
  if (end_addr > RAMDISK_MAX_SIZE || end_addr < start_addr) { // 检查溢出
    LOG_ERR("addr overflow");
    return -EOVERFLOW;
  }

  memcpy(buf, disk->mem + start_addr, sz);
  return sz;
}

int ramdisk_read_addr(ramdisk_t *disk, void *buf, size_t addr, size_t sz) {
  if (!disk || !buf) {
    LOG_ERR("invalid disk or buf");
    return -EINVAL;
  }
  if (sz == 0) {
    return 0;
  }

  if (addr >= RAMDISK_MAX_SIZE) {
    LOG_ERR("invalid addr");
    return -EINVAL;
  }

  size_t end_addr = addr + sz;
  if (end_addr > RAMDISK_MAX_SIZE || end_addr < addr) { 
    LOG_ERR("addr overflow");
    return -EOVERFLOW;
  }
  memcpy(buf, disk->mem + addr, sz);
  return sz;
}

int ramdisk_write(ramdisk_t *disk, const void *buf, uint32_t start_sec,
                  off_t off, size_t sz) {
  if (!disk || !buf) {
    LOG_ERR("invalid disk or buf");
    return -EINVAL;
  }
  if (sz == 0) {
    return 0;
  }

  if (start_sec >= CONFIG_RAM_N_SECS || off < 0) {
    LOG_ERR("invalid start_sec or off");
    return -EINVAL;
  }

  size_t start_addr = start_sec * CONFIG_RAM_SEC_SIZE + off;
  size_t end_addr = start_addr + sz;
  if (end_addr > RAMDISK_MAX_SIZE || end_addr < start_addr) { // 检查溢出
    LOG_ERR("addr overflow");
    return -EOVERFLOW;
  }

  memcpy(disk->mem + start_addr, buf, sz);
  return sz;
}

int ramdisk_write_addr(ramdisk_t *disk, const void *buf, size_t addr,
                       size_t sz) {
  if (!disk || !buf) {
    LOG_ERR("invalid disk or buf");
    return -EINVAL;
  }
  if (sz == 0) {
    return 0;
  }

  if (addr >= RAMDISK_MAX_SIZE) {
    LOG_ERR("invalid addr");
    return -EINVAL;
  }

  size_t end_addr = addr + sz;
  if (end_addr > RAMDISK_MAX_SIZE || end_addr < addr) { // 检查溢出
    LOG_ERR("addr overflow");
    return -EOVERFLOW;
  }
  memcpy(disk->mem + addr, buf, sz);
  return sz;
}

int ramdisk_erase_addr(ramdisk_t *disk, size_t addr, size_t sz) {
  if (!disk) {
    LOG_ERR("invalid disk");
    return -EINVAL;
  }
  if (sz == 0) {
    return 0;
  }

  if (addr >= RAMDISK_MAX_SIZE) {
    LOG_ERR("invalid addr");
    return -EINVAL;
  }

  size_t end_addr = addr + sz;
  if (end_addr > RAMDISK_MAX_SIZE || end_addr < addr) { // 检查溢出
    LOG_ERR("addr overflow");
    return -EOVERFLOW;
  }
  memset(disk->mem + addr, 0xFF, sz);
  return sz;
}