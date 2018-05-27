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

#define SD_OK			0
#define SD_SEEK_ERR		1
#define SD_READ_ERR		2
#define SD_WRITE_ERR	3
#define SD_SYNC_ERR		4

#define GP_NOT_FOUND	-1
#define GP_FRES_ERR		-2
#define GP_BAD_MALLOC	-3

extern uint64_t r_head;
extern uint64_t w_head;

uint8_t sd_init();
void sd_deinit();

uint8_t recall_heads();
uint8_t flush_heads();

int32_t get_next_packet(FIL* pifile, uint8_t** packet_buf);
int32_t get_packet_num(uint32_t packet_num, FIL* pifile, uint8_t** packet_buf);

#endif /* MYSD_MYSD_H_ */
