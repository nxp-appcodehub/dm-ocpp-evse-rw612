/*
 * Copyright 2025-2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to comply
 * with and are bound by, such license terms. If you do not agree to be bound by the applicable license terms, then you
 * may not retain, install, activate or otherwise use the software.
 */

#include "fsl_device_registers.h"
#include "fsl_spi.h"
#include "fsl_spi_dma.h"
#include "fsl_crc.h"
#include "fsl_gpio.h"
#include "fsl_inputmux.h"
#include "fsl_pint.h"

#include <stdbool.h>

#include "EVSE_Metrology.h"
#include "ext_afe_mkm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "math.h"
#include "app.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define AFE_SPI_BASE_INDEX   1
#define AFE_SPI_MASTER_BASEADDR_EXPAND(x)     	(SPI##x)
#define AFE_SPI_MASTER_CLK_EXPAND(x)			(kCLOCK_Flexcomm##x)
#define AFE_SPI_MASTER_BASEADDR_PARAM(x)      	AFE_SPI_MASTER_BASEADDR_EXPAND(x)
#define AFE_SPI_MASTER_CLK(x)       			AFE_SPI_MASTER_CLK_EXPAND(x)
#define AFE_SPI_MASTER_CLK_FREQ          		CLOCK_GetFlexCommClkFreq(AFE_SPI_BASE_INDEX)
#define AFE_SPI_MASTER_BASEADDR          		AFE_SPI_MASTER_BASEADDR_PARAM(AFE_SPI_BASE_INDEX)
#define AFE_SPI_MASTER_PCS    				   (kSPI_Ssel0)
#define AFE_SPI_BAUDRATE                        (5000000U)


#define AFE_SPI_MASTER_DMA_BASE DMA0
#define AFE_SPI_MASTER_DMA_TX_CHANNEL 		(3U)
#define AFE_SPI_MASTER_DMA_RX_CHANNEL 		(2U)
#define DRDY_EDMA_TRIGGER_Tx_CHANNEL      	(0U)

#ifdef DUMP_AFE_SAMPES_CH_ID
uint32_t dump_afe_ui_buff[2][95];
int dump_afe_ui_buff_index = 0;
int dump_afe_ui_buff_count = 0;
#endif

enum _afe_params_t
{
	AFE_U1_INDEX = 0,	/*!< Raw U1 voltage sample index */
	AFE_U2_INDEX,	/*!< Raw U2 voltage sample index */
	AFE_U3_INDEX,	/*!< Raw U3 voltage sample index */
	AFE_I1_INDEX,	/*!< Raw I1 voltage sample index */
	AFE_I2_INDEX,	/*!< Raw I2 voltage sample index */
	AFE_I3_INDEX,	/*!< Raw I3 voltage sample index */
	AFE_PARAMETER_COUNT
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void AFE_SPIRxDMACallback(struct _dma_handle *handle, void *userData, bool transferDone, uint32_t intmode);
static status_t AFE_SPIMasterTransferDMACustom(SPI_Type *base, spi_dma_handle_t *handle, spi_transfer_t *xfer);
static void AFE_SPIMasterInit(void);
static void AFE_SPIMasterDMASetup(void);
static void AFE_SPIMasterStartDMATransfer(void);
static void AFE_GetCRC(uint8_t *data, uint8_t datasize, uint32_t *checksum);
static void AFE_DeInterleaveDMABuffer(void);
static void AFE_SPIDMAInit(void);
static void CRC_ModuleInit();
//static void AFE_DMA_Callback_trigger(edma_handle_t *handle, void *userData, bool transferDone, uint32_t tcds);

/*******************************************************************************
 * Variables
 ******************************************************************************/

DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Rx_data_descriptor, 2);
DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Rx_data_crc_descriptor, 2);
DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Rx_command_descriptor, 2);
DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Tx_data_descriptor, 2);
DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Tx_data_crc_descriptor, 2);
DMA_ALLOCATE_LINK_DESCRIPTORS(spi_Tx_command_descriptor, 2);

