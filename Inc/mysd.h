/*
 * mysd.h
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#ifndef MYSD_MYSD_H_
#define MYSD_MYSD_H_

#define SDI_BAD_MOUNT		1
#define SDI_OPEN_ERR		2
#define SDI_HEAD_READ_ERR	3
#define SDI_EXPAND_ERR		4
#define SDI_TRUNC_ERR		5
#define SDI_BAD_MALLOC		6

#define SD_OK			0
#define SD_SEEK_ERR		1
#define SD_READ_ERR		2
#define SD_WRITE_ERR	3
#define SD_SYNC_ERR		4
#define SD_MSD_NULL		5
#define SD_FILE_FULL	6

#define GP_NOT_FOUND	-1
#define GP_FRES_ERR		-2
#define GP_BAD_MALLOC	-3
#define GP_MSD_NULL		-4
#define GP_SEEK_ERR		-5
#define GP_READ_ERR		-6
#define GP_RW_INTERSECT	-7

typedef struct {
	// Always R then W.
	uint64_t r_head, w_head;

	FATFS* sd_fs;
	FIL* head_file;
	FIL* data_file;
} mysd;

uint8_t sd_init(mysd* msd);
void sd_deinit(mysd* msd);

uint8_t recall_heads(mysd* msd);
uint8_t flush_heads(mysd* msd);
void advance_head(uint64_t* head, int32_t offset, mysd* msd);

int32_t get_next_packet(uint8_t** packet_buf, mysd* msd);
uint8_t write_packet(uint8_t* packet_buf, size_t packet_size, mysd* msd);

#endif /* MYSD_MYSD_H_ */
