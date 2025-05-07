#include <lib_def.h>
#include <lib_mem.h>

#include "errno.h"
#include "inttypes.h"
#include "logging.h"
#include "mem.h"

LOG_MODULE_REGISTER(mem, LOG_LEVEL_DBG);

int mem_pool_create(mem_pool_t *pool, size_t blk_size, size_t align,
                    size_t n_init) {
  LIB_ERR err = LIB_ERR_NONE;

  Mem_DynPoolCreate(NULL, pool, NULL, blk_size, align, n_init,
                    LIB_MEM_BLK_QTY_UNLIMITED, &err);
  if (err != LIB_ERR_NONE) {
    LOG_DBG("DynPoolCreate=%d", err);
    return -EINVAL;
  }
  return 0;
}

void *mem_pool_alloc(mem_pool_t *pool) {
  LIB_ERR err = LIB_ERR_NONE;

  void *blk = Mem_DynPoolBlkGet(pool, &err);
  if (err != LIB_ERR_NONE) {
    LOG_DBG("DynPoolGet=%d", err);
    return NULL;
  }
  return blk;
}

void mem_pool_free(mem_pool_t *pool, void *blk) {
  LIB_ERR err;

  Mem_DynPoolBlkFree(pool, blk, &err);
  if (err != LIB_ERR_NONE) {
    LOG_DBG("DynPoolFree=%d", err);
    return;
  }
}
