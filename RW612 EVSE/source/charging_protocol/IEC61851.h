/*
 * Copyright 2024-2025 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#ifndef J1772_IEC61851_H_
#define J1772_IEC61851_H_

#include "stdint.h"
#include <stdbool.h>
#include "app.h"
#include "EVSE_ChargingProtocol.h"

/* The ADC measures from 0-3.3V 16bits */
/*
    12V => 3.1V => 61563
    9V => 2.794V => 55486
    6V => 2.488V => 49409
    3V => 2.181V => 43312
    0V => 1.874V => 32215
    -12V => 0.65V => 12908
*/
#define BATTERY_CAPACITY       1000
#define BATTERY_STARTING_LEVEL 20
#define BATTERY_DESIRED_LEVEL  100

#define BC_DISPLAY_INTERVAL_MS 2000 /* MS */
#define BC_MEASURE_INTERVAL_MS 100 /* MS */
#define PWM100_MIN_VALUE       980
#define PWM100_MAX_VALUE       1000

#define PWM5_MIN_VALUE 45
#define PWM5_MAX_VALUE 55
#define PWM5_VALUE     50

#define STATEB2_TRANSITION_B1_MIN_TIME 3000

#define MIN_CURRENT (6u)
#define MAX_CURRENT (81u)

#define MIN_CURRENT_DRAW_BEFORE_DISCONNECT_SEQ7 1.0f

#define J1772_OVERCURRENT_PROTECTION_TIMEOUT_MS 1000

/* if error state either stay in state F or try to see if an EV is connected */
#define ERROR_STATE_DELAY (300u)

/* Fastest response time is 100 ms. Set the delay to 10u */
#define J1772_TICK_DELAY (10u)

/* The allowed adjustment time before changing the amps again */
#define T10T9DELAY (5000u)


typedef enum _j1772_status
{
    J1772_Succes,
    J1772_Error,
    J1772_CP_Req_Digital_mode,
} j1772_status_t;

/**
 * @brief Get the Amps from a known duty cycle
 *
 * @param dutyCycle value of the duty cycle
 * @param amps pointer where to store the amps
 * @return j1772_status_t J1772_Error if wrong duty cycle provided
 */
j1772_status_t EVSE_J1772_GetAmpsFromDutyCycle(float dutyCycle, float *amps);

/**
 * @brief Get the duty cycle from a known charge rate
 *
 * @param amps the wanted amps
 * @param dutyCycle the duty cycle that matches that provided amps
 * @return j1772_status_t
 */
j1772_status_t EVSE_J1772_GetDutyCycleFromAmps(float amps, float *dutyCycle);

/**
 * @brief Get a printable version of the current J1772 state
 *
 * @param state
 * @return const char*
 */
const char *EVSE_J1772_GetCurrentStateString();

/**
 * @brief
 *
 * @param pwm
 * @return j1772_status_t
 */
j1772_status_t EVSE_J1772_SetCPPWM(float pwm);

/**
 * @brief Get the CP State in a Char Version
 *
 * @return char 'A','B','C','D','E','F'
 */
char EVSE_J1772_GetCpStateString();

ControlPilotState_t EVSE_J1772_GetCpState();
/**
 * @brief Get the CP value.
 *
 * @param cp_value
 * @return j1772_status_t
 */
j1772_status_t EVSE_J1772_GetCPValue(uint32_t *cp_value);

/**
 * @brief Disable power delivery
 *
 * @return j1772_status_t
 */
j1772_status_t EVSE_J1772_DisablePower();

/**
 * @brief Enable power delivery
 *
 * @return j1772_status_t
 */
j1772_status_t EVSE_J1772_EnablePower();

/**
 * @brief Set the max current of the EVSE or EV
 *
 * @param max_current
 */
void EVSE_J1772_SetMaxCurrent(uint32_t max_current);

/**
 * @brief Get current charging state of the system
 *
 * @param bCharging
 * @return true if the car is charging
 * @return false if the car is not charging
 */
void EVSE_J1772_isCharging(bool *bCharging);

/**
 * @brief Stop the charging session from outside the stack
 *
 * @param bCharging true if the charge should be stopped
 */
void EVSE_J1772_StopCharging(bool bCharging);

/**
 * @brief Get vehicle data as described in the charging protocol header.
 * if the values are not valid for J1772 they will be left untouched
 *
 * @param vehicle_data
 */
void EVSE_J1772_GetVehicleData(vehicle_data_t *vehicle_data);

/**
 * @brief Init the J1772 stack
 *
 */
void EVSE_J1772_Init(const charging_hal_functions_t *hal_functions);

/**
 * @brief
 *
 * @param stopCharging
 */
void EVSE_J1772_Loop(bool *stopCharging);

#if EASYEVSE_EV
/**
 * @brief Get vehicle data as described in the charging protocol header.
 * if the values are not valid for J1772 they will be left untouched
 *
 * @param vehicle_data
 */
void EV_J1772_GetVehicleData(vehicle_data_t *vehicle_data);

/**
 * @brief Get current charging state of the system
 *
 * @param bCharging
 * @return true if the car is charging
 * @return false if the car is not charging
 */
void EV_J1772_isCharging(bool *bCharging);

/**
 * @brief Resets EV battery level to full or empty
 */
void EV_J1772_ResetBatteryLevel(battery_levels_t battery_level);

/**
 * @brief Stops charging from EV side
 */
void EV_J1772_StopCharging(void);

/**
 * @brief Sets protocol on EV side
 */
void EV_J1772_SetProtocol(evse_charging_protocol_t protocol);

/**
 * @brief Starts charging from EV side
 */
void EV_J1772_StartCharging();

#endif /* EASYEVSE_EV */
#endif /* J1772_IEC61851_H_ */
