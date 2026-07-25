/*******************************************************************************/
/*
 * File: cli.c
 * Author: Andreas Horat
 */
/*******************************************************************************/
/*
 * This file contains a very simple command line interface.
 */
/*******************************************************************************/

#include "cli.h"
#include "ctrl_main.h"

#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include "stdbool.h"
#include "event.h"
#include "error.h"
#include "aux_io_ctrl.h"
#include "bq76905.h"
#include "adc.h"
#include "ads131m04.h"
#include "bq76905.h"
#include <stdarg.h>
#include "tim.h"
#include "hrtim.h"
#include "ui_ctrl.h"
#include "gpio.h"
#include "w25n01gv.h"
#include "ctrl_param.h"
#include "rtc.h"
#include "lcd.h"
#include "input.h"
#include "config_store.h"
#include "icon_store.h"
#include "calibration.h"
#include "protection.h"

#ifdef CLI_ENABLED

#define CLI_CHECK_ARG_CNT(nof_args)                 if (number_of_args != nof_args)                             {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_MIN(nof_args)             if (number_of_args < nof_args)                              {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_MAX(nof_args)             if (number_of_args > nof_args)                              {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_RANGE(min_args, max_args) if (number_of_args < min_args || number_of_args > max_args) {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_NONE(nof_args)            if (number_of_args != nof_args && number_of_args != 0)      {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}

char line_buffer[LINE_BUF_SIZE];
char *arg_locs[MAX_NUM_ARGS+1];
uint8_t number_of_args;
uint8_t line_buffer_index;

// ToDo Check why Index of Command gets wrong some times.
#define history_SIZE 5
char history[history_SIZE][LINE_BUF_SIZE];
int  history_count = 0;
int  history_index = -1;


extern BQ76905_handle bms;
extern ADC_MEAS_DATA adc_data;
extern HRTIM_HandleTypeDef hhrtim1;
extern ADS131M04_handle ext_adc;
extern ctrl_main_t ctrl_main_handle;
extern I2C_HandleTypeDef *i2c_handle;
extern w25n01gv_handle flash;
extern PID_controller_t ctrl_pi_voltage_buck;
extern PID_controller_t ctrl_pi_voltage_boost;
extern PID_controller_t ctrl_pi_current;
extern PID_controller_t ctrl_pi_boost_iout_limit;

// Uart handling
UART_HandleTypeDef *cli_huart;

// Keyboard control mode (see cmd_control() / cli_control_process_byte())
static uint8_t cli_control_active = 0;
static uint8_t cli_control_esc_state = 0; // 0=normal, 1=ESC seen, 2='[' seen
static uint32_t cli_control_esc_tick = 0;

// Forward declarations
static void cli_process_line(void);
static void cli_control_process_byte(char c);

// Function declarations
void cmd_help(void);
void cmd_saveEEPROM(void);
void cmd_readEEPROM(void);
void cmd_turnOff(void);
void cmd_printGPIO(void);
void cmd_setGPIO(void);
void cmd_setDAC(void);
void cmd_printBMS(void);
void cmd_printADC(void);
void cmd_setDuty(void);
void cmd_PWM_en(void);
void cmd_setRef(void);
void cmd_test_i2c(void);
void cmd_setLED(void);
void cmd_setBacklight(void);
void cmd_readLux(void);
void cmd_printFlashID(void);
void cmd_readFlash(void);
void cmd_eraseFlash(void);
void cmd_writeFlash(void);
void cmd_testFlash(void);
void cmd_changeState(void);
void cmd_setCtrlGain(void);
void cmd_printRTC(void);
void cmd_setRTC(void);
void cmd_testLCD(void);
void cmd_control(void);
void cmd_zeroCal(void);
void cmd_gainCal(void);
void cmd_setSerial(void);
void cmd_flashIcons(void);
void cmd_printProtection(void);

// List of functions pointers corresponding to each command
void (*cmd_func[])(void) = {
	cmd_help,
	cmd_saveEEPROM,
	cmd_readEEPROM,
	cmd_turnOff,
	cmd_printGPIO,
	cmd_setGPIO,
	cmd_setDAC,
	cmd_printBMS,
	cmd_printADC,
	cmd_setDuty,
	cmd_PWM_en,
	cmd_setRef,
	cmd_test_i2c,
	cmd_setLED,
	cmd_setBacklight,
	cmd_readLux,
	cmd_printFlashID,
	cmd_readFlash,
	cmd_eraseFlash,
	cmd_writeFlash,
	cmd_testFlash,
	cmd_changeState,
	cmd_setCtrlGain,
	cmd_printRTC,
	cmd_setRTC,
	cmd_testLCD,
	cmd_control,
	cmd_zeroCal,
	cmd_gainCal,
	cmd_setSerial,
	cmd_flashIcons,
	cmd_printProtection
};

// List of command names
const char *cmd_str[] = {
		"help",
		"saveEEPROM",
		"readEEPROM",
		"turnOff" ,
		"pGPIO",
		"setGPIO",
		"setDAC",
		"pBMS",
		"pADC",
		"setDuty",
		"enPWM",
		"setRef",
		"test_i2c",
		"setLED",
		"setBacklight",
		"readLux",
		"pFlashID",
		"readFlash",
		"eraseFlash",
		"writeFlash",
		"testFlash",
		"state",
		"setPI",
		"pRTC",
		"setRTC",
		"testLCD",
		"control",
		"zeroCal",
		"gainCal",
		"setSerial",
		"flashIcons",
		"pProt"
};

// List of command names including arguments
const char *cmd_arg_str[] = {
		"help {command}",
		"saveEEPROM",
		"readEEPROM",
		"turnOff" ,
		"pGPIO",
		"setGPIO [pin] [value]",
		"setDAC [pin] [value]",
		"pBMS",
		"pADC {loops}",
		"setDuty [channel] [duty]",
		"enPWM [channel] [0 disable | 1 enable]",
		"setRef [value]",
		"test_i2c",
		"setLED [current, 0.1mA]",
		"setBacklight [current, 0.1mA]",
		"readLux",
		"pFlashID",
		"readFlash [address] [size]",
		"eraseFlash [address / \"Full\"] {number of blocks}",
		"writeFlash [address] [size] [data]",
		"testFlash",
		"state [new state]",
		"setPI [ID 0=Buck,1=Boost,2=Current,3=BoostIoutLimit] [P Gain] [I Gain]",
		"pRTC",
		"setRTC [year] [month] [day] [hour] [minute] [second] {weekday}",
		"testLCD",
		"control -- arrows=encoder Enter=OK Esc=ESC Space=OUT(toggle) q=quit",
		"zeroCal [channel: 0=V_TERM,1=V_SENS,2=V_OUT,3=V_HV,4=I_OUT,5=I_ISO]",
		"gainCal [channel: 0=V_TERM,1=V_SENS,2=V_OUT,3=V_HV,4=I_OUT,5=I_ISO] [reference value]",
		"setSerial [serial number]",
		"flashIcons",
		"pProt"
};

int cmd_index;

int num_commands = sizeof(cmd_str) / sizeof(char*);
#endif

/**
 * Initialises the CLI
 */
void cli_init(UART_HandleTypeDef *huart1) {
	cli_huart = huart1;
	setbuf(stdin, NULL); //TO HANDLE INPUT BUFFER WHEN USING SCANF/COUT

#ifdef CLI_ENABLED
	line_buffer_index = 0;
	number_of_args = 0;
	memset(line_buffer, 0, LINE_BUF_SIZE);
    memset(history, 0, sizeof(history));
    history_count = 0;
    history_index = -1;
	printf("Serial Interface enabled V 1.0.\r\n");
	printf(">>\r\n");
#endif
}

#ifdef CLI_ENABLED

int _write(int fd, char *ptr, int len) {
	(void) fd;
	HAL_UART_Transmit(cli_huart, (uint8_t*) ptr, len, 100);
	return len;
}

