/*
 * sd_diskio.c
 *
 *  Created on: Jun 10, 2018
 *      Author: Ben
 */

// Portions copyright (C) 2017, kiwih, all rights reserved.
// Available from https://github.com/kiwih/cubemx-mmc-sd-card/
//This code was ported by kiwih from a copywrited (C) library written by ChaN
//available at http://elm-chan.org/fsw/ff/ffsample.zip
//(text at http://elm-chan.org/fsw/ff/00index_e.html)
//This file provides the FatFs driver functions and SPI code required to manage
//an SPI-connected MMC or compatible SD card with FAT
/* Includes ------------------------------------------------------------------*/
#include "sd_diskio.h"

#include <string.h>
#include "spi.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define FCLK_SLOW() { hspi1.Instance->I2SPR = 128; }	/* Set SCLK = slow (default: 256) */
#define FCLK_FAST() { hspi1.Instance->I2SPR = 16; }	/* Set SCLK = fast (default: 16) */

//#define CS_HIGH()	{HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);}
//#define CS_LOW()	{HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);}

/* Disk Status Bits (uint8_t) */
#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* MMC/SDC specific ioctl command */
#define MMC_GET_TYPE		10	/* Get card type */
#define MMC_GET_CSD			11	/* Get CSD */
#define MMC_GET_CID			12	/* Get CID */
#define MMC_GET_OCR			13	/* Get OCR */
#define MMC_GET_SDSTAT		14	/* Get SD status */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile uint8_t Stat = STA_NOINIT;

static uint8_t CardType; /* Card type flags */

uint32_t spiTimerTickStart;
uint32_t spiTimerTickDelay;

void SPI_Timer_On(uint32_t waitTicks) {
	spiTimerTickStart = HAL_GetTick();
	spiTimerTickDelay = waitTicks;
}

