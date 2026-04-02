/*****************************************************************************
 * Copyright 2010, Freescale Semiconductor Inc.
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __METERCONFIG_H
#define __METERCONFIG_H

#include "types.h"
#include "meterlibfft.h"
/******************************************************************************
 * Macro definitions                                                          *
 ******************************************************************************/
 /*****************************************************************************
  * Default calibration structure definition
  *****************************************************************************/
#define METER_CL  " C 5-120A "              /*!< Meter class shown on LCD     */ 
#define METER_SN  95                        /*!< Meter serial number          */
#define CAL_CURR  5.0                       /*!< Calibration point - voltage  */                 
#define CAL_VOLT  230.0                     /*!< Calibration point - current  */

/******************************************************************************
 * configuration data structure definition														        *
 ******************************************************************************/
typedef struct
{
  /* basic power meter configuration data                                     */
  uint16      id;         /* Electricity meter serial number                  */
  uint16      tarif;      /* tarif T1=1,T2=2,T3=3 and T4=4                    */ 
  uint16      wh_idx;     /* number of pulses index for Wh generation         */
  uint16      varh_idx;   /* number of pulses index for VARh generation       */ 
  uint16      vrefh;      /* VREFH trimming value                             */
  uint16      vrefl;      /* VREFL trimming value                             */
  
  /* pre-calibration data - user/measured input                               */  
  double      urms1_cal;  /* preset calibration voltage [Vrms]                */
  double      urms1_msr;  /* measured voltage [Vrms]                          */
  double      irms1_cal;  /* preset calibration current [Arms]                */
  double      irms1_msr;  /* measured current [Arms]                          */
  double      angle1_cal; /* preset calibration angle [rad] = 45 degrees      */
  double      angle1_msr; /* measured angle between voltage and current [rad] */
  double      P1_msr;     /* measured active power [W]                        */
  double      Q1_msr;     /* measured reactive power [VAR]                    */
  int32       i1_msrmax;  /* measured maximum current                         */
  int32       i1_msrmin;  /* measured minimum current                         */
  double      urms2_cal;  /* preset calibration voltage [Vrms]                */
  double      urms2_msr;  /* measured voltage [Vrms]                          */
  double      irms2_cal;  /* preset calibration current [Arms]                */
  double      irms2_msr;  /* measured current [Arms]                          */
  double      angle2_cal; /* preset calibration angle [rad] = 45 degrees      */
  double      angle2_msr; /* measured angle between voltage and current [rad] */
  double      P2_msr;     /* measured active power [W]                        */
  double      Q2_msr;     /* measured reactive power [VAR]                    */
  int32       i2_msrmax;  /* measured maximum current                         */
  int32       i2_msrmin;  /* measured minimum current                         */
  double      urms3_cal;  /* preset calibration voltage [Vrms]                */
  double      urms3_msr;  /* measured voltage [Vrms]                          */
  double      irms3_cal;  /* preset calibration current [Arms]                */
  double      irms3_msr;  /* measured current [Arms]                          */
  double      angle3_cal; /* preset calibration angle [rad] = 45 degrees      */
  double      angle3_msr; /* measured angle between voltage and current [rad] */
  double      P3_msr;     /* measured active power [W]                        */
  double      Q3_msr;     /* measured reactive power [VAR]                    */
  int32       i3_msrmax;  /* measured maximum current                         */
  int32       i3_msrmin;  /* measured minimum current                         */
  
  /* post-calibration data - calculated phase delay, offsets and gains        */
  frac32      angle1;     /* Channel 1 pahase shift angle                     */
  int32       i1_offset;  /* current measurement offset			      */
  double      i1_gain;    /* current measurement gain             	      */
  double      u1_gain;    /* voltage measurement gain               	      */
  frac32      angle2;     /* Channel 2 pahase shift angle                     */
  int32       i2_offset;  /* current measurement offset (AFE ch1)             */
  double      i2_gain;    /* current measurement gain               	      */
  double      u2_gain;    /* voltage measurement gain                         */
  frac32      angle3;     /* Channel 3 pahase shift angle                     */
  int32       i3_offset;  /* current measurement offset (AFE ch1)             */
  double      i3_gain;    /* current measurement gain                         */
  double      u3_gain;    /* voltage measurement gain                         */
  
  /* configuration flag                                                       */
  uint16      flag;       /* 0xffff=read default configuration data           */
                          /* 0xfff5=perform calibration pre-processing        */
                          /* 0xffa5=calculate calibration data                */
                          /* 0xa5a5=calibration completed and stored          */
} tCONFIG_FLASH_DATA;