void cli_printf(const char *fmt, ...) {
  static char buffer[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  uint16_t len = strlen(buffer);
  HAL_UART_Transmit(cli_huart,(uint8_t*)buffer, len, 100);
}

/**
 * Read the line from the buffer
 *
 * @return Returns true if line is available in buffer.
 */
bool cli_readline(void) {
	char c;
	bool ret = false;
	// 0=normal, 1=ESC seen, 2='[' seen
    static uint8_t esc_state = 0;

    // A lone Esc keypress in keyboard-control mode never gets a follow-up
    // '[' byte, so it has to be resolved on a timeout instead of by looking
    // at the next byte (which may not exist / may belong to the next key).
    if (cli_control_active && cli_control_esc_state == 1
    		&& (HAL_GetTick() - cli_control_esc_tick) > 25) {
    	cli_control_esc_state = 0;
    	input_cli_pulse_esc();
    }

	while (HAL_UART_Receive(cli_huart, (uint8_t*) &c,1, 1) == 0) {
		ret = true;

		if (cli_control_active) {
			cli_control_process_byte(c);
			continue;
		}

        // --- Handle escape sequences (arrow keys) ---
        if (esc_state == 1) {
            if (c == '[') {
            	esc_state = 2;
            	continue;
            }
            esc_state = 0;
        } else if (esc_state == 2) {
            if (c == 'A') { // ↑ Up arrow
                if (history_count > 0) {
                    if (history_index > 0)
                        history_index--;
                    else
                        history_index = 0;

                    strcpy(line_buffer, history[history_index]);
                    line_buffer_index = strlen(line_buffer);
                    cli_printf("\r> %s\x1B[K", line_buffer);
                }
            } else if (c == 'B') { // ↓ Down arrow
                if (history_index < history_count - 1)
                    history_index++;
                else
                    history_index = history_count;

                if (history_index < history_count)
                    strcpy(line_buffer, history[history_index]);
                else
                    line_buffer[0] = '\0';

                line_buffer_index = strlen(line_buffer);
                cli_printf("\r> %s\x1B[K", line_buffer);
            }
            esc_state = 0;
            continue;
        }

        // ESC = start of escape sequence
        if (c == 27) {
        	esc_state = 1;
        	continue;
        }

        // --- Handle ENTER ---
        if (c == '\r' || c == '\n') {
            if (line_buffer_index > 0) {
                line_buffer[line_buffer_index] = 0;
                cli_printf("\r\n");
                cli_process_line();
                line_buffer_index = 0;
                memset(line_buffer, 0, sizeof(line_buffer));
                cli_printf("> ");
            } else {
            	cli_printf("\r\n> ");
            }
        }

        else if (c == '\t') { // Tab key for autocomplete
            char *current = line_buffer;
            size_t len = strlen(current);

            int matches = 0;
            const char *match = NULL;

            for (int i = 0; i < num_commands; i++) {
                if (strncmp(cmd_str[i], current, len) == 0) {
                    if (matches == 0)
                        match = cmd_str[i];
                    matches++;
                }
            }

            if (matches == 1 && match) {
                // Single match -> autocomplete
                strcpy(line_buffer, match);
                line_buffer_index = strlen(line_buffer);
                cli_printf("\r> %s\x1B[K", line_buffer); // Update line
            } else if (matches > 1) {
                // Multiple matches -> show list
            	cli_printf("\r\n");
                for (int i = 0; i < num_commands; i++) {
                    if (strncmp(cmd_str[i], current, len) == 0)
                    	cli_printf("%s\r\n", cmd_str[i]);
          }
                cli_printf("> %s", line_buffer); // Reprint current input
            }
        }

        else if ((c == '\b' || c == 127) && line_buffer_index > 0) {
            line_buffer_index--;
            cli_printf("\b \b");
        }
        else if (line_buffer_index < LINE_BUF_SIZE - 1 && c >= 32 && c <= 126) {
            line_buffer[line_buffer_index++] = c;
            cli_printf("%c", c);
        }
    }
	return ret;
}

/**
 * Byte parser for keyboard-control mode (see cmd_control()). Drives input.c's
 * CLI-simulated encoder/buttons instead of the normal line editor:
 *   Left/Down or Right/Up = encoder step, Enter = OK, Esc = ESC,
 *   Space = OUT (toggle, since a terminal has no key-release event), q = quit.
 *
 * Note: if Esc is immediately followed by an unrelated key (not '['), that
 * key is swallowed while the lone-Esc timeout resolves -- a minor limitation
 * of a dev/test-only feature, not worth a re-entrant parse for.
 */
static void cli_control_process_byte(char c) {
	if (cli_control_esc_state == 1) {
		if (c == '[') {
			cli_control_esc_state = 2;
			return;
		}
		cli_control_esc_state = 0;
		input_cli_pulse_esc();
		return;
	}
	if (cli_control_esc_state == 2) {
		switch (c) {
		case 'A': /* Up */
		case 'C': /* Right */
			input_cli_encoder_step(+1);
			break;
		case 'B': /* Down */
		case 'D': /* Left */
			input_cli_encoder_step(-1);
			break;
		default:
			break;
		}
		cli_control_esc_state = 0;
		return;
	}
	if (c == 27) {
		cli_control_esc_state = 1;
		cli_control_esc_tick = HAL_GetTick();
		return;
	}
	if (c == '\r' || c == '\n') {
		input_cli_pulse_ok();
		return;
	}
	if (c == ' ') {
		input_cli_toggle_out();
		return;
	}
	if (c == 'q' || c == 'Q') {
		cli_control_active = 0;
		input_set_source(INPUT_SOURCE_HW);
		cli_printf("\r\nExiting keyboard control mode.\r\n> ");
		return;
	}
}

/**
 * Parse the line into the arguments
 *
 * @return Returns the number of available arguments
 */
static void cli_process_line(void) {
    number_of_args = 0;
    line_buffer[strcspn(line_buffer, "\r\n")] = 0;

    char full_line_copy[LINE_BUF_SIZE];
    strncpy(full_line_copy, line_buffer, LINE_BUF_SIZE);
    full_line_copy[LINE_BUF_SIZE - 1] = '\0';

    arg_locs[number_of_args] = strtok(line_buffer, " ");
    while (arg_locs[number_of_args] != NULL && number_of_args < MAX_NUM_ARGS) {
        arg_locs[++number_of_args] = strtok(NULL, " ");
    }



    if (number_of_args == 0)
        return;

    if ((arg_locs[0] != NULL) && (arg_locs[number_of_args] == NULL))
        number_of_args--;

    for (int i = 0; i < num_commands; i++) {
        if (strcmp(arg_locs[0], cmd_str[i]) == 0) {
        	if ((history_count <= 0) || (strcmp(history[history_count-1], full_line_copy) != 0)){
            // --- Store command line in history ---
                if (strlen(full_line_copy) > 0) {
                    if (history_count < history_SIZE) {
                        strcpy(history[history_count++], full_line_copy);
                    } else {
                        for (int j = 1; j < history_SIZE; j++)
                            strcpy(history[j - 1], history[j]);
                        strcpy(history[history_SIZE - 1], full_line_copy);
                    }
                }
                history_index = history_count;
            }
        	cmd_index = i;
            (*cmd_func[i])();;
            return;
        }
    }

    printf("Invalid command.\r\n");
}
#endif

/**
 * CLI Worker function that executes the command if correct
 */
void cli_loop(void) {
#ifdef CLI_ENABLED
	if (!cli_readline())
		return;
#endif
}

#ifdef CLI_ENABLED
/**
 * Print Help Message
 */
void cmd_help(void) {
	CLI_CHECK_ARG_CNT_MAX(1);

	if (number_of_args == 1) {
		for (int i = 0; i < num_commands; i++) {
			if (strcmp(arg_locs[1], cmd_str[i]) == 0) {
				printf("Usage: \r\n");
				printf("  %s\r\n", cmd_arg_str[i]);
				return;
			}
		}
		printf("Invalid command\r\n");
	}
	printf("Simple Interface for configuration only.\r\n");
	for (int i = 0; i < num_commands; i++){
		printf("\t%s\r\n", cmd_arg_str[i]);
	}
}

/*********************** Functions ************************************/

/**
 * Snapshots the live ADC calibration (see zeroCal/gainCal) and PID
 * gains (see setPI) into config_store and persists the calibration
 * block to the EEPROM. zeroCal/gainCal already persist on their own,
 * so this is mainly useful after manually tweaking PID gains.
 */
void cmd_saveEEPROM(void) {
	config_store.calibration.v_term_offset_mv = adc_data.v_term_offset;
	config_store.calibration.v_term_gain      = adc_data.v_term_gain;
	config_store.calibration.i_out_offset_ma  = adc_data.i_out_offset;
	config_store.calibration.i_out_gain       = adc_data.i_out_gain;
	config_store.calibration.i_iso_offset_ua  = adc_data.i_iso_offset;
	config_store.calibration.i_iso_gain       = adc_data.i_iso_gain;
	config_store.calibration.v_sens_offset    = adc_data.v_sens_offset;
	config_store.calibration.v_sens_gain      = adc_data.v_sens_gain;
	config_store.calibration.v_out_offset     = adc_data.v_out_offset;
	config_store.calibration.v_out_gain       = adc_data.v_out_gain;
	config_store.calibration.v_hv_offset      = adc_data.v_hv_offset;
	config_store.calibration.v_hv_gain        = adc_data.v_hv_gain;
	config_store.calibration.v_term_ext_offset = adc_data.v_term_ext_offset;
	config_store.calibration.v_term_ext_gain   = adc_data.v_term_ext_gain;
	config_store.calibration.i_out_ext_offset  = adc_data.i_out_ext_offset;
	config_store.calibration.i_out_ext_gain    = adc_data.i_out_ext_gain;
	config_store.calibration.i_iso_ext_offset  = adc_data.i_iso_ext_offset;
	config_store.calibration.i_iso_ext_gain    = adc_data.i_iso_ext_gain;

	config_store.calibration.voltage_buck_p  = ctrl_pi_voltage_buck.P_gain;
	config_store.calibration.voltage_buck_i  = ctrl_pi_voltage_buck.I_gain;
	config_store.calibration.voltage_boost_p = ctrl_pi_voltage_boost.P_gain;
	config_store.calibration.voltage_boost_i = ctrl_pi_voltage_boost.I_gain;
	config_store.calibration.current_p       = ctrl_pi_current.P_gain;
	config_store.calibration.current_i       = ctrl_pi_current.I_gain;
	config_store.calibration.boost_iout_limit_p = ctrl_pi_boost_iout_limit.P_gain;
	config_store.calibration.boost_iout_limit_i = ctrl_pi_boost_iout_limit.I_gain;

	if (config_store_store() == 0)
		printf("EEPROM: calibration saved\r\n");
	else
		printf("EEPROM: save failed\r\n");
}

/**
 * Re-reads the config store from the EEPROM and prints it to the
 * Serial Port.
 */
/**
 * Prints a float with 6 decimal digits. This project links
 * --specs=nano.specs without -u _printf_float, so newlib-nano's printf
 * silently drops %f support - this avoids depending on it at all.
 */
static void cli_printFloat(float value) {
	int32_t whole = (int32_t) value;
	float frac = value - (float) whole;
	if (frac < 0)
		frac = -frac;
	printf("%ld.%06lu", (long) whole, (unsigned long) (frac * 1000000.0f));
}

void cmd_readEEPROM(void) {
	config_store_read();
	printf("Serial Number:       %u\r\n", config_store.hardware_data.serial_number);

	printf("V_TERM offset/gain:  %u mV / ", config_store.calibration.v_term_offset_mv);
	cli_printFloat(config_store.calibration.v_term_gain);
	printf("\r\n");

	printf("I_OUT  offset/gain:  %u mA / ", config_store.calibration.i_out_offset_ma);
	cli_printFloat(config_store.calibration.i_out_gain);
	printf("\r\n");

	printf("I_ISO  offset/gain:  %u uA / ", config_store.calibration.i_iso_offset_ua);
	cli_printFloat(config_store.calibration.i_iso_gain);
	printf("\r\n");

	printf("V_SENS offset/gain:  %ld uV / ", (long) config_store.calibration.v_sens_offset);
	cli_printFloat(config_store.calibration.v_sens_gain);
	printf("\r\n");

	printf("V_OUT  offset/gain:  %u mV / ", config_store.calibration.v_out_offset);
	cli_printFloat(config_store.calibration.v_out_gain);
	printf("\r\n");

	printf("V_HV   offset/gain:  %u mV / ", config_store.calibration.v_hv_offset);
	cli_printFloat(config_store.calibration.v_hv_gain);
	printf("\r\n");

	printf("V_TERM_EXT off/gain: %ld mV / ", (long) config_store.calibration.v_term_ext_offset);
	cli_printFloat(config_store.calibration.v_term_ext_gain);
	printf("\r\n");

	printf("I_OUT_EXT  off/gain: %ld mA / ", (long) config_store.calibration.i_out_ext_offset);
	cli_printFloat(config_store.calibration.i_out_ext_gain);
	printf("\r\n");

	printf("I_ISO_EXT  off/gain: %ld uA / ", (long) config_store.calibration.i_iso_ext_offset);
	cli_printFloat(config_store.calibration.i_iso_ext_gain);
	printf("\r\n");

	printf("Voltage buck  P / I: ");
	cli_printFloat(config_store.calibration.voltage_buck_p);
	printf(" / ");
	cli_printFloat(config_store.calibration.voltage_buck_i);
	printf("\r\n");

	printf("Voltage boost P / I: ");
	cli_printFloat(config_store.calibration.voltage_boost_p);
	printf(" / ");
	cli_printFloat(config_store.calibration.voltage_boost_i);
	printf("\r\n");

	printf("Current loop  P / I: ");
	cli_printFloat(config_store.calibration.current_p);
	printf(" / ");
	cli_printFloat(config_store.calibration.current_i);
	printf("\r\n");

	printf("Boost Iout-limit P / I: ");
	cli_printFloat(config_store.calibration.boost_iout_limit_p);
	printf(" / ");
	cli_printFloat(config_store.calibration.boost_iout_limit_i);
	printf("\r\n");
}

/**
 * Zeroes one of the six calibratable ADC channels: set the real-world
 * input to zero (short/disconnect for voltage channels, open circuit
 * for current channels) first, then run this. Samples raw ADC counts,
 * stores them as the channel's offset, and persists immediately to
 * the EEPROM.
 */
void cmd_zeroCal(void) {
	uint8_t id;
	char *end;

	CLI_CHECK_ARG_CNT(1);
	id = strtoul(arg_locs[1], &end, 10);
	if (id >= CAL_CH_COUNT) {
		printf("Invalid channel\r\n");
		return;
	}
	calibration_zero((calibration_channel_t) id);
	printf("%s zeroed.\r\n", calibration_channel_name((calibration_channel_t) id));
}

/**
 * Sets the gain (second calibration point) of one of the six
 * calibratable ADC channels: apply a known reference value to the
 * channel's input, then run this with that value. Samples raw ADC
 * counts, computes gain = reference / (raw - offset), and persists
 * immediately to the EEPROM. Run zeroCal on the channel first.
 */
void cmd_gainCal(void) {
	uint8_t id;
	float reference_value;
	char *end;

	CLI_CHECK_ARG_CNT(2);
	id = strtoul(arg_locs[1], &end, 10);
	if (id >= CAL_CH_COUNT) {
		printf("Invalid channel\r\n");
		return;
	}
	reference_value = strtof(arg_locs[2], &end);
	calibration_set_gain((calibration_channel_t) id, reference_value);
	printf("%s gain set (reference %s %s).\r\n", calibration_channel_name((calibration_channel_t) id),
			arg_locs[2], calibration_channel_unit((calibration_channel_t) id));
}

/**
 * Sets the hardware serial number and immediately persists it to the
 * EEPROM. This is factory/provisioning data - only call it deliberately.
 */
void cmd_setSerial(void) {
	CLI_CHECK_ARG_CNT(1);
	char *end;
	config_store.hardware_data.serial_number = strtoul(arg_locs[1], &end, 10);
	if (config_store_storeHW() == 0)
		printf("EEPROM: serial number saved\r\n");
	else
		printf("EEPROM: save failed\r\n");
}

void cmd_turnOff(void) {
	printf("Turning off ...\r\n");
	//Todo: disable any controller or converter that might be running before shutdown to prevent damage!
	HAL_Delay(10);
	gpio_turnOff();
	for (uint16_t i = 0; i < 100; i++){
		if (i%10 == 0){
			printf("Waiting ...\n");
		}
		HAL_Delay(1);
	}
	gpio_turnOn();
	printf("Turning off unsuccessful, reenabling on/off controller\r\n");
}

void cmd_printGPIO(void) {
	printf("Input Pins: \n");
	printf("Button Out:         %d\r\n", HAL_GPIO_ReadPin(BUTTON_OUT_GPIO_Port, BUTTON_OUT_Pin));
	printf("Button OK:          %d\r\n", HAL_GPIO_ReadPin(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin));
	printf("Button ESC:         %d\r\n", HAL_GPIO_ReadPin(BUTTON_ESC_GPIO_Port, BUTTON_ESC_Pin));

	printf("I2C Alert:          %d\r\n", HAL_GPIO_ReadPin(I2C_ALERT_GPIO_Port, I2C_ALERT_Pin));
	printf("ADC_DRDY:           %d\r\n", HAL_GPIO_ReadPin(ADC_DRDY_N_GPIO_Port, ADC_DRDY_N_Pin));
	printf("UI Present:         %d\r\n", HAL_GPIO_ReadPin(UI_PRESENT_GPIO_Port, UI_PRESENT_Pin));
	printf("OVP Pin Latch:      %d\r\n", HAL_GPIO_ReadPin(OVP_N_GPIO_Port, OVP_N_Pin));
	printf("OCP Pin Latch:      %d\r\n", HAL_GPIO_ReadPin(OCP_N_GPIO_Port, OCP_N_Pin));

	printf("HW Revision: 	    %d\r\n", aux_io_ctrl_readHW_Revision());

	printf("Encoder State:      %d\r\n", tim_encoder_read());
	printf("\n");

	printf("Output Pins: \n");
	printf("DISPLAY_WR:         %d\r\n", HAL_GPIO_ReadPin(DISPLAY_WR_GPIO_Port, DISPLAY_WR_Pin));
	printf("CONV_CTRL_EN:       %d\r\n", HAL_GPIO_ReadPin(CONV_CTRL_EN_GPIO_Port, CONV_CTRL_EN_Pin));
	printf("ON_REQ:             %d\r\n", HAL_GPIO_ReadPin(ON_REQ_GPIO_Port, ON_REQ_Pin));
	printf("OUT_SEL_ISO:        %d\r\n", HAL_GPIO_ReadPin(OUT_SEL_ISO_GPIO_Port, OUT_SEL_ISO_Pin));
	printf("BMS_CTRL_WAKEUP:    %d\r\n", HAL_GPIO_ReadPin(BMS_CTRL_WAKEUP_GPIO_Port, BMS_CTRL_WAKEUP_Pin));
	printf("SHUNT_EN:           %d\r\n", HAL_GPIO_ReadPin(SHUNT_EN_GPIO_Port, SHUNT_EN_Pin));
	printf("OUT_SEL_HV:         %d\r\n", HAL_GPIO_ReadPin(OUT_SEL_HV_GPIO_Port, OUT_SEL_HV_Pin));
	printf("OVP_RESET:          %d\r\n", HAL_GPIO_ReadPin(OVP_RESET_GPIO_Port, OVP_RESET_Pin));
	printf("DISCHARGE:          %d\r\n", HAL_GPIO_ReadPin(DISCHARGE_N_GPIO_Port, DISCHARGE_N_Pin));
	printf("HV_CTRL_EN:         %d\r\n", HAL_GPIO_ReadPin(HV_CTRL_EN_GPIO_Port, HV_CTRL_EN_Pin));
	printf("SHUNT_ISO_EN:       %d\r\n", HAL_GPIO_ReadPin(SHUNT_ISO_EN_GPIO_Port, SHUNT_ISO_EN_Pin));
	printf("ADC_SYNC_RESET:     %d\r\n", HAL_GPIO_ReadPin(ADC_SYNC_RESET_N_GPIO_Port, ADC_SYNC_RESET_N_Pin));
}

void cmd_setGPIO(void) {
	uint8_t pin, value;
	CLI_CHECK_ARG_CNT(2);

	char *end;
	pin = strtol(arg_locs[1], &end, 10);
	value = strtol(arg_locs[2], &end, 10);

	aux_io_ctrl_manual_set_io(pin, value);
}

void cmd_setDAC(void) {
	uint8_t  pin;
	uint16_t value;
	CLI_CHECK_ARG_CNT(2);

	char *end;
	pin = strtoul(arg_locs[1], &end, 10);
	value = strtoul(arg_locs[2], &end, 10);
	//if (pin == 0)
		//dac_setValueRef(value);
	//else
		//dac_setValue1ARef(value);
}

void cmd_setRef(void){
		uint8_t value;
		CLI_CHECK_ARG_CNT(1);

		char *end;
		ctrl_main_handle.poti_reference = strtoul(arg_locs[1], &end, 10);
		printf("Set Reference to: %d\r\n", ctrl_main_handle.poti_reference);
}


void cmd_printBMS(void){
	  BQ76905_readAllValues(&bms);
	  printf("SafetyRegisters.safetyAlertA         = 0x%02X\r\n", bms.SafetyRegisters.safetyAlertA);
	  printf("SafetyRegisters.safetyStatusA        = 0x%02X\r\n", bms.SafetyRegisters.safetyStatusA);
	  printf("SafetyRegisters.safetyAlertB         = 0x%02X\r\n", bms.SafetyRegisters.safetyAlertB);
	  printf("SafetyRegisters.safetyStatusB        = 0x%02X\r\n", bms.SafetyRegisters.safetyStatusB);
	  printf("\n");
	  printf("CellVoltageRegisters.BatteryStatus   = 0x%04X\r\n", bms.CellVoltageRegisters.BatteryStatus);
	  for (int i = 0; i < 5; ++i) {
	  	printf("CellVoltageRegisters.CellVoltages[%d] = %u mV\r\n", i, bms.CellVoltageRegisters.CellVoltages[i]);
	  }
	  printf("\n");
	  printf("VoltageRegisters.Reg18Voltage        = %u\r\n",    bms.VoltageRegisters.Reg18Voltage);
	  printf("VoltageRegisters.VssVoltage          = %u\r\n",    bms.VoltageRegisters.VssVoltage);
	  printf("VoltageRegisters.StackVoltage        = %u mV\r\n", bms.VoltageRegisters.StackVoltage);
	  printf("VoltageRegisters.IntTemp             = %u\r\n",    bms.VoltageRegisters.IntTemp);
	  printf("VoltageRegisters.TSTemp              = %u\r\n",    bms.VoltageRegisters.TSTemp);
	  printf("\n");
	  printf("CurrentRegisters.RawCurrent          = 0x%04X%04X\r\n",  (uint16_t)(bms.CurrentRegisters.RawCurrent>>16),(uint16_t)(bms.CurrentRegisters.RawCurrent));
	  printf("CurrentRegisters.CC2Current          = %d mA\r\n",       bms.CurrentRegisters.CC2Current);
	  printf("CurrentRegisters.CC1Current          = %d mA\r\n",       bms.CurrentRegisters.CC1Current);
	  printf("Passed Charge                        = %ld mAs\r\n",     bms.Accumulator.accumulatedCharge);
	  printf("Passed Charge (low bytes)            = %ld mAs\r\n",     (int32_t)(bms.Accumulator.accumulatedCharge&0xFFFFFFFF));
	  printf("Passed Charge (high bytes)           = %ld mAs\r\n",     (int32_t)(bms.Accumulator.accumulatedCharge>>32));
	  printf("Passed Time                          = %ld s\r\n",       bms.Accumulator.passedTime/4);
	  printf("\n");
	  printf("SystemCtrl.AlarmStatus               = 0x%04X\r\n", bms.SystemCtrl.AlarmStatus);
	  printf("SystemCtrl.AlarmRawStatus            = 0x%04X\r\n", bms.SystemCtrl.AlarmRawStatus);
	  printf("SystemCtrl.AlarmEnable               = 0x%04X\r\n", bms.SystemCtrl.AlarmEnable);
	  printf("SystemCtrl.FETCtrl                   = 0x%04X\r\n", bms.SystemCtrl.FETCtrl);
	  printf("SystemCtrl.RegoutCtrl                = 0x%04X\r\n", bms.SystemCtrl.RegoutCtrl);
	  printf("SystemCtrl.DSGFetPWM                 = 0x%04X\r\n", bms.SystemCtrl.DSGFetPWM);
	  printf("SystemCtrl.CHGFetPWM                 = 0x%04X\r\n", bms.SystemCtrl.CHGFetPWM);
}


void cmd_printADC() {
	uint16_t loopcount = 1;
	CLI_CHECK_ARG_CNT_MAX(1);
	if (number_of_args == 1) {
		char *end;
		loopcount = strtol(arg_locs[1], &end, 10);
	}
	for(int i=0; i<loopcount; i++)	{
		adc_convert_data();
		printf("Triggered ADC Measurements: \r\n");
		printf(" 	 v_in    (ADC3_IN3 ) : %u \t: %d mV\r\n",  adc_data.raw.v_in   , adc_data.converted.v_in  );
		printf(" 	 v_out   (ADC4_IN2 ) : %u \t: %ld mV\r\n", adc_data.raw.v_out  , adc_data.converted.v_out );
		printf(" 	 v_term  (ADC2_IN2 ) : %u \t: %ld mV\r\n", adc_data.raw.v_term , adc_data.converted.v_term);
		printf(" 	 v_hv    (ADC4_IN5 ) : %u \t: %ld mV\r\n", adc_data.raw.v_hv   , adc_data.converted.v_hv  );
		printf(" 	 i_bat   (ADC5_IN1 ) : %u \t: %d mA\r\n",  adc_data.raw.i_bat  , adc_data.converted.i_bat );
		printf(" 	 i_out   (ADC1_IN1)  : %u \t: %d mA\r\n",  adc_data.raw.i_out  , adc_data.converted.i_out );
		printf(" 	 i_iso   (ADC5_IN2)  : %u \t: %d mA\r\n",  adc_data.raw.i_iso  , adc_data.converted.i_iso );

		printf("ADC Measurements:\n");
		printf(" 		v_3v3          (ADC5_IN6 ) : %u \t: %u mV\r\n", adc_data.raw.v_3v3        , adc_data.converted.v_3v3       );
		printf(" 		temp_sec       (ADC5_IN7 ) : %u \t: %i °C\r\n", adc_data.raw.temp_sec     , adc_data.converted.temp_sec    );
		printf(" 		v_3v3a         (ADC5_IN8 ) : %u \t: %u mV\r\n", adc_data.raw.v_3v3a       , adc_data.converted.v_3v3a      );
		printf(" 		temp_inductor  (ADC5_IN9 ) : %u \t: %i °C\r\n", adc_data.raw.temp_trafo   , adc_data.converted.temp_trafo  );
		printf(" 		temp_current   (ADC5_IN12) : %u \t: %i °C\r\n", adc_data.raw.temp_current , adc_data.converted.temp_current);
		printf(" 		temp_prim      (ADC5_IN13) : %u \t: %i °C\r\n", adc_data.raw.temp_prim    , adc_data.converted.temp_prim   );
		printf(" 		v_15v          (ADC5_IN14) : %u \t: %u mV\r\n", adc_data.raw.v_15v        , adc_data.converted.v_15v       );
		printf(" 		v_vcc          (ADC5_IN15) : %u \t: %u mV\r\n", adc_data.raw.v_vcc        , adc_data.converted.v_vcc       );
		printf(" 		v_5v           (ADC5_IN16) : %u \t: %u mV\r\n", adc_data.raw.v_5v         , adc_data.converted.v_5v        );
		printf(" 		int_temp       (ADC5     ) : %u \t: %i °C\r\n", adc_data.raw.int_temp     , adc_data.converted.int_temp    );
		printf(" 		v_bat          (ADC5     ) : %u \t: %u mV\r\n", adc_data.raw.v_bat        , adc_data.converted.v_bat       );
		printf(" 		v_ref_int      (ADC5     ) : %u \t: %u mV\r\n", adc_data.raw.v_ref_int    , adc_data.converted.v_ref_int   );

		printf("  Ext V_Term : %li : %li mV\r\n", ext_adc.channelData[0], adc_data.converted.v_term_ext_mv);
		printf("  Ext I_Out  : %li : %li mA\r\n", ext_adc.channelData[1], adc_data.converted.i_out_ext_mA );
		printf("  Ext V_Sns  : %li : %li uV\r\n", ext_adc.channelData[2], adc_data.converted.v_sens_ext_uv);
		printf("  Ext I_Iso  : %li : %li uA\r\n", ext_adc.channelData[3], adc_data.converted.i_iso_ext_uA );
	}
}

void cmd_setDuty(void){
	uint8_t  channel;
	float value;
	CLI_CHECK_ARG_CNT(2);

	char *end;
	channel = strtoul(arg_locs[1], &end, 10);
	value = strtof(arg_locs[2], &end);
	switch(channel){
	case 0: hrtim_set_duty(HRTIM_CHANNEL_PRIM, value);
	return;
	case 1: hrtim_set_duty(HRTIM_CHANNEL_SEK, value);
	return;
	case 2: hrtim_set_duty(HRTIM_CHANNEL_HV, value);
	return;
	}
}

void cmd_PWM_en(void){
	uint8_t  channel;
	uint16_t value;
	CLI_CHECK_ARG_CNT(2);

	char *end;
	channel = strtoul(arg_locs[1], &end, 10);
	value = strtoul(arg_locs[2], &end, 10);
	if(value == 1)
		hrtim_enable(channel);
	else
		hrtim_disable(channel);
}

void cmd_test_i2c(void){
	uint64_t fw_version;
	printf("Read BMS HW Version: 0x%04X\r\n", BQ76905_GetHWVersion(&bms));
	BQ76905_GetFWVersion(&bms, (uint8_t*)&fw_version);
	printf("Read BMS FW Version: 0x%08lx%08lx\r\n", (uint32_t) fw_version&0xFFFFFFFF, (uint32_t) (fw_version>>32));
	printf("Read BMS Device Number: 0x%04X\r\n", BQ76905_GetDeviceNumber(&bms));
}

void cmd_setLED(void){
    uint8_t led_current;
	uint8_t led_currents[3];
    CLI_CHECK_ARG_CNT_RANGE(1, 3);

	if (number_of_args == 3) {
		char *end;
		for (uint8_t i = 0; i < 3; i++){
			led_currents[i] = strtoul(arg_locs[i+1], &end, 10)/2;
			if (led_currents[i] > 150) {
				led_currents[i] = 150;
				printf("LED current limited to 30mA\r\n");
			}
		}
	}
	else{
		char *end;
		led_current = strtoul(arg_locs[1], &end, 10)/2;
		if (led_current > 150) {
			led_current = 150;
			printf("LED current limited to 20mA\r\n");
		}
		led_currents[0] = led_current;
		led_currents[1] = led_current/2;
		led_currents[2] = led_current/2;
	}
	lp581x_setAnalogDimming(&hled, led_currents);
	//lp581x_setAnalogDimmingChannel(&hled, 0, led_currents[0]);
	//lp581x_setAnalogDimmingChannel(&hled, 1, led_currents[1]);
	//lp581x_setAnalogDimmingChannel(&hled, 2, led_currents[2]);
}

void cmd_setBacklight(void){
    uint16_t led_current;
	uint8_t led_currents[4];
	CLI_CHECK_ARG_CNT(1);

	char *end;
	led_current = strtoul(arg_locs[1], &end, 10);
	if (led_current > 400) {
		led_current = 400;
		printf("LED current limited to 40mA\r\n");
	}
	for (uint8_t i = 0; i < 4; i++){
		led_currents[i] = (uint8_t) (led_current/2);
	}
	lp581x_setAnalogDimming(&hbacklight, led_currents);
	//lp581x_setAnalogDimmingChannel(&hbacklight, 0, (uint8_t) led_current/2);
	//lp581x_setAnalogDimmingChannel(&hbacklight, 1, (uint8_t) led_current/2);
	//lp581x_setAnalogDimmingChannel(&hbacklight, 2, (uint8_t) led_current/2);
	//lp581x_setAnalogDimmingChannel(&hbacklight, 3, (uint8_t) led_current/2);
}

void cmd_readLux(void){

	printf("Read Optical Sensor: %lu cLux\r\n", ui_ctrl_readBrightness());
}

void cmd_printFlashID(void){
	uint8_t id[4];
	w25n01gv_status_t status = W25N01GV_OK;
	status = w25n01gv_read_id(&flash, id);
	if (status == W25N01GV_OK){
		printf("Flash Manufacturer ID:        0x%02X\r\n", id[0]);
		printf("Flash Device ID:              0x%02X%02X\r\n", id[1], id[2]);
	}
	else{
		printf("Error: Unable to read flash JEDEC ID. \r\n");
		return;
	}
}

void cmd_readFlash(void){
	uint32_t address;
	uint16_t page_addr;
	uint16_t column_addr;
	uint16_t size;
	w25n01gv_status_t status = W25N01GV_OK;
	CLI_CHECK_ARG_CNT(2);

	char *end;
	address = strtoul(arg_locs[1], &end, 10);
	size = strtoul(arg_locs[2], &end, 10);

	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	column_addr = W25N01GV_ADDR_TO_COLUMN_ADDR(address);
	if (size > 8192) {
		size = 8192;
	}
	printf("Page address:   0x%04X\r\nColumn address: 0x%04X\r\nSize:           %d Bytes\r\n", page_addr, column_addr, size);

	uint8_t data[size];
	status = w25n01gv_read(&flash, data, address, size);
	if (status == W25N01GV_OK){
		for (uint16_t i = 0; i < size; i++) {
#if CLI_FLASH_READ_SHOW_ASCII == 1
			if ((i % CLI_FLASH_READ_ASCII_LINEWIDTH == 0) && (i > 0)) {
				printf("\r\n");
			}
			printf("%c", data[i]);
#else
			if ((i % 16 == 0) && (i > 0)) {
				printf("\r\n");
			}
			printf("0x%02X ", data[i]);
#endif // CLI_FLASH_READ_SHOW_ASCII
		}
		printf("\r\n");
	}
	return;
}

void cmd_eraseFlash(void){
	uint8_t full_erase = 0;
	uint32_t address;
	uint16_t page_addr;
	uint16_t nof_blocks;
	w25n01gv_status_t status = W25N01GV_OK;
	CLI_CHECK_ARG_CNT_RANGE(1, 2);

	char *end;
	address = strtoul(arg_locs[1], &end, 10);
	full_erase = strcmp(arg_locs[1], "Full") == 0;
	if (full_erase == 1){
		nof_blocks = W25N01GV_NOF_BLOCKS;
	}
	else {
		if (number_of_args == 1) {
			nof_blocks = 1;
		}
		else {
			nof_blocks = strtoul(arg_locs[2], &end, 10);
			if (nof_blocks > W25N01GV_NOF_BLOCKS) {
				nof_blocks = W25N01GV_NOF_BLOCKS;
			}
		}
	}
	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	printf("Flash erase, starting address: 0x%04X\r\n", page_addr);
	printf("Flash erase, number of blocks: %u\r\n", nof_blocks);

	status = w25n01gv_erase(&flash, address, address + nof_blocks * W25N01GV_PAGE_SIZE * W25N01GV_PAGES_PER_BLOCK-1);
	if (status != W25N01GV_OK) {
		printf("Flash erase failed, error code: %d", status);
	}
	return;
}


void cmd_writeFlash(void){
	uint32_t address;
	uint32_t size;
	w25n01gv_status_t status = W25N01GV_OK;
#if CLI_FLASH_WRITE_LIPSUM != 1
	uint8_t data[LINE_BUF_SIZE] = {0};
	CLI_CHECK_ARG_CNT(3);
#else // CLI_FLASH_WRITE_LIPSUM == 1
	uint8_t data[16384] = {0};
	uint16_t page_addr;
	CLI_CHECK_ARG_CNT_MIN(2);
#endif // CLI_FLASH_WRITE_LIPSUM

	char *end;
	address = strtoul(arg_locs[1], &end, 10);
	size = strtoul(arg_locs[2], &end, 10);

#if CLI_FLASH_WRITE_LIPSUM != 1
	if (size > LINE_BUF_SIZE) {
		size = LINE_BUF_SIZE;
	}
	for (uint16_t i = 0; i < size; i++) {
		data[i] = arg_locs[3][i];
	}
#else // CLI_FLASH_WRITE_LIPSUM == 1
	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	if (page_addr == 0){
		char str[] = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Ut purus elit, vestibulum ut, placerat ac, adipiscing vitae, felis. Curabitur dictum gravida mauris. Nam arcu libero, nonummy eget, consectetuer id, vulputate a, magna. Donec vehicula augue eu neque. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Mauris ut leo. Cras viverra metus rhoncus sem. Nulla et lectus vestibulum urna fringilla ultrices. Phasellus eu tellus sit amet tortor gravida placerat. Integer sapien est, iaculis in, pretium quis, viverra ac, nunc. Praesent eget sem vel leo ultrices bibendum. Aenean faucibus. Morbi dolor nulla, malesuada eu, pulvinar at, mollis ac, nulla. Curabitur auctor semper nulla. Donec varius orci eget risus. Duis nibh mi, congue eu, accumsan eleifend, sagittis quis, diam. Duis eget orci sit amet orci dignissim rutrum. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 1) {
		char str[] = "Nam dui ligula, fringilla a, euismod sodales, sollicitudin vel, wisi. Morbi auctor lorem non justo. Nam lacus libero, pretium at, lobortis vitae, ultricies et, tellus. Donec aliquet, tortor sed accumsan bibendum, erat ligula aliquet magna, vitae ornare odio metus a mi. Morbi ac orci et nisl hendrerit mollis. Suspendisse ut massa. Cras nec ante. Pellentesque a nulla. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Aliquam tincidunt urna. Nulla ullamcorper vestibulum turpis. Pellentesque cursus luctus mauris. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 2) {
		char str[] = "Nulla malesuada porttitor diam. Donec felis erat, congue non, volutpat at, tincidunt tristique, libero. Vivamus viverra fermentum felis. Donec nonummy pellentesque ante. Phasellus adipiscing semper elit. Proin fermentum massa ac quam. Sed diam turpis, molestie vitae, placerat a, molestie nec, leo. Maecenas lacinia. Nam ipsum ligula, eleifend at, accumsan nec, suscipit a, ipsum. Morbi blandit ligula feugiat magna. Nunc eleifend consequat lorem. Sed lacinia nulla vitae enim. Pellentesque tincidunt purus vel magna. Integer non enim. Praesent euismod nunc eu purus. Donec bibendum quam in tellus. Nullam cursus pulvinar lectus. Donec et mi. Nam vulputate metus eu enim. Vestibulum pellentesque felis eu massa. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 3) {
		char str[] = "Quisque ullamcorper placerat ipsum. Cras nibh. Morbi vel justo vitae lacus tincidunt ultrices. Lorem ipsum dolor sit amet, consectetuer adipiscing elit. In hac habitasse platea dictumst. Integer tempus convallis augue. Etiam facilisis. Nunc elementum fermentum wisi. Aenean placerat. Ut imperdiet, enim sed gravida sollicitudin, felis odio placerat quam, ac pulvinar elit purus eget enim. Nunc vitae tortor. Proin tempus nibh sit amet nisl. Vivamus quis tortor vitae risus porta vehicula. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 4) {
		char str[] = "Fusce mauris. Vestibulum luctus nibh at lectus. Sed bibendum, nulla a faucibus semper, leo velit ultricies tellus, ac venenatis arcu wisi vel nisl. Vestibulum diam. Aliquam pellentesque, augue quis sagittis posuere, turpis lacus congue quam, in hendrerit risus eros eget felis. Maecenas eget erat in sapien mattis porttitor. Vestibulum porttitor. Nulla facilisi. Sed a turpis eu lacus commodo facilisis. Morbi fringilla, wisi in dignissim interdum, justo lectus sagittis dui, et vehicula libero dui cursus dui. Mauris tempor ligula sed lacus. Duis cursus enim ut augue. Cras ac magna. Cras nulla. Nulla egestas. Curabitur a leo. Quisque egestas wisi eget nunc. Nam feugiat lacus vel est. Curabitur consectetuer. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 5) {
		char str[] = "Suspendisse vel felis. Ut lorem lorem, interdum eu, tincidunt sit amet, laoreet vitae, arcu. Aenean faucibus pede eu ante. Praesent enim elit, rutrum at, molestie non, nonummy vel, nisl. Ut lectus eros, malesuada sit amet, fermentum eu, sodales cursus, magna. Donec eu purus. Quisque vehicula, urna sed ultricies auctor, pede lorem egestas dui, et convallis elit erat sed nulla. Donec luctus. Curabitur et nunc. Aliquam dolor odio, commodo pretium, ultricies non, pharetra in, velit. Integer arcu est, nonummy in, fermentum faucibus, egestas vel, odio. ";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
	else if (page_addr == 63) {
		char str[] = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		if (size > sizeof(str)) {
			size = sizeof(str);
		}
	}
	else {
		char str[] = "None";
		for (uint16_t i = 0; i < sizeof(str); i++){
			data[i] = (uint8_t) str[i];
		}
		size = sizeof(str);
	}
#endif // CLI_FLASH_WRITE_LIPSUM

	status = w25n01gv_write(&flash, data, address, size);
	if (status != W25N01GV_OK) {
		printf("Unable to write flash memory. \r\n");
	}
	return;
}

void cmd_testFlash(void){
	uint8_t id[4];
	w25n01gv_status_t status = W25N01GV_OK;
	w25n01gv_prot_reg_t reg_prot = {0};
	w25n01gv_conf_reg_t reg_conf = {0};
	w25n01gv_status_reg_t reg_status = {0};
	status = w25n01gv_read_id(&flash, id);
	if (status == W25N01GV_OK){
		printf("Flash Manufacturer ID:        0x%02X\r\n", id[0]);
		printf("Flash Device ID:              0x%02X%02X\r\n", id[1], id[2]);
	}
	else{
		printf("Error: Unable to read flash JEDEC ID. \r\n");
		return;
	}
#if CLI_FLASH_TEST_STATUS_REG_READ == 1
	// Status register read test
	printf("\r\n= Flash status register read test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_prot(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash protection register:    0x%02X\r\n", flash.reg_prot.reg);
	}
	else{
		printf("Error: Unable to read flash protection register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
	else{
		printf("Error: Unable to read flash configuration register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash status register:        0x%02X\r\n", flash.reg_status.reg);
	}
	else{
		printf("Error: Unable to read flash status register. \r\n");
		return;
	}
#endif // CLI_FLASH_TEST_STATUS_REG_READ
#if CLI_FLASH_TEST_STATUS_REG_WRITE == 1
	// Status register write test
	printf("\r\n= Flash status register write test: \r\n");
	reg_conf.fields.buf_mode=0;
	if (status == W25N01GV_OK){
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X (after writing)\r\n", flash.reg_conf.reg);
		reg_conf.fields.buf_mode=1;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	else{
		printf("Error: Unable to read flash configuration register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X (after resetting)\r\n", flash.reg_conf.reg);
	}
#endif // CLI_FLASH_TEST_STATUS_REG_WRITE
#if CLI_FLASH_TEST_BUF_EN_DIS == 1
	// Buffer mode enable / disable test
	printf("\r\n= Buffer mode enable / disable test: \r\n");
	printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	if (status == W25N01GV_OK){
		status = w25n01gv_buffer_mode_disable(&flash);
		printf("Disable buffer mode\r\n");
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_buffer_mode_enable(&flash);
		printf("Enable buffer mode\r\n");
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
#endif // CLI_FLASH_TEST_BUF_EN_DIS
#if CLI_FLASH_TEST_WRITE_EN_DIS == 1
	// Write enable and disable test
	printf("\r\n= Flash write enable / disable test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register before write enable: 0x%02X\r\n", flash.reg_status.reg);
		status = w25n01gv_write_enable(&flash);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register after write enable:  0x%02X\r\n", flash.reg_status.reg);
		status = w25n01gv_write_disable(&flash);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register after write disable: 0x%02X\r\n", flash.reg_status.reg);
	}
#endif // CLI_FLASH_TEST_WRITE_EN_DIS
#if CLI_FLASH_TEST_READ_BLOCK == 1
	// Read block test
	printf("\r\n= Read block test: \r\n");
	uint16_t test_read_block_nof_bytes = 16;
	for (uint16_t page_addr = 0; page_addr < 16; page_addr++) {
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(&flash);
		}
		uint8_t data[test_read_block_nof_bytes];
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, data, 0x00, test_read_block_nof_bytes);
		}
		if (status == W25N01GV_OK){
			printf("Page: %4d Data:", page_addr);
			for (uint16_t i = 0; i < test_read_block_nof_bytes; i++) {
				if ((i % 16 == 0) && (i > 0)) {
					printf("\r\n                ");
				}
				printf(" 0x%02X", data[i]);
			}
			if (data[0] != 0xFF) {
				printf(" <= Bad Block present ==");
			}
			printf("\r\n");
		}
	}
#endif // CLI_FLASH_TEST_READ_BLOCK
#if CLI_FLASH_TEST_READ_OTP_BLOCK == 1
	// Read OTP block test
	printf("\r\n= Read OTP block test: \r\n");
	uint16_t test_read_block_otp_nof_bytes = 256;
	if (status == W25N01GV_OK){
		reg_conf.fields.otp_enter = 1;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	for (uint16_t page_addr = 0; page_addr < 3; page_addr++) {
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(&flash);
		}
		uint8_t data[test_read_block_otp_nof_bytes];
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, data, 0x00, test_read_block_otp_nof_bytes);
		}
		if (status == W25N01GV_OK){
			printf("Page: %4d Data:", page_addr);
			for (uint16_t i = 0; i < test_read_block_otp_nof_bytes; i++) {
				if ((i % 16 == 0) && (i > 0)) {
					printf("\r\n                ");
				}
				printf(" 0x%02X", data[i]);
			}
			printf("\r\n");
		}
	}
	if (status == W25N01GV_OK){
		reg_conf.fields.otp_enter = 0;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
#endif // CLI_FLASH_TEST_READ_OTP_BLOCK
#if CLI_FLASH_TEST_BAD_BLOCK_LUT == 1
	// Read bad block look up table test
	printf("\r\n= Read bad block look up table test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_badblock(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Bad block LUT: \r\n");
		printf("ID | Logical | Physical\r\n");
		printf("---+---------+---------\r\n");
		for (uint8_t i = 0; i < W25N01GV_BADBLOCK_LUT_SIZE; i++) {
			printf("%2d | 0x%04X  | 0x%04X\r\n", i, flash.badblock_lut[i].addr.logic_block_addr, flash.badblock_lut[i].addr.phys_block_addr);
		}
	}