volatile bool isTransferCompleted = false;
AT_NONCACHEABLE_SECTION_ALIGN_INIT(uint16_t masterRxData[BUFFER_SIZE],32) = {0};
AT_NONCACHEABLE_SECTION_ALIGN_INIT(uint16_t masterTxData[BUFFER_SIZE],32) = {0};

uint32_t *spiTxData = (uint32_t*)&masterTxData[0];

dma_handle_t lpspiDMAMasterRxRegToRxDataHandle;
dma_handle_t lpspiDMAMasterTxDataToTxRegHandle;

spi_dma_handle_t masterHandle;

uint16_t masterRxCommand[2] = {0};
uint16_t masterRxCRC[2] = {0};

uint32_t CRC_err_count = 0;

bool g_Transfer_Done = false ;
//uint32_t trigg_data = DMA_CH_CSR_ERQ(1);
int data_ready_count = 0 ;
uint32_t buf_samples = AFE_INP_SAMPLES;
int32_t AFE_data[AFE_PARAMETER_COUNT] = {};

int32_t AFE_data_copy[AFE_PARAMETER_COUNT] = {};
uint32_t crc_copy = 0;
uint32_t crc_copy_calculated = 0;
/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief This function is a callback for DMA-Rx channel. It will call the AFE_DeInterleaveDMABuffer()
 *
 */
static void AFE_SPIRxDMACallback(struct _dma_handle *handle, void *userData, bool transferDone, uint32_t intmode)
{
	uint32_t crc_val_calculated;
	uint32_t crc_val_received;
	 
	if(transferDone)
	{
		isTransferCompleted = true;

		/* AFE_DeInterleaveDMABuffer here */
		/* 24 bytes = 12 packets of 2 bytes = 16 bit Higher Nibble then Lower nibble */
		/* store samples*/
		AFE_data[AFE_U1_INDEX] = (masterRxData[2*AFE_U1_INDEX]<<16)|(masterRxData[2*AFE_U1_INDEX + 1]); /* U1 0:1 */
		AFE_data[AFE_U2_INDEX] = (masterRxData[2*AFE_U2_INDEX]<<16)|(masterRxData[2*AFE_U2_INDEX + 1]); /* U2 2:3 */
		AFE_data[AFE_U3_INDEX] = (masterRxData[2*AFE_U3_INDEX]<<16)|(masterRxData[2*AFE_U3_INDEX + 1]); /* U3 4:5 */
		AFE_data[AFE_I1_INDEX] = (masterRxData[2*AFE_I1_INDEX]<<16)|(masterRxData[2*AFE_I1_INDEX + 1]); /* I1 6:7 */
		AFE_data[AFE_I2_INDEX] = (masterRxData[2*AFE_I2_INDEX]<<16)|(masterRxData[2*AFE_I2_INDEX + 1]); /* I2 8:9 */
		AFE_data[AFE_I3_INDEX] = (masterRxData[2*AFE_I3_INDEX]<<16)|(masterRxData[2*AFE_I3_INDEX + 1]); /* I3 10:11 */

		AFE_GetCRC((uint8_t *)AFE_data, sizeof(AFE_data), &crc_val_calculated);
		crc_val_received = masterRxCRC[0]<<16 | masterRxCRC[1];

		if(crc_val_calculated != crc_val_received)
		{
			CRC_err_count++;

			/* In case of CRC error in a packet, clear the buffer to drop the particular cycle */
			Metering_SampleReset();
		}
		else
		{
#ifdef DUMP_AFE_SAMPES_CH_ID
			dump_afe_ui_buff[dump_afe_ui_buff_index][dump_afe_ui_buff_count++] = AFE_data[DUMP_AFE_SAMPES_CH_ID];

			if(dump_afe_ui_buff_count == AFE_INP_SAMPLES)
			{
				dump_afe_ui_buff_count = 0;
				dump_afe_ui_buff_index ^= 1;
			}
#endif

			uint32_t current_sample = Metering_SampleAdd(AFE_data[AFE_U1_INDEX], AFE_data[AFE_U2_INDEX], AFE_data[AFE_U3_INDEX],
					AFE_data[AFE_I1_INDEX], AFE_data[AFE_I2_INDEX], AFE_data[AFE_I3_INDEX]);

			if(current_sample == buf_samples)
			{
				Metering_SampleFinish();
				/* Invoke metering process */
				Metering_StartProcessing();
				/* round off to nearest integer */
			//buf_samples = (double)(SAMPLE_RATE / (frequency/10.0)) + 0.5;
			}
		}
	}
}

