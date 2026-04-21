/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#include "fsl_common.h"
#include "fsl_os_abstraction.h"

#include "app.h"
#include "EVSE_Metrology.h"
#include "meterconfig.h"
#include "task_config.h"
#include "ext_afe_mkm.h"

#if ENABLE_OCPP
#include "EVSE_Ocpp.h"
#endif

static void metrology_task(osa_task_param_t arg);
static void metrology_post_process(void);
static void metrology_init(void);
static void metrology_process(uint8_t read_index);

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#if 1
#define METROLOGY_PRINTF(x) configPRINTF (x)
#else
#define METROLOGY_PRINTF(x)
#endif /* ENABLE_LOGGING_METROLOGY */



typedef enum {
	Metrology_DataRCV = 0x1,
	Metrology_TaskDelete = 0x2,
	ALL_EVENTS = 0xFFFF
}metrology_events_t;

typedef struct {
	metrology_events_t event;
	uint32_t data;
} metrology_msg_t;

#define MET_POST_PROCESS_BASE_TASK_FREQ		52U
#define MET_POST_PROCESS_NEW_TASK_FREQ		4U
#define MET_SAVE_ENERGY_BASE_TASK_FREQ		(4*5*60)
#define MET_SAVE_ENERGY_NEW_TASK_FREQ		1U

#define ADC_INP_SAMPLES AFE_INP_SAMPLES /* accurate number of input samples */
#define INP_SAMPLES MAX(FFT_SAMPLES, ADC_INP_SAMPLES)
/* Frequency conversion macros                                                */
#define TMRPRCLK        (double)(150.0e6/64.0)
#define TMR1FREQ(x)     (double)(TMRPRCLK/(double)x)

/* Metrology emulation constants */
#define WHI_EMU_DELTA 					156.15 		/* 3*(230V * 16 A / 50 Hz)/1.414 */
#define WHI_EMU_PULSE_VAL 				1800
#define VARHI_EMU_DELTA 				156.15 		/* 3*(230V * 16 A / 50 Hz)/1.414 */
#define VARHI_EMU_PULSE_VAL 			1800
#define KWH_KVARH_LED_TIMER_EMU_VAL 	23666U
#define URMS_EMU_VAL					230.0
#define IRMS_EMU_VAL					16.0
#define FREQ_EMU_VAL					500.0
#define S_EMU_VAL						3680.0		/* URMS_EMU_VAL * IRMS_EMU_VAL */
#define P_EMU_VAL						2602.546	/* S_EMU_VAL / 1.414 */
#define Q_EMU_VAL						2602.546	/* S_EMU_VAL / 1.414 */
#define Wh_EMU_VAL						6			/* 0.6 Wh with 0.1 resolution */
#define VARh_EMU_VAL					6			/* 0.6 VARh with 0.1 resolution */

#define APP_METER_PRIORITY_OSA PRIORITY_RTOS_TO_OSA(APP_METER_PRIORITY)

/*******************************************************************************
 * Variables
 ******************************************************************************/
static uint32_t buffers_not_process_in_time = 0;

static uint8_t buffer_inUse[NUM_MET_BUFFERS];
static unsigned long adc_samples[NUM_MET_BUFFERS];             /* number of true ADC samples (it can be slightly changed
                                          due to mains frequency varying in real application) */

static unsigned long current_sample;
static uint8_t wr_buffer = 0;
static uint8_t rd_buffer = 0;

/* multiplexed mandatory buffers (time domain / frequency domain in the Cartesian form) */
/* U-ADC output buffer/FFT real part output buffer (Ph1) */
frac24 u1_re[NUM_MET_BUFFERS][INP_SAMPLES];
/* I-ADC output buffer/FFT real part output buffer (Ph1) */
frac24 i1_re[NUM_MET_BUFFERS][INP_SAMPLES];
/* U-ADC output buffer/FFT real part output buffer (Ph2) */
frac24 u2_re[NUM_MET_BUFFERS][INP_SAMPLES];
/* I-ADC output buffer/FFT real part output buffer (Ph2) */
frac24 i2_re[NUM_MET_BUFFERS][INP_SAMPLES];
/* U-ADC output buffer/FFT real part output buffer (Ph3) */
frac24 u3_re[NUM_MET_BUFFERS][INP_SAMPLES];
/* I-ADC output buffer/FFT real part output buffer (Ph3) */
frac24 i3_re[NUM_MET_BUFFERS][INP_SAMPLES];