#endif // CLI_FLASH_TEST_BAD_BLOCK_LUT
#if CLI_FLASH_TEST_BAD_BLOCK == 1
	// Bad block test
	printf("\r\n= Bad block test: \r\n");
	uint16_t bad_block_present = 0;
	uint16_t bb_loop_count = 0;
	uint8_t data_page;
	uint8_t data_spare;
	test_read_block_nof_bytes = 1;
	for (uint32_t page_addr = 0; page_addr < 0xffff; page_addr+=0x40) {
		bb_loop_count++;
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, (uint16_t) page_addr);
		}
		do {
			status = w25n01gv_read_reg_status(&flash);
		} while(status == W25N01GV_OK && flash.reg_status.fields.busy != 0);
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, &data_page, 0x00, 1);
		}
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, &data_spare, 0x800, 1);
		}
		if (status == W25N01GV_OK){
			if ((data_page != 0xFF) || (data_spare != 0xFF)) {
				bad_block_present = 1;
				printf("Bad block on page %4d, Page: 0x%02X  Spare: 0x%02X\r\n", (uint16_t) (page_addr / 0x40), data_page, data_spare);
			}
		}
	}
	printf("%d blocks scanned\r\n", bb_loop_count);
	if (bad_block_present == 0) {
		printf("No bad blocks\r\n");
	}