static void AFE_SPIMasterDMAConfigureTrigger(void)
{
	/* Connect PINT2 to SPI master DMA channel request */
	/* Connect trigger sources to PINT */
	INPUTMUX_Init(INPUTMUX);

	INPUTMUX_AttachSignal(INPUTMUX, kPINT_PinInt2, kINPUTMUX_GpioPort1Pin18ToPintsel);

	PINT_Init(PINT);
	/* Setup Pin Interrupt 2 for rising edge */
	PINT_PinInterruptConfig(PINT, kPINT_PinInt2, kPINT_PinIntEnableRiseEdge);

	PINT_EnableCallbackByIndex(PINT, kPINT_PinInt2);

	/* Attach PINT output to DMA request trigger */
	INPUTMUX_AttachSignal(INPUTMUX, AFE_SPI_MASTER_DMA_TX_CHANNEL, kINPUTMUX_NsGpioPint2ToDma0);

	/* enabling hardware trigger */
	dma_channel_trigger_t trigger;
	trigger.type  = kDMA_RisingEdgeTrigger;
	trigger.burst = kDMA_SingleTransfer;
	trigger.wrap  = kDMA_NoWrap;
	DMA_ConfigureChannelTrigger(AFE_SPI_MASTER_DMA_BASE,  AFE_SPI_MASTER_DMA_TX_CHANNEL, &trigger);

}
/*!
 * @brief This function will set up SPI module.
 *
 */
static void AFE_SPIMasterInit(void)
{
    /* SPI init */
    uint32_t srcClock_Hz = 0U;
    spi_master_config_t masterConfig;

    CLOCK_EnableClock(AFE_SPI_MASTER_CLK(AFE_SPI_BASE_INDEX));
    srcClock_Hz = AFE_SPI_MASTER_CLK_FREQ;

    SPI_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = AFE_SPI_BAUDRATE;
    masterConfig.sselNum = (spi_ssel_t)AFE_SPI_MASTER_PCS;
    masterConfig.dataWidth    = kSPI_Data16Bits;
    /* Targeting 300 ns */
    masterConfig.delayConfig.preDelay = 3;
    masterConfig.delayConfig.transferDelay = 3;
    if (SPI_MasterInit(AFE_SPI_MASTER_BASEADDR, &masterConfig, srcClock_Hz) != kStatus_Success)
    {
    	while(1);
    }
}
/*!
 * @brief This function will set up DMA module.
 *
 */
static void AFE_SPIMasterDMASetup(void)
{
    /* DMA init */
    DMA_Init(AFE_SPI_MASTER_DMA_BASE);
    /* Configure the DMA channel,priority and handle. */
    DMA_EnableChannel(AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_TX_CHANNEL);
    DMA_EnableChannel(AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_RX_CHANNEL);

    DMA_SetChannelPriority(AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_TX_CHANNEL, kDMA_ChannelPriority3);
    DMA_SetChannelPriority(AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_RX_CHANNEL, kDMA_ChannelPriority2);

    DMA_CreateHandle(&lpspiDMAMasterTxDataToTxRegHandle, AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_TX_CHANNEL);
    DMA_CreateHandle(&lpspiDMAMasterRxRegToRxDataHandle, AFE_SPI_MASTER_DMA_BASE, AFE_SPI_MASTER_DMA_RX_CHANNEL);

	/* sending 0 while getting data and its CRC */
    spiTxData[0] = (kSPI_FrameDelay | SPI_FIFOWR_LEN(kSPI_Data16Bits) | kSPI_FrameAssert) & SPI_CTRLMASK;
	/* sending dummy command */
    spiTxData[1] =  (0b0101100000000110 | spiTxData[0]);

    AFE_SPIMasterDMAConfigureTrigger();

	/* Set up handle for spi master */
	SPI_MasterTransferCreateHandleDMA(AFE_SPI_MASTER_BASEADDR, &masterHandle, NULL, NULL, &lpspiDMAMasterTxDataToTxRegHandle,
			&lpspiDMAMasterRxRegToRxDataHandle);

    DMA_SetCallback(&lpspiDMAMasterRxRegToRxDataHandle, AFE_SPIRxDMACallback, NULL);
    NVIC_SetPriority(DMA0_IRQn, 3);
}


