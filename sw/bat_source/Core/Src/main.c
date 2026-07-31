/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "hrtim.h"
#include "i2c.h"
#include "quadspi.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "i2c.h"
#include "driver_pca9554.h"
#include "aux_io_ctrl.h"
#include "dac.h"
#include "bq76905.h"
#include "bq76905_config.h"
#include "ctrl_main.h"
#include "timer.h"
#include "display.h"
#include "opt3004.h"
#include "lp581x.h"
#include "hrtim.h"
#include "ui_ctrl.h"
#include "statemachine.h"
#include "adc.h"
#include "cli.h"
#include "config_store.h"
#include <stdio.h>
#include "stdint.h"
#include "lcd.h"
#include "w25n01gv.h"
#include "icon_store.h"
#include "ads131m04.h"
#include "protection.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
BQ76905_handle bms;
ADC_MEAS_DATA adc_data;
ADS131M04_handle ext_adc;
w25n01gv_handle flash;
w25n01gv_status_t flash_status = W25N01GV_OK;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	EVENT_STRUCT e;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

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
  MX_DMA_Init();
  MX_HRTIM1_Init();
  MX_QUADSPI1_Init();
  MX_SPI4_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_ADC5_Init();
  MX_DAC1_Init();
  MX_SPI3_Init();
  MX_I2C4_Init();
  MX_TIM4_Init();
  MX_TIM16_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  /*
   * Init the event and error queues. Must run before timer_init()/
   * timer_start(), since the timer interrupt is the first thing that can
   * post an event via event_Add(), and before anything that can call
   * error_Add() (e.g. i2c_Add() below).
   */
  event_Init();
  error_Init();

  /*
   * Setup internal timer structure
   */
  //gpio_turnOn();
  timer_init();

  /*
   * DAC test setup
   */
  dac_setValueRef2(2048);

  /*
   * CLI Setup
   */
  cli_init(&huart2);

  /*
   * Init UI control (LEDs)
   */
  ui_ctrl_init();


  /*
   * Load calibration / ID data from EEPROM (or default it, on first
   * boot / missing chip). Must run before adc_init() and
   * ctrl_main_init() so both see the loaded calibration.
   */
  config_store_init();

  /*
   * ADC Init
   */
  adc_init(ext_adc.channelData);
  ADS131M04_init(&ext_adc, &hspi3, SPI_CS_GPIO_Port, SPI_CS_Pin, ADC_SYNC_RESET_N_GPIO_Port, ADC_SYNC_RESET_N_Pin);
  adc_configure_mode(STATEMACHINE_MODE_10A_OUT);
  adc_start();
  /*
   * Wakeup BMS and configure if necessary
   */
  GPIO_SET_BMS_CTRL_WAKEUP(1);
  HAL_Delay(100);
  GPIO_SET_BMS_CTRL_WAKEUP(0);
  BQ76905_init(&bms, BQ76905_I2C_ADDRESS);
  bms.battery_capacity_mAs = config_store.calibration.battery_capacity_mAs;


  /*
   * PWM Init
   */
  hrtim_set_freq(HRTIM_CHANNEL_PRIM, 50000);
  hrtim_set_freq(HRTIM_CHANNEL_SEK, 150000);
  hrtim_set_duty(HRTIM_CHANNEL_PRIM, 0.985);

  hrtim_start_timer();
  timer_start();

  /*
   * QSPI flash memory init
   */
  flash_status = w25n01gv_init(&flash, &hqspi1, 1000);
/*  if (flash_status == HAL_OK){
    flash_status = w25n01gv_wait_busy(&flash);
  }
  if (flash_status == HAL_OK){
  	flash_status = w25n01gv_buffer_mode_disable(&flash);
  }
  if (flash_status == HAL_OK){
    flash_status = w25n01gv_wait_busy(&flash);
  }*/
  if (flash_status != W25N01GV_OK) {
    printf("Flash initialisation failed!\r\n");
  }
  icon_store_init(); // safe even if flash init failed above -- just reports unavailable

  gpio_turnOn();

  LCD_init();

  protection_init();
  ctrl_main_init();
  statemachine_init();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


	  /*
	   * LED test
	   */
	  /*if (led_current < 192)
	  {
		  led_current *= 2;
	  }
	  else
	  {
		  led_current = 3;
	  }
	  uint8_t led_currents[] = {0x14, led_current, led_current, led_current};
	  i2c_WriteBlocking(led_address, led_currents, sizeof(led_currents));
	  */

	  cli_loop();


	  /*
	   * Ambient light sensor test
	   */


	  /*amb_brightness = opt3004_readLux(&hamb);

	  led_current = amb_brightness/1000;

	  if (led_current > 200)
	  {
		  led_current = 200;
	  }
	  else if (amb_brightness < 1000)
	  {
		  led_current = 2;
	  }
	  uint8_t led_currents[] = {0x14, led_current, led_current/2, led_current/2};
	  lp581x_setAnalogDimming(&hled, 0, led_current);
	  lp581x_setAnalogDimming(&hled, 0, led_current/2);
	  lp581x_setAnalogDimming(&hled, 0, led_current/2);

	  i2c_WriteBlocking(led_address, led_currents, sizeof(led_currents));*/

	  e = event_Get();
	  switch(e.event){
		case EVENT_VOID:
		case EVENT_INIT:
			break;
		case EVENT_IIC_RX:
		case EVENT_IIC_TX:
			if(e.callback != 0)
				e.callback(e.argument);
			break;
		case EVENT_IIC_ERROR:
			break;
		case EVENT_EEPROM_READ:
			break;
		case EVENT_EEPROM_TIMEOUT:
			break;
		case EVENT_SM_STEP:
			statemachine_step();
			break;
		case EVENT_BMS_TIMER:
			BQ76905_readAllValuesAsync(&bms);
			break;
		case EVENT_EVENT:
		default:
			break;

	  }


	  /*
	   * Loop delay
	   */
	  HAL_Delay(1);

	  //LCD_Test();

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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * External ADC finished SPI communication
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
	ADS131M04_handleSPIComplete(&ext_adc);
}


/**
 * Interrupt for external ADC
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == ADC_DRDY_N_Pin) {
		ADS131M04_handleDRDYInterrupt(&ext_adc);
	} else {
		// __NOP();
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