#endif // CLI_FLASH_TEST_BAD_BLOCK
#if CLI_FLASH_TEST_LAST_ECC_FAIL == 1
	// Last ECC failure page address test
	printf("\r\n= Last ECC failure page address test: \r\n");
	uint16_t ecc_fail_page_addr = 0xffff;
	if (status == W25N01GV_OK){
		status = w25n01gv_last_ecc_failure(&flash, &ecc_fail_page_addr);
	}
	if (status == W25N01GV_OK){
		printf("Page address of last ECC failure: %X\r\n", ecc_fail_page_addr);
	}
#endif // CLI_FLASH_TEST_LAST_ECC_FAIL
}

void cmd_changeState(void){
	uint8_t  newstate;
	CLI_CHECK_ARG_CNT(1);

	char *end;
	newstate = strtol(arg_locs[1], &end, 10);
	statemachine_switchfromIdle(newstate);
}

void cmd_setCtrlGain(void) {
	float P, I;
	uint8_t id;
	CLI_CHECK_ARG_CNT(3);
	char *end;
	id = strtol(arg_locs[1], &end, 10);
	P = strtof(arg_locs[2], &end);
	I = strtof(arg_locs[3], &end);
	if (id == 0) {
		ctrl_pi_voltage_buck.P_gain = P;
		ctrl_pi_voltage_buck.I_gain = I / CTRL_FREQ;
	}else if (id == 1) {
			ctrl_pi_voltage_boost.P_gain = P;
			ctrl_pi_voltage_boost.I_gain = I / CTRL_FREQ;
	} else if (id == 2) {
		ctrl_pi_current.P_gain = P;
		ctrl_pi_current.I_gain = I / CTRL_FREQ;

	} else if (id == 3) {
		ctrl_pi_boost_iout_limit.P_gain = P;
		ctrl_pi_boost_iout_limit.I_gain = I / CTRL_FREQ;
	}
}

