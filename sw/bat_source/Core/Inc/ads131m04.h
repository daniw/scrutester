#ifndef ADS131M04_H
#define ADS131M04_H

#include <ads131m04_config.h>
#include "main.h"
#include "stdint.h"

// Register Addresses
#define ADS131M04_REG_ID              0x00
#define ADS131M04_REG_STATUS          0x01
#define ADS131M04_REG_MODE            0x02
#define ADS131M04_REG_CLOCK           0x03
#define ADS131M04_REG_GAIN            0x04
#define ADS131M04_REG_CFG             0x06
#define ADS131M04_REG_THRSHLD_MSB     0x07
#define ADS131M04_REG_THRSHLD_LSB     0x08

#define ADS131M04_REG_CH0_CFG         0x09
#define ADS131M04_REG_CH0_OCAL_MSB    0x0A
#define ADS131M04_REG_CH0_OCAL_LSB    0x0B
#define ADS131M04_REG_CH0_GCAL_MSB    0x0C
#define ADS131M04_REG_CH0_GCAL_LSB    0x0D
#define ADS131M04_REG_CH1_CFG         0x0E
#define ADS131M04_REG_CH1_OCAL_MSB    0x0F
#define ADS131M04_REG_CH1_OCAL_LSB    0x10
#define ADS131M04_REG_CH1_GCAL_MSB    0x11
#define ADS131M04_REG_CH1_GCAL_LSB    0x12
#define ADS131M04_REG_CH2_CFG         0x13
#define ADS131M04_REG_CH2_OCAL_MSB    0x14
#define ADS131M04_REG_CH2_OCAL_LSB    0x15
#define ADS131M04_REG_CH2_GCAL_MSB    0x16
#define ADS131M04_REG_CH2_GCAL_LSB    0x17
#define ADS131M04_REG_CH3_CFG         0x18
#define ADS131M04_REG_CH3_OCAL_MSB    0x19
#define ADS131M04_REG_CH3_OCAL_LSB    0x1A
#define ADS131M04_REG_CH3_GCAL_MSB    0x1B
#define ADS131M04_REG_CH3_GCAL_LSB    0x1C

#define ADS131M04_REG_CHANNEL_OFFSET  0x5

#define ADS131M04_REG_CRC             0x3E
#define ADS131M04_REG_RESERVED        0x3F



// Register Flags
#define ADS131M04_FLAG_STATUS_LOCK          0x8000
#define ADS131M04_FLAG_STATUS_F_RESYNC      0x4000
#define ADS131M04_FLAG_STATUS_REG_MAP       0x2000
#define ADS131M04_FLAG_STATUS_CRC_ERR       0x1000
#define ADS131M04_FLAG_STATUS_CRC_TYPE      0x0800
#define ADS131M04_FLAG_STATUS_RESET         0x0400
#define ADS131M04_FLAG_STATUS_WLENGTH_16    0x0000
#define ADS131M04_FLAG_STATUS_WLENGTH_24    0x0100
#define ADS131M04_FLAG_STATUS_WLENGTH_32    0x0200
#define ADS131M04_FLAG_STATUS_WLENGTH_32S   0x0300
#define ADS131M04_FLAG_STATUS_DRDY_3        0x0008
#define ADS131M04_FLAG_STATUS_DRDY_2        0x0004
#define ADS131M04_FLAG_STATUS_DRDY_1        0x0002
#define ADS131M04_FLAG_STATUS_DRDY_0        0x0001

#define ADS131M04_FLAG_MODE_REG_CRC_EN      0x2000
#define ADS131M04_FLAG_MODE_RX_CRC_EN       0x1000
#define ADS131M04_FLAG_MODE_CRC_TYPE        0x0800
#define ADS131M04_FLAG_MODE_RESET_CLR       0x0400
#define ADS131M04_FLAG_MODE_WLENGTH_16      0x0000
#define ADS131M04_FLAG_MODE_WLENGTH_24      0x0100
#define ADS131M04_FLAG_MODE_WLENGTH_32      0x0200
#define ADS131M04_FLAG_MODE_WLENGTH_32s     0x0300
#define ADS131M04_FLAG_MODE_TIMEOUT_EN      0x0010
#define ADS131M04_FLAG_MODE_DRDY_SEL_LAG    0x0000
#define ADS131M04_FLAG_MODE_DRDY_SEL_OR     0x0004
#define ADS131M04_FLAG_MODE_DRDY_SEL_LEAD   0x0008
//#define ADS131M04_FLAG_MODE_DRDY_SEL_LEAD   0x000C
#define ADS131M04_FLAG_MODE_DRDY_HIZ        0x0002
#define ADS131M04_FLAG_MODE_DRDY_FMT        0x0001

