#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "logging.h"

#include "fatfs/fatfs_vfs.h"
#include "vfs.h"

#include "vfs_test.h"
#include "vfs_test_fatfs.h"

#define MNT_PATH "/test"
#define FNAME1 "TEST.TXT"
#define FNAME2 "NEWFILE.TXT"
#define FNAME_RNMD "RENAMED.TXT"
#define FNAME_NXIST "NOFILE.TXT"
#define FULL_FNAME1 (MNT_PATH "/" FNAME1)
#define FULL_FNAME2 (MNT_PATH "/" FNAME2)
#define FULL_FNAME_RNMD (MNT_PATH "/" FNAME_RNMD)
#define FULL_FNAME_NXIST (MNT_PATH "/" FNAME_NXIST)
#define DIR_NAME "SOMEDIR"

static const char test_txt[] = "the test file content 123 abc";
static const char test_txt2[] = "another text";
static const char test_txt3[] = "hello world for vfs";

LOG_MODULE_REGISTER(test_fatfs, LOG_LEVEL_DBG);

static fatfs_desc_t fatfs;

static vfs_mount_t _test_vfs_mount = {
    .mount_point = MNT_PATH,
    .fs = &fatfs_file_system,
    .private_data = (void *)&fatfs,
    .dno = 0,
};

static void test_format(void) {
  print_test_result("test_format__format", vfs_format(&_test_vfs_mount) == 0);
}

