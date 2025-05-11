/*
 * Copyright (c) 2022 Lucas Dietrich <ld.adecy@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib_mem.h>
#include <os.h>
#include <osal.h>

#include <Source/clk.h>

#include <board.h>

#include <app.h>

#define OS_TICKS_PER_SEC OS_CFG_TICK_RATE_HZ

#define SYS_MAIN_TASK_STACK_SIZE 0x400
#define SYS_MAIN_TASK_PRIORITY 0

static OS_TCB sys_main_thread;
static CPU_STK sys_main_stack[SYS_MAIN_TASK_STACK_SIZE];

void sys_task(void *p_arg) {
  /* Configure systick once */
  if ((SysTick->CTRL & SysTick_CTRL_TICKINT_Msk) == 0u) {
    OS_CPU_SysTickInitFreq(FCPU);
  }

  app_init();

  app_task(p_arg);

  for (;;) {
    /* We should never get here */
  }
}

int main(void) {
  OS_ERR uce;

  CPU_Init();
  Mem_Init();

  OSInit(&uce);

  OSTaskCreate(&sys_main_thread, "sys_main", sys_task, (void *)0,
               SYS_MAIN_TASK_PRIORITY, &sys_main_stack[0], 10,
               sizeof(sys_main_stack) / 4u, 5, 10, (void *)0,
               OS_OPT_TASK_STK_CHK + OS_OPT_TASK_STK_CLR, &uce);

  OSStart(&uce);

  __builtin_unreachable();
}