/* dedicated mandatory buffers (frequency domain in the Cartesian form) */
frac24 u1_im[FFT_SAMPLES];     /* U-FFT imaginary part output buffer (Ph1) */
frac24 i1_im[FFT_SAMPLES];     /* I-FFT imaginary part output buffer (Ph1) */
frac24 u2_im[FFT_SAMPLES];     /* U-FFT imaginary part output buffer (Ph2) */
frac24 i2_im[FFT_SAMPLES];     /* I-FFT imaginary part output buffer (Ph2) */
frac24 u3_im[FFT_SAMPLES];     /* U-FFT imaginary part output buffer (Ph3) */
frac24 i3_im[FFT_SAMPLES];     /* I-FFT imaginary part output buffer (Ph3) */

/* auxiliary buffers (frequency domain in the Polar form) */
frac24 i1_mag[AUX_SAMPLES];    /* U-magnitudes output buffer (Ph1) */
frac24 u1_mag[AUX_SAMPLES];    /* I-magnitudes output buffer (Ph1) */
frac24 i2_mag[AUX_SAMPLES];    /* U-magnitudes output buffer (Ph2) */
frac24 u2_mag[AUX_SAMPLES];    /* I-magnitudes output buffer (Ph2) */
frac24 i3_mag[AUX_SAMPLES];    /* U-magnitudes output buffer (Ph3) */
frac24 u3_mag[AUX_SAMPLES];    /* I-magnitudes output buffer (Ph3) */
long i1_ph[AUX_SAMPLES];       /* U-phases output buffer (Ph1) */
long u1_ph[AUX_SAMPLES];       /* I-phases output buffer (Ph1) */
long i2_ph[AUX_SAMPLES];       /* U-phases output buffer (Ph2) */
long u2_ph[AUX_SAMPLES];       /* I-phases output buffer (Ph2) */
long i3_ph[AUX_SAMPLES];       /* U-phases output buffer (Ph3) */
long u3_ph[AUX_SAMPLES];       /* I-phases output buffer (Ph3) */

