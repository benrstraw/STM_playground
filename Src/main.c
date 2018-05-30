
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2018 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l1xx_hal.h"
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
#include "mysd.h"
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
  /* USER CODE BEGIN 2 */
/*


	 We only want the timer to run after we push the button and light the LED,
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

	// Common results variable for most of the FatFs function calls.
	// FR_OK = 0, any other result is an error and non-zero.
	FRESULT fres;

	FATFS USERFatFS;

	// Mount the SD card.
	fres = f_mount(&USERFatFS, "", 1);
	if (fres != FR_OK) {
		HAL_UART_Transmit(&huart1, "f_mount NOT OKAY", strlen("f_mount NOT OKAY"), 0xFFFF);
		while (1)
			; // Fatal SD mounting error.
	}

	FIL pifile;
	fres = f_open(&pifile, "test.txt", FA_READ);

	int32_t p_size = 0;
	do {
		uint8_t* packet = NULL;

		p_size = get_next_packet(&pifile, &packet);
		if (p_size <= 0)
			break; // memory wasn't allocated, don't need to free

		uint8_t* re_packet = realloc(packet, p_size + 3);
		if(re_packet == NULL) {
			free(packet); // reallocation failed, re_packet is invalid
			break;
		}

		re_packet[p_size - 2] = '\r';
		re_packet[p_size - 1] = '\n';
		re_packet[p_size] = '\0';

		HAL_UART_Transmit(&huart1, re_packet, strlen((char*)re_packet), 0xFFFF);

		free(re_packet); // `packet` was invalidated upon successful realloc
	} while (p_size > 0);

	uint8_t p_num[] = { 1, 3, 5, 7, 12, 15, 24, 36, 37, 42 };
	for(int i = 0; i < sizeof p_num; i++) {
		uint8_t* packet = NULL;

		p_size = get_packet_num(p_num[i], &pifile, &packet);
		if (p_size <= 0)
			break; // memory wasn't allocated, don't need to free

		uint8_t* re_packet = realloc(packet, p_size + 3);
		if(re_packet == NULL) {
			free(packet); // reallocation failed, re_packet is invalid
			break;
		}

		re_packet[p_size - 2] = '\r';
		re_packet[p_size - 1] = '\n';
		re_packet[p_size] = '\0';

		HAL_UART_Transmit(&huart1, re_packet, strlen((char*)re_packet), 0xFFFF);

		free(re_packet); // `packet` was invalidated upon successful realloc
	}


	f_close(&pifile);

	// Unmount the SD card when finished.
	// Not sure if we'll have to actually do this before the Pi can read and write to it?
	// Or if we just have to ensure we're not also reading and writing while the Pi is.
	f_mount(NULL, "", 0);
*/

  //sd_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	uint8_t cin;
	while (1) {
		HAL_UART_Receive(&huart1, &cin, 1, 0xFFFFFF);

		if(cin == 'd') { // [d]einitialize SD
			printf("Deinitializing mySD...\r\n");
			sd_deinit();
		} else if(cin == 'i') { // [i]nitialize SD
			printf("Initializing mySD... [%d]\r\n", sd_init());
		} else if(cin == 'h') { // get [h]eads
			printf("R/W heads are at: R = <%lu>, W = <%lu>\r\n", (uint32_t)r_head, (uint32_t)w_head);
		} else if(cin == 'f') { // [f]lush heads
			printf("Flushing heads... [%d]\r\n", flush_heads());
		} else if(cin == 'c') { // re[c]all heads
			printf("Recalling heads... [%d]\r\n", recall_heads());
			printf("R = <%lu> W = <%lu>\r\n", (uint32_t)r_head, (uint32_t)w_head);
		} else if(cin == 'a') { // [a]dvance heads
			printf("Heads advanced! R = <%lu> W = <%lu>\r\n", (uint32_t)(++r_head), (uint32_t)(++w_head));
		} else if(cin == 'z') { // [z]ero heads
			printf("Heads zeroed! R = <%lu> W = <%lu>\r\n", (uint32_t)(r_head = 0), (uint32_t)(w_head = 0));
		} else if(cin == 'g') { // [g]et next packet

		} else if(cin == 's') {
			printf("Seeking to read head... [%d]\r\n", f_lseek(&data_file, r_head));
		} else if(cin == 'w') {
			char s_buffer[512];
			uint16_t s_i = 0;
			do {
				HAL_UART_Receive(&huart1, &cin, 1, 0xFFFFFF);
				if(cin == '\r' || cin == '\n')
					break;
				s_buffer[s_i] = cin;
				HAL_UART_Transmit(&huart1, &cin, 1, 0xFFFFFF);
			} while (cin != '\r' && cin != '\r' && ++s_i < (sizeof s_buffer) - 1);
			s_buffer[s_i + 1] = '\0';

			printf("\rWriting: \"%s\"\r\n", s_buffer);
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
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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
