#include "mutex.h"
#include "errno.h"
#include "logging.h"

LOG_MODULE_REGISTER(mutex, LOG_LEVEL_DBG);

int mutex_init(mutex_t *mtx) {
  OS_ERR err = OS_ERR_NONE;

  OSMutexCreate(mtx, NULL, &err);
  if (err != OS_ERR_NONE) {
    LOG_DBG("MutexCreate=%d", err);
    return -EAGAIN;
  }
  return 0;
}

int mutex_lock(mutex_t *mtx) {
  OS_ERR err = OS_ERR_NONE;

  OSMutexPend(mtx, 0, OS_OPT_PEND_BLOCKING, NULL, &err);

  if (err != OS_ERR_NONE) {
    LOG_DBG("MutexPend=%d", err);
    return -EBUSY;
  }
  return 0;
}

int mutex_unlock(mutex_t *mtx) {
  OS_ERR err = OS_ERR_NONE;

  OSMutexPost(mtx, OS_OPT_POST_NONE, &err);

  if (err != OS_ERR_NONE) {
    LOG_DBG("MutexPost=%d", err);
    return -EBUSY;
  }
  return 0;
}