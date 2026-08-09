/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */
#include "irq_priority.h"

#define I2C_QUEUE_SIZE 64

/**
 * Enum to define the I2C communication direction.
 */
typedef enum
{
	I2CM_READ, //!< I2CM_READ
	I2CM_WRITE //!< I2CM_WRITE
} I2C_METHODE;

/**
 * Enum that represents the state in the work task.
 */
typedef enum
{
	WORK_I2C_IDLE,//!< WORK_I2C_IDLE
	WORK_I2C_RUN  //!< WORK_I2C_RUN
} WORK_I2C;

/**
 * Struct to serialize an I2C message.
 */
typedef struct
{
	I2C_METHODE methode;
	uint8_t address;
	uint32_t numbytes;
	uint8_t* buf;
	void* callback;
	void* argument;
	void* error_callback;
} I2C_STRUCT;


struct i2c_queue{
	I2C_STRUCT queue[I2C_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
};
volatile struct i2c_queue i2c_queue;

uint8_t i2c_head, i2c_tail;
volatile WORK_I2C i2c_work;
I2C_HandleTypeDef hi2c4;
I2C_HandleTypeDef *i2c_handle = &hi2c4;
volatile uint8_t i2c_exclusiveAccess;

/**
 * Works through the I2C queue and sends an I2C message.
 */
void i2c_Get(void)
{
	if (!i2c_exclusiveAccess && queue_NotEmpty(i2c_queue))
	{
		i2c_work = WORK_I2C_RUN;
		switch (queue_Get(i2c_queue).methode)
		{
			case I2CM_READ:
			HAL_I2C_Master_Receive_DMA(i2c_handle, queue_Get(i2c_queue).address,
					queue_Get(i2c_queue).buf, queue_Get(i2c_queue).numbytes);
			//printf("i2c.c: Read i2C Command\n\r");
			break;
			case I2CM_WRITE:
			HAL_I2C_Master_Transmit_DMA(i2c_handle, queue_Get(i2c_queue).address,
					queue_Get(i2c_queue).buf, queue_Get(i2c_queue).numbytes);
			//queue_Pop(i2c_queue, I2C_QUEUE_SIZE);
			//i2c_work = WORK_I2C_IDLE;
			//printf("i2c.c: Write i2C Command\n\r");
			break;
		}
	}
	else
	{
		i2c_work = WORK_I2C_IDLE;
	}
}

/**
 * Adds an I2C message to the queue.
 * @param methode communication direction.
 * @param address I2C address of the target.
 * @param numbytes number of bytes to send or receive.
 * @param buf pointer to the data.
 * @param ret pointer that is returned to the callback to identify the caller.
 * @param wait can be set to one, if the add should be blocking.
 */
void i2c_Add(I2C_METHODE methode, uint8_t address, uint32_t numbytes,
		uint8_t *buf, void* callback, void* argument, void* error_callback, uint8_t wait) {
	/*
	 * i2c_Add() runs in main-loop context, but the queue it manipulates
	 * (head/tail/i2c_work) is also touched from I2C4 ISR context by
	 * HAL_I2C_MasterRxCpltCallback()/MasterTxCpltCallback()/ErrorCallback()
	 * via queue_Pop()/i2c_Get(). Guard the check-push-start sequence with a
	 * PRIMASK-saving critical section rather than an unconditional
	 * __enable_irq() at the end, since a bare enable would be wrong if the
	 * caller itself was already inside a critical section.
	 */
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if (queue_Full(i2c_queue, I2C_QUEUE_SIZE)) {
		__set_PRIMASK(primask);
		error_Add(ERROR_IIC_OVF, address);
		return;
	}

	queue_Set(i2c_queue).methode = methode;
	queue_Set(i2c_queue).address = address;
	queue_Set(i2c_queue).numbytes = numbytes;
	queue_Set(i2c_queue).buf = buf;
	queue_Set(i2c_queue).callback = callback;
	queue_Set(i2c_queue).argument = argument;
	queue_Set(i2c_queue).error_callback = error_callback;
	queue_Push(i2c_queue, I2C_QUEUE_SIZE);

	if (i2c_work == WORK_I2C_IDLE) {
		i2c_Get();
	}

	__set_PRIMASK(primask);
}

/**
 * Handles the interrupt, if read was successful
 * @param hi2c Handle to I2C interface
 */
void HAL_I2C_MasterRxCpltCallback (I2C_HandleTypeDef * hi2c){
	event_Add(EVENT_IIC_RX, queue_Get(i2c_queue).callback, queue_Get(i2c_queue).argument);
	queue_Pop(i2c_queue, I2C_QUEUE_SIZE);
	i2c_Get();
}

/**
 * Handles the interrupt, if write was successful
 * @param hi2c Handle to I2C interface
 */
void HAL_I2C_MasterTxCpltCallback (I2C_HandleTypeDef * hi2c){
	event_Add(EVENT_IIC_TX, queue_Get(i2c_queue).callback, queue_Get(i2c_queue).argument);
	queue_Pop(i2c_queue, I2C_QUEUE_SIZE);
	i2c_Get();
}

void HAL_I2C_ErrorCallback (I2C_HandleTypeDef * hi2c){
	event_Add(EVENT_IIC_ERROR, queue_Get(i2c_queue).error_callback, queue_Get(i2c_queue).argument);
	queue_Pop(i2c_queue, I2C_QUEUE_SIZE);
	i2c_Get();
}

/* USER CODE END 0 */

I2C_HandleTypeDef hi2c4;
DMA_HandleTypeDef hdma_i2c4_rx;
DMA_HandleTypeDef hdma_i2c4_tx;

/* I2C4 init function */
void MX_I2C4_Init(void)
{

  /* USER CODE BEGIN I2C4_Init 0 */
	queue_Init(i2c_queue);
	i2c_work = WORK_I2C_IDLE;
	i2c_exclusiveAccess = 0;

  /* USER CODE END I2C4_Init 0 */

  /* USER CODE BEGIN I2C4_Init 1 */

  /* USER CODE END I2C4_Init 1 */
  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x10F32A82;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C4_Init 2 */

  /* USER CODE END I2C4_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(i2cHandle->Instance==I2C4)
  {
  /* USER CODE BEGIN I2C4_MspInit 0 */

  /* USER CODE END I2C4_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
    PeriphClkInit.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**I2C4 GPIO Configuration
    PC6     ------> I2C4_SCL
    PC7     ------> I2C4_SDA
    */
    GPIO_InitStruct.Pin = I2C_SCL_Pin|I2C_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_I2C4;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* I2C4 clock enable */
    __HAL_RCC_I2C4_CLK_ENABLE();

    /* I2C4 DMA Init */
    /* I2C4_RX Init */
    hdma_i2c4_rx.Instance = DMA1_Channel8;
    hdma_i2c4_rx.Init.Request = DMA_REQUEST_I2C4_RX;
    hdma_i2c4_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2c4_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c4_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c4_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c4_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c4_rx.Init.Mode = DMA_NORMAL;
    hdma_i2c4_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_i2c4_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(i2cHandle,hdmarx,hdma_i2c4_rx);

    /* I2C4_TX Init */
    hdma_i2c4_tx.Instance = DMA2_Channel1;
    hdma_i2c4_tx.Init.Request = DMA_REQUEST_I2C4_TX;
    hdma_i2c4_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2c4_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c4_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c4_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c4_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c4_tx.Init.Mode = DMA_NORMAL;
    hdma_i2c4_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_i2c4_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(i2cHandle,hdmatx,hdma_i2c4_tx);

    /* I2C4 interrupt Init */
    HAL_NVIC_SetPriority(I2C4_EV_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(I2C4_EV_IRQn);
    HAL_NVIC_SetPriority(I2C4_ER_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(I2C4_ER_IRQn);
  /* USER CODE BEGIN I2C4_MspInit 1 */
  HAL_NVIC_SetPriority(I2C4_EV_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
  HAL_NVIC_SetPriority(I2C4_ER_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
  /* USER CODE END I2C4_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C4)
  {
  /* USER CODE BEGIN I2C4_MspDeInit 0 */

  /* USER CODE END I2C4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C4_CLK_DISABLE();

    /**I2C4 GPIO Configuration
    PC6     ------> I2C4_SCL
    PC7     ------> I2C4_SDA
    */
    HAL_GPIO_DeInit(I2C_SCL_GPIO_Port, I2C_SCL_Pin);

    HAL_GPIO_DeInit(I2C_SDA_GPIO_Port, I2C_SDA_Pin);

    /* I2C4 DMA DeInit */
    HAL_DMA_DeInit(i2cHandle->hdmarx);
    HAL_DMA_DeInit(i2cHandle->hdmatx);

    /* I2C4 interrupt Deinit */
    HAL_NVIC_DisableIRQ(I2C4_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C4_ER_IRQn);
  /* USER CODE BEGIN I2C4_MspDeInit 1 */

  /* USER CODE END I2C4_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/**
 * Enables the I2C interface.
 */
void i2c_Start(void)
{
	// Enable interrupt?
}

/**
 * Adds a read message to the queue.
 * @param address address I2C address of the target.
 * @param buf numbytes number of bytes to send or receive.
 * @param numbytes buf pointer to the data.
 * @param ret ret pointer that is returned to the callback to identify the caller.
 */
void i2c_Read(uint8_t address, uint8_t* buf, uint32_t numbytes, void* callback, void* argument, void* error_callback)
{
	i2c_Add(I2CM_READ, address, numbytes, buf, callback, argument, error_callback, 0);
}

/**
 * Adds a write message to the queue.
 * @param address address I2C address of the target.
 * @param buf numbytes number of bytes to send or receive.
 * @param numbytes buf pointer to the data.
 * @param ret ret pointer that is returned to the callback to identify the caller.
 */
void i2c_Write(uint8_t address, uint8_t* buf, uint32_t numbytes, void* callback, void* argument, void* error_callback)
{
	i2c_Add(I2CM_WRITE, address, numbytes, buf, callback, argument, error_callback, 0);
}

/**
 * Performs a blocking write to the target. Waits until current operation is finished.
 * @param address address I2C address of the target.
 * @param buf numbytes number of bytes to send or receive.
 * @param numbytes buf pointer to the data.
 * @param ret ret pointer that is returned to the callback to identify the caller.
 */
uint8_t i2c_WriteBlocking(uint8_t address, uint8_t* buf, uint32_t numbytes){
	uint8_t retval;

	i2c_exclusiveAccess = 1;
	while (i2c_work!=WORK_I2C_IDLE)
		HAL_Delay(1);
	retval = HAL_I2C_Master_Transmit(i2c_handle, address, buf, numbytes, 100);
	//printf("i2c.c: Error in write: %d\n", retval);
	i2c_exclusiveAccess = 0;
	i2c_Get();
	return retval;
}

/**
 * Performs a blocking read to the target. Waits until current operation is finished.
 * @param address address I2C address of the target.
 * @param buf numbytes number of bytes to send or receive.
 * @param numbytes buf pointer to the data.
 * @param ret ret pointer that is returned to the callback to identify the caller.
 */
uint8_t i2c_ReadBlocking(uint8_t address, uint8_t* buf, uint32_t numbytes){
	uint8_t retval;

	i2c_exclusiveAccess = 1;
	while (i2c_work!=WORK_I2C_IDLE)
		HAL_Delay(1);
	retval = HAL_I2C_Master_Receive(i2c_handle, address, buf, numbytes, 100);
	i2c_exclusiveAccess = 0;
	i2c_Get();
	return retval;
}

/**
 * Checks whether a device acknowledges its address on the bus, without
 * transferring any data. Waits until the current queued operation is
 * finished, like the other _Blocking helpers.
 * @param address address I2C address of the target.
 * @return 1 if the device acknowledged, 0 otherwise.
 */
uint8_t i2c_IsDeviceReadyBlocking(uint8_t address){
	uint8_t ready;

	i2c_exclusiveAccess = 1;
	while (i2c_work!=WORK_I2C_IDLE)
		HAL_Delay(1);
	ready = (HAL_I2C_IsDeviceReady(i2c_handle, address, 1, 10) == HAL_OK);
	i2c_exclusiveAccess = 0;
	i2c_Get();
	return ready;
}

/* USER CODE END 1 */
