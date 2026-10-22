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
#ifndef CLI_H
#define CLI_H

#include "main.h"


#define LINE_BUF_SIZE 128
#define MAX_NUM_ARGS 8

#include "debug.h" // CLI_ENABLED, CLI_DEBUG, CLI_FLASH_*

void cli_init(UART_HandleTypeDef * huart1);
void cli_loop(void);

#endif
