/*
 * mysd.c
 *
 *  Created on: May 15, 2018
 *      Author: Ben
*/

#include "mysd.h"

#include <stdlib.h>
#include <string.h>

uint8_t recall_heads(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	DRESULT dres = sdio_read(msd->head_sector, 0, 1);
	if(dres)
		return dres;

	// Building ints from bytes, hooray! Stored big-endian.
	msd->r_head = (uint32_t)msd->head_sector[5] | (uint32_t)msd->head_sector[4] << 8
			| (uint32_t)msd->head_sector[3] << 16 | (uint32_t)msd->head_sector[2] << 24;

	msd->w_head = (uint32_t)msd->head_sector[9] | (uint32_t)msd->head_sector[8] << 8
			| (uint32_t)msd->head_sector[7] << 16 | (uint32_t)msd->head_sector[6] << 24;

	msd->w_wrap = msd->head_sector[10];

	return SD_OK;
}

uint8_t flush_heads(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	// Dumping the head integers into byte arrays through the magic of bit shifting.
	// head_sector: 2-5 = read head, 6-9 = write head, both stored big-endian
	msd->head_sector[0] = 'S';
	msd->head_sector[1] = 's';

	msd->head_sector[2] = (msd->r_head >> 24) & 0xFF;
	msd->head_sector[3] = (msd->r_head >> 16) & 0xFF;
	msd->head_sector[4] = (msd->r_head >> 8) & 0xFF;
	msd->head_sector[5] = msd->r_head & 0xFF;

	msd->head_sector[6] = (msd->w_head >> 24) & 0xFF;
	msd->head_sector[7] = (msd->w_head >> 16) & 0xFF;
	msd->head_sector[8] = (msd->w_head >> 8) & 0xFF;
	msd->head_sector[9] = msd->w_head & 0xFF;

	msd->head_sector[10] = msd->w_wrap;

	DRESULT dres = sdio_write(msd->head_sector, 0, 1);
	if(dres)
		return dres;

	return SD_OK;
}

uint8_t set_active(uint32_t head, mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	uint32_t packet_sector = (head / SD_PPS) + 1;
	if(msd->n_sector == packet_sector)
		return SD_OK;

	if(packet_sector % SD_FLUSH_ON == 0)
		flush_heads(msd);

	DRESULT dres;
	if(msd->n_sector != 0) {
		 dres = sdio_write(msd->c_sector, msd->n_sector, 1);
		if(dres)
			return dres;
	}

	dres = sdio_read(msd->c_sector, packet_sector, 1);
	if(dres)
		return dres;

	msd->n_sector = packet_sector;

	return SD_OK;
}

uint32_t increment_head(uint32_t* head, mysd* msd) {
	if(!msd || !head)
		return UINT32_MAX;

	if(((++(*head)) % msd->max_packets) == 0) {
		(*head) = 0;
		if(head == &msd->w_head)
			msd->w_wrap = 1;
		else if(msd->w_wrap)
			msd->w_wrap = 0;
	}

	return *head;
}

uint8_t sd_init(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	memset(msd, 0, sizeof(mysd));
	memset(msd->head_sector, 0, SD_SECTOR_SIZE);
	memset(msd->c_sector, 0, SD_SECTOR_SIZE);

	DRESULT dres = sdio_initialize();
	if(dres)
		return dres;

	uint8_t res = recall_heads(msd);
	if(res)
		return res;

	uint32_t total_sect;
	sdio_ioctl(GET_SECTOR_COUNT, &total_sect);
	msd->max_packets = (total_sect - 2000000) * SD_PPS; // -2mil sectors for ~gig of safety.

	return SD_OK;
}

uint8_t sd_reset(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	uint32_t temp_r_head = msd->r_head;
	uint32_t temp_w_head = msd->w_head;
	uint8_t temp_w_wrap = msd->w_wrap;

	uint8_t res = sd_init(msd);
	if(res)
		return res;

	msd->r_head = temp_r_head;
	msd->w_head = temp_w_head;
	msd->w_wrap = temp_w_wrap;

	return SD_OK;
}

uint8_t save_data(mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;

	uint8_t res = flush_heads(msd);
	if(res)
		return res;

	DRESULT dres = sdio_write(msd->c_sector, msd->n_sector, 1);
	if(dres)
		return dres;

	return SD_OK;
}

uint8_t get_next_packet(uint8_t* packet_buf, mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;
	if(!packet_buf)
		return SD_PACKET_NULL;

	// If R at W and W not wrapped, then no more to read.
	if(msd->r_head >= msd->w_head && msd->w_wrap == 0)
		return SD_NO_PACKET;

	uint8_t res = set_active(msd->r_head, msd);
	if(res)
		return res;

	uint16_t packet_offset = (msd->r_head % SD_PPS) * SD_PACKET_SIZE;
	memcpy(packet_buf, (msd->c_sector + packet_offset), SD_PACKET_SIZE);

	increment_head(&msd->r_head, msd);

	return SD_OK;
}

uint8_t write_next_packet(uint8_t* packet_buf, size_t in_size, mysd* msd) {
	if(!msd)
		return SD_MSD_NULL;
	if(!packet_buf)
		return SD_PACKET_NULL;

	// If W is at H and W has already wrapped, we can't write any more.
	if(msd->w_head >= msd->r_head && msd->w_wrap == 1)
		return SD_OUT_OF_SPACE;

	uint8_t res = set_active(msd->w_head, msd);
	if(res != SD_OK)
		return res;

	uint8_t* write_buf = packet_buf;

	if(in_size < SD_PACKET_SIZE) {
		write_buf = calloc(1, SD_PACKET_SIZE);
		memcpy(write_buf, packet_buf, in_size);
	}

	uint16_t packet_offset = (msd->w_head % SD_PPS) * SD_PACKET_SIZE;
	memcpy((msd->c_sector + packet_offset), write_buf, SD_PACKET_SIZE);

	if(in_size < SD_PACKET_SIZE)
		free(write_buf);

	increment_head(&msd->w_head, msd);

	return SD_OK;
}
