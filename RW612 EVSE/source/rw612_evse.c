/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include "fsl_os_abstraction.h"
#include "app.h"
#include "task_config.h"

#include "fsl_debug_console.h"

#include "ControlPilot_Process.h"
#include "EVSE_LowLevel_Handles.h"

#if ENABLE_LOCAL_METROLOGY
#include "EVSE_Metrology.h"
#endif

#if ENABLE_CONNECTIVITY
#include "EVSE_ConnectivityConfig.h"
#endif
#include "EVSE_ConfigChecks.h"

#include "els_pkc_mbedtls.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
extern void vApplicationTickHookRegister(void);
extern BaseType_t xLoggingTaskInitialize( uint16_t usStackSize,
                                   UBaseType_t uxPriority,
                                   UBaseType_t uxQueueLength );
/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/**
 * @brief Execute first run through kernel timer service
 *
 * Called from the daemon task exactly once on first execution.  Any application
 * initialization code that needs the RTOS to be running can be placed in this
 * hook function.
 *
 */
void vApplicationDaemonTaskStartupHook(void)
{
	CRYPTO_InitHardware();

	OSA_Init();

    /* Init logging task */
    xLoggingTaskInitialize(LOGGING_TASK_STACK_SIZE, LOGGING_TASK_PRIORITY, LOGGING_QUEUE_LENGTH);

    /* Register os hooks */
    vApplicationTickHookRegister();

    configPRINTF(("Start the EasyEVSE MCU-RW612 application based on FreeRTOS..."));

#if ENABLE_FILESYSTEM
    FLASH_LITTLEFS_Init(false);
#endif

    /* Proceed to init the various components of the application. */
#if (ENABLE_CONNECTIVITY)
    EVSE_Connectivity_Init();;
#endif

#if (ENABLE_LCD)
    /* Init UI */
    EVSE_UI_Init();
#endif

#if (ENABLE_SE)
    /* Init Secure Element */
//     EVSE_Secure_Element_Init();
#endif

#if (CLEV663_ENABLE == 1)
    /* Init NFC Frontend */
    EVSE_NFC_Init();
#endif

#if (ENABLE_METER == 1)
    /* Init Meter */
    EVSE_Meter_Init();
#endif

#if (ENABLE_LOCAL_METROLOGY == 1)
    EVSE_MetrologyInit();
#endif

#if (ENABLE_CHARGING_PROTOCOL == 1)
    EVSE_ChargingProtocol_Init(&ll_charging_hal);
#endif

#if (ENABLE_SHELL == 1)
    EVSE_Shell_Init();
#endif

}

/*!
 * @brief Main function
 */
int main(void)
{
    /* Init hardware*/
    /* Use 16 MHz clock for the Ctimer0 */
    CLOCK_EnableClock(kCLOCK_InputMux);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    vTaskStartScheduler();
    for (;;)
        ;
}