/*!
 * @brief This function calculates CRC of data payload using MCU internal peripheral.
 *
 */
static void AFE_GetCRC(uint8_t *data, uint8_t datasize, uint32_t *checksum)
{
	/* ***************
	 * CRC-16/CCITT-FALSE *
	 *************** */
	*checksum = 0;
	CRC_WriteSeed(CRC_ENGINE, 0xFFFFU);
	CRC_WriteData(CRC_ENGINE, data, datasize);
	*checksum = CRC_Get16bitResult(CRC_ENGINE);
}

/*!
 * @brief This function will create DMA handles and set callback for DMA Rx.
 *
 */
static void AFE_SPIMasterStartDMATransfer(void)
{
	spi_transfer_t masterXfer;

	/* Start master transfer */
	masterXfer.dataSize    = sizeof(masterTxData[0]) * (BUFFER_SIZE);
	masterXfer.configFlags = kSPI_FrameAssert;

	masterXfer.txData      = (uint8_t *)&(masterTxData[0]);
	masterXfer.rxData      = (uint8_t *)&(masterRxData[0]);

	AFE_SPIMasterTransferDMACustom(AFE_SPI_MASTER_BASEADDR, &masterHandle, &masterXfer);
}

/*!
 * @brief This function will help to configure DMA Rx and Tx channels for data transfer.
 *
 */
