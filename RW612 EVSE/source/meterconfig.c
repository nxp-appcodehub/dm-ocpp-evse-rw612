/*
* Copyright 2013, Freescale Semiconductor Inc.
* Copyright 2025-2026 NXP
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <string.h>
#include <stdint.h>
#include <math.h>
#include "fraclib.h"
#include "meterconfig.h"
#include "app.h"


/******************************************************************************
* public data definitions
******************************************************************************/
#ifndef SECTOR_INDEX_FROM_END
#define SECTOR_INDEX_FROM_END 1U
#define FLASH_DATA_NSU_START 0x10060000
#endif

#define TIMEOUT_IN_SEC(x) x*4

/* this variables is stored in parameter section of the flash  addr=0x001fc00 */
#if defined (__ICCARM__) /* IAR   */
#pragma location = ".config"
const tCONFIG_FLASH_DATA nvmcfg =
#elif defined(__GNUC__) /* CW GCC */
const tCONFIG_FLASH_DATA nvmcfg __attribute__ ((section(".config"))) = 
#endif
{
    /* basic power meter configuration data                                   */
    METER_SN,             /* Electricity meter serial number                  */
    1,                    /* tarif T1=1,T2=2,T3=3 and T4=4                    */ 
    5,                    /* number of pulses index for Wh generation   5k    */
    5,                    /* number of pulses index for VARh generation 5k    */ 
    48,                   /* VREFH trimming value                             */
    3,                    /* VREFL trimming value                             */
    /* pre-calibration data - user/measured inputs                            */  
    CAL_VOLT,             /* preset calibration voltage [Vrms]                */
    0.0,                  /* measured voltage [Vrms]                          */
    CAL_CURR,             /* preset calibration current [Arms]                */
    0.0,                  /* measured current [Arms]                          */
    0.785398163,          /* preset calibration angle [rad] = 45 degrees      */
    0.0,                  /* measured angle between voltage and current [rad] */
    0.0,                  /* measured active power [W]                        */
    0.0,                  /* measured reactive power [VAR]                    */
    0,			  /* measured maximum current- for offset calculation */
    0,         		  /* measured minimum current- for offset calculation */                
    CAL_VOLT,             /* preset calibration voltage [Vrms]                */
    0.0,                  /* measured voltage [Vrms]                          */
    CAL_CURR,             /* preset calibration current [Arms]                */
    0.0,                  /* measured current [Arms]                          */
    0.785398163,          /* preset calibration angle [rad] = 45 degrees      */
    0.0,                  /* measured angle between voltage and current [rad] */
    0.0,                  /* measured active power [W]                        */
    0.0,                  /* measured reactive power [VAR]                    */
    0,   		  /* measured maximum current- for offset calculation */
    0,         		  /* measured minimum current- for offset calculation */                
    CAL_VOLT,             /* preset calibration voltage [Vrms]                */
    0.0,                  /* measured voltage [Vrms]                          */
    CAL_CURR,             /* preset calibration current [Arms]                */
    0.0,                  /* measured current [Arms]                          */
    0.785398163,          /* preset calibration angle [rad] = 45 degrees      */
    0.0,                  /* measured angle between voltage and current [rad] */
    0.0,                  /* measured active power [W]                        */
    0.0,                  /* measured reactive power [VAR]                    */
    0,      		  /* measured maximum current- for offset calculation */
    0,         		  /* measured minimum current- for offset calculation */                
    /* post-calibration data - calculated phase delay, offsets and gains      */
    0,                    /* AFE ch0 vs SAR ch0 delay in modulator clocks     */
    0,                    /* current measurement offset			      */
    1.0, 	          /* current measurement gain            	      */
    1.0,                  /* voltage measurement gain                         */
    0,                    /* AFE ch1 vs SAR ch1 delay in modulator clocks     */
    0,                    /* current measurement offset                       */
    1.0,                  /* current measurement gain                         */
    1.0,                  /* voltage measurement gain                         */
    0,                    /* AFE ch2 vs SAR ch2 delay in modulator clocks     */
    0,                    /* current measurement offset                       */
    1.0,                  /* current measurement gain                         */
    1.0,                  /* voltage measurement gain                         */
    /* configuration flag                                                     */
    0xffff                /* 0xffff=read default configuration data           */
};

