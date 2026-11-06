/*
 */

#ifndef SRC_CTRL_PID_CONTROL_H_
#define SRC_CTRL_PID_CONTROL_H_
#include <stdint.h>

#define ctrl_PID_controller_execute_lin(contr,data_in_PI,data_in_D) ctrl_PID_controller_execute(contr,data_in_PI,data_in_D, 0)
#define ctrl_PID_controller_execute_rot(contr,data_in_PI,data_in_D) ctrl_PID_controller_execute(contr,data_in_PI,data_in_D, 1)

#define TRAPEZIODAL_INTEGRATOR
#define NO_D_PART

typedef struct
{
	float ref;
#ifndef NO_D_PART
	float err;
	double deriv;
#endif

	float P_gain;
	float I_gain;
#ifndef NO_D_PART
	float D_gain;
	float D_filt;
	float D_sigma;
#endif

	float P_action;
	float I_action_out;
	// double on purpose: float drops I steps < ~6e-8 (CC/HV limiter dead band).
	double I_action;
#ifndef NO_D_PART
	float D_action;
#endif
	float action;
#ifndef NO_D_PART
	float prev_ref;
#endif
	double prev_I_action;

	float sat_limit_high;
	float sat_limit_low;
	int    sat_flag;
} PID_controller_t;

void ctrl_PID_controller_init(PID_controller_t *contr, float kp, float ki, float kd, float sat_high, float sat_low);
void ctrl_PID_controller_execute(PID_controller_t *contr, float data_in_P, float data_in_I, float data_in_D);
void ctrl_PID_reset(PID_controller_t *contr);



#endif /* SRC_CTRL_PID_CONTROL_H_ */
