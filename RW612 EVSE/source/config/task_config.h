/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Task priorities. */
#include "FreeRTOS.h"
#include "task.h"

/* Logging task configuration */
#define LOGGING_TASK_PRIORITY   (tskIDLE_PRIORITY + 1)
#define LOGGING_TASK_STACK_SIZE 512
#define LOGGING_QUEUE_LENGTH    256

#define APP_CHARGINGPROTOCOL_PRIORITY  (configMAX_PRIORITIES - 4)
#define APP_CHARGINGPROTOCOL_STACK_SIZE (2 * 1024U)

#define APP_METER_PRIORITY   (configMAX_PRIORITIES - 2)
#define APP_METER_STACK_SIZE (2 * 1024U)

#define APP_SHELL_PRIORITY   (tskIDLE_PRIORITY + 1)
#define APP_SHELL_STACK_SIZE (512 * 4)

#define APP_EVSE_OCPP_PRIORITY   (configMAX_PRIORITIES - 5)
#define APP_EVSE_OCPP_STACK_SIZE (2048 * 8)

#define configDEFAULT_STD_THREAD_STACK_SIZE (2816U)
#define configDEFAULT_STD_THREAD_PRIORITY (tskIDLE_PRIORITY + 1)

#define APP_CONNECTIVITY_PRIORITY (configMAX_PRIORITIES - 4)
#define APP_CONECTIVITY_STACK_SIZE (configMINIMAL_STACK_SIZE * 30)

#ifdef __cplusplus
}
#endif
