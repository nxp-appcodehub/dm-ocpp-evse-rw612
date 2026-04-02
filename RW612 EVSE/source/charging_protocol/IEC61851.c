/*
 * Copyright 2024-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#include "app.h"
#include "fsl_common.h"

#if ENABLE_SIGBRD
#include "hal_uart_bridge.h"
#endif
#if ENABLE_OCPP
#include "EVSE_Ocpp.h"
#endif /* ENABLE_OCPP */

#include "EVSE_ChargingProtocol.h"
#include "EVSE_Utils.h"

#include "IEC61851.h"

#if ENABLE_LOGGING_J1772
#include "FreeRTOS.h"
#define j1772PRINTF(x) configPRINTF( x )
#else
#define j1772PRINTF(x)
#endif /* ENABLE_LOGGING_J1772 */

/* Macros */
#define CALL_IF_NOT_NULL_ARGS(func_ptr, ...) \
    do {                                     \
        if ((func_ptr) != NULL) {            \
            (func_ptr)(__VA_ARGS__);         \
        }                                    \
    } while (0)

/* Type definitions */
typedef enum _j1772_state
{
    STATE_A = 0,
    STATE_A2,
    STATE_B,
    STATE_B2,
    STATE_C,
    STATE_C2,
    STATE_D,
    STATE_D2,
    STATE_E,
    STATE_F,
    J1772_LAST_STATE,
} j1772_state_t;

typedef struct _j1772_simulation_evse
{
    double fMeasuredVoltage;
    double fMeasuredCurrent;
    double fMeasuredPower;
    double fMeterEnergyDelivered;
    double fMeterReading;
    uint32_t uElapseTime;
    uint32_t uCarDetectTime;
} j1772_simulation_evse_t;

typedef struct _j1772_faults
{
	bool gfci;
	bool over_current;
}j1772_faults_t;

typedef struct _j1772_timers
{
	uint32_t ulLastCalcTime;
	uint32_t ulLastDisplayTime;
	uint32_t ulOverCurrentTime;
	uint32_t ulRelayClosedTimer;
}j1772_timers_t;

typedef struct _j1772_simulation
{
    float max_current;
    float max_voltage;
    j1772_timers_t timers;
    j1772_simulation_evse_t evse_data;
    j1772_state_t current_state;
    ControlPilotState_t cp_state;
    ControlPilotState_t last_cp_state;
    /* the value is in mil (pwm *10)*/
    float current_pwm;
    bool isCharging;
    bool auth_needed;
    bool ready_to_charge;
    j1772_faults_t faults;
} j1772_simulation_t;


/* Static variables */
static char *j1772_state_prompt[J1772_LAST_STATE] = {
    [STATE_A] = "Standby",          [STATE_A2] = "Standby, PWM ON",
    [STATE_B] = "Vehicle detected", [STATE_B2] = "Vehicle detected",
    [STATE_C] = "Ready (charging)", [STATE_C2] = "Ready (No charging)",
    [STATE_D] = "Ventilation",      [STATE_D2] = "Ventilation(No charging)",
    [STATE_E] = "No power",         [STATE_F] = "Error",
};

static const char j1772_state_string[ControlPilot_StateUnknown + 1] = {
    [ControlPilot_StateA] = 'A',
    [ControlPilot_StateB] = 'B',
    [ControlPilot_StateC] = 'C',
    [ControlPilot_StateD] = 'D',
    [ControlPilot_StateE] = 'E',
    [ControlPilot_StateF] = 'F',
    [ControlPilot_StateUnknown] = 'U'
};

static const charging_hal_functions_t *s_hal_functions = NULL;

#if ENABLE_J1772
static j1772_simulation_t j1772_simulation;
static uint32_t s_u32OpenRelayTimeout;
#endif

