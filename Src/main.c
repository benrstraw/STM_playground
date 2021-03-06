
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2018 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l1xx_hal.h"
#include "crc.h"
#include "fatfs.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>

#include "ff.h"
#include "mysd/mysd.h"
#include "fnv/fnv.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

int _write(int file, char *data, int len) {
	if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) {
		errno = EBADF;
		return -1;
	}

	HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t*) data, len, 0xFFFF);

	// return # of bytes written - as best we can tell
	return (status == HAL_OK ? len : 0);
}

void print_buf(uint8_t* buffer, size_t buf_size) {
	const uint8_t line_length = 16;
	for(int i = 0; i < (buf_size / line_length); i++) {
		for(int j = 0; j < line_length; j++)
			printf("%02X", buffer[(i * line_length) + j]);
		printf("\r\n");
	}

	if(buf_size % 16) {
		for(int k = 0; k < buf_size % line_length; k++)
			printf("%02X", buffer[((buf_size / line_length) * line_length) + k]);
		printf("\r\n");
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */

/*	 We only want the timer to run after we push the button and light the LED,
	 so immediately stop it. For some reason, initializing the timer sets the
	 interrupt update bit, meaning as soon as we start up the timer after the
	 first button press, it raises the interrupt, immediately turning the LED
	 back off. This is fixed by clearing the interrupt bit after we stop it,
	 before the first run of the timer. Setting the counter to 0 isn't necessary,
	 as that's done in the button's interrupt handler.

	HAL_TIM_Base_Stop_IT(&htim2);
	__HAL_TIM_CLEAR_IT(&htim2, TIM_IT_UPDATE);

	// Delay to allow SD card to get set up internally.
	HAL_Delay(1000);
*/

  // Create a null pointer to a `mysd`, to be malloc'd upon request (see: cin == 'i')
  // and populated by `sd_init`
  mysd* sd = NULL;
  uint8_t fnvtest1[] = { 'a', 'b', 'c', 'd' };
  uint8_t fnvtest2[] = { 'a', 'b', 'c', 'd', '\n' };

  printf("fnv1 'abcd' with null %lx#, without %lx#\r\n", fnv1a_32(fnvtest2, 5), fnv1a_32(fnvtest1, 4));

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	uint8_t cin;
	while (1) {
		HAL_UART_Receive(&huart1, &cin, 1, 0xFFFFFF);

		if(cin == 'd') { // [d]einitialize SD
			printf("Deinitializing mySD...\r\n");
			sd_deinit(sd);
			free(sd);
			sd = NULL; // set to NULL for the pointer validity checks in every branch!
		} else if(cin == 'i') { // [i]nitialize SD
			if(!sd)
				sd = calloc(1, sizeof(mysd)); // either calloc or memset zero so the struct's fields are null!

			printf("Initializing mySD... [%d]\r\n", sd_init(sd)); // otherwise this call will fail!
		} else if(cin == 'h') { // get [h]eads
			if(sd) // this validity check in every branch that directly accesses `sd`
				printf("R/W heads are at: R = <%lu>, W = <%lu>\r\n", (uint32_t)sd->r_head, (uint32_t)sd->w_head);
		} else if(cin == 'z') { // [z]ero heads
			if(sd)
				printf("Heads zeroed! R = <%lu> W = <%lu>\r\n", (uint32_t)(sd->r_head = 0), (uint32_t)(sd->w_head = 0));
		} else if(cin == 'g') { // [g]et next packet
			printf("Getting next packet...\r\n");

			uint8_t p_buf[MSD_PACKET_SIZE] = {0};

			uint32_t start_time = HAL_GetTick();
			int32_t p_size = get_next_packet(p_buf, sd);

			if(p_size <= 0) {
				printf("ERR [%ld]\r\n", p_size);
				continue;
			}

			uint32_t stop_time = HAL_GetTick();
			HAL_UART_Transmit(&huart1, p_buf, p_size, 0xFFFFFF);
			printf(" [%ld] Took %lu ms.\r\n", p_size, stop_time - start_time);
		} else if(cin == 'x') { // [g]et next packet
			printf("Getting next packet (hex)...\r\n");

			uint8_t p_buf[MSD_PACKET_SIZE] = {0};
			int32_t p_size = get_next_packet(p_buf, sd);

			if(p_size <= 0) {
				printf("ERR [%ld]\r\n", p_size);
				continue;
			}

			print_buf(p_buf, sizeof p_buf);
		} else if(cin == 'w') { // w = input a string to be added to the packet file
			uint8_t s_buffer[512];
			uint16_t s_i = 0;
			do { // we must assemble our own strings from individual characters, done in this loop
				HAL_UART_Receive(&huart1, &cin, 1, 0xFFFFFF);
				if(cin == '\r' || cin == '\n')
					break; // while is just to provide an upper limit, this is the useful break
				s_buffer[s_i++] = cin; // collect value then increment
				HAL_UART_Transmit(&huart1, &cin, 1, 0xFFFFFF); // echo input back to user
			} while (s_i < (sizeof s_buffer) - 1);
			s_buffer[s_i] = '\0';

			printf("\rWriting: \"%s\" ... [%d]\r\n", (char*)s_buffer, write_next_packet(s_buffer, strlen((char*)s_buffer), sd));
		} else if(cin == '/') { // / = increment R head
			if(sd)
				printf("R = <%lu>\r\n", (uint32_t)(++(sd->r_head)));
		} else if(cin == '.') { // . = reset R head to 0
			if(sd)
				printf("R = <%lu>\r\n", (uint32_t)(sd->r_head = 0));
		} else if(cin == '\'') { // ' = increment W head
			if(sd)
				printf("W = <%lu>\r\n", (uint32_t)(++(sd->w_head)));
		} else if(cin == ';') { // ; = reset W head to 0
			if(sd)
				printf("W = <%lu>\r\n", (uint32_t)(sd->w_head = 0));
		} else if(cin == 's') { // ; = reset W head to 0
			if(sd)
				printf("Saving data... [%d]\r\n", save_data(sd));
		} else if(cin == 'f') {
			if(!sd)
				continue;

			printf("Filling 1000 random packets... [%lu]\r\n", HAL_GetTick());

			uint8_t rand_buf[MSD_PACKET_SIZE];
			srand(HAL_GetTick());
			for(int i = 0; i < 1000; i++) {
				for(uint8_t j = 0; j < MSD_PACKET_SIZE; j++)
					rand_buf[j] = rand() % 0x0100;

				write_next_packet(rand_buf, sizeof rand_buf, sd);
			}

			printf("Done! [%lu]\r\n", HAL_GetTick());
		} else if(cin == 'l') {
			printf("Refreshing data... [%d]\r\n", refresh_data(sd));
		} else if(cin == 'u') {
			if(!sd)
				continue;

			printf("Filling 1000 sequential packets... [%lu]\r\n", HAL_GetTick());

			uint8_t num_buf[MSD_PACKET_SIZE] = {0};
			for(int i = 0; i < 1000; i++) {
				snprintf((char*)num_buf, sizeof num_buf, "test packet #%lu", sd->w_head);
				write_next_packet(num_buf, sizeof num_buf, sd);
			}

			printf("Done! [%lu]\r\n", HAL_GetTick());
		} else if(cin == 'c') {
			if(!sd)
				continue;

			uint8_t s_buffer[MSD_PACKET_SIZE] = {0};
			snprintf((char*)s_buffer, sizeof s_buffer, "test packet #%lu", sd->w_head);
			uint32_t start_time = HAL_GetTick();
			uint8_t res = write_next_packet(s_buffer, sizeof s_buffer, sd);
			printf("\rWriting: \"%s\" ... [%d]. Took %lu ms.\r\n", (char*)s_buffer, res, HAL_GetTick() - start_time);
		} else if(cin == '+') {
			if(!sd)
				continue;

			const static uint32_t howmany = 2000;

			uint8_t pak_buf[MSD_PACKET_SIZE] = {0};
			uint8_t res = 0;

			sd->r_head = sd->w_head;

			printf("Beginning %lu packet R/W test...\r\n", howmany);
			uint32_t start_time = HAL_GetTick();

			for(uint32_t i = 0; i < howmany; i++) {
//				memset(pak_buf, 0, MSD_PACKET_SIZE);
				for(uint8_t j = 0; j < MSD_PACKET_SIZE - (sizeof howmany - 1); j += sizeof howmany) {
					pak_buf[j] = (i >> 24) & 0xFF;
					pak_buf[j+1] = (i >> 16) & 0xFF;
					pak_buf[j+2] = (i >> 8) & 0xFF;
					pak_buf[j+3] = i & 0xFF;
				}

				res = write_next_packet(pak_buf, sizeof pak_buf, sd);
				if(res) {
					printf("Write fail on packet %lu!\r\n", i);
					break;
				}

				if(i % 1000 == 0 && i > 0)
					printf("%lu @ %lu ms.\r\n", i, HAL_GetTick() - start_time);
			}

			printf("Writes done! %lu packets took %lu ms. Beginning reads...\r\n", howmany, HAL_GetTick() - start_time);
			start_time = HAL_GetTick();

			for(uint32_t i = 0; i < howmany; i++) {
				uint32_t test = 0;

//				memset(pak_buf, 0, MSD_PACKET_SIZE);
				int32_t p_size = get_next_packet(pak_buf, sd);
				if(p_size <= 0) {
					printf("Read fail on packet %lu!\r\n", i);
					break;
				}

				for(uint8_t j = 0; j < MSD_PACKET_SIZE - (sizeof howmany - 1); j += sizeof howmany) {
					test = (uint16_t)pak_buf[j] << 24 | (uint16_t)pak_buf[j+1] << 16
							| (uint16_t)pak_buf[j+2] << 8 | (uint16_t)pak_buf[j+3];
					if(test != i) {
						printf("Read fail on packet %lu!\r\n", i);
						i = howmany;
						break;
					}
				}

				if(i % 1000 == 0 && i > 0)
					printf("%lu @ %lu ms.\r\n", i, HAL_GetTick() - start_time);
			}

			printf("Reads done! %lu packets took %lu ms.\r\n", howmany, HAL_GetTick() - start_time);
		} else if(cin == 'k') {
			printf("Making filesystem... [%d]\r\n", f_mkfs("", 0, 0));
		}


  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

	}
  /* USER CODE END 3 */

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL3;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
