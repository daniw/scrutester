/*
 * driver_bq76905.h
 *
 *  Created on: Jun 14, 2025
 *      Author: ahorat
 */

#ifndef SRC_DRIVER_BQ76905_H_
#define SRC_DRIVER_BQ76905_H_

#include <stdint.h>
#include <stdbool.h>
#include "stdio.h"

// Register addresses
#define BQ76905_COMMAND_ALERT_A        0x02
#define BQ76905_COMMAND_STATUS_A       0x03
#define BQ76905_COMMAND_ALERT_B        0x04
#define BQ76905_COMMAND_STATUS_B       0x05

#define BQ76905_COMMAND_BAT_STATUS     0x12
#define BQ76905_COMMAND_VCELL_1        0x14
#define BQ76905_COMMAND_VCELL_2        0x16
#define BQ76905_COMMAND_VCELL_3        0x18
#define BQ76905_COMMAND_VCELL_4        0x1A
#define BQ76905_COMMAND_VCELL_5        0x1C

#define BQ76905_COMMAND_VREG18         0x22
#define BQ76905_COMMAND_VSS            0x24
#define BQ76905_COMMAND_VSTACK         0x26
#define BQ76905_COMMAND_TEMP_INT       0x28
#define BQ76905_COMMAND_TEMP_SENSOR    0x2A

#define BQ76905_COMMAND_CURRENT_RAW    0x36
#define BQ76905_COMMAND_CURRENT_CC2    0x3A
#define BQ76905_COMMAND_CURRENT_CC1    0x3C

#define BQ76905_COMMAND_ALARM_STATUS   0x62
#define BQ76905_COMMAND_ALARM_RAW      0x64
#define BQ76905_COMMAND_ALARM_EN       0x66
#define BQ76905_COMMAND_CTRL_FET       0x68
#define BQ76905_COMMAND_CTRL_REGOUT    0x69
#define BQ76905_COMMAND_CTRL_DSG_PWM   0x6A
#define BQ76905_COMMAND_CTRL_CHG_PWM   0x6C



// Use little Endian
#define BQ76905_SUBCOMMAND_ADDRESS_LOW    0x3E
#define BQ76905_SUBCOMMAND_ADDRESS_HIGH   0x3F

#define BQ76905_SUBCOMMAND_RESET_PASSQ      0x0005 
#define BQ76905_SUBCOMMAND_EXIT_DEEPSLEEP   0x000E
#define BQ76905_SUBCOMMAND_DEEPSLEEP        0x000F
#define BQ76905_SUBCOMMAND_SHUTDOWN         0x0010
#define BQ76905_SUBCOMMAND_RESET            0x0012
#define BQ76905_SUBCOMMAND_FET_ENABLE       0x0022
#define BQ76905_SUBCOMMAND_SEAL             0x0030
#define BQ76905_SUBCOMMAND_SET_CFGUPDATE    0x0090
#define BQ76905_SUBCOMMAND_EXIT_CFGUPDATE   0x0092
#define BQ76905_SUBCOMMAND_SLEEP_ENABLE     0x0099
#define BQ76905_SUBCOMMAND_SLEEP_DISABLE    0x009A

#define BQ76905_SUBCOMMAND_DEVICE_NUMBER    0x0001
#define BQ76905_SUBCOMMAND_FW_VERSION       0x0002
#define BQ76905_SUBCOMMAND_HW_VERSION       0x0003
#define BQ76905_SUBCOMMAND_PASSQ            0x0004
#define BQ76905_SUBCOMMAND_SECURITY_KEYS    0x0035
#define BQ76905_SUBCOMMAND_CB_ACTIVE_CELLS  0x0083
#define BQ76905_SUBCOMMAND_PROG_TIMER       0x0094
#define BQ76905_SUBCOMMAND_PROT_RECOVERY    0x009B

#define BQ76905_TRANSFER_BUFFER_LOW       0x40
#define BQ76905_TRANSFER_BUFFER_HIGH      0x5F

#define BQ76905_TRANSFER_CHKSUM      0x60
#define BQ76905_TRANSFER_LEN         0x61

typedef enum {
        IDLE,
        READ_SAFETY,
        READ_CELLVOLTAGE,
        READ_VOLTAGE,
        READ_CURRENT,
        READ_ALARM,
		READ_PASSQ
    } BQ76905_asyncState;