/* Private function prototypes */
static double prvCalculateEnergy(double current, double voltage, double time_interval);
#if ENABLE_J1772
static ControlPilotState_t EVSE_J1772_GetState(void);
static j1772_status_t EVSE_J1772_SetCPPwmFromState(j1772_state_t j1772_state, uint16_t amps);
static void EVSE_J1772_SetState(j1772_state_t new_state);
static void EVSE_J1772_Loop_EVSE(void);
static void EVSE_J1772_HandleStateA(j1772_state_t new_state, uint16_t *new_pwm);
static void EVSE_J1772_HandleStateA2(j1772_state_t new_state, uint16_t *new_pwm);
static void EVSE_J1772_HandleStateB(j1772_state_t new_state, uint16_t *new_pwm, float new_pwm_duty);
static void EVSE_J1772_HandleStateB2(j1772_state_t new_state, uint16_t *new_pwm);
static void EVSE_J1772_HandleStateCD(j1772_state_t new_state, uint16_t *new_pwm, float new_pwm_duty);
static void EVSE_J1772_HandleStateCD2(j1772_state_t new_state, uint16_t *new_pwm, float new_pwm_duty);
static void EVSE_J1772_HandleStateEF(void);
static void EVSE_J1772_UpdateMeasurements(uint32_t time_lapse);
static void EVSE_J1772_ResetVehicleData(void);
#endif

/* Private function implementations */
static double prvCalculateEnergy(double current, double voltage, double time_interval)
{
    double fEnergy = current * voltage * time_interval / 1000.0 / 3600.0;
    return fEnergy;
}

/* Public function implementations */
j1772_status_t EVSE_J1772_GetAmpsFromDutyCycle(float dutyCycle, float *amps)
{
    if (amps == NULL)
    {
        return J1772_Error;
    }

    if (dutyCycle < 3)
    {
        *amps = 0.0f;
    }
    else if ((dutyCycle >= 3) && (dutyCycle <= 7))
    {
        return J1772_CP_Req_Digital_mode;
    }
    else if ((dutyCycle > 7) && (dutyCycle < 8))
    {
        *amps = 0.0f;
    }
    else if ((dutyCycle >= 8) && (dutyCycle < 10))
    {
        *amps = 6;
    }
    else if (dutyCycle < 85)
    {
        *amps = (uint8_t)dutyCycle * 0.6f;
    }
    else if (dutyCycle < 97)
    {
        *amps = (uint8_t)(dutyCycle - 64) * 2.5f;
    }
    else if (dutyCycle == 100)
    {
        *amps = 0.0f;
    }
    else
    {
        return J1772_Error;
    }
    
    return J1772_Succes;
}

j1772_status_t EVSE_J1772_GetDutyCycleFromAmps(float amps, float *dutyCycle)
{
    if (dutyCycle == NULL)
    {
        return J1772_Error;
    }

    if (amps < 6)
    {
        return J1772_Error;
    }
    else if ((amps >= 6) && (amps < 51))
    {

        *dutyCycle = (uint8_t)amps / 0.6f;
    }
    else if ((amps >= 51) && (amps <= 81))
    {

        *dutyCycle = (uint8_t)((amps / 2.5f) + 64);
    }
    else
    {
        *dutyCycle = 0;

        return J1772_Error;
    }


    return J1772_Succes;
}

#if ENABLE_J1772

void EVSE_J1772_SetMaxCurrent(uint32_t max_current)
{
    if (j1772_simulation.max_current != max_current)
    {
        j1772_simulation.max_current = max_current;
    }
}

j1772_status_t EVSE_J1772_SetCPPWM(float pwm)
{
    s_hal_functions->set_pwm_duty(pwm);
    return J1772_Succes;
}

const char *EVSE_J1772_GetCurrentStateString(void)
{
    return j1772_state_prompt[j1772_simulation.current_state];
}

ControlPilotState_t EVSE_J1772_GetCpState(void)
{
	return j1772_simulation.cp_state;
}

