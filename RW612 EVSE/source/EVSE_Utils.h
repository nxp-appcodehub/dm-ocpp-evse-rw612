/*
 * Copyright 2023-2025 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#ifndef EVSE_UTILS_H_
#define EVSE_UTILS_H_

#include "fsl_common.h"

/* A block time of 0 just means don't block. */
#define loggingDONT_BLOCK    0

#define HOURS_DIVIDER          3600
#define MINUTES_DIVIDER        60
#define MAX_TIME_FORMAT_H      7
#define MAX_TIME_FORMAT_M      2
#define MAX_TIME_FORMAT_S      2
#define MAX_TIME_FORMAT_LENGTH (MAX_TIME_FORMAT_H + MAX_TIME_FORMAT_M + MAX_TIME_FORMAT_S + 3)

/**
 * @brief Used by the Peripheral functions
 *
 */
typedef enum
{
    EVSE_Peripheral_Success,
    EVSE_Peripheral_Fail,
    EVSE_Peripheral_AlreadyInit, /* The interface has already been inited */
} evse_peripheral_status_t;

typedef enum _timestamp_formats
{
    HH_MM_SS,
    HH_MM,
    MM_SS
} timestamp_formats_t;

typedef enum _evse_return_status
{
    SUCCESS,
    FAIL,
    NOT_IMPLEMENTED,
}evse_return_status_t;

/**
 * Get the the number of miliseconds since the scheduler started
 * @return number of miliseconds
 */
uint32_t EVSE_GetMsSinceBoot();

/**
 * Get the the number of seconds since the scheduler started
 * @return number of seconds
 */
uint32_t EVSE_GetSecondsSinceBoot();

/**
 * Return a random generated number
 * @return
 */
uint32_t EVSE_Random();

/**
 * Return a string that displays time (given in seconds) in a specified format
 * @param seconds     period of time that we want to convert (given in seconds)
 * @param time_format the desired time format, chosen from timestamp_formats_t enum
 */
const char *convertSecToFormat(uint32_t seconds, timestamp_formats_t time_format);

#endif /* EVSE_UTILS_H_ */
