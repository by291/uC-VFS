#include <fcntl.h>

#include "logging.h"

#include "fatfs/fatfs_vfs.h"
#include "spiffs/spiffs_vfs.h"
#include "vfs.h"

#include "vfs_test.h"
#include "vfs_test_inter.h"

#define MNT_FATFS "/mnt/fatfs"
#define MNT_SPIFFS "/mnt/spiffs"

#define FNAME_TEST "TEST.txt"
#define FULL_FNAME_BEFORE_RENAME (MNT_FATFS "/" FNAME_TEST)
#define FULL_FNAME_AFTER_RENAME (MNT_SPIFFS "/" FNAME_TEST)

#define FNAME_R_FATFS "R_FATFS"
#define FNAME_W_SPIFFS "W_SPIFFS"
#define FNAME_R_SPIFFS "R_SPIFFS"
#define FNAME_W_FATFS "W_FATFS"
#define FULL_FNAME_R_FATFS (MNT_FATFS "/" FNAME_R_FATFS)
#define FULL_FNAME_W_SPIFFS (MNT_SPIFFS "/" FNAME_W_SPIFFS)
#define FULL_FNAME_R_SPIFFS (MNT_SPIFFS "/" FNAME_R_SPIFFS)
#define FULL_FNAME_W_FATFS (MNT_FATFS "/" FNAME_W_FATFS)

static const char c_before_rename[] = "content test.txt";
static const char c_r_fatfs[] = "content read fatfs";
static const char c_r_spiffs[] = "content read spiffs";

LOG_MODULE_REGISTER(test_inter, LOG_LEVEL_DBG);

static fatfs_desc_t fatfs_desc;
static vfs_mount_t _test_fatfs_mount = {
    .mount_point = MNT_FATFS,
    .fs = &fatfs_file_system,
    .private_data = (void *)&fatfs_desc,
    .dno = 0,
};

static spiffs_desc_t spiffs_desc;
static vfs_mount_t _test_spiffs_mount = {
    .mount_point = MNT_SPIFFS,
    .fs = &fatfs_file_system,
    .private_data = (void *)&spiffs_desc,
    .dno = 1,
};

// format newly created filesystems
static void test_inter_format(void) {
  print_test_result("test_inter_format__format_fatfs",
                    vfs_format(&_test_fatfs_mount) == 0);
  print_test_result("test_inter_format__format_spiffs",
                    vfs_format(&_test_spiffs_mount) == 0);
}

static void test_inter_make_content(void) {
  print_test_result("test_inter_make_content__mount_fatfs",
                    vfs_mount(&_test_fatfs_mount) == 0);
  print_test_result("test_inter_make_content__mount_spiffs",
                    vfs_mount(&_test_spiffs_mount) == 0);

  // make content for rename
  int fd = vfs_open(FULL_FNAME_BEFORE_RENAME, O_CREAT | O_WRONLY, 0);
  print_test_result("test_inter_make_content__open_before_rename", fd >= 0);
  int nw = vfs_write(fd, c_before_rename, sizeof(c_before_rename));
  print_test_result("test_inter_make_content__write_before_rename",
                    nw == sizeof(c_before_rename));
  print_test_result("test_inter_make_content__close_before_rename",
                    vfs_close(fd) == 0);

  // make content for r_fatf
  fd = vfs_open(FULL_FNAME_R_FATFS, O_CREAT | O_WRONLY, 0);
  print_test_result("test_inter_make_cotent__open_r_fatfs", fd >= 0);
  nw = vfs_write(fd, c_r_fatfs, sizeof(c_r_fatfs));
  print_test_result("test_inter_make_content__write_r_fatfs",
                    nw == sizeof(c_r_fatfs));
  print_test_result("test_inter_make_content__close_r_fatfs",
                    vfs_close(fd) == 0);

  // make content_for r_spiffs
  fd = vfs_open(FULL_FNAME_R_SPIFFS, O_CREAT | O_WRONLY, 0);
  print_test_result("test_inter_make_content__open_r_spiffs", fd >= 0);
  nw = vfs_write(fd, c_r_spiffs, sizeof(c_r_spiffs));
  print_test_result("test_inter_make_content__write_r_spiffs",
                    nw == sizeof(c_r_spiffs));
  print_test_result("test_inter_make_content__close_r_spiffs",
                    vfs_close(fd) == 0);

  print_test_result("test_inter_make_content__umount_fatfs",
                    vfs_umount(&_test_fatfs_mount, false) == 0);
  print_test_result("test_inter_make_content__umount_spiffs",
                    vfs_umount(&_test_spiffs_mount, false) == 0);
}