static status_t AFE_SPIMasterTransferDMACustom(SPI_Type *base, spi_dma_handle_t *handle, spi_transfer_t *xfer)
{

	assert(!((NULL == handle) || (NULL == xfer)));
	uint32_t xfer_cfg = 0 ;
	spi_config_t *spi_config_p;
	uint32_t address;
	void *nextDesc                   = NULL;
	uint32_t firstTimeSize           = 0;
	spi_config_p                     = (spi_config_t *)SPI_GetConfig(base);

	uint8_t bytesPerFrame = (uint8_t)((spi_config_p->dataWidth > kSPI_Data8Bits) ? (sizeof(uint16_t)) : (sizeof(uint8_t)));
	handle->bytesPerFrame = bytesPerFrame;
	uint8_t lastwordBytes = 0U;

	if ((xfer->configFlags & (uint32_t)kSPI_FrameAssert) != 0U)
	{
		handle->lastwordBytes = bytesPerFrame;
		lastwordBytes         = bytesPerFrame;
	}
	else
	{
		handle->lastwordBytes = 0U;
		lastwordBytes         = 0U;
	}

	if ((NULL == handle) || (NULL == xfer))
	{
		return kStatus_InvalidArgument;
	}

	/* Byte size is zero. */
	if ((xfer->dataSize == 0U) || (xfer->rxData == NULL) || (xfer->txData == NULL))
	{
		return kStatus_InvalidArgument;
	}

	/* Clear FIFOs before transfer. */
	base->FIFOCFG |= SPI_FIFOCFG_EMPTYTX_MASK | SPI_FIFOCFG_EMPTYRX_MASK;
	base->FIFOSTAT |= SPI_FIFOSTAT_TXERR_MASK | SPI_FIFOSTAT_RXERR_MASK;

	handle->transferSize = xfer->dataSize;

	/* =============== Setup RCV DMA descriptors ======================*/

	SPI_EnableRxDMA(base, true);

	address = (uint32_t)&base->FIFORD;
	handle->rxRemainingBytes = xfer->dataSize;

	/* Descriptor configuration for receiving response while sending command */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                        /* Reload link descriptor after current exhaust, */
										false,                                       /* Clear trigger status. */
										false,                                       /* Not enable interruptA. */
										false,                                       /* Not enable interruptB. */
										sizeof(uint16_t),                            /* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               /* Dma source address no interleave  */
										kDMA_AddressInterleave1xWidth,               /* Dma destination address no interleave  */
										sizeof(uint16_t) * (COMMAND_SIZE)            /* Dma transfer byte. */
	);
	/* Descriptor for receiving response while sending command */
	/* This response is meaningless in SigBrd context,
	 * as it's a response to the command sent to AFE just to notify the AFE to start data transmission */
	DMA_SetupDescriptor(&spi_Rx_command_descriptor[0], xfer_cfg, (uint32_t *)address,
			masterRxCommand, &spi_Rx_data_descriptor[0]);

	/* Descriptor configuration for receiving data */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                        /* Reload link descriptor after current exhaust, */
										false,                                       /* Clear trigger status. */
										false,                                       /* Not enable interruptA. */
										false,                                       /* Not enable interruptB. */
										sizeof(uint16_t),                            /* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               /* Dma source address no interleave  */
										kDMA_AddressInterleave1xWidth,               /* Dma destination address no interleave  */
										sizeof(uint16_t) * (TRANSFER_SIZE)           /* Dma transfer byte. */
	);
	/* Descriptor for receiving data (AFE samples) */
	DMA_SetupDescriptor(&spi_Rx_data_descriptor[0], xfer_cfg, (uint32_t *)address,
			xfer->rxData, &spi_Rx_data_crc_descriptor[0]);

	/* Descriptor configuration for receiving CRC */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                        /* Reload link descriptor after current exhaust, */
										false,                                       /* Clear trigger status. */
										true,                                        /* Enable interruptA. */
										false,                                       /* Not enable interruptB. */
										sizeof(uint16_t),                            /* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               /* Dma source address no interleave  */
										kDMA_AddressInterleave1xWidth,               /* Dma destination address no interleave  */
										sizeof(uint16_t) * (CRC_SIZE)        		 /* Dma transfer byte. */
	);
	/* Descriptor for receiving CRC of Data */
	DMA_SetupDescriptor(&spi_Rx_data_crc_descriptor[0], xfer_cfg, (uint32_t *)address,
			masterRxCRC, &spi_Rx_command_descriptor[0]);

	DMA_EnableChannelPeriphRq(handle->rxHandle->base, handle->rxHandle->channel);

	handle->rxNextData = xfer->rxData + firstTimeSize;

	DMA_SubmitChannelDescriptor(handle->rxHandle, &spi_Rx_command_descriptor[0]);
	handle->rxInProgress = true;
	DMA_StartTransfer(handle->rxHandle);

	/* =============== Setup Transmit DMA descriptors ======================*/

	SPI_EnableTxDMA(base, true);
	address = (uint32_t)&base->FIFOWR;

	/* Descriptor configuration for command */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                        /* Reload link descriptor after current exhaust, */
										false,                                       /* Clear trigger status. */
										false,                                       /* Not enable interruptA. */
										false,                                       /* Not enable interruptB. */
										sizeof(uint32_t),                            /* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               /* Dma source address no interleave  */
										kDMA_AddressInterleave0xWidth,               /* Dma destination address no interleave  */
										sizeof(uint32_t) * (COMMAND_SIZE)            /* Dma transfer byte. */
	);
	/* Descriptor for command  */
	DMA_SetupDescriptor(&spi_Tx_command_descriptor[0], xfer_cfg, &spiTxData[1],
						(uint32_t *)address, &spi_Tx_data_descriptor[0]);


	/* Descriptor configuration for transfer of dummy data to receive AFE samples */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                      			/* Reload link descriptor after current exhaust, */
										false,                                      		/* Clear trigger status. */
										false,                                     		  	/* Not enable interruptA. */
										false,                                       		/* Not enable interruptB. */
										sizeof(uint32_t),                            		/* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               		/* Dma source address no interleave  */
										kDMA_AddressInterleave0xWidth,               		/* Dma destination address no interleave  */
										sizeof(uint32_t) * ((TRANSFER_SIZE))     			/* Dma transfer byte. */
										/* data except first 4 byte (1 sample), those will come with command's CRC */
	);
	/* Descriptor for transfer of dummy data to receive AFE samples */
	DMA_SetupDescriptor(&spi_Tx_data_descriptor[0], xfer_cfg, &spiTxData[1],
			(uint32_t *)address, &spi_Tx_data_crc_descriptor[0]);

	/* Descriptor configuration for data's CRC */
	xfer_cfg = DMA_CHANNEL_XFER(		true,                                        /* Reload link descriptor after current exhaust, */
										true,                                        /* Clear trigger status. */
										false,                                       /* Not enable interruptA. */
										false,                                       /* Not enable interruptB. */
										sizeof(uint32_t),                            /* Dma transfer width. */
										kDMA_AddressInterleave0xWidth,               /* Dma source address no interleave  */
										kDMA_AddressInterleave0xWidth,               /* Dma destination address no interleave  */
										sizeof(uint32_t) * (CRC_SIZE)                /* Dma transfer byte. */
	);
	/* Descriptor for data's CRC  */
	DMA_SetupDescriptor(&spi_Tx_data_crc_descriptor[0], xfer_cfg, &spiTxData[1],
												(uint32_t *)address, &spi_Tx_command_descriptor[0]);

	DMA_EnableChannelPeriphRq(handle->txHandle->base, handle->txHandle->channel);

	DMA_SubmitChannelDescriptor(handle->txHandle, &spi_Tx_command_descriptor[0]);

	DMA_StartTransfer(handle->txHandle);

	return kStatus_Success;
}

