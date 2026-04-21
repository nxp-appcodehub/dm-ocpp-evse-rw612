/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#ifndef EXT_AFE_MKM_H_
#define EXT_AFE_MKM_H_

#include "fsl_common.h"
#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @addtogroup external_afe
 * @{
 */

/*! @file */

/*******************************************************************************
 * Definitions
 ******************************************************************************/


#define VOLTAGE_SIZE            sizeof(uint32_t)
#define CURRENT_SIZE            sizeof(uint32_t)
#define COMMAND_SIZE      		sizeof(uint16_t)
#define CRC_SIZE 		  		sizeof(uint16_t)
#define TRANSFER_SIZE     		12     /* Transfer dataSize */
#define BUFFER_SIZE 	  		TRANSFER_SIZE
#define SPI_TRANSFER_BAUDRATE 	40000000U /* Transfer baudrate - 40M */

#if AFE_MODE == 1

#define SAMPLE_RATE			3200U	/* External AFE is pre-configured to generate
									   3200 samples per second at a constant rate. */
#define DEFAULT_LINE_FREQ	50U		/* Typically AC line frequency is 50 Hertz */
/* There will be AFE_SAMPLES/DEFAULT_LINE_FREQ = 64 samples captured by external AFE for each
   channel. FFT algorithm will need these 64 samples buffered for a cycle to
   start FFT metrology without interpolation. */
#define AFE_INP_SAMPLES (SAMPLE_RATE/DEFAULT_LINE_FREQ)
#endif

//#define DUMP_AFE_SAMPES_CH_ID	  AFE_U1_INDEX	// 0 = U1, 1 = U2, 2 = U3, 4 = I1, 5 = I2, 6 = I3
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * API
 ******************************************************************************/
/*!
 * @brief Initialize SPI and DMA channels for external AFE interface.
 *
 * This function can be used to initialize the SPI master and DMA channels
 * to drive and get data from external AFE SPI slave.
 *
 */
void AFE_Init(void);

/*!
 * @brief De-Initialize SPI and DMA channels for external AFE interface.
 */
void AFE_DeInit(void);

#if defined(__cplusplus)
}
#endif

/*!
 * @}
 */

#endif /* EXT_AFE_MKM_H_ */
