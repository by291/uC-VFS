#include <app.h>

#include <board.h>
#include <device.h>
#include <osal.h>

#include <Source/clk.h>

#include "logging.h"

#include "vfs/vfs_app.h"
#include "vfs/vfs_test_fatfs.h"
#include "vfs/vfs_test_inter.h"
#include "vfs/vfs_test_spiffs.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#if defined(serial_console) && defined(serial_log)
int _write(int fd, char *buf, int count) {
  for (; count != 0; --count) {
    serial_poll_out(serial_console, *buf++);
  }
  return count;
}
#endif

#if defined(sys_timer0)
void timer0_app_handler(const struct device *dev, void *user_data) {
  // serial_poll_out(serial_console, '!');
}
#endif

void app_init(void) {

  serial_init(serial_console);
  serial_init(serial_log);

  int ret = app_vfs_init();
  LOG_DBG("app_vfs_init=%d", ret);

#if defined(sys_timer0)
  timer_set_callback(sys_timer0, timer0_app_handler, NULL);
  timer_start(sys_timer0, 25000000lu);
#endif /* sys_timer0 */
}

void app_task(void *p_arg) {
  LOG_INF("App task starting");

  test_vfs_fatfs();
  test_vfs_spiffs();
  test_vfs_inter();
}