/* this variables is stored in flash                                          */
const tCONFIG_NOINIT_DATA nvmcnt = 
{
    /* basic power meter configuration data                                   */
    0l,                   /* total active energy counter                      */
	0l,                   /* active energy import counter                     */
	0l,                   /* active energy export counter                     */
	0l,                   /* total reactive energy counter                    */
	0l,                   /* reactive energy import counter                   */
	0l,                   /* reactive energy export counter                   */
	0,                    /* menu index                                       */
    FALSE,                /* hardfault didn't occur                           */
    /* configuration flag                                                     */
    0x5555                /* 0x5555=configuration valid                       */
};

/* these variables are stored in non-initialized ram                          */
#if defined (__ICCARM__) /* IAR   */
__no_init volatile tCONFIG_NOINIT_DATA  ramcnt;
__no_init volatile tCONFIG_FLASH_DATA   ramcfg;
#elif defined(__GNUC__) /* CW GCC */
volatile tCONFIG_NOINIT_DATA  ramcnt;
volatile tCONFIG_FLASH_DATA   ramcfg __attribute__ ((section(".noinit")));
#endif



uint32_t pflashBlockBase  = 0U;
uint32_t pflashTotalSize  = 0U;
uint32_t pflashSectorSize = 0U;
uint32_t PflashPageSize   = 0U;
/******************************************************************************
* public function definitions
******************************************************************************/
/*
* @brief Gets called when an error occurs.
*
* @details Print error message and trap forever.
*/
void error_trap(void)
{
    /* HALTED DUE TO FLASH ERROR! */
    while (1)
    {
    }
}

void PFlash_Init(void)
{
/* TO DO */
}

void CONFIG_ReadFromNV(tCONFIG_FLASH_DATA *ptr)
{
	/* TODO */
}

void CONFIG_SaveToNV (tCONFIG_FLASH_DATA *ptr, uint16 flag)
{ 
 /* TODO */
}

void CONFIG_UpdateOffsets   (tCONFIG_FLASH_DATA *ptr, int32 i1, int32 i2, int32 i3 )
{
    if (ptr->flag == 0xfff5) /* update offsets if pre-processing active         */
    { 
        if (ptr->i1_msrmax < i1) { ptr->i1_msrmax = i1; } /* find current max. value  */
        if (ptr->i1_msrmin > i1) { ptr->i1_msrmin = i1; } /* find current min. value  */
        
        if (ptr->i2_msrmax < i2) { ptr->i2_msrmax = i2; } /* find current max. value  */
        if (ptr->i2_msrmin > i2) { ptr->i2_msrmin = i2; } /* find current min. value  */
        
        if (ptr->i3_msrmax < i3) { ptr->i3_msrmax = i3; } /* find current max. value  */
        if (ptr->i3_msrmin > i3) { ptr->i3_msrmin = i3; } /* find current min. value  */
    }
}

void CONFIG_PreProcessing    (tCONFIG_FLASH_DATA *ptr, 
                              double urms1, double irms1,
                              double urms2, double irms2,
                              double urms3, double irms3,
                              double w1, double var1,
                              double w2, double var2,
                              double w3, double var3 )
{
    static int timeout = 0;
    
    if (ptr->flag == 0xfff5)     /* store measurements if pre-processing active */
    {
    	ptr->urms1_msr    = urms1;
    	ptr->irms1_msr    = irms1;
    	ptr->urms2_msr    = urms2;
    	ptr->irms2_msr    = irms2;
    	ptr->urms3_msr    = urms3;
    	ptr->irms3_msr    = irms3;
    	ptr->P1_msr       = w1;
    	ptr->Q1_msr       = var1;
    	ptr->P2_msr       = w2;
    	ptr->Q2_msr       = var2;
    	ptr->P3_msr       = w3;
    	ptr->Q3_msr       = var3;
    	/* timeout check - when timeout expires then finish pre-processing state  */
        /* by setting state at which calibration data are calculated after reset  */
        if ((timeout++) > TIMEOUT_IN_SEC(10)) 
        { ptr->flag = 0xffa4; }
    }
}

