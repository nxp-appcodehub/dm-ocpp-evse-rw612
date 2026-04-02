/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */
#ifndef EVSE_METROLOGY_H
#define EVSE_METROLOGY_H

#include "types.h"
#include "meterlibfft.h"

#define DEMO_DISTRIBUTION_FREQUENCY         50U    /* The frequency is 50Hz */

/*Metering*/
#define FFT_SAMPLES  SAMPLES64    /* number of required FFT samples (power-of-two) */
#define AUX_SAMPLES  (FFT_SAMPLES / 2)       /*!< Number of samples for one FFT calculation */
/* AFE_MODE=1 - asynchronous mode, sampling isn't synchronized with the mains, AFE is running in
                the S/W-triggered continuous conversion mode
   AFE_MODE=0 - synchronous mode, sampling is synchronized with the mains, AFE is running in the
                H/W-triggered single conversion mode */
#define AFE_MODE     1

#define TMRCLK 2343750 /* timer clock base (MHz) */
/* Computes timer compare value for energy LED driving:
ledclk - active/reactive energy LED flashing frequency in the current line period [mHz]
tmrclk - timer clock base [Hz], should be < 4294967 Hz */
#define TMRCMPVAL(ledclk,tmrclk) (((unsigned long)(tmrclk)*1000)/(unsigned long)(ledclk))

#define NUM_MET_BUFFERS 2

#define DEMO_PHASE							3U

#define U_MAX         390.000          /*!< Maximal voltage U-peak                    */
#define I_MAX         172.000          /*!< Maximal current I-peak, 100A 2x1r5,AR     */

typedef struct phase_data {
	double ph1;
	double ph2;
	double ph3;
}phase_data_t;

typedef struct metrology_data{
	/* Instant power metering values */
	phase_data_t urms;
	phase_data_t irms;
	phase_data_t p;
	phase_data_t q;
	phase_data_t s;
	phase_data_t pf;
	phase_data_t thdu;
	phase_data_t thdi;
	/* billing (accumulative) metering values */
	unsigned long frequency;       /* line frequency [mHz] */
	tENERGY_REG energy_cnt;
} metrology_data_t;

void Metering_SampleReset(void);
uint32_t Metering_SampleAdd(frac24 u1, frac24 u2, frac24 u3, frac24 i1, frac24 i2, frac24 i3);
void Metering_StartProcessing(void);
void Metering_SampleFinish(void);

const phase_data_t Metrology_ReadCurrent();

const phase_data_t Metrology_ReadVoltage();
const phase_data_t Metrology_ReadActivePower();
const tENERGY_REG Metrology_ReadEnergy();

bool EVSE_MetrologyInit();
void EVSE_MetrologyDeinit();

#endif /* EVSE_METROLOGY_H */
