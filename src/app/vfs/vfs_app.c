#include "fatfs/fatfs_vfs.h"
#include "spiffs/spiffs_vfs.h"
#include "vfs.h"

#include "vfs_app.h"

int app_vfs_init() {
  int ret = 0;

  if ((ret = vfs_init())) {
    return ret;
  }
  if ((ret = fatfs_vfs_init())) {
    return ret;
  }
  if ((ret = spiffs_vfs_init())) {
    return ret;
  }
  return 0;
}