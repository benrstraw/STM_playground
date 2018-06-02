/*
 * mysd.c
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#include "ff.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mysd.h"

// ASCII 23: ETB (end of transmission block) and 30: RS (record separator) codes
const uint8_t packet_sepr[] = { 0x17, 0x1e };
const char name_head_file[] = "head.bin";
const char name_data_file[] = "data.bin";
const uint64_t data_filesize = 64;
const size_t read_buffer_size = 64; // probably 512 for flight?

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

	FRESULT fres = f_mount(msd->sd_fs, "", 1);
	if(fres != FR_OK)
		return SDI_BAD_MOUNT;

	fres = f_open(msd->head_file, name_head_file, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SDI_OPEN_ERR;

	if(f_size(msd->head_file) > 0) {
		recall_heads(msd);
	} else {
		flush_heads(msd);
	}

	fres = f_open(msd->data_file, name_data_file, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SDI_OPEN_ERR;

	if(f_size(msd->data_file) != data_filesize) {
		fres = f_truncate(msd->data_file);
		if(fres != FR_OK)
			return SDI_TRUNC_ERR;

		fres = f_expand(msd->data_file, data_filesize, 1);
		if(fres != FR_OK)
			return SDI_EXPAND_ERR;

		fres = f_sync(msd->data_file);
		if(fres != FR_OK)
			return SD_SYNC_ERR;
	}

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

	// Always R then W.
	// r_head is up first, with 8 bytes to read.
	msd->r_head = (uint64_t)head_buff[7] | (uint64_t)head_buff[6] << 8
			| (uint64_t)head_buff[5] << 16 | (uint64_t)head_buff[4] << 24
			| (uint64_t)head_buff[3] << 32 | (uint64_t)head_buff[2] << 40
			| (uint64_t)head_buff[1] << 48 | (uint64_t)head_buff[0] << 56;
	// Same with w_head.
	msd->w_head = (uint64_t)head_buff[15] | (uint64_t)head_buff[14] << 8
			| (uint64_t)head_buff[13] << 16 | (uint64_t)head_buff[12] << 24
			| (uint64_t)head_buff[11] << 32 | (uint64_t)head_buff[10] << 40
			| (uint64_t)head_buff[9] << 48 | (uint64_t)head_buff[8] << 56;

	return SD_OK;
}

uint8_t flush_heads(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	BYTE head_buff[sizeof msd->r_head + sizeof msd->w_head];

	head_buff[0] = (msd->r_head >> 56) & 0xFF;
	head_buff[1] = (msd->r_head >> 48) & 0xFF;
	head_buff[2] = (msd->r_head >> 40) & 0xFF;
	head_buff[3] = (msd->r_head >> 32) & 0xFF;
	head_buff[4] = (msd->r_head >> 24) & 0xFF;
	head_buff[5] = (msd->r_head >> 16) & 0xFF;
	head_buff[6] = (msd->r_head >> 8) & 0xFF;
	head_buff[7] = msd->r_head & 0xFF;

	head_buff[8] = (msd->w_head >> 56) & 0xFF;
	head_buff[9] = (msd->w_head >> 48) & 0xFF;
	head_buff[10] = (msd->w_head >> 40) & 0xFF;
	head_buff[11] = (msd->w_head >> 32) & 0xFF;
	head_buff[12] = (msd->w_head >> 24) & 0xFF;
	head_buff[13] = (msd->w_head >> 16) & 0xFF;
	head_buff[14] = (msd->w_head >> 8) & 0xFF;
	head_buff[15] = msd->w_head & 0xFF;

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

void advance_head(uint64_t* head, int32_t offset, mysd* msd) {
	if(*head >= data_filesize)
		*head = 0; // wtf?! this shouldn't happen

	uint64_t dte = data_filesize - *head;
	if(offset < dte)
		*head += offset;
	else
		*head = (offset - dte);
	flush_heads(msd);
}

static int32_t get_next_packet_size(mysd* msd) {
	uint8_t header_found = 0;

	uint8_t next_found = 0;
	uint32_t next_begin = 0;

	uint8_t readBuf[read_buffer_size];
	UINT bytesRead = 0;
	FRESULT fres;

	// while loop to find beginning of the next header
	while(!next_found) {
		fres = f_read(msd->data_file, readBuf, sizeof readBuf, &bytesRead);
		if(fres != FR_OK)
			return GP_FRES_ERR;
		else if(bytesRead == 0) {
			fres = f_lseek(msd->data_file, 0);
			continue;
		}

		uint32_t block_start = (f_tell(msd->data_file) - bytesRead);

		for(int i = 0; i < bytesRead; i++) {
			if(block_start + i == msd->w_head && msd->r_head != msd->w_head)
				return GP_RW_INTERSECT; // should never see this, W head should immediately follow a p_sepr

			if(readBuf[i] == packet_sepr[header_found++]) {
				if(header_found == 1) {
					next_begin = block_start + i;
				} else if(header_found == sizeof packet_sepr) {
					next_found = 1;
					break;
				}
			} else {
				header_found = 0;
			}
		}
	}

	if(next_begin < msd->r_head)
		return (data_filesize - msd->r_head) + next_begin;
	else
		return next_begin - msd->r_head;
}

int32_t get_next_packet(uint8_t** packet_buf, mysd* msd) {
	if(!msd)
		return GP_MSD_NULL;

	FRESULT fres = f_lseek(msd->data_file, msd->r_head);
	if(fres != FR_OK)
		return GP_SEEK_ERR;

	int32_t p_size = get_next_packet_size(msd);
	if(p_size < 0)
		return p_size; // less than zero, error out without malloc-ing
	else if(p_size == 0) { // if size == 0 then the R head is on the start of a packet separator
		do {
			advance_head(&(msd->r_head), sizeof packet_sepr, msd); // advance by 2 to get past the packet separator
			p_size = get_next_packet_size(msd);
		} while (p_size == 0);
		if(p_size < 0)
			return p_size;
	}

	fres = f_lseek(msd->data_file, msd->r_head);
	if(fres != FR_OK)
		return GP_SEEK_ERR;

	*packet_buf = malloc(p_size);
	if(*packet_buf == NULL)
		return GP_BAD_MALLOC;

	memset(*packet_buf, 0, p_size);

	UINT bytesRead = 0;
	fres = f_read(msd->data_file, *packet_buf, p_size, &bytesRead);
	if(fres != FR_OK)
		return GP_READ_ERR;
	else if(bytesRead < p_size) {
		uint32_t overflow = p_size - bytesRead;

		fres = f_lseek(msd->data_file, 0);
		if(fres != FR_OK)
				return GP_SEEK_ERR;

		fres = f_read(msd->data_file, *packet_buf + bytesRead, overflow, &bytesRead);
		if(fres != FR_OK)
				return GP_READ_ERR;
		else if(bytesRead < overflow)
			return (p_size - overflow) + bytesRead;
	}

	msd->r_head = f_tell(msd->data_file) + sizeof packet_sepr;
	if(msd->r_head >= data_filesize)
		msd->r_head = msd->r_head - data_filesize;
	flush_heads(msd);

	return p_size;
}

uint8_t write_packet(uint8_t* packet_buf, size_t packet_size, mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	FRESULT fres = f_lseek(msd->data_file, msd->w_head);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	UINT bytes_written;
	uint64_t dte = data_filesize - msd->w_head;
	if(packet_size <= dte) {
		// This branch won't wrap around, so if R < W then we're fine, since it's safe on the other
		// side of W. If R > W we could still be fine if the write size isn't big enough to reach R.
		// If R > W and W + packet_size is >= R, we have an intersection and must return.
		if(msd->r_head > msd->w_head && msd->w_head + (packet_size + sizeof packet_sepr) >= msd->r_head)
			return SD_FILE_FULL;
		if((packet_size + sizeof packet_sepr) > dte && ((packet_size + sizeof packet_sepr) - dte) >= msd->r_head)
			return SD_FILE_FULL;

		fres = f_write(msd->data_file, packet_buf, packet_size, &bytes_written);
		if(fres != FR_OK || bytes_written < packet_size)
			return SD_WRITE_ERR;
	} else {
		// This branch is guaranteed to wrap around, so if R > W, that means R will be overwritten,
		// since W will fill to the end then loop to zero. Likewise, if the data to write minus the
		// distance to the end is greater than R, that means even after the wrap around they'll hit.
		if(msd->r_head > msd->w_head || ((packet_size + sizeof packet_sepr) - dte) >= msd->r_head)
			return SD_FILE_FULL;

		fres = f_write(msd->data_file, packet_buf, dte, &bytes_written);
		if(fres != FR_OK || bytes_written < dte)
			return SD_WRITE_ERR;

		fres = f_lseek(msd->data_file, 0);
		if(fres != FR_OK)
			return SD_SEEK_ERR;

		fres = f_write(msd->data_file, packet_buf + dte, packet_size - dte, &bytes_written);
		if(fres != FR_OK || bytes_written < packet_size - dte)
			return SD_WRITE_ERR;
	}

	for(int i = 0; i < sizeof packet_sepr; i++) {
		if(f_tell(msd->data_file) >= data_filesize) {
			fres = f_lseek(msd->data_file, 0);
			if(fres != FR_OK)
				return SD_SEEK_ERR;
		}

		fres = f_write(msd->data_file, packet_sepr + i, 1, &bytes_written);
		if(fres != FR_OK || bytes_written < 1)
			return SD_WRITE_ERR;
	}

	fres = f_sync(msd->data_file);
	if(fres != FR_OK)
		return SD_SYNC_ERR;

	msd->w_head = f_tell(msd->data_file);
	if(msd->w_head >= data_filesize)
		msd->w_head = msd->w_head - data_filesize;
	flush_heads(msd);

	return SD_OK;
}
