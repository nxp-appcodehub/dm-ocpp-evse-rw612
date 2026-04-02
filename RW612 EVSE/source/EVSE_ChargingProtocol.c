/*
 * Copyright 2024-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */


#include "EVSE_Utils.h"
#include "EVSE_ChargingProtocol.h"

#include "app.h"
#include "task_config.h"
#include "fsl_os_abstraction.h"
#include "fsl_common.h"

/* ================================================================================================
 * CONSTANTS AND MACROS
 * ================================================================================================ */

#define WAIT_FOR_LL_RETRY_BLOCKING 0xFFFFFFFF
#define WAIT_FOR_LL_RETRY_COUNT 0x100
#define MASK_INTERRUPT_TIME_MS 1000U
#define BUTTON_DEBOUNCE_MS 100U
#define DIRECTION_CHANGE_DEBOUNCE_MS 500U
#define CHARGING_PROTOCOL_MAX_DELAY_MS 5000U

#define CALL_IF_NOT_NULL(func_ptr) \
    do {                           \
        if ((func_ptr) != NULL) {  \
            (func_ptr)();          \
        }                          \
    } while (0)

#define SAFE_STRCPY(dest, src, size) \
    do { \
        strncpy((dest), (src), (size) - 1); \
        (dest)[(size) - 1] = '\0'; \
    } while (0)

#define APP_CHARGINGPROTOCOL_PRIORITY_OSA PRIORITY_RTOS_TO_OSA(APP_CHARGINGPROTOCOL_PRIORITY)
/* ================================================================================================
 * STATIC FUNCTION DECLARATIONS
 * ================================================================================================ */

static bool EVSE_ChargingProtocol_InitializeResources(void);
static void EVSE_ChargingProtocol_CleanupResources(void);
static bool EVSE_ChargingProtocol_ValidateHandlers(void);
static void EVSE_ChargingProtocol_InitializeVehicleData(void);
static void EVSE_ChargingProtocol_RefreshVehicleData(void);
static void EVSE_ChargingProtocol_HandleEvents(void);
static void EVSE_ChargingProtocol_HandleButtonPress(void);
static bool EVSE_ChargingProtocol_IsTimeoutExpired(uint32_t last_time, uint32_t timeout_ms);
static void EVSE_ChargingProtocol_WaitForLowLevelReady(void);
static void EVSE_ChargingProtocol_DataInit(void);
static void EVSE_ChargingWaitEvent(void);
static void chargingprotocol_task(void *pvParameters);

/* ================================================================================================
 * GLOBAL VARIABLES AND CONFIGURATION
 * ================================================================================================ */

static const charging_hal_functions_t *s_charging_handlers = NULL;


/* ================================================================================================
 * STATIC VARIABLES
 * ================================================================================================ */

static uint16_t s_stack_delay = CHARGINGPROTOCOL_TICK_DELAY;
static vehicle_data_t s_vehicle_data = {0};
static evse_charging_protocol_t s_current_charging_protocol = EVSE_BasicCharging_J1772;

static charging_states_t s_charging_state = EVSE_ChargingNone;

typedef struct {
    bool button_state;
    uint32_t last_button_press;
    uint32_t last_direction_change;
} runtime_state_t;

static runtime_state_t s_runtime_state = {0};
static bool is_init = false;

static OSA_TASK_DEFINE(chargingprotocol_task, APP_CHARGINGPROTOCOL_PRIORITY_OSA, 1, APP_CHARGINGPROTOCOL_STACK_SIZE, 0);
static OSA_EVENT_HANDLE_DEFINE(charging_event_handle);
static OSA_MUTEX_HANDLE_DEFINE(charging_mutex_handle);
static OSA_TASK_HANDLE_DEFINE(charging_task_handle);

/* ================================================================================================
 * CONSTANT ARRAYS
 * ================================================================================================ */

const char *EVSE_PROTOCOL_strings[EVSE_LastChargingProtocol + 1] = {
    [EVSE_BasicCharging_J1772]           = "BASIC",
    [EVSE_HighLevelCharging_ISO15118]    = "ISO15118-2",
    [EVSE_HighLevelCharging_ISO15118_20] = "ISO15118-20",
    [EVSE_LastChargingProtocol]          = "LAST"
};