typedef struct{

    uint8_t address;
    bool crcEnabled;

	struct{
	    uint8_t safetyAlertA;
	    uint8_t safetyStatusA;
	    uint8_t safetyAlertB;
	    uint8_t safetyStatusB;
	} SafetyRegisters;

	struct{
	    uint16_t BatteryStatus;
	    uint16_t CellVoltages[5];
	} CellVoltageRegisters;


	struct{
	    uint16_t Reg18Voltage;
	    uint16_t VssVoltage;
	    uint16_t StackVoltage;
	    uint16_t IntTemp;
	    uint16_t TSTemp;
	} VoltageRegisters;

	struct{
	    int32_t RawCurrent;
	    int16_t CC2Current;
	    int16_t CC1Current;
	} CurrentRegisters;


	struct{
	    uint16_t AlarmStatus;
	    uint16_t AlarmRawStatus;
	    uint16_t AlarmEnable;
	    uint8_t FETCtrl;
	    uint8_t RegoutCtrl;
	    uint16_t DSGFetPWM;
	    uint16_t CHGFetPWM;
	} SystemCtrl;

	// Mirrors exactly the BQ76905_SUBCOMMAND_PASSQ RAM register (12 bytes,
	// int64_t + uint32_t) - see the explicit "12" byte count in the reads
	// in bq76905.c. Do NOT add fields here: struct padding after this
	// point would silently grow past the 12 real hardware bytes.
	struct{
		int64_t accumulatedCharge;
		uint32_t passedTime;
	} Accumulator;

	// State-of-charge, computed by firmware from Accumulator.accumulatedCharge
	// vs. battery_capacity_mAs (see BQ76905_updateChargePercentage()) - not a
	// raw BQ76905 register, so it does not belong inside Accumulator above.
	uint8_t charge_percentage;

	// Rated pack capacity in mA-seconds (same unit as accumulatedCharge),
	// loaded from config_store.calibration.battery_capacity_mAs at startup -
	// see main.c. Kept adaptable via EEPROM rather than hardcoded so a
	// different pack size doesn't need a recompile.
	uint32_t battery_capacity_mAs;

	BQ76905_asyncState asyncState;
	uint8_t command_tx;
	uint8_t command_tx2[4];

} BQ76905_handle;
    /**
     * Initializes the BQ76905 device and applies configuration.
     * Should be called once during setup.
     */
    void BQ76905_init(BQ76905_handle* handle, uint8_t i2c_address);

    /**
     * Starts asynchronous read of all measurement values.
     * Results will be stored in internal arrays.
     */
    void BQ76905_readAllValuesAsync(BQ76905_handle* handle);
    
    /**
     * Starts synchropnous read of all measurement values.
     * Results will be stored in internal arrays.
     */
    void BQ76905_readAllValues(BQ76905_handle* handle);


    void BQ76905_resetAlarmStatus(BQ76905_handle* handle);

    void BQ76905_resetChargeAccumulator(BQ76905_handle* handle);

    void BQ76905_resetDevice(BQ76905_handle* handle);

    void BQ76905_ExitDeepsleep(BQ76905_handle* handle);

    void BQ76905_Deepsleep(BQ76905_handle* handle);

    void BQ76905_Shutdown(BQ76905_handle* handle);

    void BQ76905_FETEnable(BQ76905_handle* handle);

    //inline void BQ76905_Seal(BQ76905_handle* handle);

    void BQ76905_SleepEnable(BQ76905_handle* handle);

    void BQ76905_SleepDisable(BQ76905_handle* handle);

    void BQ76905_GetFWVersion(BQ76905_handle* handle, uint8_t* out);

    uint16_t BQ76905_GetDeviceNumber(BQ76905_handle* handle);

    uint16_t BQ76905_GetHWVersion(BQ76905_handle* handle);

    void BQ76905_WriteSecurityKey(BQ76905_handle* handle);

    /**
     * Enable balancing of specified cells
     * @param cellmask set bits to one to balance cells. Bit 0 balances the first active cell.
     */
    void BQ76905_EnableBalancing(BQ76905_handle* handle, uint8_t cellmask);

    void BQ76905_RecoverProtection(BQ76905_handle* handle, uint8_t mask);

    /**
     * Write a RAM Register. Can also be used to write a subcommand register
     */
    void BQ76905_WriteRamRegister(BQ76905_handle* handle, uint16_t reg, uint16_t data, uint8_t datalen);
    /**
     * Read a RAM register. Can also be used to read a subcommand register
     */
    void BQ76905_ReadRamRegister(BQ76905_handle* handle, uint16_t command, uint8_t* out, uint8_t len);

#endif /* SRC_DRIVER_BQ76905_H_ */
