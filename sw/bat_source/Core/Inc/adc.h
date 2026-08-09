/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "statemachine.h"
#include "temp_conv.h"

//#include "ctrl_main.h"

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

extern ADC_HandleTypeDef hadc2;

extern ADC_HandleTypeDef hadc3;

extern ADC_HandleTypeDef hadc4;

extern ADC_HandleTypeDef hadc5;

/* USER CODE BEGIN Private defines */



// One-pole low-pass for the milliohm reading (r_mOhmx10), used only while
// RESISTANCE_1A is active (see adc_convert_data()) - fc ~= 2Hz at the 50Hz
// statemachine_step() tick rate the gated samples arrive at (Ts = 20ms):
// alpha = 1 - exp(-2*pi*fc*Ts) = 1 - exp(-2*pi*2*0.02) ~= 0.2222
#define ADC_R_MOHM_FILT_ALPHA 0.2222f
#define ADC_R_MOHMX10_MAX_VALUE 5000

								// Empirical Value	// Calculated Value 	// Calculation

#define ADC_VTERM_OFFSET_MV 2006
#define ADC_VTERM_GAIN_MV  296.444458 //(2500.0f * 1001/4096/1.0417f)
#define ADC_IOUT_OFFSET  2060
#define ADC_IOUT_GAIN_MA   (2500.0f*1000/43.33/4096) // ToDo: Calculate
#define ADC_IISO_OFFSET_UA  0 // ToDo: Measure. See known issue: 129uA offset on isolation current channel
#define ADC_IISO_GAIN_MA   0 // ToDo: Calculate

#define ADC_EXT_VTERM_GAIN_MV 	0.1431914341802f 		//0.1373291015625 		//(1200.0f/8388608 * 14400/15) (ADC * Resistor divider) Max Value 1152 V
#define ADC_EXT_VSENS_GAIN_UV	0.7152557f  //(1200.0f/8388608 * 1 * 5)(ADC * ADC GAIN * Resistor divider) Max Value 6V
#define ADC_EXT_IISO_GAIN_UA 	0.00143051f  //(1200.0f/8388608 * 1 * 0.01) (ADC * ADC Gain * mA/mV)
#define ADC_EXT_IOUT_GAIN_mA 	0.0016881f			//0.00178813934326171875 	//(1200.0f/8388608 * 12.5) (ADC * ADC Gain * mA/mV)

#define ADC_GAIN_V_3V3         (2500.0f*53/10/65536)
#define ADC_GAIN_V_3V3A        (2500.0f*53/10/65536)
#define ADC_GAIN_V_15V         (2500.0f*25/65536)
#define ADC_GAIN_V_VCC         (2500.0f*19/65536)
#define ADC_GAIN_V_5V          (2500.0f*78/10/65536)
#define ADC_GAIN_V_BAT         (2500.0f*3/4096)


#define ADC_VOUT_OFFSET_MV  0
#define ADC_VOUT_GAIN_MV   (2500.0f * 2731/4096/91)
#define ADC_VHV_OFFSET_MV   0
#define ADC_VHV_GAIN_MV    (2500.0f * 441 /4096)

#define ADC_VIN_OFFSET_MV   0
#define ADC_VIN_GAIN_MV    (2500.0f * 21 / 4096)
#define ADC_IBAT_OFFSET_MA  1926
#define ADC_IBAT_GAIN_MA  0.004761905f //(3300 *200 / 10/4096)


#define ADC_POTI_MAX 127

#define ADC_TRIGGER_HRTIM_PRIM ADC_EXTERNALTRIG_HRTIM_TRG1
#define ADC_TRIGGER_HRTIM_HV ADC_EXTERNALTRIG_HRTIM_TRG5
#define ADC_TRIGGER_HRTIM_SEK ADC_EXTERNALTRIG_HRTIM_TRG3

/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);
void MX_ADC3_Init(void);
void MX_ADC4_Init(void);
void MX_ADC5_Init(void);

/* USER CODE BEGIN Prototypes */

// adc_data.raw is a DMA destination (see HAL_ADC_Start_DMA() calls in adc.c)
// and the injected channels (i_iso/i_bat) are written from ISR context
// (HAL_ADCEx_InjectedConvCpltCallback()) -- both write it fully asynchronously
// to any main-context code reading it (adc_convert_data(), CLI commands,
// etc.). Without volatile, the compiler is free to cache a raw.* read across
// a whole function at -Os and never observe an update; volatile forces every
// access back to memory.
typedef struct  {
	volatile uint16_t v_in;		        // ADC3_IN3
	volatile uint16_t v_hv;              // ADC4_IN5
	volatile uint16_t v_term;            // ADC2_IN2
	volatile uint16_t i_out;             // ADC1_IN1
	volatile uint16_t i_iso;             // ADC5_IN2
	volatile uint16_t v_out;             // ADC4_IN2
	volatile uint16_t i_bat;             // ADC5_IN1

	volatile uint16_t v_3v3; 		    // ADC5_IN6
	volatile uint16_t temp_sec;      	// ADC5_IN7
	volatile uint16_t v_3v3a;           	// ADC5_IN8
	volatile uint16_t temp_trafo;   		// ADC5_IN9
	volatile uint16_t temp_current;      // ADC5_IN12
	volatile uint16_t temp_prim;         // ADC5_IN13
	volatile uint16_t v_15v;             // ADC5_IN14
	volatile uint16_t v_vcc;             // ADC5_IN15
	volatile uint16_t v_5v;              // ADC5_IN16
	volatile uint16_t int_temp;          // ADC5
	volatile uint16_t v_bat;             // ADC5
	volatile uint16_t v_ref_int;         // ADC5
} ADC_RAW_DATA;

