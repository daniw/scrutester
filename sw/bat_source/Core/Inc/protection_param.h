#ifndef INC_PROTECTION_PARAM_H_
#define INC_PROTECTION_PARAM_H_

#define PROTECTION_TEMP_WARNING_C     70
#define PROTECTION_TEMP_ERROR_C       85
#define PROTECTION_TEMP_HYSTERESIS_C   5
#define PROTECTION_TEMP_ERROR_OPEN_NTC -50

// Per-mode limits on the external ADC (|V_TERM|, |I_OUT|, I_ISO), 0 = no check.
// Placeholders, to be confirmed on the bench.
#define PROTECTION_60V_MAX_mV         65000
#define PROTECTION_60V_MAX_mA          1500
#define PROTECTION_10A_MAX_mV         10000
#define PROTECTION_10A_MAX_mA         13000
#define PROTECTION_RES_1A_MAX_mV       5000
#define PROTECTION_RES_1A_MAX_mA       1500
#define PROTECTION_RES_1mA_MAX_mV      5000
#define PROTECTION_RES_1mA_MAX_mA       100
#define PROTECTION_ISO_MAX_mV       1100000
#define PROTECTION_ISO_MAX_uA          1500
#define PROTECTION_VOLTMETER_MAX_mV 1000000
#define PROTECTION_AMPMETER_MAX_mA    12000
#define PROTECTION_CHARGE_MAX_mV      25000
#define PROTECTION_CHARGE_MAX_mA       1500
#define PROTECTION_EMI_MAX_mV         55000 // above CTRL_PARAM_EMI_VOLTAGE_MAX_mV
#define PROTECTION_EMI_MAX_mA          3000
// Consecutive statemachine ticks over a limit before it trips.
#define PROTECTION_LIMIT_DEBOUNCE_TICKS   2

#endif /* INC_PROTECTION_PARAM_H_ */
