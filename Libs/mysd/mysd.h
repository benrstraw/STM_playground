/*
 * mysd.h
 *
 *  Created on: May 15, 2018
 *      Author: Ben
 */

#ifndef MYSD_MYSD_H_
#define MYSD_MYSD_H_

#define SD_OK			0
#define SD_MSD_NULL		5
#define SD_PACKET_NULL	6
#define SD_READ_ERR		7
#define SD_WRITE_ERR	8
#define SD_OUT_OF_SPACE	9
#define SD_NO_PACKET	10

#include <stddef.h>
#include <stdint.h>
#include "sd_diskio.h"

#define SD_SECTOR_SIZE 512
#define SD_PACKET_SIZE 128
#define SD_PPS (SD_SECTOR_SIZE / SD_PACKET_SIZE)

typedef struct {
	uint32_t r_head, w_head;			// keeps track of the current packet numbers
	uint8_t w_wrap;						// has the W head wrapped already?

	uint8_t head_sector[SD_SECTOR_SIZE];// always keep head sector in memory
	uint8_t c_sector[SD_SECTOR_SIZE];	// current sector
	uint32_t n_sector;					// current sector number

	uint32_t max_packets;				// max number of sector as per SD card
} mysd;

/**
 * Populates a SD struct, opens the head file, loads the stored read and write
 * heads into memory, then opens the first data file. (If this is not the correct
 * file, that will be remedied on the first read or write operation.) The passed
 * pointer to a SD struct must not be NULL, as it is not created allocated by
 * this function and must be handled by the caller. The initialized struct will be
 * required for all subsequent calls to this SD library, so it should be kept in
 * a safe place in the heap or static storage. This function does allocate memory
 * buffers in the struct for SD card handling. Any calls to this library with a SD
 * struct that has not yet been passed into this function are undefined behavior
 * and must be avoided. Returns according to the SD_* defines.
 */
uint8_t sd_init(mysd* msd);

/**
 * Saves the current state of the heads, forces a reinitialization of SD interface,
 * then reloads the current heads. Makes no attempt to flush cached data to disk.
 * This command is not graceful and should only be used if the SD card ends up in
 * an unstable state. Returns according to the SD_* defines.
 */
uint8_t sd_reset(mysd* msd);

/**
 * Writes the in-memory head locations to the head file, then flushes any pending
 * cached writes to the disk. Prior to this call, no guarantees are made about the
 * persistence of written data in event of power loss. Packets are flushed to disk
 * on sector boundaries (every fourth packet), but this call ensures data safety
 * should a series of writes end in the middle of a sector. Additionally, this is
 * the only function which saves the heads. Returns according to the SD_* defines.
 */
uint8_t save_data(mysd* msd);


/**
 * Retrieves the next packet msd's read head. The buffer pointed to by the argument
 * packet_buf MUST be at least of size SD_PACKET_SIZE. This function WILL attempt
 * to write MSD_PACKET_SIZE bytes to the pointed-to packet_buf and will make no
 * attempt at bounds checking. Returns according to the SD_* defines.
 */
uint8_t get_next_packet(uint8_t* packet_buf, mysd* msd);

/**
 * Writes the data in the buffer pointed to by the packet_buf argument to the SD
 * packet pointer to by msd's write head. The written packet will be of size
 * MSD_PACKET_SIZE. If packet_size == MSD_PACKET_SIZE, packet_buf is passed directly
 * to the underlying write function. If packet_size < MSD_PACKET_SIZE, the contents
 * of packet_buf are copied to the beginning of a zero'd buffer of length
 * MSD_PACKET_SIZE, essentially zero-padding the data up to MSD_PACKET_SIZE bytes.
 * If packet_size > MSD_PACKET_SIZE, only the first MSD_PACKET_SIZE bytes of
 * packet_buf are written to disk, essentially truncating the buffer. packet_buf
 * is not modified in this process. Returns according to the SD_* defines.
 */
uint8_t write_next_packet(uint8_t* packet_buf, size_t packet_size, mysd* msd);

#endif /* MYSD_MYSD_H_ */
