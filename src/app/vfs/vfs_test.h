#ifndef UC_VFS_VFS_TEST_H
#define UC_VFS_VFS_TEST_H

#include "logging.h"

static const struct log_module _log_module;

static inline void print_test_banner(const char *banner) {
  LOG_INF("======================== %s ========================", banner);
}

static inline void print_test_result(const char *test_name, int ok) {
  LOG_INF("%s: [%s]", test_name, ok ? "OK" : "FAILED");
}

#endif