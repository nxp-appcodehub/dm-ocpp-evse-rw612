/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef APP_H
#define APP_H

#define ENABLE_LCD 0
#define ENABLE_CONNECTIVITY 1
#define ENABLE_METER 0
#define ENABLE_CHARGING_PROTOCOL 1
#define ENABLE_CHARGING_LOW_LEVEL_HANDLING 1
#define ENABLE_FILESYSTEM 0
#define ENABLE_SIGBRD 0
#define ENABLE_LOCAL_METROLOGY 1
#define ENABLE_WIFI 1
#define ENABLE_OCPP 1

/* CHARGING PROTOCOL */
#define ENABLE_ISO15118 0
#define ENABLE_J1772 1

/* LOGGING */
#define ENABLE_LOGGING_J1772 1
#define ENABLE_LOGGING_CP    1
#define ENABLE_LOGGING_ADC    1
#define ENABLE_LOGGING_METROLOGY 0
#if (ENABLE_ISO15118 == 1)
#include "appl-main.h"
#include "ISO15118.h"
#elif (ENABLE_J1772 == 1)
#include "IEC61851.h"
#endif /* (ENABLE_ISO15118 == 1)*/

#if ENABLE_CONNECTIVITY
#include "EVSE_Connectivity.h"
#endif
#include "EVSE_ChargingProtocol.h"
#if (ENABLE_LCD)
#include "EVSE_UI.h"
#endif
#if (CLEV663_ENABLE == 1)
#include "EVSE_NFC.h"
#endif /* (CLEV663_ENABLE == 1) */
#if ENABLE_METER
#include "EVSE_Metering.h"
#endif


#define CRC_ENGINE CRC

#define RED_TEXT(x)    "\033[31;1m" x "\033[0m"
#define GREEN_TEXT(x)  "\033[32;1m" x "\033[0m"
#define YELLOW_TEXT(x) "\033[33;1m" x "\033[0m"
#define BLUE_TEXT(x)   "\033[36;1m" x "\033[0m"

#define error(x)   RED_TEXT(x)
#define success(x) GREEN_TEXT(x)
#define warning(x) YELLOW_TEXT(x)
#define info(x)    BLUE_TEXT(x)

#endif
