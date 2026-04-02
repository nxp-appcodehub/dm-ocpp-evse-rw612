/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/


#include "app.h"
#include "ControlPilot_Process.h"

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"

#include "fsl_os_abstraction.h"

#include "fsl_debug_console.h"
#include "fsl_ctimer.h"
#include "fsl_adc.h"
#include "fsl_power.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define CTIMER          CTIMER3         /* Timer 0 */
#define CTIMER_MAT_OUT  kCTIMER_Match_0 /* Match output 0 */
#define CTIMER_CLK_FREQ CLOCK_GetCTimerClkFreq(3)
#ifndef CTIMER_MAT_s_u32PWMPeriod_CHANNEL
#define CTIMER_MAT_s_u32PWMPeriod_CHANNEL kCTIMER_Match_3
#endif

#define ADC_MATCH_TRIGGER  kCTIMER_Match_1
#define ADC_IRQHANDLER     GAU_GPADC0_INT_FUNC11_IRQHandler
#define ADC_BASE           GAU_GPADC0
#define ADC_CHANNEL_SOURCE kADC_CH0
#define ADC_IRQn           GAU_GPADC0_INT_FUNC11_IRQn
#define ADC_TRIGGER_SOURCE kADC_TriggerSourceSoftware

#if ENABLE_LOGGING_CP
#define CP_PRINTF(x) configPRINTF (x)
#else
#define CP_PRINTF(x)
#endif /* ENABLE_LOGGING_CP */
#if ENABLE_LOGGING_ADC
#define ADC_PRINTF(x) configPRINTF (x)
#else
#define ADC_PRINTF(x)
#endif /* ENABLE_LOGGING_ADC */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void ADC_Manager_SetPWM_1kHZ(uint32_t pwmFreqHz, uint8_t dutyCyclePercent);
uint32_t ADC_Manager_ReadCPRawValue(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

static uint8_t s_u8CtimerInit = 0;
static uint32_t s_u32CtimerPrescale = 0;
static uint32_t s_u32PWMPeriod = 0;
static uint32_t s_u32PulseWidth = 0;
static volatile bool s_bConversionDataReady = false;
static volatile uint16_t s_u16AdcCPRawValue = 0;
static float s_fCPDutyCycle = 0.0f;
/*******************************************************************************
 * Code
 ******************************************************************************/
void CTIMER3_IRQHandler_Match1CB(uint32_t flags)
{
	CTIMER->EMR |= CTIMER_EMR_EM1_MASK;
	ADC_DoSoftwareTrigger(ADC_BASE);
}

void ADC_IRQHANDLER(void)
{
    if ((ADC_GetStatusFlags(ADC_BASE) & kADC_DataReadyInterruptFlag) != 0UL)
    {
    	CTIMER->EMR &= ~CTIMER_EMR_EM1_MASK;
    	uint16_t tmp_value = ADC_GetConversionResult(ADC_BASE);
        s_u16AdcCPRawValue = ((tmp_value & 0x7FFF) << 1) + 1200;
        ADC_ClearStatusFlags(ADC_BASE, kADC_DataReadyInterruptFlag);
        ADC_StopConversion(ADC_BASE);
        s_bConversionDataReady = true;
    }
}

static uint32_t CTIMER_CalculateADCTriggerPoint(uint32_t pwm_period, uint32_t pulse_width)
{
    if ((pulse_width == 0) || (pulse_width == pwm_period + 1))
    {
    	/* We can sample anywhere */
    	pulse_width = pwm_period / 2;
    }

    return pulse_width + ((pwm_period - pulse_width)/2);
}

static uint32_t CTIMER_CalculatePWMPeriod(uint32_t pwmFreqHz, uint32_t timerClock_Hz)
{
	return (timerClock_Hz / pwmFreqHz) - 1U;
}

static uint32_t CTIMER_CalculatePWMPulseWidth(float dutyCyclePercent, uint32_t pwm_period)
{
    if (dutyCyclePercent < 0.0f)
    {
        dutyCyclePercent = 0.0f;
    }
    else if (dutyCyclePercent > 100.0f)
    {
        dutyCyclePercent = 100.0f;
    }
    
	return (pwm_period + 1U) * (100.0f - dutyCyclePercent) / 100.0f;
}


static status_t ControlPilot_SetPWM_1kHZ()
{
    uint32_t timerClock_Hz = CTIMER_CLK_FREQ / s_u32CtimerPrescale;
    s_u32PWMPeriod = CTIMER_CalculatePWMPeriod(CP_PWM_FREQ, timerClock_Hz);
    s_u32PulseWidth = CTIMER_CalculatePWMPulseWidth(CP_PWM_DEFAULT_DUTYCYCLE, s_u32PWMPeriod);
    return CTIMER_SetupPwmPeriod(CTIMER, CTIMER_MAT_s_u32PWMPeriod_CHANNEL, CTIMER_MAT_OUT, s_u32PWMPeriod, s_u32PulseWidth, false);
}

void ControlPilot_PWM_Init(void)
{
    ctimer_config_t config;

    CTIMER_GetDefaultConfig(&config);

    /* Use 16 MHz clock for the Ctimer0 */
    CLOCK_EnableClock(kCLOCK_InputMux);
    CLOCK_AttachClk(kSFRO_to_CTIMER3);

    s_u32CtimerPrescale = config.prescale + 1;

    CTIMER_Init(CTIMER, &config);
    ControlPilot_SetPWM_1kHZ();
    CTIMER_StartTimer(CTIMER);
    s_u8CtimerInit = 1;
}

static void ADC_Manager_UpdateADCTriggerPeriod(uint32_t pulse_width, uint32_t pwm_period)
{
	uint32_t adc_triger_width = CTIMER_CalculateADCTriggerPoint(pwm_period, pulse_width);
	CTIMER_UpdatePwmPulsePeriod(CTIMER, ADC_MATCH_TRIGGER, adc_triger_width);
}

int ADC_Manager_ConfigureADCTrigger()
{
	if (s_u8CtimerInit == 0)
	{
		ADC_PRINTF(("Ctimer not Init. Can't configure ADC Trigger"));
		return -1;
	}

    static ctimer_callback_t myCallbackPtr[] = {NULL, CTIMER3_IRQHandler_Match1CB, NULL, NULL, NULL, NULL, NULL, NULL};
    ctimer_match_config_t config_match;
    
    config_match.enableCounterReset  = false;
    config_match.enableCounterStop = false;
    config_match.enableInterrupt = true;
    config_match.matchValue = CTIMER_CalculateADCTriggerPoint(s_u32PWMPeriod, s_u32PulseWidth);

    CTIMER_SetupMatch(CTIMER, ADC_MATCH_TRIGGER, &config_match);
    CTIMER_RegisterCallBack(CTIMER, myCallbackPtr, kCTIMER_MultipleCallback);
    
    return 0;
}

status_t ADC_Manager_Init(void)
{
    status_t status_adc_init;
    adc_config_t adcConfig;
    
    CLOCK_AttachClk(kMAIN_CLK_to_GAU_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivGauClk, 1U);

    RESET_PeripheralReset(kGAU_RST_SHIFT_RSTn);

    CLOCK_EnableClock(kCLOCK_Gau);
    POWER_PowerOnGau();

    /*
     *  config->clockDivider = kADC_ClockDivider1;
     *  config->powerMode = kADC_PowerModeFullBiasingCurrent;
     *  config->resolution = kADC_Resolution12Bit;
     *  config->warmupTime = kADC_WarmUpTime16us;
     *  config->vrefSource = kADC_Vref1P2V;
     *  config->inputMode = kADC_InputSingleEnded;
     *  config->conversionMode = kADC_ConversionContinuous;
     *  config->scanLength = kADC_ScanLength_1;
     *  config->averageLength = kADC_AverageNone;
     *  config->triggerSource = kADC_TriggerSourceSoftware;
     *  config->inputGain = kADC_InputGain1;
     *  config->enableInputGainBuffer = false;
     *  config->resultWidth = kADC_ResultWidth16;
     *  config->fifoThreshold = kADC_FifoThresholdData1;
     *  config->enableInputBufferChop = true;
     *  config->enableDMA = false;
     *  config->enableADC = false;
     */
    ADC_GetDefaultConfig(&adcConfig);
    adcConfig.vrefSource            = kADC_Vref1P8V;
    adcConfig.inputMode             = kADC_InputSingleEnded;
    adcConfig.conversionMode        = kADC_ConversionOneShot;
    adcConfig.inputGain             = kADC_InputGain1;
    adcConfig.resolution            = kADC_Resolution16Bit;
    adcConfig.fifoThreshold         = kADC_FifoThresholdData1;
    adcConfig.averageLength         = kADC_AverageNone;
    adcConfig.enableInputGainBuffer = true;
    adcConfig.enableADC             = true;
    adcConfig.enableInputBufferChop = false;
    adcConfig.triggerSource         = ADC_TRIGGER_SOURCE;
    
    ADC_Init(ADC_BASE, &adcConfig);

    status_adc_init = ADC_DoAutoCalibration(ADC_BASE, kADC_CalibrationVrefInternal);
    if (status_adc_init != kStatus_Success)
    {
        ADC_PRINTF(("ADC calibration failed with status: %d", status_adc_init));
        return status_adc_init;
    }
    uint16_t offsetCal, gainCal;
    ADC_GetAutoCalibrationData(ADC_BASE, &offsetCal, &gainCal);
    ADC_PRINTF(("ADC calibration successful"));
    
    ADC_ClearStatusFlags(ADC_BASE, kADC_DataReadyInterruptFlag);
    ADC_SetScanChannel(ADC_BASE, kADC_ScanChannel0, ADC_CHANNEL_SOURCE);
    ADC_EnableInterrupts(ADC_BASE, kADC_DataReadyInterruptEnable);
    EnableIRQ(ADC_IRQn);
    return status_adc_init;
}

status_t ControlPilot_UpdatePWM(float dutyCyclePercent)
{
    if (dutyCyclePercent < 0.0f || dutyCyclePercent > 100.0f)
    {
        CP_PRINTF(("Invalid duty cycle: %.1f. Must be between 0.0 and 100.0", dutyCyclePercent));
        return kStatus_InvalidArgument;
    }
    
    if (s_fCPDutyCycle == dutyCyclePercent)
    {
        return kStatus_Success;
    }
    s_fCPDutyCycle = dutyCyclePercent;
    s_u32PulseWidth = CTIMER_CalculatePWMPulseWidth(dutyCyclePercent, s_u32PWMPeriod);
    
	CP_PRINTF(("DutyCycle changed to %.1f", dutyCyclePercent));

	CTIMER_UpdatePwmPulsePeriod(CTIMER, CTIMER_MAT_OUT, s_u32PulseWidth);
	/* We need to adjust the  ADC Trigger   */
	ADC_Manager_UpdateADCTriggerPeriod(s_u32PulseWidth, s_u32PWMPeriod);

    return kStatus_Success;
}

void ControlPilot_GetDutyCycle(float *dutyCyclePercent)
{
    if (dutyCyclePercent == NULL)
    {
        return;
    }
    
    *dutyCyclePercent = s_fCPDutyCycle;
}


status_t ControlPilot_PWM_SetState(bool enable)
{
    if (enable)
    {
        CTIMER_StartTimer(CTIMER);
    }
    else
    {
        CTIMER_StopTimer(CTIMER);
    }

    return kStatus_Success;
}

uint16_t ADC_Manager_ReadControlPilotVoltage(void)
{
    return s_u16AdcCPRawValue;
}