char EVSE_J1772_GetCpStateString(void)
{
    return j1772_state_string[EVSE_J1772_GetCpState()];
}

void EVSE_J1772_isCharging(bool *bCharging)
{
    if (bCharging != NULL)
    {
        *bCharging = j1772_simulation.isCharging;
    }
}

void EVSE_J1772_StopCharging(bool bCharging)
{
    j1772_simulation.ready_to_charge = !bCharging;
}

void EVSE_J1772_GetVehicleData(vehicle_data_t *vehicle_data)
{
    if (vehicle_data == NULL)
    {
        return;
    }

    vehicle_data->chargeCurrent   = (float)j1772_simulation.evse_data.fMeasuredCurrent;
    vehicle_data->deliveredEnergy = (uint32_t)j1772_simulation.evse_data.fMeterEnergyDelivered;
    vehicle_data->elapsedTime = j1772_simulation.evse_data.uElapseTime / 1000;
}

void EVSE_J1772_Init(const charging_hal_functions_t *hal_functions)
{
    /* Validate HAL function pointers */
    if (hal_functions == NULL)
    {
        return;
    }

    /* Initialize simulation parameters */
    j1772_simulation.max_voltage = 230;
    s_hal_functions = hal_functions;

    if (j1772_simulation.max_current == 0)
    {
        EVSE_J1772_SetMaxCurrent(25);
    }

    j1772_simulation.ready_to_charge = true;
    j1772_simulation.current_state = J1772_LAST_STATE;
    j1772_simulation.current_pwm = PWM100_MAX_VALUE;

    s_hal_functions->set_pwm_state(true);
    s_hal_functions->set_pwm_duty(j1772_simulation.current_pwm / 10.0f);

}

j1772_status_t EVSE_J1772_DisablePower()
{
	//s_u32OpenRelayTimeout = max_timeout;
#if (ENABLE_OCPP == 1)
    EVSE_OCPP_SetEvent(EVSE_CHARGING_STOPPED_EVENT);
#endif
	j1772_simulation.timers.ulRelayClosedTimer = EVSE_GetMsSinceBoot();
    s_hal_functions->set_contactor_state(true);
    return J1772_Succes;
}

j1772_status_t EVSE_J1772_EnablePower(void)
{
#if (ENABLE_OCPP == 1)
    EVSE_OCPP_SetEvent(EVSE_CHARGING_STARTED_EVENT);
#endif
    s_hal_functions->set_contactor_state(false);
    return J1772_Succes;
}

j1772_status_t EVSE_J1772_GetCPValue(uint32_t *cp_value)
{
    if (cp_value != NULL)
    {
        *cp_value = s_hal_functions->read_pilot_voltage();
    }

    return J1772_Succes;
}

void EVSE_J1772_Loop(bool *stopCharging)
{
    EVSE_J1772_Loop_EVSE();
    if (stopCharging != NULL)
    {
        *stopCharging = false;
    }
}

/* Private function implementations */
static ControlPilotState_t EVSE_J1772_GetState(void)
{
    return s_hal_functions->read_control_pilot();
}

static j1772_status_t EVSE_J1772_SetCPPwmFromState(j1772_state_t j1772_state, uint16_t amps)
{
    j1772_status_t status = J1772_Succes;
    float pwm = 0;

    switch (j1772_state)
    {
        case STATE_A:
        case STATE_B:
        case STATE_C:
        case STATE_D:
            pwm = 100;
            break;
        case STATE_A2:
        case STATE_B2:
        case STATE_C2:
        case STATE_D2:
            status = EVSE_J1772_GetDutyCycleFromAmps(amps, &pwm);
            break;
        case STATE_E:
            pwm = 0;
            break;
        default:
            status = J1772_Error;
            break;
    }

    if (status == J1772_Succes)
    {
        status = EVSE_J1772_SetCPPWM(pwm);
    }

    return status;
}