static void test_mount(void) {
  print_test_result("test_mount__mount", vfs_mount(&_test_vfs_mount) == 0);
  print_test_result("test_mount__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_create(void) {
  int fd;
  print_test_result("test_create__mount", vfs_mount(&_test_vfs_mount) == 0);

  fd = vfs_open(FULL_FNAME1, O_CREAT, 0);
  print_test_result("test_create__open_creat", fd >= 0);
  print_test_result("test_create__close", vfs_close(fd) == 0);

  print_test_result("test_create__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_open(void) {
  int fd;
  print_test_result("test_open__mount", vfs_mount(&_test_vfs_mount) == 0);

  /* try to open file that doesn't exist */
  fd = vfs_open(FULL_FNAME_NXIST, O_RDONLY, 0);
  print_test_result("test_open__open", fd == -ENOENT);

  /* open file with RO, WO and RW access */
  fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
  print_test_result("test_open__open_ro", fd >= 0);
  print_test_result("test_open__close_ro", vfs_close(fd) == 0);
  fd = vfs_open(FULL_FNAME1, O_WRONLY, 0);
  print_test_result("test_open__open_wo", fd >= 0);
  print_test_result("test_open__close_wo", vfs_close(fd) == 0);
  fd = vfs_open(FULL_FNAME1, O_RDWR, 0);
  print_test_result("test_open__open_rw", fd >= 0);
  print_test_result("test_open__close_rw", vfs_close(fd) == 0);

  print_test_result("test_open__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_rw(void) {
  char buf[sizeof(test_txt) + sizeof(test_txt2)];
  int fd;
  ssize_t nr, nw;
  off_t new_pos;

  print_test_result("test_rw__mount", vfs_mount(&_test_vfs_mount) == 0);

  /* try to write to RO file (success if no bytes are actually written) */
  fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
  print_test_result("test_rw__open_ro", fd >= 0);

  nw = vfs_write(fd, test_txt2, sizeof(test_txt2));
  print_test_result("test_rw__write_ro", nw <= 0);
  print_test_result("test_rw__close_ro", vfs_close(fd) == 0);

  /* try to read from WO file (success if no bytes are actually read) */
  fd = vfs_open(FULL_FNAME1, O_WRONLY, 0);
  print_test_result("test_rw__open_wo", fd >= 0);

  nr = vfs_read(fd, buf, sizeof(test_txt));
  print_test_result("test_rw__read_wo", nr <= 0);
  print_test_result("test_rw__close_wo", vfs_close(fd) == 0);

  /* write then read and compare */
  fd = vfs_open(FULL_FNAME1, O_RDWR, 0);
  print_test_result("test_rw__open_rw", fd >= 0);

  nw = vfs_write(fd, test_txt, sizeof(test_txt));
  print_test_result("test_rw__write_rw", nw == sizeof(test_txt));
  new_pos = vfs_lseek(fd, 0, SEEK_SET);
  print_test_result("test_rw__lseek_rw", new_pos == 0);
  memset(buf, 0, sizeof(buf));
  nr = vfs_read(fd, buf, sizeof(buf));
  print_test_result("test_rw__read_rw",
                    nr == sizeof(test_txt) &&
                        strncmp(buf, test_txt, sizeof(test_txt)) == 0);
  print_test_result("test_rw__close_rw", vfs_close(fd) == 0);

  /* write then read on new created file */
  fd = vfs_open(FULL_FNAME2, O_RDWR | O_CREAT, 0);
  print_test_result("test_rw__open_rwc", fd >= 0);

  nw = vfs_write(fd, test_txt3, sizeof(test_txt3));
  print_test_result("test_rw__write_rwc", nw == sizeof(test_txt3));
  new_pos = vfs_lseek(fd, 0, SEEK_SET);
  print_test_result("test_rw__lseek_rwc", new_pos == 0);
  memset(buf, 0, sizeof(buf));
  nr = vfs_read(fd, buf, sizeof(test_txt3));
  print_test_result("test_rw__read_rwc",
                    (nr == sizeof(test_txt3)) &&
                        (strncmp(buf, test_txt3, sizeof(test_txt3)) == 0));
  print_test_result("test_rw__close_rwc", vfs_close(fd) == 0);

  print_test_result("test_rw__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_mkrmdir(void) {
  vfs_DIR dir;

  print_test_result("test_mkrmdir__mount", vfs_mount(&_test_vfs_mount) == 0);

  print_test_result("test_mkrmdir__mkdir",
                    vfs_mkdir(MNT_PATH "/" DIR_NAME, 0) == 0);

  print_test_result("test_mkrmdir__opendir1",
                    vfs_opendir(&dir, MNT_PATH "/" DIR_NAME) == 0);

  print_test_result("test_mkrmdir__closedir", vfs_closedir(&dir) == 0);

  print_test_result("test_mkrmdir__rmdir",
                    vfs_rmdir(MNT_PATH "/" DIR_NAME) == 0);

  print_test_result("test_mkrmdir__opendir1",
                    vfs_opendir(&dir, MNT_PATH "/" DIR_NAME) < 0);

  print_test_result("test_mkrmdir__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_dir(void) {
  vfs_DIR dir;
  vfs_dirent_t entry;
  vfs_dirent_t entry2;

  print_test_result("test_dir__mount", vfs_mount(&_test_vfs_mount) == 0);
  print_test_result("test_dir__opendir", vfs_opendir(&dir, MNT_PATH) == 0);
  print_test_result("test_dir__readdir1", vfs_readdir(&dir, &entry) == 1);
  print_test_result("test_dir__readdir2", vfs_readdir(&dir, &entry2) == 1);

  print_test_result(
      "test_dir__readdir_name",
      ((strncmp(FNAME1, entry.d_name, sizeof(FNAME1)) == 0) &&
       (strncmp(FNAME2, entry2.d_name, sizeof(FNAME2)) == 0)) ||
          ((strncmp(FNAME1, entry2.d_name, sizeof(FNAME1)) == 0) &&
           (strncmp(FNAME2, entry.d_name, sizeof(FNAME2)) == 0)));

  print_test_result("test_dir__readdir3", vfs_readdir(&dir, &entry2) == 0);
  print_test_result("test_dir__closedir", vfs_closedir(&dir) == 0);
  print_test_result("test_dir__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_rename(void) {
  vfs_DIR dir;
  vfs_dirent_t entry;
  vfs_dirent_t entry2;

  print_test_result("test_rename__mount", vfs_mount(&_test_vfs_mount) == 0);

  print_test_result("test_rename__rename",
                    vfs_rename(FULL_FNAME1, FULL_FNAME_RNMD) == 0);

  print_test_result("test_rename__opendir", vfs_opendir(&dir, MNT_PATH) == 0);
  print_test_result("test_rename__readdir1", vfs_readdir(&dir, &entry) == 1);
  print_test_result("test_rename__readdir2", vfs_readdir(&dir, &entry2) == 1);

  print_test_result(
      "test_rename__check_name",
      ((strncmp(FNAME_RNMD, entry.d_name, sizeof(FNAME_RNMD)) == 0) &&
       (strncmp(FNAME2, entry2.d_name, sizeof(FNAME2)) == 0)) ||
          ((strncmp(FNAME_RNMD, entry2.d_name, sizeof(FNAME_RNMD)) == 0) &&
           (strncmp(FNAME2, entry.d_name, sizeof(FNAME2)) == 0)));

  print_test_result("test_rename__readdir3", vfs_readdir(&dir, &entry2) == 0);
  print_test_result("test_rename__closedir", vfs_closedir(&dir) == 0);

  print_test_result("test_rename__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_unlink(void) {
  vfs_DIR dir;
  vfs_dirent_t entry;

  print_test_result("test_unlink__mount", vfs_mount(&_test_vfs_mount) == 0);

  print_test_result("test_unlink__unlink1", vfs_unlink(FULL_FNAME2) == 0);
  print_test_result("test_unlink__unlink2", vfs_unlink(FULL_FNAME_RNMD) == 0);
  print_test_result("test_unlink__opendir", vfs_opendir(&dir, MNT_PATH) == 0);
  print_test_result("test_unlink__readdir", vfs_readdir(&dir, &entry) == 0);
  print_test_result("test_unlink__closedir", vfs_closedir(&dir) == 0);

  print_test_result("test_unlink__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

static void test_fstat(void) {
  int fd;
  struct stat stat_buf;

  print_test_result("test_stat__mount", vfs_mount(&_test_vfs_mount) == 0);

  fd = vfs_open(FULL_FNAME1, O_WRONLY | O_CREAT | O_TRUNC, 0);
  print_test_result("test_stat__open", fd >= 0);
  print_test_result("test_stat__write",
                    vfs_write(fd, test_txt, sizeof(test_txt)) ==
                        sizeof(test_txt));
  print_test_result("test_stat__close", vfs_close(fd) == 0);

  print_test_result("test_stat__direct", vfs_stat(FULL_FNAME1, &stat_buf) == 0);

  fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
  print_test_result("test_stat__open", fd >= 0);
  print_test_result("test_stat__stat", vfs_fstat(fd, &stat_buf) == 0);
  print_test_result("test_stat__close", vfs_close(fd) == 0);
  print_test_result("test_stat__size", stat_buf.st_size == sizeof(test_txt));
  print_test_result("test_stat__umount",
                    vfs_umount(&_test_vfs_mount, false) == 0);
}

void test_vfs_fatfs(void) {
  print_test_banner("FatFS VFS TESTS");

  test_format();
  test_mount();
  test_create();
  test_open();
  test_rw();

  test_mkrmdir();
  test_dir();

  test_rename();
  test_unlink();

  test_fstat();
}