typedef struct
{
  tENERGY_REG energy;     /* saved energy counters                            */
  uint16      menu_idx;   /* menu index                                       */
  uint16      hardfault;  /* hardfault flag:TRUE-occurred, FALSE-didn't occur */                                     
  uint16      flag;       /* 0x5555= valid data                               */
                          /* 0x----= not valid data - initialization needed   */
} tCONFIG_NOINIT_DATA;

/******************************************************************************
 * exported data declarations                                                 *
 ******************************************************************************/
#if defined (__ICCARM__) /* IAR   */
  extern const              tCONFIG_NOINIT_DATA nvmcnt;
  extern __no_init volatile tCONFIG_NOINIT_DATA ramcnt; 
  extern __no_init volatile tCONFIG_FLASH_DATA  ramcfg;
  extern 		   const 	tCONFIG_FLASH_DATA nvmcfg;
#elif defined(__GNUC__) /* CW GCC */
  extern const              tCONFIG_NOINIT_DATA nvmcnt;
  extern           volatile tCONFIG_NOINIT_DATA ramcnt; 
  extern           volatile tCONFIG_FLASH_DATA  ramcfg;
  extern 		   const 	tCONFIG_FLASH_DATA nvmcfg;
#endif
  
/******************************************************************************
 * exported function prototypes														                    *
 ******************************************************************************/
/*
* @brief   Initialize NVM flash memory.
*/
void PFlash_Init(void);

/*
* @brief   Reads configuration data from NVM memory conditionally.
* @param   ptr   - pointer to tCONFIG_DATA to be read
* @note    Implemented as a function call.
*/
void CONFIG_ReadFromNV      (tCONFIG_FLASH_DATA *ptr);

/*
* @brief   Writes configuration data to NVM memory.
* @param   ptr   - pointer to tCONFIG_DATA to be saved
* @param   flag  - configuration flag
* @note    Implemented as a function call.
*/
void CONFIG_SaveToNV      (tCONFIG_FLASH_DATA *ptr, uint16 flag);

/*
* @brief   Updates offset of the phase voltage and current measurements
*          conditionally.
* @param   ptr   - pointer to tCONFIG_FLASH_DATA
* @param   i1    - phase 1 current sample
* @param   i2    - phase 2 current sample
* @param   i3    - phase 3 current sample
* @note    Implemented as a function call.
*/
void CONFIG_UpdateOffsets  (tCONFIG_FLASH_DATA *ptr, int32 i1, int32 i2, int32 i3 );

/*
* @brief   Pre-processes measurements conditionally.
* @param   ptr   - pointer to tCONFIG_FLASH_DATA
* @param   urms1 - RMS phase voltage
* @param   irms1 - RMS phase current
* @param   urms2 - RMS phase voltage
* @param   irms2 - RMS phase current
* @param   urms3 - RMS phase voltage
* @param   irms3 - RMS phase current
* @param   w     - active power
* @param   var   - reactive power
* @note    Implemented as a function call.
*/
void CONFIG_PreProcessing  (tCONFIG_FLASH_DATA *ptr,
                                   double urms1, double irms1,
                                   double urms2, double irms2,
                                   double urms3, double irms3,
                                   double w1, double var1,
                                   double w2, double var2,
                                   double w3, double var3 );

/*
* @brief   Calculates calibration data conditionally.
* @param   ptr   - pointer to tCONFIG_DATA
* @return  FALSE - error
*          TRUE  - success
* @note    Implemented as a function call.
*/
int16 CONFIG_CalcCalibData (tCONFIG_FLASH_DATA *ptr);
#endif /* __METERCONFIG_H */