static void test_inter_rw(void) {
  print_test_result("test_inter_rw__mount_fatfs",
                    vfs_mount(&_test_fatfs_mount) == 0);
  print_test_result("test_inter_rw__mount_spiffs",
                    vfs_mount(&_test_spiffs_mount) == 0);

  char buf[100];

  // from fatfs to spiffs
  int rfd = vfs_open(FULL_FNAME_R_FATFS, O_RDONLY, 0);
  print_test_result("test_inter_rw__open_r_fatfs", rfd >= 0);
  int wfd = vfs_open(FULL_FNAME_W_SPIFFS, O_RDWR | O_CREAT | O_TRUNC, 0);
  print_test_result("test_inter_rw__open_w_spiffs", wfd >= 0);
  memset(buf, 0, sizeof(buf));
  int nr = vfs_read(rfd, buf, sizeof(buf));
  print_test_result("test_inter_rw__read_r_fatfs", nr == sizeof(c_r_fatfs));
  int nw = vfs_write(wfd, buf, nr);
  print_test_result("test_inter_rw__write_w_spiffs", nw == nr);
  off_t pos = vfs_lseek(wfd, 0, SEEK_SET);
  print_test_result("test_inter_rw__lseek_w_spiffs", pos == 0);
  memset(buf, 0, sizeof(buf));
  int check_nr = vfs_read(wfd, buf, sizeof(buf));
  print_test_result("test_inter_rw__check_w_spiffs",
                    check_nr == sizeof(c_r_fatfs) &&
                        strncmp(buf, c_r_fatfs, sizeof(c_r_fatfs)) == 0);
  print_test_result("test_inter_rw__close_r_fatfs", vfs_close(rfd) == 0);
  print_test_result("test_inter_rw__close_w_spiffs", vfs_close(wfd) == 0);

  // from spiffs to fatfs
  rfd = vfs_open(FULL_FNAME_R_SPIFFS, O_RDONLY, 0);
  print_test_result("test_inter_rw__open_r_spiffs", rfd >= 0);
  wfd = vfs_open(FULL_FNAME_W_FATFS, O_RDWR | O_CREAT | O_TRUNC, 0);
  print_test_result("test_inter_rw__open_w_fatfs", wfd >= 0);
  memset(buf, 0, sizeof(buf));
  nr = vfs_read(rfd, buf, sizeof(buf));
  print_test_result("test_inter_rw__read_r_spiffs", nr == sizeof(c_r_spiffs));
  nw = vfs_write(wfd, buf, nr);
  print_test_result("test_inter_rw__write_w_fatfs", nw == nr);
  pos = vfs_lseek(wfd, 0, SEEK_SET);
  print_test_result("test_inter_rw__lseek_w_fatfs", pos == 0);
  memset(buf, 0, sizeof(buf));
  check_nr = vfs_read(wfd, buf, sizeof(buf));
  print_test_result("test_inter_rw__check_w_fatfs",
                    check_nr == sizeof(c_r_spiffs) &&
                        strncmp(buf, c_r_spiffs, sizeof(c_r_spiffs)) == 0);
  print_test_result("test_inter_rw__close_r_spiffs", vfs_close(rfd) == 0);
  print_test_result("test_inter_rw__close_w_fatfs", vfs_close(wfd) == 0);

  print_test_result("test_inter_rw__umount_fatfs",
                    vfs_umount(&_test_fatfs_mount, false) == 0);
  print_test_result("test_inter_rw__umount_spiffs",
                    vfs_umount(&_test_spiffs_mount, false) == 0);
}

void test_vfs_inter() {
  print_test_banner("Inter FS Operation Tests");

  test_inter_format();
  test_inter_make_content();

  test_inter_rw();
}