void cmd_printRTC(void){
	RTC_DateTypeDef date = {0};
	RTC_TimeTypeDef time = {0};
	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
	char weekday[4];
	switch(date.WeekDay) {
	case RTC_WEEKDAY_MONDAY:
		strcpy(weekday, "Mon");
		break;
	case RTC_WEEKDAY_TUESDAY:
		strcpy(weekday, "Tue");
		break;
	case RTC_WEEKDAY_WEDNESDAY:
		strcpy(weekday, "Wed");
		break;
	case RTC_WEEKDAY_THURSDAY:
		strcpy(weekday, "Thu");
		break;
	case RTC_WEEKDAY_FRIDAY:
		strcpy(weekday, "Fri");
		break;
	case RTC_WEEKDAY_SATURDAY:
		strcpy(weekday, "Sat");
		break;
	case RTC_WEEKDAY_SUNDAY:
		strcpy(weekday, "Sun");
		break;
	default:
		strcpy(weekday, "_?_");
		break;
	}
	printf("20%02d-%02d-%02d %02d:%02d.%02d %s\r\n", date.Year, date.Month, date.Date, time.Hours, time.Minutes, time.Seconds, weekday);
	return;
}

void cmd_setRTC(void){
	RTC_DateTypeDef date = {0};
	RTC_TimeTypeDef time = {0};
	HAL_StatusTypeDef status = HAL_OK;
	CLI_CHECK_ARG_CNT_RANGE(6, 7);
	char *end;
	date.Year = (strtoul(arg_locs[1], &end, 10) % 100);
	date.Month = strtoul(arg_locs[2], &end, 10);
	date.Date = strtoul(arg_locs[3], &end, 10);
	time.Hours = strtoul(arg_locs[4], &end, 10);
	time.Minutes = strtoul(arg_locs[5], &end, 10);
	time.Seconds = strtoul(arg_locs[6], &end, 10);
	if (!IS_RTC_MONTH(date.Month) || !IS_RTC_DATE(date.Date) || !IS_RTC_HOUR24(time.Hours) || !IS_RTC_MINUTES(time.Minutes) || !IS_RTC_SECONDS(time.Seconds)) {
		printf("Error, date or time out of range: \r\n");
		printf("20%02d-%02d-%02d %02d:%02d.%02d\r\n", date.Year, date.Month, date.Date, time.Hours, time.Minutes, time.Seconds);
		return;
	}
	if (number_of_args >= 7) {
		if      (strcmp(arg_locs[7], "Mon") == 0 || strcmp(arg_locs[7], "Monday")    == 0) {
			date.WeekDay = RTC_WEEKDAY_MONDAY;
		}
		else if (strcmp(arg_locs[7], "Tue") == 0 || strcmp(arg_locs[7], "Tuesday")   == 0) {
			date.WeekDay = RTC_WEEKDAY_TUESDAY;
		}
		else if (strcmp(arg_locs[7], "Wed") == 0 || strcmp(arg_locs[7], "Wednesday") == 0) {
			date.WeekDay = RTC_WEEKDAY_WEDNESDAY;
		}
		else if (strcmp(arg_locs[7], "Thu") == 0 || strcmp(arg_locs[7], "Thursday")  == 0) {
			date.WeekDay = RTC_WEEKDAY_THURSDAY;
		}
		else if (strcmp(arg_locs[7], "Fri") == 0 || strcmp(arg_locs[7], "Friday")    == 0) {
			date.WeekDay = RTC_WEEKDAY_FRIDAY;
		}
		else if (strcmp(arg_locs[7], "Sat") == 0 || strcmp(arg_locs[7], "Saturday")  == 0) {
			date.WeekDay = RTC_WEEKDAY_SATURDAY;
		}
		else if (strcmp(arg_locs[7], "Sun") == 0 || strcmp(arg_locs[7], "Sunday")    == 0) {
			date.WeekDay = RTC_WEEKDAY_SUNDAY;
		}
		else {
			printf("Error, unable to parse day of week: %s\r\n", arg_locs[7]);
			return;
		}
	}
	status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
	if (status == HAL_OK) {
		status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
	}
	else {
		printf("Setting date failed. \r\n");
	}
	if (status != HAL_OK) {
		printf("Setting time failed. \r\n");
	}
	return;
}

