#include <ads131m04.h>
#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "debug.h"
#include "irq_priority.h"

const int32_t adc_offset[4]={
		0,
		0,
		0,
		0
};

/**
 * @brief Sends a 16-bit command to the ADC.
 * @param cmd Command to send
 */
void ADS131M04_sendCommand(ADS131M04_handle* handle, uint16_t cmd);

/**
 * @brief Configures the SPI peripheral for ADS131M04 communication.
 */
void ADS131M04_configureSPI(ADS131M04_handle* handle);

void ADS131M04_selftest(ADS131M04_handle* handle);

void ADS131M04_init(ADS131M04_handle* handle, SPI_HandleTypeDef* _hspi, GPIO_TypeDef* _drdyPort, uint16_t _drdyPin,
		 GPIO_TypeDef* _rstPort, uint16_t _rstPin){
    uint16_t temp;
	handle->hspi = _hspi;
    handle->drdyPort=_drdyPort;
    handle->drdyPin = _drdyPin;
    handle->rstPort=_rstPort;
    handle->rstPin = _rstPin;

    HAL_GPIO_WritePin(handle->rstPort,  handle->rstPin, 1);
    HAL_Delay(1);
    memset(handle->txBuffer,0,sizeof(handle->txBuffer));
    /* EXTI interrupt init*/

    //ADS131M04_configureSPI(handle);
    ADS131M04_sendCommand(handle, ADS131M04_CMD_RESET);
    temp = ADS131M04_readRegister(handle, ADS131M04_REG_ID);


    ADS131M04_configureADC(handle);
    ADS131M04_readData(handle);
    ADS131M04_readData(handle);

	HAL_Delay(100);

    ADS131M04_selftest(handle);
    /* EXTI2_IRQn is the DRDY pin for this external ADC -- part of the
     * ADS131M04 pipeline tier along with SPI3_IRQn and its DMA channels
     * (spi.c). This is the actual owner of the setting (it also enables the
     * IRQ); gpio.c's MX_GPIO_Init() sets the same value earlier for the
     * same vector so the outcome no longer depends on init order. */
    HAL_NVIC_SetPriority(EXTI2_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}



/* Internal test signal: 2/15 of full scale at any gain (datasheet 8.3.9). */
#define SELFTEST_COUNTS ((int32_t) (2.0 / 15.0 * (1 << 23)))
#define SELFTEST_TOL    (SELFTEST_COUNTS / 4)  // magnitude window
#define SELFTEST_SYM    (SELFTEST_COUNTS / 20) // |pos + neg|

/**
 * Averages n samples of all channels with the given input mux.
 * @param mux ADS131M04_FLAG_CH0_CFG_MUX_*.
 * @param avg Result per channel.
 * @param n Number of samples (~4 ms each).
 */
static void selftest_average(ADS131M04_handle *handle, uint16_t mux,
		int32_t avg[4], int n) {
	int32_t sum[4] = { 0 };
	for (int ch = 0; ch < 4; ch++)
		ADS131M04_setChannelConfig(handle, ch, mux);
	HAL_Delay(20); // filter settling
	for (int i = 0; i < n; i++) {
		ADS131M04_readData(handle);
		HAL_Delay(5);
		for (int ch = 0; ch < 4; ch++)
			sum[ch] += handle->channelData[ch];
	}
	for (int ch = 0; ch < 4; ch++)
		avg[ch] = sum[ch] / n;
}

/**
 * Offset calibration on shorted inputs, then a check of every channel with
 * the internal positive/negative test signals. Sets handle->selftest_ok.
 */
void ADS131M04_selftest(ADS131M04_handle *handle) {
	int32_t zero[4], pos[4], neg[4];

	selftest_average(handle, ADS131M04_FLAG_CH0_CFG_MUX_SHOR, zero, 16);
	for (int ch = 0; ch < 4; ch++)
		ADS131M04_setOffsetCalibration(handle, ch,
				(uint32_t) (zero[ch] + adc_offset[ch]));
	selftest_average(handle, ADS131M04_FLAG_CH0_CFG_MUX_TPOS, pos, 4);
	selftest_average(handle, ADS131M04_FLAG_CH0_CFG_MUX_TNEG, neg, 4);

	handle->selftest_ok = 1;
	for (int ch = 0; ch < 4; ch++) {
		uint8_t ok = labs(pos[ch] - SELFTEST_COUNTS) < SELFTEST_TOL
				&& labs(neg[ch] + SELFTEST_COUNTS) < SELFTEST_TOL
				&& labs(pos[ch] + neg[ch]) < SELFTEST_SYM;
		if (!ok)
			handle->selftest_ok = 0;
#ifdef EXT_ADC_DEBUG
		printf("ExtADC ch%d: zero %ld pos %ld neg %ld (expect +/-%ld) %s\r\n",
				ch, (long) zero[ch], (long) pos[ch], (long) neg[ch],
				(long) SELFTEST_COUNTS, ok ? "OK" : "FAIL");
#endif
	}
	if (!handle->selftest_ok)
		printf("ExtADC self-test FAILED, outputs blocked\r\n");

	for (int ch = 0; ch < 4; ch++)
		ADS131M04_setChannelConfig(handle, ch, ADS131M04_FLAG_CH0_CFG_MUX_DIFF);
	HAL_Delay(20);
	ADS131M04_readData(handle);
	HAL_Delay(5);
}

void ADS131M04_configureSPI(ADS131M04_handle* handle) {
    handle->hspi->Init.Mode = SPI_MODE_MASTER;
    handle->hspi->Init.Direction = SPI_DIRECTION_2LINES;
    handle->hspi->Init.DataSize = SPI_DATASIZE_8BIT;
    handle->hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
    handle->hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
    handle->hspi->Init.NSS = SPI_NSS_SOFT;
    handle->hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    handle->hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
    handle->hspi->Init.TIMode = SPI_TIMODE_DISABLE;
    handle->hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    //hspi->Init.CRCPolynomial = 7;
    HAL_SPI_Init(handle->hspi);
}

void ADS131M04_sendCommand(ADS131M04_handle* handle, uint16_t cmd) {
	handle->txBuffer[0] = (cmd >> 8) & 0xFF;
	handle->txBuffer[1] = (cmd & 0xFF);
	handle->txBuffer[2] = 0x00;
	handle->txBuffer[3] = 0 ;
	handle->txBuffer[4] = 0;
	handle->txBuffer[5] =  0x00; // Padding       ;
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 0);
    HAL_SPI_TransmitReceive(handle->hspi, handle->txBuffer, handle->rxBuffer, 3*6, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 1);
}

