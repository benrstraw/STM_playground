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
const char packet_sepr[] = { 0x17, 0x1e };
const char name_head_file[] = "head.bin";
const char name_data_file[] = "data.bin";

// Always R then W.
uint64_t r_head = 0;
uint64_t w_head = 0;

FATFS sd_fs;
FIL head_file;
FIL data_file;
FRESULT fres;

uint8_t sd_init() {
	fres = f_mount(&sd_fs, "", 1);
	if(fres != FR_OK)
		return SDI_BAD_MOUNT;

	fres = f_open(&head_file, name_head_file, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
	if(fres != FR_OK)
		return SDI_OPEN_ERR;

	if(f_size(&head_file) > 0) {
		recall_heads();
	} else {
		flush_heads();
	}

	return 0;
}

void sd_deinit() {
	flush_heads();
	f_close(&head_file);
	f_close(&data_file);
	f_mount(NULL, "", 1);
}

uint8_t recall_heads() {
	UINT bytes_read;
	BYTE head_buff[sizeof r_head + sizeof w_head];

	fres = f_lseek(&head_file, 0);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	fres = f_read(&head_file, head_buff, sizeof head_buff, &bytes_read);
	if(fres != FR_OK || bytes_read < sizeof head_buff)
		return SD_READ_ERR;

	// Always R then W.
	// r_head is up first, with 8 bytes to read.
	r_head = (uint64_t)head_buff[7] | (uint64_t)head_buff[6] << 8
			| (uint64_t)head_buff[5] << 16 | (uint64_t)head_buff[4] << 24
			| (uint64_t)head_buff[3] << 32 | (uint64_t)head_buff[2] << 40
			| (uint64_t)head_buff[1] << 48 | (uint64_t)head_buff[0] << 56;
	// Same with w_head.
	w_head = (uint64_t)head_buff[15] | (uint64_t)head_buff[14] << 8
			| (uint64_t)head_buff[13] << 16 | (uint64_t)head_buff[12] << 24
			| (uint64_t)head_buff[11] << 32 | (uint64_t)head_buff[10] << 40
			| (uint64_t)head_buff[9] << 48 | (uint64_t)head_buff[8] << 56;

	return SD_OK;
}

uint8_t flush_heads() {
	BYTE head_buff[sizeof r_head + sizeof w_head];

	head_buff[0] = (r_head >> 56) & 0xFF;
	head_buff[1] = (r_head >> 48) & 0xFF;
	head_buff[2] = (r_head >> 40) & 0xFF;
	head_buff[3] = (r_head >> 32) & 0xFF;
	head_buff[4] = (r_head >> 24) & 0xFF;
	head_buff[5] = (r_head >> 16) & 0xFF;
	head_buff[6] = (r_head >> 8) & 0xFF;
	head_buff[7] = r_head & 0xFF;

	head_buff[8] = (w_head >> 56) & 0xFF;
	head_buff[9] = (w_head >> 48) & 0xFF;
	head_buff[10] = (w_head >> 40) & 0xFF;
	head_buff[11] = (w_head >> 32) & 0xFF;
	head_buff[12] = (w_head >> 24) & 0xFF;
	head_buff[13] = (w_head >> 16) & 0xFF;
	head_buff[14] = (w_head >> 8) & 0xFF;
	head_buff[15] = w_head & 0xFF;

	fres = f_lseek(&head_file, 0);
	if(fres != FR_OK)
		return SD_SEEK_ERR;

	UINT bytes_written;
	fres = f_write(&head_file, head_buff, sizeof head_buff, &bytes_written);
	if(fres != FR_OK || bytes_written < sizeof head_buff)
		return SD_WRITE_ERR;

	fres = f_sync(&head_file);
	if(fres != FR_OK)
		return SD_SYNC_ERR;

	return SD_OK;
}

static int32_t get_next_packet_bounds(FIL* pifile) {
	if(f_tell(pifile) == f_size(pifile))
		return 0; // hit end of file, return 0 bytes

	FRESULT fres;

	DWORD p_begin = f_tell(pifile);

	uint8_t header_found = 0;

	uint8_t next_found = 0;
	uint32_t next_begin = 0;

	uint8_t readBuf[512];
	UINT bytesRead = 0;

	// while loop to find beginning of the next header
	while(!next_found) {
		fres = f_read(pifile, readBuf, sizeof readBuf, &bytesRead);
		if(fres != FR_OK)
			return GP_FRES_ERR;
		else if(bytesRead == 0)
			break;

		uint32_t block_start = (f_tell(pifile) - bytesRead);

		for(int i = 0; i < bytesRead; i++) {
			if(block_start + i > p_begin && readBuf[i] == packet_sepr[header_found++]) {
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

	if(!next_found)
		next_begin = f_size(pifile);

	f_lseek(pifile, p_begin);

	return (next_begin - p_begin);
}

int32_t get_next_packet(FIL* pifile, uint8_t** packet_buf) {
	int32_t p_size = get_next_packet_bounds(pifile);
	if(p_size <= 0)
		return p_size; // less than zero, error out without malloc-ing

	*packet_buf = malloc(p_size);
	if(*packet_buf == NULL)
		return GP_BAD_MALLOC;

	memset(*packet_buf, 0, p_size);

	UINT bytesRead = 0;
	f_read(pifile, *packet_buf, p_size, &bytesRead);
	if(bytesRead != p_size)
		return bytesRead; // WTF?! TODO: Handle this error, if it's even possible?

	return p_size;
}

// Takes in a packet number and a pointer to a pointer to a uint8_t,
// which will be populated with the selected packet. Returns positive
// buffer size on success, negative value on failure. packer_num starts
// from 1 and counts upwards. TODO: update these docs, they're wrong.
int32_t get_packet_num(uint32_t packet_num, FIL* pifile, uint8_t** packet_buf) {
	// Store old tell point and seek to beginning of file.
	DWORD old_tell = f_tell(pifile);

	int32_t p_size = 0;
	f_lseek(pifile, p_size);

	// Repeat n times to get our file packet at the start of n-th packet.
	// Note, returns n-th packet, 1-based. 0th packet returns nothing.
	for(uint32_t i = 0; i < packet_num; i++) {
		// Seek to the beginning of this packet, returned by the previous iteration.
		f_lseek(pifile, f_tell(pifile) + p_size);

		p_size = get_next_packet_bounds(pifile);
		if(p_size <= 0)
			return p_size; // error, probably end of file?
	}

	if(p_size <= 0)
		return p_size; // less than zero, error out without malloc-ing

	*packet_buf = malloc(p_size);
	if(*packet_buf == NULL)
		return GP_BAD_MALLOC;

	memset(*packet_buf, 0, p_size);

	UINT bytesRead = 0;
	f_read(pifile, *packet_buf, p_size, &bytesRead);
	if(bytesRead != p_size)
		return bytesRead; // WTF?! TODO: Handle this error, if it's even possible?

	f_lseek(pifile, old_tell);

	return p_size;
}
