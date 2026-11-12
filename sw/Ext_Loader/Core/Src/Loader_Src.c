/*
 * Loader_Src.c
 *
 *  Created on: May 7, 2026
 *      Author: DWI
 */

#include "Loader_Src.h"

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "quadspi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "w25n01gv.h"
#include "stm32g4xx_hal.h"
#include "stdio.h"
#include "cli.h"

extern w25n01gv_handle flash;

int Init (void) {
	w25n01gv_status_t status;
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_QUADSPI1_Init();
	MX_USART2_UART_Init();

	gpio_turnOn();

	/*
	 * CLI Setup
	 */
	cli_init(&huart2);

	/*
	* QSPI flash memory init
	*/
	//printf("Initializing external flash\n");
	status = w25n01gv_init(&flash, &hqspi1, 1000);

	if (status == W25N01GV_OK) {
		//printf("Success!\n");
		return 1;
	}
	else {
		//printf("Init: Error %u\n", status);
		return 0;
	}
}

int Write(uint32_t Address, uint32_t Size, uint8_t* buffer) {
	printf("W a: 0x%lX, s: %ld\n", Address, Size);
	w25n01gv_status_t status;
	Address = Address & (W25N01GV_SIZE-1);
	status = w25n01gv_write(&flash, buffer, Address, Size);

	if (status == W25N01GV_OK) {
		//printf("Success!\n");
		return 1;
	}
	else {
		//printf("W err %u, a: 0x%lX, s: %ld\n", status, Address, Size);
		printf("W err %u\n", status);
		return 0;
	}
}

int SectorErase(uint32_t StartAddress, uint32_t EndAddress) {
	w25n01gv_status_t status;
	StartAddress = StartAddress & (W25N01GV_SIZE-1);
	EndAddress   = EndAddress   & (W25N01GV_SIZE-1);
	//printf("Erasing sector of external flash\n");
	//printf("Start address: 0x%4lX\n", StartAddress);
	//printf("End address:   0x%4lX\n", EndAddress);
	status = w25n01gv_erase(&flash, StartAddress, EndAddress);

	if (status == W25N01GV_OK) {
		//printf("Success!\n");
		return 1;
	}
	else {
		printf("Er: Err %u\n", status);
		return 0;
	}
}

int Read(uint32_t Address, uint32_t Size, uint8_t* buffer) {
	//printf("Reading external flash\n");
	//printf("Address: 0x%4lX\n", Address);
	//printf("Size:    0x%4lX\n", Size);
	printf("R a: 0x%lX, s: %ld\n", Address, Size);
	w25n01gv_status_t status;
	Address = Address & (W25N01GV_SIZE-1);
	status = w25n01gv_read(&flash, buffer, Address, Size);

	if (status == W25N01GV_OK) {
		//printf("Success!\n");
		return 1;
	}
	else {
		printf("Err: %u\n", status);
		return 0;
	}
}

//uint64_t Verify(uint32_t FlashAddr, uint32_t RAMBufferAddr, uint32_t Size) {
//printf("Verifying external flash\n");
//	return;
//}

int MassErase(void) {
	//printf("Erasing complete external flash\n");
	w25n01gv_status_t status;
	status = w25n01gv_erase(&flash, 0, W25N01GV_SIZE-1);

	if (status == W25N01GV_OK) {
		//printf("Success!\n");
		return 1;
	}
	else {
		printf("M Er: Err %u\n", status);
		return 0;
	}
}

//int Checksum() { // Todo: Parameters not specified in documentation
//printf("Calculating checksum of external flash\n");
//	return;
//}