void ADS131M04_writeRegister(ADS131M04_handle* handle, uint8_t reg, uint16_t value) {
    handle->txBuffer[0] = ((ADS131M04_CMD_WREG>>8) | (reg>>1));
    handle->txBuffer[1] = (reg<<7) |0x00;
    handle->txBuffer[2] = 0x00;
    handle->txBuffer[3] = ((value >> 8) & 0xFF) ;
    handle->txBuffer[4] = (value & 0xFF)        ;
    handle->txBuffer[5] =  0x00; // Padding       ;

    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 0);
    HAL_SPI_TransmitReceive(handle->hspi, handle->txBuffer, handle->rxBuffer, 3*6, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 1);
}

uint16_t ADS131M04_readRegister(ADS131M04_handle* handle, uint8_t reg) {

	handle->txBuffer[0] = ((ADS131M04_CMD_RREG>>8) | (reg>>1));
	handle->txBuffer[1] = (reg<<7) |0x00;
	handle->txBuffer[2] = 0x00;
	handle->txBuffer[3] = 0 ;
	handle->txBuffer[4] = 0;
	handle->txBuffer[5] =  0x00; // Padding       ;
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 0);
    HAL_SPI_TransmitReceive(handle->hspi, handle->txBuffer, handle->rxBuffer, 3*6, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 1);
    memset(handle->txBuffer, 0, sizeof(handle->txBuffer));
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 0);
    HAL_SPI_TransmitReceive(handle->hspi, handle->txBuffer, handle->rxBuffer, 3*6, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 1);
    return (handle->rxBuffer[0] << 8) | handle->rxBuffer[1];
}

void ADS131M04_readData(ADS131M04_handle* handle) {
    for (int i = 0; i < 6; ++i) handle->txBuffer[i] = 0x00;
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 0);
    HAL_SPI_TransmitReceive_DMA( handle->hspi, handle->txBuffer, handle->rxBuffer, 3*5);
}

void ADS131M04_handleDRDYInterrupt(ADS131M04_handle* handle) {
	ADS131M04_readData(handle);
}

void ADS131M04_handleSPIComplete(ADS131M04_handle* handle) {
    HAL_GPIO_WritePin(handle->drdyPort, handle->drdyPin, 1);
    for (int i = 1; i < 5; ++i) {
    	handle->channelData[i-1] = ( handle->rxBuffer[i * 3] << 16) | ( handle->rxBuffer[i * 3 + 1] << 8) | handle->rxBuffer[i * 3 + 2];
    	if(handle->channelData[i-1] & 0x800000)
    		handle->channelData[i-1] |= 0xFF000000;
    }
}

void ADS131M04_setGainCalibration(ADS131M04_handle* handle,uint8_t channel, uint32_t gain) {
	ADS131M04_writeRegister(handle, ADS131M04_REG_CH0_GCAL_MSB + ADS131M04_REG_CHANNEL_OFFSET*channel, ((gain>>8) & 0xFFFF));
	ADS131M04_writeRegister(handle, ADS131M04_REG_CH0_GCAL_LSB + ADS131M04_REG_CHANNEL_OFFSET*channel, (gain<<8 & 0xFFFF));
}


void ADS131M04_setOffsetCalibration(ADS131M04_handle* handle,uint8_t channel, uint32_t offset) {
	ADS131M04_writeRegister(handle, ADS131M04_REG_CH0_OCAL_MSB + ADS131M04_REG_CHANNEL_OFFSET*channel, ((offset>>8) & 0xFFFF));
	ADS131M04_writeRegister(handle, ADS131M04_REG_CH0_OCAL_LSB + ADS131M04_REG_CHANNEL_OFFSET*channel, (offset<<8 & 0xFFFF));
}

void ADS131M04_setGain(ADS131M04_handle* handle,uint8_t channel, uint8_t gain) {
    uint16_t reg = ADS131M04_readRegister(handle, ADS131M04_REG_GAIN);
    reg &= ~(0x07 << (channel * 3));
    reg |= (gain & 0x07) << (channel * 3);
    ADS131M04_writeRegister(handle, ADS131M04_REG_GAIN, reg);
}

void ADS131M04_setClockRegister(ADS131M04_handle* handle,uint16_t config) {
	ADS131M04_writeRegister(handle, ADS131M04_REG_CLOCK, config);
}

void ADS131M04_setChannelConfig(ADS131M04_handle* handle, uint8_t channel, uint16_t config) {
	ADS131M04_writeRegister(handle, ADS131M04_REG_CH0_CFG + ADS131M04_REG_CHANNEL_OFFSET*channel, config);
}

int32_t* ADS131M04_getChannelData(ADS131M04_handle* handle) {
    return handle->channelData;
}
