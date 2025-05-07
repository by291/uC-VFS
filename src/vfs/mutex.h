#ifndef UC_VFS_MUTEX_H
#define UC_VFS_MUTEX_H

#include "os.h"

typedef OS_MUTEX mutex_t;

int mutex_init(mutex_t *mtx);

int mutex_lock(mutex_t *mtx);

int mutex_unlock(mutex_t *mtx);

#endif