#define ADS131M04_FLAG_CLOCK_CH3_EN         0x0800
#define ADS131M04_FLAG_CLOCK_CH2_EN         0x0400
#define ADS131M04_FLAG_CLOCK_CH1_EN         0x0200
#define ADS131M04_FLAG_CLOCK_CH0_EN         0x0100
#define ADS131M04_FLAG_CLOCK_TBM            0x0020
#define ADS131M04_FLAG_CLOCK_OSR_128        0x0000
#define ADS131M04_FLAG_CLOCK_OSR_256        0x0004
#define ADS131M04_FLAG_CLOCK_OSR_512        0x0008
#define ADS131M04_FLAG_CLOCK_OSR_1024       0x000C
#define ADS131M04_FLAG_CLOCK_OSR_2048       0x0010
#define ADS131M04_FLAG_CLOCK_OSR_4096       0x0014
#define ADS131M04_FLAG_CLOCK_OSR_8192       0x0018
#define ADS131M04_FLAG_CLOCK_OSR_16256      0x001C
#define ADS131M04_FLAG_CLOCK_PWR_VLP        0x0000
#define ADS131M04_FLAG_CLOCK_PWR_LP         0x0001
#define ADS131M04_FLAG_CLOCK_PWR_HR         0x0002
//#define ADS131M04_FLAG_CLOCK_PWR_HR         0x0003

#define ADS131M04_FLAG_GAIN_1               0x0000
#define ADS131M04_FLAG_GAIN_2               0x0001
#define ADS131M04_FLAG_GAIN_4               0x0002
#define ADS131M04_FLAG_GAIN_8               0x0003
#define ADS131M04_FLAG_GAIN_16              0x0004
#define ADS131M04_FLAG_GAIN_32              0x0005
#define ADS131M04_FLAG_GAIN_64              0x0006
#define ADS131M04_FLAG_GAIN_128             0x0007

#define ADS131M04_FLAG_CFG_GC_DLY_2         0x0000
#define ADS131M04_FLAG_CFG_GC_DLY_4         0x0200
#define ADS131M04_FLAG_CFG_GC_DLY_8         0x0400
#define ADS131M04_FLAG_CFG_GC_DLY_16        0x0600
#define ADS131M04_FLAG_CFG_GC_DLY_32        0x0800
#define ADS131M04_FLAG_CFG_GC_DLY_64        0x0A00
#define ADS131M04_FLAG_CFG_GC_DLY_128       0x0C00
#define ADS131M04_FLAG_CFG_GC_DLY_256       0x0E00
#define ADS131M04_FLAG_CFG_GC_DLY_512       0x1000
#define ADS131M04_FLAG_CFG_GC_DLY_1024      0x1200
#define ADS131M04_FLAG_CFG_GC_DLY_2048      0x1400
#define ADS131M04_FLAG_CFG_GC_DLY_4096      0x1600
#define ADS131M04_FLAG_CFG_GC_DLY_8192      0x1800
#define ADS131M04_FLAG_CFG_GC_DLY_16384     0x1A00
#define ADS131M04_FLAG_CFG_GC_DLY_32768     0x1C00
#define ADS131M04_FLAG_CFG_GC_DLY_65536     0x1E00
#define ADS131M04_FLAG_CFG_GC_EN            0x0100
#define ADS131M04_FLAG_CFG_CD_ALLCH         0x0080
#define ADS131M04_FLAG_CFG_CD_NUM_1         0x0000
#define ADS131M04_FLAG_CFG_CD_NUM_2         0x0010
#define ADS131M04_FLAG_CFG_CD_NUM_4         0x0020
#define ADS131M04_FLAG_CFG_CD_NUM_8         0x0030
#define ADS131M04_FLAG_CFG_CD_NUM_16        0x0040
#define ADS131M04_FLAG_CFG_CD_NUM_32        0x0050
#define ADS131M04_FLAG_CFG_CD_NUM_64        0x0060
#define ADS131M04_FLAG_CFG_CD_NUM_128       0x0070
#define ADS131M04_FLAG_CFG_CD_LEN_128       0x0000
#define ADS131M04_FLAG_CFG_CD_LEN_256       0x0002
#define ADS131M04_FLAG_CFG_CD_LEN_512       0x0004
#define ADS131M04_FLAG_CFG_CD_LEN_768       0x0006
#define ADS131M04_FLAG_CFG_CD_LEN_1280      0x0008
#define ADS131M04_FLAG_CFG_CD_LEN_1792      0x000A
#define ADS131M04_FLAG_CFG_CD_LEN_2560      0x000C
#define ADS131M04_FLAG_CFG_CD_LEN_3584      0x000E
#define ADS131M04_FLAG_CFG_CD_EN            0x0001

