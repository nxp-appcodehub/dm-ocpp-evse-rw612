  /*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#include "EVSE_LowLevel_Handles.h"
#include "EVSE_ChargingProtocol.h"
#include "EVSE_Metrology.h"
#include "ControlPilot_Process.h"
#include "FreeRTOS.h"

#define CP_SUCCESIVE_SAME_STATE 2

static bool contactor_state = true;
static ControlPilotState_t EVSE_J1772_GetStateFromCPValue(uint32_t cp_value)
{
	static ControlPilotState_t latest_state_returned = ControlPilot_StateUnknown;
	static ControlPilotState_t latest_state_detected = ControlPilot_StateA;
	static uint8_t consecutive_same_state_returned = 0;
	ControlPilotState_t state = ControlPilot_StateUnknown;
	cp_value = cp_value/SCALING_FACTOR;
	//configPRINTF(("%d", cp_value));

    if ((cp_value > STATEA_MIN_LEVEL) && (cp_value < STATEA_MAX_LEVEL))
    {
       state = ControlPilot_StateA;
    }
    else if ((cp_value > STATEB_MIN_LEVEL) && (cp_value < STATEB_MAX_LEVEL))
    {
        state = ControlPilot_StateB;
    }
    else if ((cp_value > STATEC_MIN_LEVEL) && (cp_value < STATEC_MAX_LEVEL))
    {
        state = ControlPilot_StateC;
    }
    else if ((cp_value > STATED_MIN_LEVEL) && (cp_value < STATED_MAX_LEVEL))
    {
        state = ControlPilot_StateD;
    }
    else if ((cp_value > STATEE_MIN_LEVEL) && (cp_value < STATEE_MAX_LEVEL))
    {
        state = ControlPilot_StateE;
    }
    else if ((cp_value > STATEF_MIN_LEVEL) && (cp_value < STATEF_MAX_LEVEL))
    {
        state = ControlPilot_StateF;
    }

    latest_state_detected = state;

    if (ControlPilot_StateUnknown == state)
    {
    	consecutive_same_state_returned = 0;
    }
    else if (CP_SUCCESIVE_SAME_STATE == consecutive_same_state_returned)
    {
    	latest_state_returned = state;
    }
    else if (latest_state_detected == state)
    {
    	consecutive_same_state_returned++;
    }
    else
    {
    	consecutive_same_state_returned = 0;
    }

    return latest_state_returned;
}

static void EVSE_LL_Init(void)
{
	ControlPilot_PWM_Init();
	ADC_Manager_Init();
    ADC_Manager_ConfigureADCTrigger();
}

static void EVSE_LL_Deinit(void)
{
	/* Do Nothing */
}

static bool EVSE_LL_IsReady(void)
{
	/* TO DO add check for the metrology */
	return true;
}

static void EVSE_LL_SetPwmDuty(float duty)
{
	ControlPilot_UpdatePWM(duty);
}

static float EVSE_LL_GetPwmDuty(void)
{
	float duty_cycle;
	ControlPilot_GetDutyCycle(&duty_cycle);
	return duty_cycle;
}

static void EVSE_LL_SetPwmState(bool enable)
{
	ControlPilot_PWM_SetState(enable);
}

static uint16_t EVSE_LL_ReadPilotVoltage(void)
{
	uint16_t pilot_voltage = ADC_Manager_ReadControlPilotVoltage();
    return pilot_voltage;
}

static ControlPilotState_t EVSE_LL_ReadControlPilot(void)
{
	return  EVSE_J1772_GetStateFromCPValue(EVSE_LL_ReadPilotVoltage());
}

static uint16_t EVSE_LL_ReadProximityVoltage(void)
{
	return 0;
}

static ProximityPilotState_t EVSE_LL_ReadProximityPilot(void)
{
	return ProximityPilot_Unplugged;
}

static void EVSE_LL_SetContactorState(bool relay_open)
{
	configPRINTF(("Relay Open %d", relay_open));
	contactor_state = relay_open;
}

static bool EVSE_LL_GetContactorStatus(void)
{
	return contactor_state;
}

static void EVSE_LL_ReadCurrent(double *IL1, double *IL2, double *IL3)
{
	const phase_data_t current = Metrology_ReadCurrent();
	if (IL1)
	{
		*IL1 = current.ph1;
	}
	if (IL2)
	{
		*IL2 = current.ph2;
	}
	if (IL3)
	{
		*IL3 = current.ph3;
	}
}

static void EVSE_LL_ReadVoltage(double *VL1, double *VL2, double *VL3)
{
	const phase_data_t voltage = Metrology_ReadVoltage();
	if (VL1)
	{
		*VL1 = voltage.ph1;
	}
	if (VL2)
	{
		*VL2 = voltage.ph2;
	}
	if (VL3)
	{
		*VL3 = voltage.ph3;
	}
}

static void EVSE_LL_ReadPower(double *P1, double *P2, double *P3)
{
	const phase_data_t active_power = Metrology_ReadActivePower();
	if (P1)
	{
		*P1 = active_power.ph1;
	}
	if (P2)
	{
		*P2 = active_power.ph2;
	}
	if (P3)
	{
		*P3 = active_power.ph3;
	}
}

static float EVSE_LL_ReadEnergy(void)
{
	const tENERGY_REG energy_cnt = Metrology_ReadEnergy();
	return (float)energy_cnt.wh_t;
}

static void EVSE_LL_SetVentilation(bool enable)
{
	configPRINTF(("Ventilation started %d", enable));
}

const charging_hal_functions_t ll_charging_hal = {
		.init = EVSE_LL_Init,
		.deinit = EVSE_LL_Deinit,
		.isReady = EVSE_LL_IsReady,
		.set_pwm_duty = EVSE_LL_SetPwmDuty,
		.get_pwm_duty = EVSE_LL_GetPwmDuty,
		.set_pwm_state = EVSE_LL_SetPwmState,
		.read_pilot_voltage = EVSE_LL_ReadPilotVoltage,
		.read_control_pilot = EVSE_LL_ReadControlPilot,
		.read_proximity_voltage = EVSE_LL_ReadProximityVoltage,
		.read_proximity_pilot = EVSE_LL_ReadProximityPilot,
		.set_contactor_state = EVSE_LL_SetContactorState,
		.get_contactor_status = EVSE_LL_GetContactorStatus,
		.read_current = EVSE_LL_ReadCurrent,
		.read_voltage = EVSE_LL_ReadVoltage,
		.read_energy = EVSE_LL_ReadEnergy,
		.read_power = EVSE_LL_ReadPower,
		.ventilation_state = EVSE_LL_SetVentilation,
};