static void EVSE_J1772_ResetVehicleData(void)
{
    j1772_simulation.evse_data.fMeasuredVoltage = 0;
    j1772_simulation.evse_data.fMeasuredCurrent = 0;
    j1772_simulation.evse_data.fMeasuredPower   = 0;
    j1772_simulation.evse_data.fMeterEnergyDelivered = 0;
	j1772_simulation.evse_data.uCarDetectTime   = 0;
    j1772_simulation.evse_data.fMeterReading    = 0;
    j1772_simulation.evse_data.uElapseTime      = 0;
    j1772_simulation.timers.ulLastCalcTime      = 0;
}

static void EVSE_J1772_SetState(j1772_state_t new_state)
{
    if (new_state == j1772_simulation.current_state)
    {
        return;
    }

    switch (new_state)
    {
        case STATE_A:
#if (ENABLE_OCPP == 1)
        	EVSE_OCPP_SetEvent(EV_DISCONNECTED_EVENT);
        	j1772_simulation.auth_needed = false;
#endif /* (ENABLE_OCPP == 1) */
        case STATE_A2:
            j1772PRINTF(("Car Disconnected"));
            EVSE_J1772_ResetVehicleData();
            EVSE_J1772_DisablePower();
            j1772_simulation.isCharging      = false;
            j1772_simulation.ready_to_charge = true;
            j1772_simulation.faults.over_current = false;

			EVSE_J1772_StopCharging(false);

            break;
            
        case STATE_B:
#if (ENABLE_OCPP == 1)
        	if ((j1772_simulation.current_state == STATE_A) || (j1772_simulation.current_state == STATE_A2))
        	{
        		EVSE_OCPP_SetEvent(EV_CONNECTED_EVENT);
                EVSE_OCPP_SetEvent(EV_AUTH_NEEDED_EVENT);
                j1772_simulation.auth_needed = true;
        	}
#endif /* ENABLE_OCPP */
        case STATE_B2:
            j1772_simulation.isCharging = false;
            if ((new_state == STATE_B2) &&
            		((j1772_simulation.current_state == STATE_C2) || (j1772_simulation.current_state == STATE_D2)))
            {
            	if (j1772_simulation.evse_data.fMeasuredCurrent < MIN_CURRENT_DRAW_BEFORE_DISCONNECT_SEQ7)
            	{
            		j1772PRINTF(("AC supply remains available, small current"));
            	}
            	else
            	{
            		EVSE_J1772_DisablePower();
            	}
            }
            else if (new_state == STATE_B)
            {
            	EVSE_J1772_DisablePower();
            }

			EVSE_J1772_StopCharging(false);

            break;
            
        case STATE_C:
        case STATE_D:
        	if (new_state == STATE_C)
        	{
        		EVSE_J1772_StopCharging(false);
        	}

        	if ((j1772_simulation.current_state != STATE_C) && (j1772_simulation.current_state != STATE_D))
        	{
                j1772_simulation.isCharging = false;
                EVSE_J1772_DisablePower();
        	}
            break;
            
        case STATE_C2:
        case STATE_D2:

    		if (s_hal_functions->ventilation_state == NULL)
    		{
				if (new_state == STATE_D2)
				{
					/* Don't allow charge */
					j1772PRINTF(("Ventilation to available, stop charging"));
					EVSE_J1772_StopCharging(true);
				}
        	}
    		else
    		{
				if (new_state == STATE_D2)
				{
					s_hal_functions->ventilation_state(true);
				}
				else
				{
					s_hal_functions->ventilation_state(false);
				}
    		}

        	if (j1772_simulation.ready_to_charge == true)
        	{
                EVSE_J1772_EnablePower();
                j1772_simulation.timers.ulLastCalcTime = 0;
                j1772_simulation.isCharging     = true;
        	}

            break;
        case STATE_E:
        case STATE_F:

			EVSE_J1772_DisablePower();
			j1772_simulation.isCharging = false;

        default:
            break;
    }

    j1772_simulation.current_state = new_state;
}

