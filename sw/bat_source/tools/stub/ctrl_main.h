#pragma once
#define CTRL_PID_TABLE_LEN 10
typedef struct { float *cal_p; float *cal_i; float default_p; float default_i; } ctrl_pid_entry_t;
extern const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN];
