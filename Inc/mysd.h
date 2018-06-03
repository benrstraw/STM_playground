/*
 * mysd.h
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#ifndef MYSD_MYSD_H_
#define MYSD_MYSD_H_

#define SDI_BAD_MOUNT			1
#define SDI_HEAD_READ_ERR		2
#define SDI_BAD_MALLOC			3
#define SDI_FATFS_LINK_ERR		4
#define SDI_FATFS_UNLINK_ERR	4

#define SD_OK			0
#define SD_MSD_NULL		1
#define SD_SEEK_ERR		2
#define SD_READ_ERR		3
#define SD_WRITE_ERR	4
#define SD_SYNC_ERR		5
#define SD_CLOSE_ERR	6
#define SD_OPEN_ERR		7
#define SD_FILE_FULL	8

#define GP_NOT_FOUND	-1
#define GP_FRES_ERR		-2
#define GP_BAD_MALLOC	-3
#define GP_MSD_NULL		-4
#define GP_SEEK_ERR		-5
#define GP_READ_ERR		-6
#define GP_RW_INTERSECT	-7

#include <stdint.h>
#include "fatfs.h"

typedef struct {
	// Always R then W.
	uint32_t r_head, w_head;

	uint8_t max_files;
	uint8_t active_file;
	char active_file_name[4];

	FATFS* sd_fs;
	FIL* head_file;
	FIL* data_file;
} mysd;

uint8_t sd_init(mysd* msd);
void sd_deinit(mysd* msd);

uint8_t save_data(mysd* msd);

uint32_t increment_head(uint32_t* head, mysd* msd);

int16_t get_next_packet(uint8_t** packet_buf, mysd* msd);
uint8_t write_next_packet(uint8_t* packet_buf, size_t packet_size, mysd* msd);

#endif /* MYSD_MYSD_H_ */