static const char *CHARGING_DIRECTION_strings[] = {
    [EVSE_NoChargingDirection] = "No charging direction",
    [EVSE_G2V] = "Grid to Vehicle",
    [EVSE_V2G] = "Vehicle to Grid"
};

/* ================================================================================================
 * CALLBACK FUNCTIONS
 * ================================================================================================ */

static void EVSE_ChargingProtocol_PauseResumeBtn_callback(void)
{
    uint32_t current_time = EVSE_GetMsSinceBoot();
    
    /* Debounce button press */
    if (EVSE_ChargingProtocol_IsTimeoutExpired(s_runtime_state.last_button_press, BUTTON_DEBOUNCE_MS))
    {
        s_runtime_state.button_state = true;
        s_runtime_state.last_button_press = current_time;
    }
}

static void EVSE_ChargingProtocol_ChangeDirection_callback(void)
{
    uint32_t current_time = EVSE_GetMsSinceBoot();

    if (EVSE_ChargingProtocol_IsTimeoutExpired(s_runtime_state.last_direction_change, DIRECTION_CHANGE_DEBOUNCE_MS))
    {
#if ENABLE_ISO15118
        EVSE_ISO15118_ChangeEnergyDirection();
#endif /* ENABLE_ISO15118 */
        s_runtime_state.last_direction_change = current_time;
    }
}

/* ================================================================================================
 * UTILITY FUNCTIONS
 * ================================================================================================ */

static bool check_init()
{
	return is_init;
}

static bool EVSE_ChargingProtocol_IsTimeoutExpired(uint32_t last_time, uint32_t timeout_ms)
{
    uint32_t current_time = EVSE_GetMsSinceBoot();
    return (current_time - last_time) > timeout_ms;
}

static bool EVSE_ChargingProtocol_ValidateHandlers(void)
{
#if ENABLE_CHARGING_LOW_LEVEL_HANDLING
    assert(s_charging_handlers != NULL);
    return true;
#else
    return true; /* No handlers to validate */
#endif
}

static void EVSE_ChargingProtocol_InitializeVehicleData(void)
{
	if (OSA_MutexLock((osa_mutex_handle_t)charging_mutex_handle, 100) == KOSA_StatusSuccess)
    {
        memset(&s_vehicle_data, 0, sizeof(vehicle_data_t));
        
        SAFE_STRCPY(s_vehicle_data.vehicleID, "N/A", sizeof(s_vehicle_data.vehicleID));
        const char *data_protocol = EVSE_ChargingProtocol_GetStringFromCurrentProtocol();
        if (data_protocol != NULL)
        {
            SAFE_STRCPY(s_vehicle_data.protocol, data_protocol, sizeof(s_vehicle_data.protocol));
        }
        
        s_vehicle_data.charging_protocol = EVSE_ChargingProtocol_GetProtocol();
        
        OSA_MutexUnlock((osa_mutex_handle_t)charging_mutex_handle);
    }
}

static void EVSE_ChargingProtocol_RefreshVehicleData(void)
{
    if (OSA_MutexLock((osa_mutex_handle_t)charging_mutex_handle, 10) == KOSA_StatusSuccess)
    {
#if ENABLE_ISO15118
        EVSE_ISO15118_GetVehicleData(&s_vehicle_data);
#elif ENABLE_J1772
        EVSE_J1772_GetVehicleData(&s_vehicle_data);
#endif /* ENABLE_ISO15118 */

        s_vehicle_data.charging_protocol = EVSE_ChargingProtocol_GetProtocol();
        SAFE_STRCPY(s_vehicle_data.protocol, EVSE_ChargingProtocol_GetStringFromCurrentProtocol(), sizeof(s_vehicle_data.protocol));

#if (CLEV663_ENABLE == 1)
        SAFE_STRCPY(s_vehicle_data.vehicleID, EVSE_NFC_Get_VehicleID(), sizeof(s_vehicle_data.vehicleID));
#endif /* (CLEV663_ENABLE == 1) */

        OSA_MutexUnlock((osa_mutex_handle_t)charging_mutex_handle);
    }
}