#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_DIS    0x0000
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_4      0x0001
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_8      0x0002
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_16     0x0003
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_32     0x0004
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_64     0x0005
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_128    0x0006
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_256    0x0007
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_512    0x0008
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_1024   0x0009
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_2048   0x000A
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_4096   0x000B
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_8192   0x000C
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_16384  0x000D
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_32768  0x000E
#define ADS131M04_FLAG_THRSHLD_LSB_DCBLOCK_65536  0x000F

#define ADS131M04_FLAG_CH0_CFG_DCBLK0_DIS0        0x0004
#define ADS131M04_FLAG_CH0_CFG_MUX_DIFF           0x0000
#define ADS131M04_FLAG_CH0_CFG_MUX_SHOR           0x0001
#define ADS131M04_FLAG_CH0_CFG_MUX_TPOS           0x0002
#define ADS131M04_FLAG_CH0_CFG_MUX_TNEG           0x0003


// Command Definitions
#define ADS131M04_CMD_NULL       0x0000
#define ADS131M04_CMD_RESET      0x0011
#define ADS131M04_CMD_STANDBY    0x0022
#define ADS131M04_CMD_WAKEUP     0x0033
#define ADS131M04_CMD_LOCK       0x0555
#define ADS131M04_CMD_UNLOCK     0x0655
#define ADS131M04_CMD_RREG       0xA000
#define ADS131M04_CMD_WREG       0x6000



typedef struct{
    SPI_HandleTypeDef* hspi;
    GPIO_TypeDef* drdyPort;
    uint16_t drdyPin;
    GPIO_TypeDef* rstPort;
    uint16_t rstPin;
    uint8_t txBuffer[6*3];
    uint8_t rxBuffer[6*3];
    int32_t channelData[4];
    uint8_t selftest_ok;   // boot self-test passed (ADS131M04_selftest())
} ADS131M04_handle;


/**
 * @brief Sets up the handle
 */
void ADS131M04_init(ADS131M04_handle* handle, SPI_HandleTypeDef* _hspi, GPIO_TypeDef* _drdyPort, uint16_t _drdyPin,
		 GPIO_TypeDef* _rstPort, uint16_t _rstPin);



    /**
     * @brief Reads ADC conversion data using DMA.
     */
    void ADS131M04_readData(ADS131M04_handle* handle);

    /**
     * @brief Handles the DRDY interrupt and initiates SPI read.
     */
    void ADS131M04_handleDRDYInterrupt(ADS131M04_handle* handle);

    /**
     * @brief Handles the SPI DMA transfer complete callback.
     */
    void ADS131M04_handleSPIComplete(ADS131M04_handle* handle);

    /**
     * @brief Writes a 16-bit value to the specified register.
     * @param reg Register address
     * @param value Value to write
     */
    void ADS131M04_writeRegister(ADS131M04_handle* handle, uint8_t reg, uint16_t value);

    /**
     * @brief Reads a 16-bit value from the specified register.
     * @param reg Register address
     * @return Value read
     */
    uint16_t ADS131M04_readRegister(ADS131M04_handle* handle, uint8_t reg);

    /**
     * @brief Sets the gain calibration value for a specific channel.
     * @param channel Channel number (0-3)
     * @param value Calibration value
     */
    void ADS131M04_setGainCalibration(ADS131M04_handle* handle, uint8_t channel, uint32_t gain);

    /**
     * @brief Sets the offset calibration value for a specific channel.
     * @param channel Channel number (0-3)
     * @param value Calibration value
     */
    void ADS131M04_setOffsetCalibration(ADS131M04_handle* handle, uint8_t channel, uint32_t offset);

    /**
     * @brief Sets the gain for a specific channel.
     * @param channel Channel number (0-3)
     * @param gain Gain value
     */
    void ADS131M04_setGain(ADS131M04_handle* handle, uint8_t channel, uint8_t gain);

    /**
     * @brief Sets the configuration for the clock register.
     * @param config Configuration value
     */
    void ADS131M04_setClockRegister(ADS131M04_handle* handle, uint16_t config);


    /**
     * @brief Sets the configuration for a specific channel.
     * @param channel Channel number (0-3)
     * @param config Configuration value
     */
    void ADS131M04_setChannelConfig(ADS131M04_handle* handle, uint8_t channel, uint16_t config);

    /**
     * @brief Returns the latest ADC data for all channels.
     * @return Pointer to array of 4 channel values
     */
    int32_t* ADS131M04_getChannelData(ADS131M04_handle* handle);

    /**
     * @brief Configures the ADC with prestored configuration values.
     */
    void ADS131M04_configureADC(ADS131M04_handle* handle);

#endif
