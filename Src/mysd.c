/*
 * mysd.c
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#include "fatfs.h"
#include <stdlib.h>
#include <string.h>
#include "mysd.h"

const char packet_header[] = { '_', '.', ',', '_' };

// Takes in a packet number and a pointer to a pointer to a uint8_t,
// which will be populated with the selected packet. Returns positive
// buffer size on success, negative value on failure. packer_num starts
// from 1 and counts upwards.
/*int32_t get_by_packet_num(uint32_t packet_num, char* filename, uint8_t** packet_buf) {
	FRESULT fres;
	FIL pifile;

	fres = f_open(&pifile, "test.txt", FA_READ);
	if (fres != FR_OK) {
		return GP_FRES_ERR;
	}

	uint8_t readBuf[1024]; // 256 sector size * 4 // 17
	UINT bytesRead = 0;
	int32_t ret = 0; // acts as packet_buf size
	uint32_t packets_seen = 0, target_begin = 0, target_end = 0;
	int8_t target_found = 0, w_break = 0, end_on_header = 0; // simple bool, 0 = false, 1 = true

	while (!w_break) {
		fres = f_read(&pifile, readBuf, sizeof readBuf, &bytesRead);
		if(fres != FR_OK || bytesRead == 0) {
			// if we've found the target but also hit the end of the of the
			// file, then this must be the last packet, set target_end to
			// the end of the file (file size). else, return not found.
			if(target_found) {
				target_end = pifile.fsize;
				break;
			} else {
				f_close(&pifile);
				return GP_NOT_FOUND;
			}
		}

		int32_t local_begin = -1;

		for(int i = 0; i < (bytesRead - 1); i++) {
			// check for two packet header bytes consecutively
			if((readBuf[i] == packet_header[0] && readBuf[i + 1] == packet_header[1])
			|| (end_on_header && i == 0 && readBuf[i] == packet_header[1])) {
				packets_seen++;
				if(packets_seen == packet_num) { // found beginning of our packet
					target_found = 1;
					local_begin = i;
					target_begin = ((pifile.fptr - bytesRead) + i); // inclusive
					if(end_on_header && i == 0)
						target_begin = ((pifile.fptr - bytesRead) - 1);
				} else if (packets_seen > packet_num) { // found end of our packet
					target_end = ((pifile.fptr - bytesRead) + i); // exclusive
					if(end_on_header && i == 0)
						target_end = ((pifile.fptr - bytesRead) - 1);

					// If we've found both beginning and end in the same f_read,
					// don't do another read, finish up here.
					if(local_begin > 0) {
						ret = i - local_begin;
						*packet_buf = malloc(ret);

						for(int j = 0; j < ret; j++) {
							(*packet_buf)[j] = readBuf[local_begin + j];
						}

						f_close(&pifile);
						return ret;
					} else { // both not in the same pull, do another
						w_break = 1;
						break;
					}
				} // end found end else if
			} // end packet header check if
		} // end for

		end_on_header = 0;
		if(readBuf[bytesRead - 1] == packet_header[0])
			end_on_header = 1;

	} // end while

	if(target_end <= target_begin) {
		f_close(&pifile);
		return GP_END_LTE_BEGIN;
	}

	if(!target_found) {
		f_close(&pifile);
		return ret;
	}

	ret = target_end - target_begin;
	*packet_buf = malloc(ret);

	f_lseek(&pifile, target_begin);
	f_read(&pifile, *packet_buf, ret, &bytesRead);

	f_close(&pifile);
	return ret;
}*/

int32_t get_next_packet_bounds(FIL* pifile, uint8_t** packet_buf) {
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
			if(block_start + i > p_begin && readBuf[i] == packet_header[header_found++]) {
				if(header_found == 1) {
					next_begin = block_start + i;
				} else if(header_found == sizeof packet_header) {
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

	uint32_t p_size = next_begin - p_begin;
	*packet_buf = malloc(p_size);
	memset(*packet_buf, 0, p_size);

	f_lseek(pifile, p_begin);
	f_read(pifile, *packet_buf, p_size, &bytesRead);

	return p_size;
}

int32_t get_packet_num(uint32_t packet_num, FIL* pifile, uint8_t** packet_buf) {
	FRESULT fres;

	DWORD old_tell = f_tell(pifile);

	f_lseek(pifile, 0);

	uint8_t target_found = 0;
	uint32_t target_begin = 0;

	uint8_t next_found = 0;
	uint32_t next_begin = 0;

	while(!target_found) {
		uint8_t header_found = 0;

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
				if(block_start + i > p_begin && readBuf[i] == packet_header[header_found++]) {
					if(header_found == 1) {
						next_begin = block_start + i;
					} else if(header_found == sizeof packet_header) {
						next_found = 1;
						break;
					}
				} else {
					header_found = 0;
				}
			}
		}

		if(bytesRead == 0 && !next_found)
			next_begin = f_size(pifile);
	}

	if(!target_found)
		return GP_NOT_FOUND;

	uint32_t p_size = next_begin - target_begin;
	*packet_buf = malloc(p_size);
	memset(*packet_buf, 0, p_size);

	f_lseek(pifile, p_begin);
	f_read(pifile, *packet_buf, p_size, &bytesRead);

	return p_size;
}
