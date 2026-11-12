/*******************************************************************************/
/*
 * File: cli_flash_test.h
 * Created on: Nov 9, 2026
 * Author: ahorat
 */
/*******************************************************************************/
/*
 * Flash bench-test CLI commands, moved out of cli.c.
 */
/*******************************************************************************/
#ifndef CLI_FLASH_TEST_H
#define CLI_FLASH_TEST_H

#include "main.h"
#include "debug.h" // CLI_ENABLED, CLI_FLASH_*

#ifdef CLI_ENABLED

#define CLI_FLASH_BUF_SIZE 512
extern uint8_t cli_flash_buf[CLI_FLASH_BUF_SIZE];

void cmd_writeFlash(void);
void cmd_testFlash(void);

#endif

#endif
