/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#ifndef EVSE_LOWLEVEL_HANDLES_H_
#define EVSE_LOWLEVEL_HANDLES_H_

#include "EVSE_ChargingProtocol.h"

#define MAX_LEVEL 						65535
#define STATEA_MIN_LEVEL				60000
#define STATEA_LEVEL					61000
#define STATEA_MAX_LEVEL				62000
#define STATEB_MIN_LEVEL				54000
#define STATEB_LEVEL					55000
#define STATEB_MAX_LEVEL				56000
#define STATEC_MIN_LEVEL				47000
#define STATEC_LEVEL					48000
#define STATEC_MAX_LEVEL				49000
#define STATED_MIN_LEVEL				43000
#define STATED_LEVEL					44000
#define STATED_MAX_LEVEL				45000

#define STATEE_MIN_LEVEL				30000
#define STATEE_LEVEL					31000
#define STATEE_MAX_LEVEL				32000

#define STATEF_MIN_LEVEL				25000	/* -12V low state value */
#define STATEF_LEVEL					26000
#define STATEF_MAX_LEVEL				29000	/* -12V low state value */


#define CP_REFERENCE_SIGNAL_VOLTAGE (1.8f)
#define CP_MAX_VOLTAGE (1.6f)
#define SCALING_FACTOR (CP_MAX_VOLTAGE/CP_REFERENCE_SIGNAL_VOLTAGE)

#define DAC_OUTPUT_VALUE(x) (((float)CP_REFERENCE_SIGNAL_VOLTAGE * SCALING_FACTOR * x) / MAX_LEVEL)
#define DAC_INTPUT_CODE(STATE) (DAC_OUTPUT_VALUE(STATE) - 0.18) * 1023 / 1.42f

#define STATE_VALUE_SCALED(x)  (x*SCALING_FACTOR)

extern const charging_hal_functions_t ll_charging_hal;

#endif /* EVSE_LOWLEVEL_HANDLES_H_ */
