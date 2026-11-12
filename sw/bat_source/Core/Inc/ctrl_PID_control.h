/*
 */

#ifndef SRC_CTRL_PID_CONTROL_H_
#define SRC_CTRL_PID_CONTROL_H_
#include <stdint.h>

typedef struct
{
	float ref;

	float P_gain;
	float I_gain;

	float P_action;
	// double on purpose: float drops I steps < ~6e-8 (CC/HV limiter dead band).
	double I_action;
	float action;
	double prev_I_action;

	float sat_limit_high;
	float sat_limit_low;
} PID_controller_t;

void ctrl_PID_controller_init(PID_controller_t *contr, float kp, float ki, float sat_high, float sat_low);
void ctrl_PID_controller_execute(PID_controller_t *contr, float data_in_P, float data_in_I);
void ctrl_PID_reset(PID_controller_t *contr);



#endif /* SRC_CTRL_PID_CONTROL_H_ */