/* ================================================================================================
 * CHARGING CONTROL FUNCTIONS
 * ================================================================================================ */

void EVSE_ChargingProtocol_StartCharging(void)
{
#if ENABLE_ISO15118
    EVSE_ISO15118_StartCharging();
#endif
}

void EVSE_ChargingProtocol_StopCharging(void)
{
#if ENABLE_ISO15118
    EVSE_ISO15118_StopCharging(true);
#elif ENABLE_J1772
    EVSE_J1772_StopCharging(true);
#endif /* ENABLE_ISO15118 */
}

static void EVSE_ChargingProtocol_PauseCharging(void)
{
#if ENABLE_ISO15118
    // TODO check iso15118_20 and pause charging session
#endif /* ENABLE_ISO15118 */
}


/* ================================================================================================
 * EVENT HANDLING
 * ================================================================================================ */

static void EVSE_ChargingWaitEvent(void)
{
	osa_event_flags_t uxBits;

	if (OSA_EventWait((osa_event_handle_t)charging_event_handle,
			EVSE_Charging_ALL_EVENTS,
			false, s_stack_delay, &uxBits) == KOSA_StatusTimeout)
	{
		return;
	}

    if ((uxBits & EVSE_Charging_EVSERefreshData) == EVSE_Charging_EVSERefreshData)
    {
        EVSE_ChargingProtocol_RefreshVehicleData();
    }
    if ((uxBits & EVSE_Charging_EVSEStopCharging) == EVSE_Charging_EVSEStopCharging)
    {
        configPRINTF(("Stop charge request"));
        EVSE_ChargingProtocol_StopCharging();
    }
    if ((uxBits & EVSE_Charging_EVSESuspendCharging) == EVSE_Charging_EVSESuspendCharging)
    {
        configPRINTF(("Suspend charge request"));
        EVSE_ChargingProtocol_PauseCharging();
    }
}

/* ================================================================================================
 * INITIALIZATION FUNCTIONS
 * ================================================================================================ */

static void EVSE_ChargingProtocol_WaitForLowLevelReady(void)
{
#if ENABLE_CHARGING_LOW_LEVEL_HANDLING
    if (s_charging_handlers && s_charging_handlers->isReady)
    {
        uint32_t retry_count = 0;
        const uint32_t max_retries = WAIT_FOR_LL_RETRY_COUNT;
        
        while ((s_charging_handlers->isReady() != true) && (retry_count < max_retries))
        {
        	OSA_TimeDelay(s_stack_delay);
            
            if (s_stack_delay < CHARGING_PROTOCOL_MAX_DELAY_MS)
            {
                s_stack_delay = (s_stack_delay < (CHARGING_PROTOCOL_MAX_DELAY_MS / 2)) ? 
                               (s_stack_delay * 2) : CHARGING_PROTOCOL_MAX_DELAY_MS;
            }
            
            if(max_retries != WAIT_FOR_LL_RETRY_BLOCKING)
            {
                retry_count++;
            }        
        }
        
        if (retry_count >= max_retries)
        {
            configPRINTF(("WARNING: Low level handlers not ready after %u retries\r\n", max_retries));
        }
        else
        {
            configPRINTF(("Low level handlers ready after %u attempts\r\n", retry_count));
        }
    }
#endif
}

static bool EVSE_ChargingProtocol_InitializeResources(void)
{
	osa_status_t status;
    /* Create data mutex */

	status = OSA_MutexCreate((osa_mutex_handle_t)charging_mutex_handle);
    if (status != KOSA_StatusSuccess)
    {
        configPRINTF(("Failed to create Charging Mutex Group !\r\n"));
        return false;
    }

	status = OSA_EventCreate((osa_event_handle_t)charging_event_handle, 1);
    if (status != KOSA_StatusSuccess)
    {
        configPRINTF(("Failed to create Charging Event Group !\r\n"));
        return false;
    }

    return true;
}

