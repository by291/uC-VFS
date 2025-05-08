/*
 * Copyright (c) 2022 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app.h>

#include <board.h>
#include <osal.h>
#include <device.h>

#include <Source/clk.h>

#if defined(CONFIG_FS)
#	include <Source/fs.h>
#	include <Source/fs_file.h>
#	include <fs/fs_app.h>
#endif

#include "logging.h"

LOG_MODULE_REGISTER(app_net, LOG_LEVEL_DBG);


#if defined(serial_console) && defined(serial_log)
int _write(int fd, char *buf, int count)
{
	for (; count != 0; --count) {
		serial_poll_out(serial_console, *buf++);
	}
	return count;
}
#endif

#if defined(sys_timer0)
void timer0_app_handler(const struct device *dev, void *user_data)
{
	// serial_poll_out(serial_console, '!');
}
#endif

#if defined(CONFIG_FS)

CPU_CHAR *fs_path = "ram:0:\\test.txt";
CPU_CHAR fs_wr_buf[0x100] = "Hello World\n";
CPU_SIZE_T fs_wr_len;
CPU_CHAR fs_rd_buf[0x100];
CPU_BOOLEAN fs_rd_match;

static void fs_file_create_n_write(void)
{
	FS_ERR err;
	FS_FILE *file;
	file = FSFile_Open(fs_path,
		    FS_FILE_ACCESS_MODE_CREATE |
		    FS_FILE_ACCESS_MODE_WR |
		    FS_FILE_ACCESS_MODE_TRUNCATE,
		    &err
	);
	LOG_DBG("FSFile_Open file=%p err=%d", file, err);

	fs_wr_len = MIN(strlen(fs_wr_buf), sizeof(fs_wr_buf));
	CPU_SIZE_T written = FSFile_Wr(file, fs_wr_buf, fs_wr_len, &err);
	LOG_DBG("FSFile_Wr written=%d err=%d", written, err);

	FSFile_Close(file, &err);
	LOG_DBG("FSFile_Close err=%d", err);
}

static void fs_file_read()
{
	FS_ERR err;
	FS_FILE *file;
	file = FSFile_Open(fs_path,
		    FS_FILE_ACCESS_MODE_RD,
		    &err
	);
	LOG_DBG("FSFile_Open file=%p err=%d", file, err);

	CPU_SIZE_T read = FSFile_Rd(file, fs_rd_buf, fs_wr_len, &err);
	LOG_DBG("FSFile_Rd read=%d err=%d", read, err);

	/* Compare read data with written data */
	fs_rd_match = OS_TRUE;
	for (CPU_CHAR *w = fs_wr_buf, *r = fs_rd_buf;
	     w - fs_wr_buf < fs_wr_len;
	     w++, r++) {
		if (*w != *r) {
			fs_rd_match = OS_FALSE;
			break;
		}
	}

	FSFile_Close(file, &err);
	LOG_DBG("FSFile_Close err=%d", err);

	LOG_DBG("FS file %s read matches read = %u", fs_path, 
		(uint32_t)fs_rd_match);
}

#endif /* CONFIG_FS */

#define TCP_SERVER_TASK_PRIORITY 10
void tcpsrv(void *arg);
OS_TCB  tcpsrv_thread;
CPU_STK tcpsrv_stack[0x400u];

void app_init(void)
{
#if defined(CONFIG_NETWORKING)
	/* Networking stack must be initialized after OS has beenstarted */
	app_net_init();
#endif

	serial_init(serial_console);
	serial_init(serial_log);

#if defined(CONFIG_OS_CLK)
	/* TODO That's terrible, get rid of this savage task */
	CLK_ERR err;
	Clk_OS_Init(&err); 
	LOG_INF("Clk_OS_Init err=%d", err);
#endif /* CONFIG_OS_CLK */

#if defined(CONFIG_FS)

	CPU_BOOLEAN fs = App_FS_Init();
	LOG_INF("App_FS_Init -> %d", fs);

	fs_file_create_n_write();
	fs_file_read();
#endif /* CONFIG_FS */

#if defined(sys_timer0)
	timer_set_callback(sys_timer0, timer0_app_handler, NULL);
	timer_start(sys_timer0, 25000000lu);
#endif /* sys_timer0 */

}

void app_task(void *p_arg)
{
	LOG_INF("App task starting");

	unsigned char c;
	for (;;) {
		if (serial_poll_in(serial_console, &c) == 0) {
			serial_poll_out(serial_console, c);
			serial_poll_out(serial_log, c);
		}
		
		k_sleep(K_MSEC(100u));

#if defined(CONFIG_OS_CLK)
		CLK_TS_SEC ts = Clk_GetTS();
		LOG_INF("Clk_GetTS -> %u s", ts);
#endif /* CONFIG_OS_CLK */


#if defined(NETWORKING_UDP_CLIENT)
		App_UDP_Client("192.168.10.216");

		k_sleep(K_SECONDS(5));
#endif
	}
}

#if defined(NETWORKING_TCP_SERVER)
void tcpsrv(void *arg)
{
	ARG_UNUSED(arg);

	for (;;) {
		App_TCP_ServerIPv4();
	}
}
#endif /* NETWORKING_TCP_SERVER */