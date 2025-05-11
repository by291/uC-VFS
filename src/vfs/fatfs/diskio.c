/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ramdisk.h"

#include "ff.h" /* Obtains integer types */

#include "diskio.h" /* Declarations of disk functions */

extern ramdisk_t *fat_disk;

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS
disk_status(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  return 0;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS
disk_initialize(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv,  /* Physical drive nmuber to identify the drive */
                  BYTE *buff, /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  UINT count    /* Number of sectors to read */
) {
  int res =
      ramdisk_read(fat_disk, buff, sector, 0, count * CONFIG_RAM_SEC_SIZE);
  if (res < 0) {
    return RES_ERROR;
  }
  return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE pdrv, /* Physical drive nmuber to identify the drive */
                   const BYTE *buff, /* Data to be written */
                   LBA_t sector,     /* Start sector in LBA */
                   UINT count        /* Number of sectors to write */
) {
  int res =
      ramdisk_write(fat_disk, buff, sector, 0, count * CONFIG_RAM_SEC_SIZE);
  if (res < 0) {
    return RES_ERROR;
  }
  return RES_OK;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, /* Physical drive nmuber (0..) */
                   BYTE cmd,  /* Control code */
                   void *buff /* Buffer to send/receive control data */
) {
  switch (cmd) {
  case CTRL_SYNC: // 同步命令(对于RAMDisk不需要操作)
    return RES_OK;

  case GET_SECTOR_COUNT: // 获取总扇区数
    *((LBA_t *)buff) = CONFIG_RAM_N_SECS;
    return RES_OK;

  case GET_SECTOR_SIZE: // 获取扇区大小
    *((WORD *)buff) = CONFIG_RAM_SEC_SIZE;
    return RES_OK;

  case GET_BLOCK_SIZE: // 获取擦除块大小(对于RAMDisk无意义)
    *((DWORD *)buff) = 1;
    return RES_OK;

  default:
    return RES_PARERR;
  }
}