/* auxiliary U-I phase shift buffers [0.001°] - used for S/W phase shift correction (Ph1..Ph3) */
frac32 shift1[FFT_SAMPLES/2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
frac32 shift2[FFT_SAMPLES/2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
frac32 shift3[FFT_SAMPLES/2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static tMETERLIBFFT3PH_DATA mlib;               /* 3-PH main metering structure */
static metrology_data_t metrology_data;
double u12_ph, u13_ph, u23_ph;        /* phase shifts */

frac24 afe1_offs = 0;                 /* AFE1 I-offset correction value [bit] */
frac24 afe2_offs = 0;                 /* AFE2 I-offset correction value [bit] */
frac24 afe3_offs = 0;                 /* AFE3 I-offset correction value [bit] */

static OSA_TASK_DEFINE(metrology_task, APP_METER_PRIORITY_OSA, 1, APP_METER_STACK_SIZE, 0);
static OSA_MSGQ_HANDLE_DEFINE(metrology_queue_handle, 3, sizeof(metrology_msg_t));
static OSA_TASK_HANDLE_DEFINE(metrology_task_handle);

static bool s_task_deleted = false;
static bool is_init = false;

static bool check_init()
{
	return is_init;
}

static void print_metrology_data(void)
{
	METROLOGY_PRINTF((
	    "Metering Data AVG: "
	    "URMS[%.3f, %.3f, %.3f]\r\n "
	    "IRMS[%.3f, %.3f, %.3f]\r\n "
	    "P[%.3f, %.3f, %.3f]\r\n "
	    "Q[%.3f, %.3f, %.3f]\r\n "
	    "S[%.3f, %.3f, %.3f]\r\n",
	    metrology_data.urms.ph1, metrology_data.urms.ph2, metrology_data.urms.ph3,
	    metrology_data.irms.ph1, metrology_data.irms.ph2, metrology_data.irms.ph3,
	    metrology_data.p.ph1,   metrology_data.p.ph2,   metrology_data.p.ph3,
	    metrology_data.q.ph1,   metrology_data.q.ph2,   metrology_data.q.ph3,
	    metrology_data.s.ph1,   metrology_data.s.ph2,   metrology_data.s.ph3
	));
}

static void metrology_post_process(void)
{
	/* Set all channels fake voltages */
//	metrology_data.urms.ph1 = metrology_data.urms.ph2 = metrology_data.urms.ph3 = URMS_EMU_VAL;

	/* Set all channels fake current and power only if relay is closed */
//	irms1 = irms2 = irms3 = (g_relayClosedState)?IRMS_EMU_VAL:0;
//	p1 = p2 = p3 = (g_relayClosedState)?P_EMU_VAL:0;
//	q1 = q2 = q3 = (g_relayClosedState)?Q_EMU_VAL:0;
//	s1 = s2 = s3 = (g_relayClosedState)?S_EMU_VAL:0;
	metrology_data.frequency = 50000;

	/* add this energy to the cumulative energy counter for saving to NVM later */
	/* integrate power with time = 1/4 seconds every time */
	ramcnt.energy.wh_t   += Wh_EMU_VAL;
	ramcnt.energy.wh_i   += Wh_EMU_VAL;
	ramcnt.energy.wh_e   += 0;
	ramcnt.energy.varh_t += VARh_EMU_VAL;
	ramcnt.energy.varh_i += VARh_EMU_VAL;
	ramcnt.energy.varh_e += 0;
}

static void metrology_process(uint8_t read_index)
{
	static uint32_t read_period = 0;                 /* reading period for non-billing averaged values */
	long fcn_out = 0;
	long seq = 0;                             			/* sense of rotation */

	if (buffer_inUse[read_index] != 1)
	{
		return;
	}

	METERLIBFFT3PH_InitMainBuffPh1(&mlib, u1_re[read_index], i1_re[read_index], u1_im, i1_im, shift1);
	METERLIBFFT3PH_InitMainBuffPh2(&mlib, u2_re[read_index], i2_re[read_index], u2_im, i2_im, shift2);
	METERLIBFFT3PH_InitMainBuffPh3(&mlib, u3_re[read_index], i3_re[read_index], u3_im, i3_im, shift3);

	if (adc_samples[read_index] > FFT_SAMPLES)
	{
		/* performs interpolation only for asynchronous processing */
		fcn_out = METERLIBFFT3PH_Interpolation(&mlib, ORD2, ORD3, adc_samples[read_index]);
	}

	/* main calculation (FFT, scaling, averaging) */
	METERLIBFFT3PH_CalcMain(&mlib);

	/* gets max. 32 FFT magnitudes for all lines (auxiliary output only) */
	METERLIBFFT3PH_GetMagnitudesPh1(&mlib, AUX_SAMPLES);
	METERLIBFFT3PH_GetMagnitudesPh2(&mlib, AUX_SAMPLES);
	METERLIBFFT3PH_GetMagnitudesPh3(&mlib, AUX_SAMPLES);

	/* gets max. 32 FFT phases for all lines (auxiliary output only) */
	METERLIBFFT3PH_GetPhasesPh1(&mlib, AUX_SAMPLES);
	METERLIBFFT3PH_GetPhasesPh2(&mlib, AUX_SAMPLES);
	METERLIBFFT3PH_GetPhasesPh3(&mlib, AUX_SAMPLES);

	buffer_inUse[read_index] = 0;

	/* sense of rotation measurement */
	seq = METERLIBFFT3PH_GetRotation(&mlib, &u12_ph, &u13_ph, &u23_ph);

	/* calculates active energy increment and threshold value for active energy timer */
	//tmr_cmp = TMRCMPVAL(METERLIBFFT3PH_CalcWattHours(&ui, &whi, &whe, frequency), TMRCLK);
	METERLIBFFT3PH_CalcWattHours(&mlib, &metrology_data.energy_cnt.wh_i, &metrology_data.energy_cnt.wh_e, metrology_data.frequency);
	metrology_data.energy_cnt.wh_i = metrology_data.energy_cnt.wh_i/EN_RES10;
	metrology_data.energy_cnt.wh_e = metrology_data.energy_cnt.wh_e/EN_RES10;
	/* calculates reactive energy increment and threshold value for reactive energy timer */
	//tmr_cmp = TMRCMPVAL(METERLIBFFT3PH_CalcVarHours(&ui, &varhi, &varhe, frequency), TMRCLK);
	METERLIBFFT3PH_CalcVarHours(&mlib, &metrology_data.energy_cnt.varh_i, &metrology_data.energy_cnt.varh_e, metrology_data.frequency);
	metrology_data.energy_cnt.varh_i = metrology_data.energy_cnt.varh_i/EN_RES10;
	metrology_data.energy_cnt.varh_e = metrology_data.energy_cnt.varh_e/EN_RES10;

	/* reads all non-billing instantaneous metering values (all lines) */
#if USE_METROLOGY_INSTVALUES
	METERLIBFFT3PH_GetInstValuesPh1(&mlib, &metrology_data.urms.ph1, &metrology_data.irms.ph1, &metrology_data.p.ph1,
			&metrology_data.q.ph1, &metrology_data.s.ph1, &metrology_data.pf.ph1, &metrology_data.thdu.ph1, &metrology_data.thdi.ph1);
	METERLIBFFT3PH_GetInstValuesPh2(&mlib, &metrology_data.urms.ph2, &metrology_data.irms.ph1, &metrology_data.p.ph2,
			&metrology_data.q.ph2, &metrology_data.s.ph2, &metrology_data.pf.ph2, &metrology_data.thdu.ph2, &metrology_data.thdi.ph2);
	METERLIBFFT3PH_GetInstValuesPh3(&mlib, &metrology_data.urms.ph3, &metrology_data.irms.ph1, &metrology_data.p.ph3,
			&metrology_data.q.ph3, &metrology_data.s.ph3, &metrology_data.pf.ph3, &metrology_data.thdu.ph3, &metrology_data.thdi.ph3);
#else

	/* reads all non-billing averaged metering values every 50-th cycle */
	if (++read_period == 50)
	{ /* simulate asynchronous reading of all averaged values */
		METERLIBFFT3PH_GetAvrgValuesPh1(&mlib, &metrology_data.urms.ph1, &metrology_data.irms.ph1, &metrology_data.p.ph1,
				&metrology_data.q.ph1, &metrology_data.s.ph1, &metrology_data.pf.ph1, &metrology_data.thdu.ph1, &metrology_data.thdi.ph1);
		METERLIBFFT3PH_GetAvrgValuesPh2(&mlib, &metrology_data.urms.ph2, &metrology_data.irms.ph2, &metrology_data.p.ph2,
				&metrology_data.q.ph2, &metrology_data.s.ph2, &metrology_data.pf.ph2, &metrology_data.thdu.ph2, &metrology_data.thdi.ph2);
		METERLIBFFT3PH_GetAvrgValuesPh3(&mlib, &metrology_data.urms.ph3, &metrology_data.irms.ph3, &metrology_data.p.ph3,
				&metrology_data.q.ph3, &metrology_data.s.ph3, &metrology_data.pf.ph3, &metrology_data.thdu.ph3, &metrology_data.thdi.ph3);
		print_metrology_data();
#if ENABLE_OCPP
        EVSE_OCPP_SetEvent(EVSE_TELEMETRY_READY_EVENT);
#endif
		read_period = 0;
	}
#endif
}

static void Metering_CleanEverything(void)
{
	OSA_SR_ALLOC();
	OSA_ENTER_CRITICAL();
	wr_buffer = 0;
	rd_buffer = 0;
	memset(adc_samples, 0, sizeof(adc_samples));
	memset(buffer_inUse, 0, sizeof(buffer_inUse));
	current_sample = 0;
	OSA_EXIT_CRITICAL();
}

static void metrology_init(void)
{
	long fcn_out = 0;

	configPRINTF(("Initializing metering"));
	/* if calibration data were collected then calibration parameters are       */
	/* calculated and saved to flash                                            */

	/* Mandatory initialization section - for main FFT calculation             */
	fcn_out = METERLIBFFT3PH_InitParam(&mlib, FFT_SAMPLES, SENS_PROP, IMP2000, IMP2000, EN_RES10);
	fcn_out = METERLIBFFT3PH_SetCalibCoeffPh1(&mlib, 634, 64.00, &afe1_offs, 0.0, 0.0);
	fcn_out = METERLIBFFT3PH_SetCalibCoeffPh2(&mlib, 634, 64.00, &afe2_offs, 0.0, 0.0);
	fcn_out = METERLIBFFT3PH_SetCalibCoeffPh3(&mlib, 634, 64.00, &afe3_offs, 0.0, 0.0);

//	shift1[1] = ramcfg.angle1;
//	shift2[1] = ramcfg.angle2;
//	shift3[1] = ramcfg.angle3;

	/* Energy registers can be zeroed or filled-up with some initial values    */
	METERLIBFFT_SetEnergy(mlib, 0, 0, 0, 0);

	/* Auxiliary initialization - for magnitudes and phase shifts computing only */
	METERLIBFFT3PH_InitAuxBuffPh1(&mlib, u1_mag, i1_mag, u1_ph, i1_ph);
	METERLIBFFT3PH_InitAuxBuffPh2(&mlib, u2_mag, i2_mag, u2_ph, i2_ph);
	METERLIBFFT3PH_InitAuxBuffPh3(&mlib, u3_mag, i3_mag, u3_ph, i3_ph);

	metrology_data.frequency = 500.0f;
	Metering_CleanEverything();
}

static void metrology_task(osa_task_param_t arg)
{
	(void)arg;
	
	for (uint16_t index = 0; index < sizeof(tCONFIG_FLASH_DATA); index++)
	{
		*(uint8_t *)((uint8_t *)&ramcfg + index) = 0xFF;
	}

    CONFIG_ReadFromNV ((tCONFIG_FLASH_DATA*)&ramcfg);
    if (ramcfg.flag == 0xffff)
    {
    	/* TODO Save in NV */
    	ramcfg = nvmcfg;
    }

    metrology_init();
	
	AFE_Init();
	
	Metering_CleanEverything();
	configPRINTF(("Lost packets %d", buffers_not_process_in_time));
	while (s_task_deleted == false)
	{
		osa_status_t status;
		metrology_msg_t msg;
		status = OSA_MsgQGet((osa_msgq_handle_t)metrology_queue_handle, (osa_msg_handle_t) &msg, osaWaitForever_c);

		if (status == KOSA_StatusSuccess)
		{
			if (msg.event == Metrology_DataRCV)
			{
				metrology_process(msg.data);
			}
		}
	}

	OSA_MsgQDestroy((osa_msgq_handle_t)metrology_queue_handle);
	OSA_TaskDestroy((osa_task_handle_t)metrology_task_handle);
}

static void EVSE_Metrology_SetEvent(metrology_events_t evse_metrology_event, uint32_t data)
{
	osa_status_t status;

	if (check_init() != true)
	{
		return;
	}
	metrology_msg_t msg = {
			.event = evse_metrology_event,
			.data = data};

	OSA_MsgQPut((osa_msgq_handle_t)metrology_queue_handle, (osa_msg_handle_t)&msg);
}

void Metering_StartProcessing(void)
{
	EVSE_Metrology_SetEvent(Metrology_DataRCV, rd_buffer);
}

void Metering_SampleReset(void)
{
	current_sample = 0;
	adc_samples[wr_buffer] = 0;	
}

uint32_t Metering_SampleAdd(frac24 u1, frac24 u2, frac24 u3, frac24 i1, frac24 i2, frac24 i3)
{
	if ((current_sample < INP_SAMPLES) && (buffer_inUse[wr_buffer] == 0))
	{
		u1_re[wr_buffer][current_sample] = u1;
		u2_re[wr_buffer][current_sample] = u2;
		u3_re[wr_buffer][current_sample] = u3;
		i1_re[wr_buffer][current_sample] = i1;
		i2_re[wr_buffer][current_sample] = i2;
		i3_re[wr_buffer][current_sample] = i3;

		current_sample++;
	}
	else if (buffer_inUse[wr_buffer])
	{
		buffers_not_process_in_time++;
	}

	return current_sample;
}

void Metering_SampleFinish(void)
{
	/* This is in the IRQ context */
	/* Save number of samples added */
	adc_samples[wr_buffer] = current_sample;
	/* Set the buffer as complete to track that no one is using it  */
	buffer_inUse[wr_buffer] = 1;
	/* Reset the buffer */
	current_sample = 0;
	/* Save written buffer */
	rd_buffer = wr_buffer;
	/* switch buffer at the completion of one sin~wave */
	wr_buffer = 1 - wr_buffer;
}

bool EVSE_MetrologyInit()
{
	osa_status_t status;

	if (check_init() == true)
	{
		return true;
	}

	status = OSA_MsgQCreate((osa_msgq_handle_t)metrology_queue_handle, 3, sizeof(metrology_msg_t));

    if (status != KOSA_StatusSuccess)
    {
        configPRINTF(("Failed to create Event Group !\r\n"));
        while (1);
    }

    status = OSA_TaskCreate((osa_task_handle_t)metrology_task_handle, OSA_TASK(metrology_task), NULL);

    if (status != KOSA_StatusSuccess)
    {
        configPRINTF(("Failed to create Metrology Task!\r\n"));
        while (1);
    }

    is_init = true;
    return true;
}

const phase_data_t Metrology_ReadCurrent()
{
	return metrology_data.irms;
}

const phase_data_t Metrology_ReadVoltage()
{
	return metrology_data.urms;
}

const phase_data_t Metrology_ReadActivePower()
{
	return metrology_data.p;
}

const tENERGY_REG Metrology_ReadEnergy()
{
	return metrology_data.energy_cnt;
}

void EVSE_MetrologyDeinit()
{
	if (check_init() != true)
	{
		return;
	}

	s_task_deleted = true;
	is_init = false;
	EVSE_Metrology_SetEvent(Metrology_TaskDelete, 0);
}
