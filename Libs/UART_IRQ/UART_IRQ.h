#ifndef __UART_IRQ_H__
#define __UART_IRQ_H__

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "stm32l1xx_hal.h"
#include "stm32l1xx_hal_uart.h"

void UART_IRQ(UART_HandleTypeDef *huart, uint8_t time);
void UART_printSOS(UART_HandleTypeDef *huart, uint32_t time);


////////////////////////////////////////////////////////////////////////////
// The simple functions below are inlined to prevent function call overhead.

inline void UART_PUT(UART_HandleTypeDef *huart, char *str) {
	HAL_UART_Transmit(huart, (uint8_t *) str, (uint16_t) sizeof(str), 0xFFFF);
}

inline void getS(UART_HandleTypeDef *huart, char *buf, uint16_t max_len) {
	for(uint16_t i = 0; i < max_len; i++) {
		HAL_UART_Receive(huart, (uint8_t*)(buf + i), 1, 0xFFFF);
		if(buf[i] == '\0')
			break;
	}
}

inline void putS(UART_HandleTypeDef *huart, char* buf) {
	HAL_UART_Transmit(huart, (uint8_t *)buf, strlen(buf), 0xFFFF);
}

inline void putB(UART_HandleTypeDef *huart, char* buf, uint16_t len) {
	HAL_UART_Transmit(huart, (uint8_t *) buf, len, 0xFFFF);
}

inline void getB(UART_HandleTypeDef *huart, char *buf, uint16_t len) {
	HAL_UART_Receive(huart, (uint8_t *)buf, len, 0xFFFF);
}

#endif