int16 CONFIG_CalcCalibData (tCONFIG_FLASH_DATA *ptr)
{  
    /* calculates calibration data if pre-processing completed sucessfully      */
    if (ptr->flag == 0xffa5) 
    {
        /* check calibration conditions to eliminate pre-heating states           */
        if ((ptr->irms1_msr >= ptr->irms1_cal*0.9) && 
            (ptr->irms1_msr <= ptr->irms1_cal*1.1) &&
            (ptr->urms1_msr >= ptr->urms1_cal    ) && 
            (ptr->irms1_msr >= ptr->irms1_cal    ) &&
            (ptr->irms2_msr >= ptr->irms2_cal*0.9) && 
            (ptr->irms2_msr <= ptr->irms2_cal*1.1) &&
            (ptr->urms2_msr >= ptr->urms2_cal    ) && 
            (ptr->irms2_msr >= ptr->irms2_cal    ) && 
            (ptr->irms3_msr >= ptr->irms3_cal*0.9) && 
            (ptr->irms3_msr <= ptr->irms3_cal*1.1) &&
            (ptr->urms3_msr >= ptr->urms3_cal    ) && 
            (ptr->irms3_msr >= ptr->irms3_cal    ) )
        {
            /* store offsets                                                    */
            ptr->i1_offset = (ptr->i1_msrmax+ptr->i1_msrmin)>>1;
            ptr->i2_offset = (ptr->i2_msrmax+ptr->i2_msrmin)>>1;
            ptr->i3_offset = (ptr->i3_msrmax+ptr->i3_msrmin)>>1;
            
            /* calculate and store voltage measurement gain (gain >= double(1.0))*/
            ptr->u1_gain   = (ptr->urms1_cal/ptr->urms1_msr);
            ptr->u2_gain   = (ptr->urms2_cal/ptr->urms2_msr);
            ptr->u3_gain   = (ptr->urms3_cal/ptr->urms3_msr);
            
            /* calculate and store current measurement gain (gain >= double(1.0))*/
            /* constant 0.9998 is the gain adjustment to calibrate to 0.00% error */
            ptr->i1_gain   = (ptr->irms1_cal/ptr->irms1_msr);
            ptr->i2_gain   = (ptr->irms2_cal/ptr->irms2_msr);
            ptr->i3_gain   = (ptr->irms3_cal/ptr->irms3_msr);
            
            /* calculate and store phase shift angle [0.001�]                   */
            /* alfa = (rad * 180.000) / Pi                                      */
            ptr->angle1_msr = atan2 (ptr->Q1_msr, ptr->P1_msr);
            ptr->angle1_msr = (ptr->angle1_msr - ptr->angle1_cal);
            ptr->angle1     = (frac32)((ptr->angle1_msr * 180000) / 3.1415926535897932384626433832795);
            
            ptr->angle2_msr = atan2 (ptr->Q2_msr, ptr->P2_msr);
            ptr->angle2_msr = (ptr->angle2_msr - ptr->angle2_cal);
            ptr->angle2     = (frac32)((ptr->angle2_msr * 180000) / 3.1415926535897932384626433832795);
            
            ptr->angle3_msr = atan2 (ptr->Q3_msr, ptr->P3_msr);
            ptr->angle3_msr = (ptr->angle3_msr - ptr->angle3_cal);
            ptr->angle3     = (frac32)((ptr->angle3_msr * 180000) / 3.1415926535897932384626433832795);
            
            ptr->flag = 0xa5a5;     /* calibration completed sucesfully         */
            return TRUE;
        }
        else
            ptr->flag = 0xfff5;     /* reinitiate calibration                       */  
    }
    return FALSE;
}
/******************************************************************************
* End of module                                                              *
******************************************************************************/
