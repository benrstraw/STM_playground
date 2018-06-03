/*
 * mysd.c
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#include "mysd.h"

#include <stdlib.h>
#include <string.h>


const char name_head_file[] = "head.bin";

const uint8_t packet_size = 128;
const uint32_t packets_per_file = 33554431; // = floor of (4294967295 / 128)

uint8_t recall_heads(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	UINT bytes_read;
	BYTE head_buff[sizeof msd->r_head + sizeof msd->w_head];

	FRESULT fres = f_lseek(msd->head_file, 0);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	fres = f_read(msd->head_file, head_buff, sizeof head_buff, &bytes_read);
	if(fres != FR_OK || bytes_read < sizeof head_buff)
		return SD_READ_ERR;

	// Building ints from bytes, hooray! Stored big-endian.
	msd->r_head = (uint32_t)head_buff[3] | (uint32_t)head_buff[2] << 8
			| (uint32_t)head_buff[1] << 16 | (uint32_t)head_buff[0] << 24;
	msd->w_head = (uint32_t)head_buff[7] | (uint32_t)head_buff[6] << 8
			| (uint32_t)head_buff[5] << 16 | (uint32_t)head_buff[4] << 24;

	return SD_OK;
}

uint8_t flush_heads(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	BYTE head_buff[sizeof msd->r_head + sizeof msd->w_head];

	// Dumping the head integers into byte arrays through the magic of bit shifting.
	// head_buff: 0-3 = read head, 4-7 = write head, both stored big-endian
	head_buff[0] = (msd->r_head >> 24) & 0xFF;
	head_buff[1] = (msd->r_head >> 16) & 0xFF;
	head_buff[2] = (msd->r_head >> 8) & 0xFF;
	head_buff[3] = msd->r_head & 0xFF;

	head_buff[4] = (msd->w_head >> 24) & 0xFF;
	head_buff[5] = (msd->w_head >> 16) & 0xFF;
	head_buff[6] = (msd->w_head >> 8) & 0xFF;
	head_buff[7] = msd->w_head & 0xFF;

	FRESULT fres = f_lseek(msd->head_file, 0);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	UINT bytes_written;
	fres = f_write(msd->head_file, head_buff, sizeof head_buff, &bytes_written);
	if(fres != FR_OK || bytes_written < sizeof head_buff)
		return SD_WRITE_ERR;

	fres = f_sync(msd->head_file);
	if(fres != FR_OK)
		return SD_SYNC_ERR;

	return SD_OK;
}

uint8_t change_file(uint8_t new_file, mysd* msd) {
	if(new_file == msd->active_file)
		return SD_OK;

	FRESULT fres = f_close(msd->data_file);
	if(fres != FR_OK)
		return SD_CLOSE_ERR;

	msd->active_file = new_file;
	snprintf(msd->active_file_name, sizeof msd->active_file_name, "%u", msd->active_file);

	fres = f_open(msd->data_file, msd->active_file_name, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SD_OPEN_ERR;

	return SD_OK;
}

uint8_t set_active(uint32_t* head, mysd* msd) {
	uint64_t packet_pos = (*head) * packet_size;
	uint32_t packet_offset = packet_pos % packets_per_file;
	uint8_t packet_file = packet_pos / packets_per_file;

	uint8_t res = change_file(packet_file, msd);
	if(res != SD_OK)
		return res;

	// Move file pointer to specified head.
	FRESULT fres = f_lseek(msd->data_file, packet_offset);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	return SD_OK;
}

uint32_t increment_head(uint32_t* head, mysd* msd) {
	if((++(*head)) % (packets_per_file * msd->max_files))
		(*head) = 0;

	return *head;
}

uint8_t sd_init(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	msd->r_head = msd->w_head = 0;

	if(!msd->sd_fs || !msd->head_file || !msd->data_file) {
		msd->sd_fs = calloc(1, sizeof(FATFS));
		msd->head_file = calloc(1, sizeof(FIL));
		msd->data_file = calloc(1, sizeof(FIL));

		if(!msd->sd_fs || !msd->head_file || !msd->data_file) {
			free(msd->head_file);
			free(msd->data_file);
			free(msd->sd_fs);

			msd->sd_fs = NULL;
			msd->head_file = NULL;
			msd->data_file = NULL;

			return SDI_BAD_MALLOC;
		}
	}

	FATFS_UnLinkDriver(USERPath);
	uint8_t ret = FATFS_LinkDriver(&USER_Driver, USERPath);
	if(ret)
		return SDI_FATFS_LINK_ERR;

	FRESULT fres = f_mount(msd->sd_fs, "", 1);
	if(fres != FR_OK)
		return SDI_BAD_MOUNT;

	fres = f_open(msd->head_file, name_head_file, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SD_OPEN_ERR;

	if(f_size(msd->head_file) > 0) {
		recall_heads(msd);
	} else {
		flush_heads(msd);
	}

	msd->active_file = 0;
	snprintf(msd->active_file_name, sizeof msd->active_file_name, "%u", msd->active_file);

	DWORD fre_clust, fre_sect, tot_sect;
	fres = f_getfree("", &fre_clust, &msd->sd_fs);
	tot_sect = (msd->sd_fs->n_fatent - 2) * msd->sd_fs->csize;
	fre_sect = fre_clust * msd->sd_fs->csize;

	// tot_sect - 1 to accommodate the head file
	uint32_t total_packets = (tot_sect - 1) * (512 / 128);
	uint32_t free_packets = (fre_sect - 1) * (512 / 128);

	UNUSED(free_packets);

	msd->max_files = total_packets / packets_per_file;

	fres = f_open(msd->data_file, msd->active_file_name, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SD_OPEN_ERR;

	return SD_OK;
}

void sd_deinit(mysd* msd) {
	if(!msd)
		return;

	if(msd->sd_fs && msd->head_file) {
		flush_heads(msd);
		f_close(msd->head_file);
	}

	if(msd->sd_fs && msd->data_file)
		f_close(msd->data_file);

	if(msd->sd_fs)
		f_mount(NULL, "", 1);

	free(msd->head_file);
	free(msd->data_file);
	free(msd->sd_fs);
}

uint8_t save_data(mysd* msd) {
	uint8_t ret = flush_heads(msd);
	if(ret != SD_OK)
		return ret;

	FRESULT fres = f_sync(msd->data_file);
	if(fres != FR_OK)
		return SD_SYNC_ERR;

	return SD_OK;
}

int16_t get_next_packet(uint8_t** packet_buf, mysd* msd) {
	if(!msd)
		return GP_MSD_NULL;

	set_active(&msd->r_head, msd);

	// Allocate memory block for packet buffer. Caller of the function is responsible for free'ing!
	*packet_buf = malloc(packet_size);
	if(*packet_buf == NULL)
		return GP_BAD_MALLOC;

	memset(*packet_buf, 0, packet_size); // TODO: probably not needed, immediately filled by f_read. Remove for speedup.

	UINT bytesRead = 0;
	FRESULT fres = f_read(msd->data_file, *packet_buf, packet_size, &bytesRead);
	if(fres != FR_OK)// || bytesRead < packet_size) TODO: ?
		return GP_READ_ERR;

	increment_head(&msd->r_head, msd);

	return bytesRead;
}

uint8_t write_next_packet(uint8_t* packet_buf, size_t packet_size, mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	set_active(&msd->w_head, msd);

	UINT bytes_written;
	FRESULT fres = f_write(msd->data_file, packet_buf, packet_size, &bytes_written);
	if(fres != FR_OK || bytes_written < packet_size)
		return SD_WRITE_ERR;

	increment_head(&msd->w_head, msd);

	return SD_OK;
}