static void EVSE_ChargingProtocol_CleanupResources(void)
{
	(void)OSA_MutexDestroy((osa_mutex_handle_t)charging_mutex_handle);
	(void)OSA_EventDestroy(charging_event_handle);

}

static void EVSE_ChargingProtocol_DataInit(void)
{
    EVSE_ChargingProtocol_InitializeVehicleData();
}

/* ================================================================================================
 * MAIN TASK FUNCTION
 * ================================================================================================ */

static void chargingprotocol_task(osa_task_param_t arg)
{
    (void)arg;
	bool stopCharging = false;

    EVSE_ChargingProtocol_WaitForLowLevelReady();

    EVSE_ChargingProtocol_SetTaskDelay(CHARGINGPROTOCOL_SHORTTICK_DELAY);

#if ENABLE_ISO15118
    EVSE_ISO15118_Init();
#else
    EVSE_J1772_Init(s_charging_handlers);
#endif /* ENABLE_ISO15118 */

    while (1)
    {
        if (stopCharging == false)
        {
#if ENABLE_ISO15118
            EVSE_ISO15118_Loop(&stopCharging);
#else
            EVSE_J1772_Loop(&stopCharging);
#endif /* ENABLE_ISO15118 */
        }
        EVSE_ChargingWaitEvent();
    }
}

/* ================================================================================================
 * PUBLIC API FUNCTIONS
 * ================================================================================================ */

void EVSE_ChargingProtocol_SetTaskPriority(charging_priority_t priority_mode)
{
    if (priority_mode == EVSE_Charging_EVSEMaxPriority)
    {
    	(void)OSA_TaskSetPriority((osa_task_handle_t)charging_task_handle, OSA_TASK_PRIORITY_MAX);
    }
    else
    {
    	(void)OSA_TaskSetPriority((osa_task_handle_t)charging_task_handle, APP_CHARGINGPROTOCOL_PRIORITY_OSA);
    }
}

void EVSE_ChargingProtocol_SetEvent(charging_events_t event)
{
	if (check_init() == false)
	{
		return;
	}
    (void)OSA_EventSet((osa_event_handle_t)charging_event_handle, (osa_event_flags_t)event);
}

void EVSE_ChargingProtocol_SetTaskDelay(uint16_t new_stack_delay)
{
    if ((new_stack_delay >= 1) && (new_stack_delay <= CHARGINGPROTOCOL_LONGTICK_DELAY))
    {
        s_stack_delay = new_stack_delay;
#if ENABLE_SIGBRD

        SIGBRD_SetChargingProtocol_TickDelayMs(s_stack_delay);
#endif /* ENABLE_SIGBRD */
    }
}

void EVSE_ChargingProtocol_SetMaxCurrent(uint32_t max_current)
{
#if ENABLE_ISO15118
    EVSE_ISO15118_SetMaxCurrent(max_current);
#elif ENABLE_J1772
    EVSE_J1772_SetMaxCurrent(max_current);
#endif /* ENABLE_ISO15118 */
}


const char *EVSE_ChargingProtocol_GetStringFromCurrentProtocol(void)
{
    if (s_current_charging_protocol < EVSE_LastChargingProtocol)
    {
        return EVSE_PROTOCOL_strings[s_current_charging_protocol];
    }
    else
    {
        return NULL;
    }
}

const char *EVSE_ChargingProtocol_GetStringFromProtocol(evse_charging_protocol_t charging_protocol)
{
    if (charging_protocol < EVSE_LastChargingProtocol)
    {
        return EVSE_PROTOCOL_strings[charging_protocol];
    }
    else
    {
        return NULL;
    }
}

void EVSE_ChargingProtocol_SetProtocol(evse_charging_protocol_t charging_protocol)
{
    if (s_current_charging_protocol != charging_protocol)
    {
        s_current_charging_protocol = charging_protocol;
    }
}


evse_charging_protocol_t EVSE_ChargingProtocol_GetProtocol(void)
{

    return s_current_charging_protocol;
}

const char *EVSE_ChargingProtocol_GetProtocolString(void)
{

    return EVSE_PROTOCOL_strings[s_current_charging_protocol];
}