static void EVSE_J1772_Loop_EVSE()
{
	uint32_t u32CurrentTime = EVSE_GetMsSinceBoot();
    float new_usPWMDutyCyclePerMil = j1772_simulation.current_pwm;
    float new_usPwmDutyCycle        = 0;
    j1772_simulation.cp_state       = EVSE_J1772_GetState();

    if ((u32CurrentTime - j1772_simulation.timers.ulOverCurrentTime) > J1772_OVERCURRENT_PROTECTION_TIMEOUT_MS)
    {
    	j1772_simulation.faults.over_current = false;
    }

    switch (j1772_simulation.current_state)
    {
        case STATE_A:
        {
            if (j1772_simulation.cp_state == ControlPilot_StateB)
            {
                EVSE_J1772_SetState(STATE_B);
                new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                j1772PRINTF(("AC Basic Charging state transition from A1 to B1"));
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateA)
            {

            }
        }
        break;
        case STATE_A2:
        {
            if (j1772_simulation.cp_state == ControlPilot_StateA)
            {
                if (j1772_simulation.current_pwm < PWM100_MIN_VALUE)
                {
                    EVSE_J1772_SetState(STATE_A);
                    new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                    j1772PRINTF(("AC Basic Charging state transition from A2 to A1"));
                }
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateB)
            {
                EVSE_J1772_SetState(STATE_B2);
                j1772PRINTF(("AC Basic Charging state transition from A2 to B2"));
                /* Nothing to do */
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
        }
        break;
        case STATE_B:
        {
            if (j1772_simulation.cp_state == ControlPilot_StateA)
            {
                j1772PRINTF(("AC Basic Charging state transition from B1 to A1"));
                new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                EVSE_J1772_SetState(STATE_A);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateB)
            {
            	if(j1772_simulation.auth_needed)
            	{
            		j1772_simulation.ready_to_charge = false;
#if (ENABLE_OCPP == 1)
            		bool ocpp_auth_result = false;
    				if((EVSE_OCPP_GetAuthResponse(0, &ocpp_auth_result) == true) &&
    						(ocpp_auth_result == true))
    				{
    					j1772_simulation.ready_to_charge = true;
    					j1772_simulation.auth_needed = false;
    				}
#endif /* ENABLE_OCPP */
            	}

                if ((j1772_simulation.current_pwm == PWM100_MAX_VALUE) && (j1772_simulation.ready_to_charge == true))
                {
                    j1772PRINTF(("AC Basic Charging state transition from B1 to B2"));
                    EVSE_J1772_GetDutyCycleFromAmps(j1772_simulation.max_current, &new_usPwmDutyCycle);
                    new_usPWMDutyCyclePerMil = new_usPwmDutyCycle * 10;
                    EVSE_J1772_SetState(STATE_B2);
                }
                else if (j1772_simulation.current_pwm != PWM100_MAX_VALUE)
                {
                	new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                }
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
        }
        break;
        case STATE_B2:
        {
            if (j1772_simulation.cp_state == ControlPilot_StateA)
            {
                j1772PRINTF(("AC Basic Charging state transition from B2 to A2"));
                EVSE_J1772_SetState(STATE_A2);
            }
            else if ((j1772_simulation.cp_state == ControlPilot_StateB) && (j1772_simulation.ready_to_charge == false))
            {
                j1772PRINTF(("AC Basic Charging state transition from B2 to B1"));
                new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                EVSE_J1772_SetState(STATE_B);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateC)
            {
            	 j1772PRINTF(("AC Basic Charging state transition from B2 to C2"));
            	 EVSE_J1772_SetState(STATE_C2);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateD)
            {
                j1772PRINTF(("AC Basic Charging state transition from B2 to D2"));
                EVSE_J1772_SetState(STATE_D2);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
        }
        break;
        case STATE_C:
        {
            if ((j1772_simulation.cp_state == ControlPilot_StateD) && (j1772_simulation.current_state != STATE_D))
            {
            	j1772PRINTF(("AC Basic Charging transition C1 to D1"));
            	EVSE_J1772_SetState(STATE_D);
            }
            /* Intentional fall through */
        }
        case STATE_D:
        {
        	if (j1772_simulation.cp_state == ControlPilot_StateC)
        	{
        		EVSE_J1772_SetState(STATE_C);
        	}

            if (j1772_simulation.cp_state == ControlPilot_StateB)
            {
                j1772PRINTF(("AC Basic Charging state transition from C1/D1 to B1"));
                new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                EVSE_J1772_SetState(STATE_B);
            }
            else if ((j1772_simulation.cp_state == ControlPilot_StateC) || (j1772_simulation.cp_state == ControlPilot_StateD))
            {
                if ((j1772_simulation.cp_state == ControlPilot_StateC)
                		&& (j1772_simulation.last_cp_state == ControlPilot_StateD))
                {
                	j1772PRINTF(("AC Basic Charging transition D1 to C1"));
                	EVSE_J1772_SetState(STATE_C);
                }

            	if (j1772_simulation.current_pwm == PWM100_MAX_VALUE)
            	{
                    if ((j1772_simulation.ready_to_charge == true) && (j1772_simulation.faults.over_current == false))
                    {
                        j1772PRINTF(("AC Basic Charging state transition from C1/D1 to C2/D2"));
                        EVSE_J1772_GetDutyCycleFromAmps(j1772_simulation.max_current, &new_usPwmDutyCycle);
                        new_usPWMDutyCyclePerMil = new_usPwmDutyCycle * 10;
                        EVSE_J1772_SetState(j1772_simulation.current_state + 1);
                    }
            	}
                else if (j1772_simulation.current_pwm != PWM100_MAX_VALUE)
                {
                	new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                }
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateA)
            {
                j1772PRINTF(("AC Basic Charging state transition from C1/D1 to A1"));
                EVSE_J1772_SetState(STATE_A);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
        }
        break;
        case STATE_C2:
        {
            if (j1772_simulation.cp_state == ControlPilot_StateD)
            {
            	j1772PRINTF(("AC Basic Charging transition C2 to D2"));
            	EVSE_J1772_SetState(STATE_D2);
            }
        }
        // Intentional fall thrue
        case STATE_D2:
        {
            if (j1772_simulation.timers.ulLastCalcTime == 0)
            {
                j1772_simulation.timers.ulLastCalcTime = u32CurrentTime;
                j1772_simulation.timers.ulLastDisplayTime = u32CurrentTime;
            }
            EVSE_J1772_GetDutyCycleFromAmps(j1772_simulation.max_current, &new_usPwmDutyCycle);
            new_usPWMDutyCyclePerMil = new_usPwmDutyCycle * 10;
            uint32_t time_lapse      = u32CurrentTime - j1772_simulation.timers.ulLastCalcTime;
            uint32_t time_lapse_display = u32CurrentTime - j1772_simulation.timers.ulLastDisplayTime;
            if ((time_lapse >= BC_MEASURE_INTERVAL_MS) || (j1772_simulation.cp_state == ControlPilot_StateB)
            		|| (j1772_simulation.cp_state == ControlPilot_StateA))
            {
                j1772_simulation.evse_data.uElapseTime += time_lapse;
                s_hal_functions->read_voltage(&j1772_simulation.evse_data.fMeasuredVoltage, NULL, NULL);
                s_hal_functions->read_current(&j1772_simulation.evse_data.fMeasuredCurrent, NULL, NULL);
                s_hal_functions->read_power(&j1772_simulation.evse_data.fMeasuredPower, NULL, NULL);
#if ENABLE_OVERCURRENT_TEST
                if (j1772_simulation.evse_data.fMeasuredCurrent > j1772_simulation.max_current)
                {
                	j1772_simulation.faults.over_current = true;
                	j1772_simulation.timers.ulOverCurrentTime = u32CurrentTime;
                }
#endif /* ENABLE_OVERCURRENT_TEST */
                j1772_simulation.evse_data.fMeasuredVoltage =
                    MIN(j1772_simulation.evse_data.fMeasuredVoltage, j1772_simulation.max_voltage);
                double energy = prvCalculateEnergy(j1772_simulation.evse_data.fMeasuredCurrent, j1772_simulation.evse_data.fMeasuredVoltage, time_lapse);
                j1772_simulation.evse_data.fMeterReading += energy;
                j1772_simulation.evse_data.fMeterEnergyDelivered += energy;
                j1772_simulation.timers.ulLastCalcTime = u32CurrentTime;
            }

            if (time_lapse_display >= BC_DISPLAY_INTERVAL_MS)
            {
            	j1772PRINTF(("AC Basic Charging state:%.2fV, %.2fA, Index:%.2f ",
            			j1772_simulation.evse_data.fMeasuredVoltage,
            			j1772_simulation.evse_data.fMeasuredCurrent,
						j1772_simulation.evse_data.fMeterReading));
            	j1772_simulation.timers.ulLastDisplayTime = u32CurrentTime;
            }

            if (j1772_simulation.cp_state == ControlPilot_StateA)
            {
                j1772PRINTF(("AC Basic Charging state transition from C2/D2 to A2"));
                EVSE_J1772_SetState(STATE_A2);
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateB)
            {
                j1772PRINTF(("AC Basic Charging state transition from C2/D2 to B2"));
                EVSE_J1772_SetState(STATE_B2);
            }
            else if ((j1772_simulation.cp_state == ControlPilot_StateC) || (j1772_simulation.cp_state == ControlPilot_StateD))
            {
                if ((j1772_simulation.cp_state == ControlPilot_StateC)
                		&& (j1772_simulation.last_cp_state == ControlPilot_StateD))
                {
                	j1772PRINTF(("AC Basic Charging transition D2 to C2"));
                	EVSE_J1772_SetState(STATE_C2);
                }

                if ((j1772_simulation.ready_to_charge == false) || (j1772_simulation.faults.over_current == true))
                {
                    j1772PRINTF(("AC Basic Charging state transition from C2/D2 to C1/D1"));
                    new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
                    EVSE_J1772_SetState(j1772_simulation.current_state - 1);
                }
            }
            else if (j1772_simulation.cp_state == ControlPilot_StateF)
            {
            	j1772PRINTF(("AC Basic Charging state transition to fault"));
            	EVSE_J1772_SetState(STATE_F);
            }
        }
        break;
        case STATE_E:
        case STATE_F:
        {

        	if (j1772_simulation.cp_state == ControlPilot_StateA)
			{
        		j1772PRINTF(("AC Basic Charging state transition from E/F to A1"));
        		EVSE_J1772_SetState(STATE_A);
			}
        	else if (j1772_simulation.cp_state == ControlPilot_StateF)
			{
        		EVSE_J1772_SetState(STATE_F);
			}

        	new_usPWMDutyCyclePerMil = PWM100_MAX_VALUE;
        }
        break;
        case J1772_LAST_STATE:
        {
        	if (j1772_simulation.cp_state != ControlPilot_StateUnknown)
        	{
        		EVSE_J1772_SetState(STATE_A);
        	}
        }
        break;
    }

    j1772_simulation.last_cp_state = j1772_simulation.cp_state;
    if (new_usPWMDutyCyclePerMil != j1772_simulation.current_pwm)
    {
    	s_hal_functions->set_pwm_duty(new_usPWMDutyCyclePerMil / 10);
        j1772_simulation.current_pwm = new_usPWMDutyCyclePerMil;
    }
}

#endif /* ENABLE_J1772 */
