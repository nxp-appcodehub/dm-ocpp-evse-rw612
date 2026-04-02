/*
 * Copyright 2023-2026 NXP
 *
 * 
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * FreeRTOS Common V1.1.3
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "board.h"
#include "pin_mux.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "EVSE_Utils.h"


/*
 * The queue used to pass pointers to log messages from the task that created
 * the message to the task that will performs the output.
 */
QueueHandle_t xQueue = NULL;

/*-----------------------------------------------------------*/

/*
 * The task that actually performs the print output.  Using a separate task
 * enables the use of slow output, such as as a UART, without the task that is
 * outputting the log message having to wait for the message to be completely
 * written.  Using a separate task also serializes access to the output port.
 *
 * The structure of this task is very simple; it blocks on a queue to wait for
 * a pointer to a string, sending any received strings to a macro that performs
 * the actual output.  The macro is port specific, so implemented outside of
 * this file.  This version uses dynamic memory, so the buffer that contained
 * the log message is freed after it has been output.
 */
static void prvLoggingTask( void * pvParameters );


uint32_t EVSE_GetMsSinceBoot()
{
    uint32_t offsetTick = 0;
    uint32_t fromISR    = __get_IPSR();

    if (fromISR)
    {
        offsetTick = xTaskGetTickCountFromISR();
    }
    else
    {
        offsetTick = xTaskGetTickCount();
    }

    return offsetTick * (1000 / configTICK_RATE_HZ);
}

uint32_t EVSE_GetSecondsSinceBoot()
{
    uint32_t offsetTick = 0;
    offsetTick          = EVSE_GetMsSinceBoot();

    return offsetTick / 1000;
}

const char *convertSecToFormat(uint32_t seconds, timestamp_formats_t time_format)
{
    uint32_t hours                                    = 0;
    uint8_t min                                       = 0;
    uint8_t sec                                       = 0;
    static char string_format[MAX_TIME_FORMAT_LENGTH] = {0};

    hours = seconds / HOURS_DIVIDER;
    min   = (seconds % HOURS_DIVIDER) / MINUTES_DIVIDER;
    sec   = seconds - (hours * HOURS_DIVIDER) - (min * MINUTES_DIVIDER);

    if (time_format == HH_MM_SS)
    {
        sprintf(string_format, "%02d:%02d:%02d", hours, min, sec);
    }
    else if (time_format == HH_MM)
    {
        sprintf(string_format, "%02d:%02d", hours, min);
    }
    else
    {
        sprintf(string_format, "%02d:%02d", min, sec);
    }

    return string_format;
}


/*-----------------------------------------------------------*/

BaseType_t xLoggingTaskInitialize( uint16_t usStackSize,
                                   UBaseType_t uxPriority,
                                   UBaseType_t uxQueueLength )
{
    BaseType_t xReturn = pdFAIL;

    /* Ensure the logging task has not been created already. */
    if( xQueue == NULL )
    {
        /* Create the queue used to pass pointers to strings to the logging task. */
        xQueue = xQueueCreate( uxQueueLength, sizeof( char ** ) );

        if( xQueue != NULL )
        {
            if( xTaskCreate( prvLoggingTask, "Logging", usStackSize, NULL, uxPriority, NULL ) == pdPASS )
            {
                xReturn = pdPASS;
            }
            else
            {
                /* Could not create the task, so delete the queue again. */
                vQueueDelete( xQueue );
            }
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

static void prvLoggingTask( void * pvParameters )
{
    /* Disable unused parameter warning. */
    ( void ) pvParameters;

    char * pcReceivedString = NULL;

    for( ; ; )
    {
        /* Block to wait for the next string to print. */
        if( xQueueReceive( xQueue, &pcReceivedString, portMAX_DELAY ) == pdPASS )
        {
            configPRINT_STRING( pcReceivedString );

            vPortFree( ( void * ) pcReceivedString );
        }
    }
}

/* TODO - Integrate with SE */
uint32_t EVSE_Random()
{
    uint32_t output = 0;
    TRNG_GetRandomData(TRNG, &output, sizeof(uint32_t));
    return output;
}