bool EVSE_ChargingProtocol_isCharging(void)
{
    bool bCharging = false;
#if ENABLE_ISO15118
    EVSE_ISO15118_isCharging(&bCharging);
#elif ENABLE_J1772
    EVSE_J1772_isCharging(&bCharging);
#endif /* ENABLE_ISO15118 */

    return bCharging;
}


charging_directions_t EVSE_ChargingProtocol_ChargingDirection(void)
{
    charging_directions_t charging_direction = EVSE_NoChargingDirection;

    if (EVSE_ChargingProtocol_isCharging())
    {

        if (s_current_charging_protocol == EVSE_BasicCharging_J1772)
        {
            charging_direction = EVSE_G2V;
        }
#if ENABLE_ISO15118

        else if ((s_current_charging_protocol == EVSE_HighLevelCharging_ISO15118) ||
                 (s_current_charging_protocol == EVSE_HighLevelCharging_ISO15118_20))
        {
            charging_direction = EVSE_ISO15118_GetEnergyDirection();
        }
#endif /* ENABLE_ISO15118 */
    }

    return charging_direction;
}

const char *EVSE_ChargingProtocol_GetStringFromDirection(charging_directions_t charging_direction)
{

    if (charging_direction < (sizeof(CHARGING_DIRECTION_strings) / sizeof(CHARGING_DIRECTION_strings[0])))
    {
        return CHARGING_DIRECTION_strings[charging_direction];
    }
    return "Unknown";
}


charging_states_t EVSE_ChargingProtocol_GetChargingState(void)
{
    return s_charging_state;
}

void EVSE_ChargingProtocol_SetChargingState(charging_states_t charging_state)
{
    s_charging_state = charging_state;
}


const char *EVSE_ChargingProtocol_isChargingString(void)
{
    return EVSE_ChargingProtocol_isCharging() ? "YES" : "NO";
}

const vehicle_data_t *EVSE_ChargingProtocol_GetVehicleData(void)
{

    EVSE_ChargingProtocol_RefreshVehicleData();
    
    return &s_vehicle_data;
}


char *EVSE_ChargingProtocol_GetCPStateString(void)
{
    static char cpStateString[2] = "A";
#if ENABLE_ISO15118
    cpStateString[0] = EVSE_ISO15118_GetCpStateString();
#elif ENABLE_J1772
    cpStateString[0] = EVSE_J1772_GetCpStateString();
#endif /* ENABLE_ISO15118 */

    return cpStateString;
}

void EVSE_ChargingProtocol_Init(const charging_hal_functions_t *charging_handlers)
{
	if (check_init() == true)
	{
		return;
	}

    s_stack_delay = CHARGINGPROTOCOL_TICK_DELAY;

    s_charging_handlers = charging_handlers;
    /* Initialize resources */
    if (EVSE_ChargingProtocol_InitializeResources() != true)
    {
        configPRINTF(("ERROR: Failed to initialize charging protocol resources"));
        while (1);
    }

    /* Validate handlers */
    if (!EVSE_ChargingProtocol_ValidateHandlers())
    {
        configPRINTF(("ERROR: Invalid charging handlers"));
        EVSE_ChargingProtocol_CleanupResources();
        while (1);
    }

    CALL_IF_NOT_NULL(s_charging_handlers->init);

    EVSE_ChargingProtocol_DataInit();

    osa_status_t status = OSA_TaskCreate((osa_task_handle_t)charging_task_handle, OSA_TASK(chargingprotocol_task), NULL);

	if (status != KOSA_StatusSuccess)
	{
		configPRINTF(("Failed to create charging Task!\r\n"));
		while (1);
	}

    is_init = true;
}

#if ENABLE_ISO15118
void EVSE_ChargingProtocol_SetPaymentMethod(vehicle_auth_methods_t method)
{
    EVSE_ISO15118_SetPaymentMethod(method);
}

void EVSE_ChargingProtocol_SetFNCAuthentication(uint8_t *cardUID, uint8_t size)
{
    EVSE_ISO15118_SetNFCAuthentication(cardUID, size);
}
#endif /* ENABLE_ISO15118 */