// Holds adc_data.converted's field set. Two distinct instances of this type
// exist (see adc.c): one is ISR-private and is what adc_convert_fast_data()
// (called from HAL_ADC_ConvCpltCallback()) actually writes every ADC
// interrupt -- ctrl_main_ctrl() reads that instance directly, in the same
// ISR, so its control loop always sees this tick's fresh values. The other is
// ADC_MEAS_DATA.converted below, which main context (display_*(), CLI
// commands, protection_update(), adc_convert_data()) reads; it is only ever
// updated as a whole via adc_snapshot_converted()'s critical-section copy
// from the ISR-private instance, so main context always sees a coherent,
// same-instant set of fields instead of a torn mix of two samples.
typedef struct {
	int32_t  v_in;		        // ADC3_IN3
	int32_t  v_hv;              // ADC4_IN5
	int32_t  v_term;            // ADC2_IN2
	int16_t  i_out;             // ADC1_IN1
	int16_t  i_iso;             // ADC5_IN2
	int32_t  v_out;             // ADC4_IN2
	int16_t  i_bat;             // ADC5_IN1

	uint16_t v_3v3; 		    // ADC5_IN6
	int16_t  temp_sec;      	// ADC5_IN7
	uint16_t v_3v3a;           	// ADC5_IN8
	int16_t  temp_trafo;   		// ADC5_IN9
	int16_t  temp_current;      // ADC5_IN12
	int16_t  temp_prim;         // ADC5_IN13
	uint16_t v_15v;             // ADC5_IN14
	uint16_t v_vcc;             // ADC5_IN15
	uint16_t v_5v;              // ADC5_IN16
	int16_t  int_temp;          // ADC5
	uint16_t v_bat;             // ADC5
	uint16_t v_ref_int;         // ADC5
    int32_t  v_term_ext_mv;		// Extern ADC 1
    int32_t  v_term_ext_mv_filt; // Extern ADC 1
    int32_t  i_out_ext_mA;		// Extern ADC 2
    int32_t  v_sens_ext_uv;      // Extern ADC 3
    int32_t  i_iso_ext_uA;		// Extern ADC 4
} ADC_CONVERTED_DATA;

typedef struct {
	ADC_RAW_DATA raw;
	ADC_CONVERTED_DATA converted;


	uint16_t v_in_offset;
    float    v_in_gain;
    uint16_t v_hv_offset;
    float    v_hv_gain;
    uint16_t i_bat_offset;
    uint16_t v_out_offset;
    float    v_out_gain;
    uint16_t v_term_offset;
    float    v_term_gain;
    uint16_t temp_int_offset;

    uint16_t i_out_offset;
    float    i_out_gain;
    uint16_t temp_prim_offset;
    uint16_t temp_current_offset;
    uint16_t temp_sec_offset;
    uint16_t i_iso_offset;
    float    i_iso_gain;
    uint16_t temp_trafo_offset;

    int32_t  v_sens_ext_offset;
    float    v_sens_ext_gain;
    int32_t  v_term_ext_offset;
    float    v_term_ext_gain;
    int32_t  i_out_ext_offset;
    float    i_out_ext_gain;
    int32_t  i_iso_ext_offset;
    float    i_iso_ext_gain;

    int32_t* ext_adc_data;

    //uint16_t reference_poti;

    uint32_t r_mOhmx10;
    uint32_t r_Ohmx10;

    uint16_t vref_mV;

}ADC_MEAS_DATA;


void adc_init(int32_t* ext_adc_data);
void adc_start(void);
void adc_convert_fast_data(void);
void adc_convert_data(void);
void adc_configure_mode(statemachine_modes_t mode);
// Takes a coherent, race-free copy of the ISR-written "converted" fields into
// adc_data.converted, under a short save/restore critical section. Must be
// called once per statemachine_step() tick, before adc_convert_data(), so
// every main-context reader (adc_convert_data() itself, protection_update(),
// display_*(), CLI commands) sees one consistent snapshot instead of racing
// the ADC ISR. See adc.c for the field-ownership rationale.
void adc_snapshot_converted(void);


/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