void cmd_testLCD(void){
	LCD_Test();
	return;
}

/**
 * Enters keyboard-control mode: drives the on-device menu/encoder/buttons
 * from the serial terminal instead of the physical hardware, for developing
 * and testing the UI without the encoder/buttons/display board attached.
 */
void cmd_control(void) {
	cli_control_active = 1;
	cli_control_esc_state = 0;
	input_set_source(INPUT_SOURCE_CLI);
	cli_printf(
			"\r\nKeyboard control mode.\r\n"
			"  Arrows = encoder   Enter = OK   Esc = ESC   Space = OUT (toggle)   q = quit\r\n");
}

/**
 * (Re)provisions the menu icon store on the QSPI flash from the compiled-in
 * seed artwork (icon_seed_data.c). Erases and rewrites the whole store.
 */
void cmd_flashIcons(void) {
	if (icon_store_flash_seed() == 0) {
		printf("Icon store flashed OK.\r\n");
	} else {
		printf("Icon store flash FAILED.\r\n");
	}
}

/**
 * Debug dump of the protection module's current classification: worst level,
 * the warning/error source masks decoded by name, and the raw inputs
 * (4 temps + OVP/OCP pin states) that fed them.
 */
void cmd_printProtection(void) {
	static const protection_source_t all_sources[] = {
		PROTECTION_SRC_TEMP_SEC, PROTECTION_SRC_TEMP_TRAFO,
		PROTECTION_SRC_TEMP_CURRENT, PROTECTION_SRC_TEMP_PRIM,
		PROTECTION_SRC_OVP, PROTECTION_SRC_OCP
	};
	uint16_t warning_mask = protection_get_warning_mask();
	uint16_t error_mask = protection_get_error_mask();

	printf("Worst level:        %d (0=OK, 1=WARNING, 2=ERROR)\r\n", protection_get_worst_level());

	printf("Warnings: ");
	for (uint8_t i = 0; i < sizeof(all_sources) / sizeof(all_sources[0]); i++) {
		if (warning_mask & all_sources[i]) {
			printf("%s ", protection_source_name(all_sources[i]));
		}
	}
	printf("\r\n");

	printf("Errors:   ");
	for (uint8_t i = 0; i < sizeof(all_sources) / sizeof(all_sources[0]); i++) {
		if (error_mask & all_sources[i]) {
			printf("%s ", protection_source_name(all_sources[i]));
		}
	}
	printf("\r\n");

	printf("temp_sec:            %d\r\n", adc_data.converted.temp_sec);
	printf("temp_trafo:          %d\r\n", adc_data.converted.temp_trafo);
	printf("temp_current:        %d\r\n", adc_data.converted.temp_current);
	printf("temp_prim:           %d\r\n", adc_data.converted.temp_prim);
	printf("OVP Pin Latch:       %d\r\n", HAL_GPIO_ReadPin(OVP_N_GPIO_Port, OVP_N_Pin));
	printf("OCP Pin Latch:       %d\r\n", HAL_GPIO_ReadPin(OCP_N_GPIO_Port, OCP_N_Pin));
}

#endif

