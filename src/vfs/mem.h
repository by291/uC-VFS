#ifndef UC_VFS_SLAB_H
#define UC_VFS_SLAB_H

#include <lib_mem.h>

#include "inttypes.h"

typedef MEM_DYN_POOL mem_pool_t;

int mem_pool_create(mem_pool_t *pool, size_t blk_size, size_t align,
                    size_t n_init);

void *mem_pool_alloc(mem_pool_t *pool);

void mem_pool_free(mem_pool_t *pool, void *blk);

#endif