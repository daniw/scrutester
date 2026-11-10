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

#ifdef CLI_ENABLED
extern char *arg_locs[MAX_NUM_ARGS+1];
extern uint8_t number_of_args;
extern const char *cmd_arg_str[];
extern int cmd_index;

#define CLI_CHECK_ARG_CNT(nof_args)                 if (number_of_args != nof_args)                             {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_MIN(nof_args)             if (number_of_args < nof_args)                              {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_MAX(nof_args)             if (number_of_args > nof_args)                              {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_RANGE(min_args, max_args) if (number_of_args < min_args || number_of_args > max_args) {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#define CLI_CHECK_ARG_CNT_NONE(nof_args)            if (number_of_args != nof_args && number_of_args != 0)      {printf("Incorrect number of arguments\nUsage: %s\n", cmd_arg_str[cmd_index]); return;}
#endif

void cli_init(UART_HandleTypeDef * huart1);
void cli_loop(void);

#endif
