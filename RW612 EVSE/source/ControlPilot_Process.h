/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

/**
 * @file ControlPilot_Process.h
 * @brief Control Pilot PWM and ADC management interface for EV charging applications
 * 
 * This header file provides the API for Control Pilot signal processing including
 * PWM generation at 1kHz frequency and ADC measurements for voltage monitoring.
 * The Control Pilot is used in electric vehicle charging systems to communicate
 * charging parameters and monitor connection status.
 * 
 * Key features:
 * - PWM signal generation with configurable duty cycle (0-100%)
 * - ADC-based voltage measurement for Control Pilot monitoring
 * - Timer-based PWM period calculation utilities
 * - Interrupt-driven ADC processing
 * 
 * @note Default PWM frequency is set to 1kHz as per EV charging standards
 */
#ifndef _CONTROLPILOT_PROCESS_H_
#define _CONTROLPILOT_PROCESS_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "fsl_common.h"


#define CP_PWM_FREQ 1000U  // Default Control Pilot PWM frequency of 1 kHz
#define CP_PWM_DEFAULT_DUTYCYCLE 100U
/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
/*!
 * @brief Initialize Control Pilot PWM functionality
 */
void ControlPilot_PWM_Init(void);

/*!
 * @brief Set Control Pilot PWM duty cycle
 * @param dutyCyclePercent Duty cycle percentage as float (0.0-100.0)
 */
status_t ControlPilot_UpdatePWM(float dutyCyclePercent);


/*!
 * @brief Get the current Control Pilot PWM duty cycle
 * @param dutyCyclePercent Pointer to store the current duty cycle percentage (0.0-100.0)
 */
void ControlPilot_GetDutyCycle(float *dutyCyclePercent);

status_t ControlPilot_PWM_SetState(bool enable);

/*!
 * @brief Calculate PWM period values based on frequency and duty cycle
 * @param pwmFreqHz PWM frequency in Hz
 * @param dutyCyclePercent Duty cycle percentage (0-100)
 * @param timerClock_Hz Timer clock frequency in Hz
 * @return Status of the operation
 */
status_t CTIMER_GetPwmPeriodValue(uint32_t pwmFreqHz, uint8_t dutyCyclePercent, uint32_t timerClock_Hz);



/*!
 * @brief Initialize the ADC manager for Control Pilot processing
 */
status_t ADC_Manager_Init(void);

/*!
 * @brief Set PWM frequency to 1kHz with specified duty cycle
 * @param pwmFreqHz PWM frequency in Hz
 * @param dutyCyclePercent Duty cycle percentage (0-100)
 */
void ADC_Manager_SetPWM_1kHZ(uint32_t pwmFreqHz, uint8_t dutyCyclePercent);


/*!
 * @brief Configure ADC trigger for Control Pilot measurements
 */
int ADC_Manager_ConfigureADCTrigger(void);


/*!
 * @brief Read Control Pilot voltage from ADC
 * @return Control Pilot voltage value as 16-bit unsigned integer
 */
uint16_t ADC_Manager_ReadControlPilotVoltage(void);

/*!
 * @brief ADC interrupt handler for Control Pilot measurements
 */
void DEMO_ADC_IRQHANDLER(void);

#endif /* _CONTROLPILOT_PROCESS_H_ */