uint8_t SPI_Timer_Status() {
	return ((HAL_GetTick() - spiTimerTickStart) < spiTimerTickDelay);
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/
/* Exchange a byte */
/* Data to send */
static uint8_t xchg_spi(uint8_t dat) {
	uint8_t rxDat;
	HAL_SPI_TransmitReceive(&hspi1, &dat, &rxDat, 1, 50);
	return rxDat;
}

/* Receive multiple byte */
/* Pointer to data buffer */
/* Number of bytes to receive (even number) */
static void rcvr_spi_multi(uint8_t* buff, uint32_t btr) {
	for (uint32_t i = 0; i < btr; i++) {
		*(buff + i) = xchg_spi(0xFF);
	}
}

/* Send multiple byte */
/* Pointer to the data */
/* Number of bytes to send (even number) */
static void xmit_spi_multi(const uint8_t* buff, uint32_t btx) {
	for (uint32_t i = 0; i < btx; i++) {
		xchg_spi(*(buff + i));
	}
}

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/
/* 1:Ready, 0:Timeout */
/* Timeout [ms] */
static int wait_ready(uint32_t wt) {
	uint8_t d;
	//wait_ready needs its own timer, unfortunately, so it can't use the
	//spi_timer functions
	uint32_t waitSpiTimerTickStart;
	uint32_t waitSpiTimerTickDelay;

	waitSpiTimerTickStart = HAL_GetTick();
	waitSpiTimerTickDelay = (uint32_t) wt;
	do {
		d = xchg_spi(0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multitask envilonment. */
	} while (d != 0xFF
			&& ((HAL_GetTick() - waitSpiTimerTickStart) < waitSpiTimerTickDelay)); /* Wait for card goes ready or timeout */

	return (d == 0xFF) ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
/* Despiselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/
static void despiselect(void) {
//  	CS_HIGH();		/* Set CS# high */
	//xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/
/* 1:OK, 0:Timeout */
static int spiselect(void) {
//  	CS_LOW();		/* Set CS# low */
	xchg_spi(0xFF); /* Dummy clock (force DO enabled) */
	if (wait_ready(1000))
		return 1; /* Wait for card ready */

	despiselect();
	return 0; /* Timeout */
}

/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/
/* 1:OK, 0:Error */
/* Data buffer */
/* Data block length (byte) */
static int rcvr_datablock(uint8_t* buff, uint32_t btr) {
	uint8_t token;

	SPI_Timer_On(200);
	do { /* Wait for DataStart token in timeout of 200ms */
		token = xchg_spi(0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && SPI_Timer_Status());
	if (token != 0xFE)
		return 0; /* Function fails if invalid DataStart token or timeout */

	rcvr_spi_multi(buff, btr); /* Store trailing data to the buffer */
	xchg_spi(0xFF);
	xchg_spi(0xFF); /* Discard CRC */

	return 1; /* Function succeeded */
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/
/* 1:OK, 0:Failed */
/* Ponter to 512 byte data to be sent */
/* Token */
static int xmit_datablock(const uint8_t* buff, uint8_t token) {
	uint8_t resp;

	if (!wait_ready(500))
		return 0; /* Wait for card ready */

	xchg_spi(token); /* Send token */
	if (token != 0xFD) { /* Send data if token is other than StopTran */
		xmit_spi_multi(buff, 512); /* Data */
		xchg_spi(0xFF);
		xchg_spi(0xFF); /* Dummy CRC */

		resp = xchg_spi(0xFF); /* Receive data resp */
		if ((resp & 0x1F) != 0x05)
			return 0; /* Function fails if the data packet was not accepted */
	}
	return 1;
}

/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/
/* Return value: R1 resp (bit7==1:Failed to send) */
/* Command index */
/* Argument */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
	uint8_t n, res;

	if (cmd & 0x80) { /* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1)
			return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		despiselect();
		if (!spiselect())
			return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd); /* Start + command index */
	xchg_spi((uint8_t) (arg >> 24)); /* Argument[31..24] */
	xchg_spi((uint8_t) (arg >> 16)); /* Argument[23..16] */
	xchg_spi((uint8_t) (arg >> 8)); /* Argument[15..8] */
	xchg_spi((uint8_t) arg); /* Argument[7..0] */
	n = 0x01; /* Dummy CRC + Stop */
	if (cmd == CMD0)
		n = 0x95; /* Valid CRC for CMD0(0) */
	if (cmd == CMD8)
		n = 0x87; /* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12)
		xchg_spi(0xFF); /* Diacard following one byte when CMD12 */
	n = 10; /* Wait for response (10 bytes max) */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n);

	return res; /* Return received response */
}

/**
 * @brief  Initializes a Drive
 * @retval uint8_t: Operation status
 */
uint8_t sdio_initialize() {
	uint8_t n, cmd, ty, ocr[4];

	//assume SPI already init init_spi();	/* Initialize SPI */

	if (Stat & STA_NODISK)
		return Stat; /* Is card existing in the soket? */

	FCLK_SLOW();
	for (n = 10; n; n--)
		xchg_spi(0xFF); /* Send 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) { /* Put the card SPI/Idle state */
		SPI_Timer_On(5000); /* Initialization timeout = 1 sec */
		if (send_cmd(CMD8, 0x1AA) == 1) { /* SDv2? */
			for (n = 0; n < 4; n++)
				ocr[n] = xchg_spi(0xFF); /* Get 32 bit return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) { /* Is the card supports vcc of 2.7-3.6V? */
				while (SPI_Timer_Status() && send_cmd(ACMD41, 1UL << 30)) ; /* Wait for end of initialization with ACMD41(HCS) */
				if (SPI_Timer_Status() && send_cmd(CMD58, 0) == 0) { /* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++)
						ocr[n] = xchg_spi(0xFF);
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2; /* Card id SDv2 */
				}
			}
		} else { /* Not SDv2 card */
			if (send_cmd(ACMD41, 0) <= 1) { /* SDv1 or MMC? */
				ty = CT_SD1;
				cmd = ACMD41; /* SDv1 (ACMD41(0)) */
			} else {
				ty = CT_MMC;
				cmd = CMD1; /* MMCv3 (CMD1(0)) */
			}
			while (SPI_Timer_Status() && send_cmd(cmd, 0)) ; /* Wait for end of initialization */
			if (!SPI_Timer_Status() || send_cmd(CMD16, 512) != 0) /* Set block length: 512 */
				ty = 0;
		}
	}
	CardType = ty; /* Card type */
	despiselect();

	if (ty) { /* OK */
		FCLK_FAST(); /* Set fast clock */
		Stat &= ~STA_NOINIT; /* Clear STA_NOINIT flag */
	} else { /* Failed */
		Stat = STA_NOINIT;
	}

	return Stat;
}

/**
 * @brief  Gets Disk Status
 * @retval uint8_t: Operation status
 */
uint8_t sdio_status() {
	return Stat; /* Return disk status */
}

/**
 * @brief  Reads Sector(s)
 * @param  *buff: Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to read (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT sdio_read(uint8_t* buff, uint32_t sector, uint32_t count) {
	if (!count)
		return RES_PARERR; /* Check parameter */
	if (Stat & STA_NOINIT)
		return RES_NOTRDY; /* Check if drive is ready */

	if (!(CardType & CT_BLOCK))
		sector *= 512; /* LBA ot BA conversion (byte addressing cards) */

	if (count == 1) { /* Single sector read */
		if ((send_cmd(CMD17, sector) == 0) /* READ_SINGLE_BLOCK */
		&& rcvr_datablock(buff, 512)) {
			count = 0;
		}
	} else { /* Multiple sector read */
		if (send_cmd(CMD18, sector) == 0) { /* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512))
					break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
		}
	}

	despiselect();

	return count ? RES_ERROR : RES_OK; /* Return result */
}

/**
 * @brief  Writes Sector(s)
 * @param  *buff: Data to be written
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to write (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT sdio_write (const uint8_t* buff, uint32_t sector, uint32_t count) {
	if (!count) return RES_PARERR; /* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY; /* Check drive status */
	if (Stat & STA_PROTECT) return RES_WRPRT; /* Check write protect */

	if (!(CardType & CT_BLOCK)) sector *= 512; /* LBA ==> BA conversion (byte addressing cards) */

	if (count == 1) { /* Single sector write */
		if ((send_cmd(CMD24, sector) == 0) /* WRITE_BLOCK */
				&& xmit_datablock(buff, 0xFE)) {
			count = 0;
		}
	}
	else { /* Multiple sector write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count); /* Predefine number of sectors */
		if (send_cmd(CMD25, sector) == 0) { /* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD)) count = 1; /* STOP_TRAN token */
		}
	}

	despiselect();

	return count ? RES_ERROR : RES_OK; /* Return result */
}

/**
 * @brief  I/O control operation
 * @param  cmd: Control code
 * @param  *buff: Buffer to send/receive control data
 * @retval DRESULT: Operation result
 */
DRESULT sdio_ioctl(uint8_t cmd, void* buff) {
	DRESULT res;
	uint8_t n, csd[16];
	uint32_t *dp, st, ed, csize;

	if (Stat & STA_NOINIT) return RES_NOTRDY; /* Check if drive is ready */

	res = RES_ERROR;

	switch (cmd) {
		case CTRL_SYNC : /* Wait for end of internal write process of the drive */
		if (spiselect()) res = RES_OK;
		break;

		case GET_SECTOR_COUNT : /* Get drive capacity in unit of sector (uint32_t) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) { /* SDC ver 2.00 */
				csize = csd[9] + ((uint16_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
				*(uint32_t*)buff = csize << 10;
			} else { /* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 1;
				*(uint32_t*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

		case GET_BLOCK_SIZE : /* Get erase block size in unit of sector (uint32_t) */
		if (CardType & CT_SD2) { /* SDC ver 2.00 */
			if (send_cmd(ACMD13, 0) == 0) { /* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) { /* Read partial block */
					for (n = 64 - 16; n; n--) xchg_spi(0xFF); /* Purge trailing data */
					*(uint32_t*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else { /* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) { /* Read CSD */
				if (CardType & CT_SD1) { /* SDC ver 1.XX */
					*(uint32_t*)buff = (((csd[10] & 63) << 1) + ((uint16_t)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else { /* MMC */
					*(uint32_t*)buff = ((uint16_t)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

		case CTRL_TRIM : /* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(CardType & CT_SDC)) break; /* Check if the card is SDC */
		if (sdio_ioctl(MMC_GET_CSD, csd)) break; /* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break; /* Check if sector erase can be applied to the card */
		dp = buff;
		st = dp[0];
		ed = dp[1]; /* Load sector block */
		if (!(CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000)) { /* Erase sector block */
			res = RES_OK; /* FatFs does not check result of this command */
		}
		break;

		default:
		res = RES_PARERR;
	}

	despiselect();

	return res;
}
