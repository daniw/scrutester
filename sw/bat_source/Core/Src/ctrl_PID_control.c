/*
 */

#include "ctrl_PID_control.h"
#include "math.h"
#include "ctrl_param.h"

//#####################################################################################################################################################
// General purpose PI control

/**
 * Initialises Control structure
 */
void ctrl_PID_controller_init(PID_controller_t *contr, float kp, float ki,
		float sat_high, float sat_low) {
	contr->ref = 0;

	contr->P_gain = kp;
	contr->I_gain = ki;
	contr->P_action = 0;
	contr->I_action = 0;
	contr->action = 0;

	contr->prev_I_action = 0;

	contr->sat_limit_high = sat_high;
	contr->sat_limit_low = sat_low;
}

/**
 * Executes the PID Controller
 * @param data_in_P value used for the P part
 * @param data_in_I value used for the I part
 */
void ctrl_PID_controller_execute(PID_controller_t *contr, float data_in_P,
		float data_in_I) {
	contr->P_action = contr->P_gain * (contr->ref - data_in_P);

	contr->I_action = (double) (contr->I_gain * (contr->ref - data_in_I))
			+ contr->prev_I_action;

	contr->action = contr->P_action + contr->I_action;

	/* Integrator clamping: when the output saturates, pull I_action back
	 * to exactly the value consistent with sitting at the boundary given
	 * this cycle's P_action, instead of
	 * freezing it at whatever raw value first caused the overshoot. That
	 * keeps prev_I_action always representative of the current operating
	 * point, so resuming after saturation doesn't carry forward a stale
	 * or inconsistent baseline. */
	if (contr->action > contr->sat_limit_high) {
		contr->action = contr->sat_limit_high;
		contr->I_action = contr->sat_limit_high - contr->P_action;
	} else if (contr->action < contr->sat_limit_low) {
		contr->action = contr->sat_limit_low;
		contr->I_action = contr->sat_limit_low - contr->P_action;
	}
	contr->prev_I_action = contr->I_action;
}

void ctrl_PID_reset(PID_controller_t *contr)
{
	contr->action = 0;
	contr->I_action = 0;
	contr->prev_I_action = 0;
}