/*!
 * @brief This function will initialize DMA and SPI peripherals.
 *
 */
static void AFE_SPIDMAInit()
{
	CLOCK_EnableClock(kCLOCK_Dma0);
	/* Initialize SPI master with configuration. */
	AFE_SPIMasterInit();

	/* Set up DMA for SPI master TX and RX channel. */
	AFE_SPIMasterDMASetup();

	AFE_SPIMasterStartDMATransfer();
}

/*!
 * @brief Init for CRC-16-CCITT.
 * @details Init CRC peripheral module for CRC-16/CCITT-FALSE protocol:
 *          width=16 poly=0x1021 init=0xffff refin=false refout=false xorout=0x0000 check=0x29b1
 *          http://reveng.sourceforge.net/crc-catalogue/
 * name="CRC-16/CCITT-FALSE"
 * Beware the CRC algo must match the AFE CRC algo and the flash algo
 */
static void CRC_ModuleInit()
{
    crc_config_t config;
    CLOCK_EnableClock(kCLOCK_Crc);

    /*
     * config.polynomial = kCRC_Polynomial_CRC_CCITT;
     * config.reverseIn = false;
     * config.complementIn = false;
     * config.reverseOut = false;
     * config.complementOut = false;
     * config.seed = 0xFFFFU;
     */
    CRC_GetDefaultConfig(&config);
    CRC_Init(CRC_ENGINE, &config);
}

/*!
 * @brief Initialize SPI and DMA channels for external AFE interface.
 *
 * This function can be used to initialize the SPI master and DMA channels
 * to drive and get data from external AFE SPI slave.
 */
void AFE_Init()
{
	/*!< Set up clock selectors  */
	CLOCK_AttachClk(kMAIN_PLL_to_CLKOUT);
	CLOCK_SetClkDiv(kCLOCK_DivClockOut, 40U);

	CLOCK_AttachClk(kFFRO_to_FLEXCOMM1);
	CLOCK_SetClkDiv(kCLOCK_Flexcomm1, 1U);

	/* setup the crc module used by External AFE and Flash Saftey check */
	CRC_ModuleInit();

	AFE_SPIDMAInit();

	while (!isTransferCompleted)
	{
		vTaskDelay(5);
	}
}

/*!
 * @brief Initialize SPI and DMA channels for external AFE interface.
 *
 * This function can be used to initialize the SPI master and DMA channels
 * to drive and get data from external AFE SPI slave.
 */
void AFE_DeInit(void)
{
	//SPI_Deinit(AFE_LPSPI_MASTER_BASEADDR);
}



