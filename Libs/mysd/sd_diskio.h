/*
 * sd_diskio.h
 *
 *  Created on: Jun 10, 2018
 *      Author: Ben
 */

#ifndef SD_DISKIO_SD_DISKIO_H_
#define SD_DISKIO_SD_DISKIO_H_

#include "stdint.h"

/* Generic command (Used by FatFs) */
#define CTRL_SYNC			0	/* Complete pending write process (needed at FF_FS_READONLY == 0) */
#define GET_SECTOR_COUNT	1	/* Get media size (needed at FF_USE_MKFS == 1) */
#define GET_SECTOR_SIZE		2	/* Get sector size (needed at FF_MAX_SS != FF_MIN_SS) */
#define GET_BLOCK_SIZE		3	/* Get erase block size (needed at FF_USE_MKFS == 1) */
#define CTRL_TRIM			4	/* Inform device that the data on the block of sectors is no longer used (needed at FF_USE_TRIM == 1) */

/* Results of Disk Functions */
typedef enum {
	RES_OK = 0, /* 0: Successful */
	RES_ERROR, /* 1: R/W Error */
	RES_WRPRT, /* 2: Write Protected */
	RES_NOTRDY, /* 3: Not Ready */
	RES_PARERR /* 4: Invalid Parameter */
} DRESULT;

uint8_t sdio_initialize();
uint8_t sdio_status();
DRESULT sdio_read(uint8_t* buff, uint32_t sector, uint32_t count);
DRESULT sdio_write(const uint8_t* buff, uint32_t sector, uint32_t count);
DRESULT sdio_ioctl(uint8_t cmd, void* buff);

#endif /* SD_DISKIO_SD_DISKIO_H_ */
