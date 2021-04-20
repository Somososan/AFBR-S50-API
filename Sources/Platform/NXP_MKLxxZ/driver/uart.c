/*************************************************************************//**
 * @file
 * @brief    	This file is part of the AFBR-S50 API.
 * @details		This file provides UART driver functionality.
 * 
 * @copyright
 * 
 * Copyright (c) 2021, Broadcom Inc
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*******************************************************************************
 * Include Files
 ******************************************************************************/
#include "uart.h"

#include "board/board_config.h"
#include "driver/dma.h"
#include "driver/irq.h"
#include "driver/fsl_clock.h"
#include "driver/fsl_port.h"
#include "utility/debug_console.h"

#include <stdarg.h>
#include <stdlib.h>


/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define UART 					UART_BASEADDR	/*!< Alias for UART base address. */

//#define DMA_CHANNEL_UART_TX 	2U				/*!< DMA channel for transmitting. */
#define DEBUGCONSOLE_BUFFERSIZE 1024U			/*!< Output buffer size for debug console. */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
#if defined (BRIDGE)
static status_t SetBaudRate2(uint32_t baudRate_Bps, uint32_t srcClock_Hz);
static void TxDMACallbackFunction2(status_t status);
#endif
static status_t SetBaudRate(uint32_t baudRate_Bps, uint32_t srcClock_Hz);
static void TxDMACallbackFunction(status_t status);
static int PutChar(void * buf, int a);
status_t print(const char  *fmt_s, ...);

/*!***************************************************************************
 * @brief	A vprintf() function for the uart connection.
 * @details	This function is similar to #UART_Printf except that, instead of
 * 			taking a variable number of arguments directly, it takes an
 * 			argument list pointer ap. Requires a call to #va_start(ap, fmt_s);
 * 			before and va_end(ap); after this function.
 * @param	fmt_s The printf() format string.
 * @param	ap The argument list.
 * @return 	Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
static inline status_t vprint(const char  *fmt_s, va_list * ap);

/*******************************************************************************
 * Variables
 ******************************************************************************/
#if defined (BRIDGE)
static bool isInitialized2 = false;
static uart_error_callback_t myErrorCallback2 = 0;
static uart_rx_callback_t myRxCallback2 = 0;
static uart_tx_callback_t myTxCallback2 = 0;
static void * myTxCallbackState2 = 0;
static volatile bool isTxOnGoing2 = false;
#endif

static bool isInitialized = false;

static uart_error_callback_t myErrorCallback = 0;
static uart_rx_callback_t myRxCallback = 0;
static uart_tx_callback_t myTxCallback = 0;
static void * myTxCallbackState = 0;

static volatile bool isTxOnGoing = false;
static uint8_t myDebugConsole_Buffer[DEBUGCONSOLE_BUFFERSIZE] = {0};
static uint8_t * myDebugConsole_WritePtr = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/


#if defined (BRIDGE)
status_t UART2_Init(void)
{
	status_t status = STATUS_OK;

	if (!isInitialized2)
	{
		/*****************************************
		 * Initialize pins
		 *****************************************/
		CLOCK_EnableClock(kCLOCK_PortE); 			/* Ungate the port clock */
		PORT_SetPinMux(PORTE, 16u, kPORT_MuxAlt3); 	/* PORTE16 */
		PORT_SetPinMux(PORTE, 17u, kPORT_MuxAlt3); 	/* PORTE17 */


		/*****************************************
		 * Setup DMA module
		 *****************************************/
		DMA_Init();

		/* Request DMA channel for TX/RX */
		DMA_ClaimChannel(DMA_CHANNEL_UART2_TX, DMA_REQUEST_MUX_UART2_TX);

		/* Register callback for DMA interrupt */
		DMA_SetTransferDoneCallback(DMA_CHANNEL_UART2_TX, TxDMACallbackFunction2);

		/* Set up this channel's control which includes enabling the DMA interrupt */
		DMA_ConfigTransfer(DMA_CHANNEL_UART2_TX, 1, DMA_MEMORY_TO_PERIPHERAL, 0, (uint32_t) (&UART2->D), 0); /* dest is data register */


		/*****************************************
		 * Setup hardware module
		 *****************************************/
		CLOCK_EnableClock(kCLOCK_Uart2);

		/* Disable TX RX before setting. */
		UART2->C2 &= (uint8_t) (~(UART_C2_TE_MASK | UART_C2_RE_MASK));

		status = SetBaudRate2(UART2_BAUDRATE, CLOCK_GetFreq(BUS_CLK));
		if (status != STATUS_OK) return status;


		/* disable parity mode */
		UART2->C1 &= (uint8_t) (~(UART_C1_PE_MASK | UART_C1_PT_MASK | UART_C1_M_MASK));

		/* set one stop bit per char */
		UART2->BDH &= (uint8_t) (~UART_BDH_SBNS_MASK);
		UART2->BDH |= UART_BDH_SBNS(0U);

		/* enable error interrupts */
		UART2->C3 |= UART_C3_ORIE_MASK; /* Overrun IRQ enable. */
		UART2->C3 |= UART_C3_NEIE_MASK; /* Noise Error IRQ enable. */
		//		UART->C3 |= UART0_C3_FEIE_MASK; /* Framing Error enable. */

		/* enable rx interrupt */
		UART2->C2 |= UART_C2_RIE_MASK;

		/* Enable the LPSCI TX DMA Request */
		UART2->C4 |= UART_C4_TDMAS_MASK;
		UART2->C2 |= UART_C2_TIE_MASK;

		/* Enable TX/RX. */
		UART2->C2 |= UART_C2_TE_MASK | UART_C2_RE_MASK;

		/* Enable interrupt in NVIC. */
		NVIC_SetPriority(UART2_IRQn, IRQPRIO_UART2);

		isInitialized2 = true;
	}
	return status;
}

static status_t SetBaudRate2(uint32_t baudRate_Bps, uint32_t srcClock_Hz)
{
	uint16_t sbr = 0;
	uint32_t baudDiff = 0;

	/* Calculate the baud rate modulo divisor, sbr*/
	sbr = srcClock_Hz / (baudRate_Bps * 16);
	/* set sbrTemp to 1 if the sourceClockInHz can not satisfy the desired baud rate */
	if (sbr == 0) {
		sbr = 1;
	}

	/* Calculate the baud rate based on the temporary SBR values */
	baudDiff = (srcClock_Hz / (sbr * 16)) - baudRate_Bps;

	/* Select the better value between sbr and (sbr + 1) */
	if (baudDiff > (baudRate_Bps - (srcClock_Hz / (16 * (sbr + 1))))) {
		baudDiff = baudRate_Bps - (srcClock_Hz / (16 * (sbr + 1)));
		sbr++;
	}

	/* next, check to see if actual baud rate is within 3% of desired baud rate
	 * based on the calculate SBR value */
	if (baudDiff > ((baudRate_Bps / 100) * 3)) {
		/* Unacceptable baud rate difference of more than 3%*/
		return ERROR_UART_BAUDRATE_NOT_SUPPORTED;
	}

	/* Write the sbr value to the BDH and BDL registers*/
	UART2->BDH = (UART2->BDH & ~UART_BDH_SBR_MASK) | (uint8_t) (sbr >> 8);
	UART2->BDL = (uint8_t) sbr;

	 return STATUS_OK;
}

#endif


status_t UART_Init(void)
{
	status_t status = STATUS_OK;

	if (!isInitialized)
	{
		/*****************************************
		 * Initialize pins
		 *****************************************/
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)
		CLOCK_EnableClock(kCLOCK_PortA); 			/* Ungate the port clock */
		PORT_SetPinMux(PORTA, 1u, kPORT_MuxAlt2); 	/* PORTA_PCR1 */
		PORT_SetPinMux(PORTA, 2u, kPORT_MuxAlt2); 	/* PORTA_PCR2 */
#elif defined (CPU_MKL17Z256VFM4)
		CLOCK_EnableClock(kCLOCK_PortA); 			/* Ungate the port clock */
		PORT_SetPinMux(PORTA, 1u, kPORT_MuxAlt2); 	/* PORTA_PCR1 */
		PORT_SetPinMux(PORTA, 2u, kPORT_MuxAlt2); 	/* PORTA_PCR2 */
#endif

		/*****************************************
		 * Setup DMA module
		 *****************************************/
		DMA_Init();

		/* Request DMA channel for TX/RX */
		DMA_ClaimChannel(DMA_CHANNEL_UART_TX, DMA_REQUEST_MUX_UART_TX);

		/* Register callback for DMA interrupt */
		DMA_SetTransferDoneCallback(DMA_CHANNEL_UART_TX, TxDMACallbackFunction);

		/* Set up this channel's control which includes enabling the DMA interrupt */
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)
		DMA_ConfigTransfer(DMA_CHANNEL_UART_TX, 1, DMA_MEMORY_TO_PERIPHERAL, 0, (uint32_t) (&UART->D), 0); /* dest is data register */
#elif defined (CPU_MKL17Z256VFM4)
		DMA_ConfigTransfer(DMA_CHANNEL_UART_TX, 1, DMA_MEMORY_TO_PERIPHERAL, 0, (uint32_t) (&UART->DATA), 0); /* dest is data register */
#endif

		/*****************************************
		 * Setup hardware module
		 *****************************************/
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)
		CLOCK_SetLpsci0Clock(0x1U);
		CLOCK_EnableClock(kCLOCK_Uart0);

		/* Disable TX RX before setting. */
		UART->C2 &= (uint8_t) (~(UART_C2_TE_MASK | UART0_C2_RE_MASK));

#elif defined (CPU_MKL17Z256VFM4)

		CLOCK_SetLpuart0Clock(0x1U);
		CLOCK_EnableClock(kCLOCK_Lpuart0);

		/* Disable LPUART TX RX before setting. */
		UART->CTRL &= ~(LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK);
#endif

		status = SetBaudRate(UART_BAUDRATE, CLOCK_GetFreq(UART_CLKSRC));
		if (status != STATUS_OK) return status;

#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)

		/* disable parity mode */
		UART->C1 &= (uint8_t) (~(UART_C1_PE_MASK | UART_C1_PT_MASK | UART_C1_M_MASK));

		/* set one stop bit per char */
		UART->BDH &= (uint8_t) (~UART0_BDH_SBNS_MASK);
		UART->BDH |= UART0_BDH_SBNS(0U);

		/* enable error interrupts */
		UART->C3 |= UART0_C3_ORIE_MASK; /* Overrun IRQ enable. */
		UART->C3 |= UART0_C3_NEIE_MASK; /* Noise Error IRQ enable. */
		//		UART->C3 |= UART0_C3_FEIE_MASK; /* Framing Error enable. */

		/* enable rx interrupt */
		UART->C2 |= UART0_C2_RIE_MASK;

		/* Enable the LPSCI TX DMA Request */
		UART->C5 |= UART0_C5_TDMAE_MASK;

		/* Enable TX/RX. */
		UART->C2 |= UART0_C2_TE_MASK | UART0_C2_RE_MASK;

		/* Enable interrupt in NVIC. */
		NVIC_SetPriority(UART0_IRQn, IRQPRIO_UART0);

#elif defined (CPU_MKL17Z256VFM4)

		/* disable parity mode */
		UART->CTRL &= (uint8_t)(~(LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK));

		/* set one stop bit per char */
		UART->BAUD &= (~LPUART_BAUD_SBNS_MASK);
		UART->BAUD |= LPUART_BAUD_SBNS(0U);

		/* enable error interrupts */
		UART->CTRL |= LPUART_CTRL_ORIE_MASK; /* Overrun IRQ enable. */
		UART->CTRL |= LPUART_CTRL_NEIE_MASK; /* Noise Error IRQ enable. */
		//		UART->C3 |= UART0_C3_FEIE_MASK; /* Framing Error enable. */
		/* enable rx interrupt */
		UART->CTRL |= LPUART_CTRL_RIE_MASK;

		/* Enable the LPSCI TX DMA Request */
		UART->BAUD |=LPUART_BAUD_TDMAE_MASK;

		/* enable Break Detect interrupt */
		//UART->BAUD |= LPUART_BAUD_LBKDIE_MASK;
		/* enable RX input Active Edge Interrupt Enable */
		//UART->BAUD |= LPUART_BAUD_RXEDGIE_MASK;
		/* Enable TX/RX. */
		UART->CTRL |= LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK;

		/* Enable interrupt in NVIC. */
		NVIC_SetPriority(LPUART0_IRQn, LPUART0_IRQn);
#endif

		isInitialized = true;
	}
	return status;
}


static status_t SetBaudRate(uint32_t baudRate_Bps, uint32_t srcClock_Hz)
{
	/* This LPSCI instantiation uses a slightly different baud rate calculation
	 * The idea is to use the best OSR (over-sampling rate) possible
	 * Note, OSR is typically hard-set to 16 in other LPSCI instantiations
	 * loop to find the best OSR value possible, one that generates minimum baudDiff
	 * iterate through the rest of the supported values of OSR */

	uint16_t sbr = 0;
	uint32_t osr = 0;
	uint32_t baudDiff = baudRate_Bps;

	for (uint32_t osrTemp = 4; osrTemp <= 32; osrTemp++)
	{
		/* calculate the temporary sbr value   */
		uint16_t sbrTemp = (uint16_t) (srcClock_Hz / (baudRate_Bps * osrTemp));

		/* set sbrTemp to 1 if the sourceClockInHz can not satisfy the desired baud rate */
		if (sbrTemp == 0) sbrTemp = 1;

		/* Calculate the baud rate based on the temporary OSR and SBR values */
		uint32_t calculatedBaud = (srcClock_Hz / (osrTemp * sbrTemp));

		uint32_t tempDiff = calculatedBaud - baudRate_Bps;

		/* Select the better value between srb and (sbr + 1) */
		if (tempDiff > (baudRate_Bps - (srcClock_Hz / (osrTemp * (sbrTemp + 1U)))))
		{
			tempDiff = baudRate_Bps - (srcClock_Hz / (osrTemp * (sbrTemp + 1U)));
			sbrTemp++;
		}

		if (tempDiff <= baudDiff)
		{
			baudDiff = tempDiff;
			osr = osrTemp; /* update and store the best OSR value calculated*/
			sbr = sbrTemp; /* update store the best SBR value calculated*/
		}
	}

	/* next, check to see if actual baud rate is within 3% of desired baud rate
	 * based on the best calculate OSR value */
	if (baudDiff > ((baudRate_Bps / 100U) * 3U))
	{
		/* Unacceptable baud rate difference of more than 3%*/
		return ERROR_UART_BAUDRATE_NOT_SUPPORTED;
	}

#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)

	/* Acceptable baud rate */
	/* Check if OSR is between 4x and 7x oversampling */
	/* If so, then "BOTHEDGE" sampling must be turned on*/
	if ((osr > 3U) && (osr < 8U))
	{
		UART->C5 |= UART0_C5_BOTHEDGE_MASK;
	}

	/* program the osr value (bit value is one less than actual value)*/
	UART->C4 = (uint8_t) ((UART->C4 & ~UART0_C4_OSR_MASK) | (osr - 1));

	/* program the sbr (divider) value obtained above*/
	UART->BDH = (uint8_t) ((UART->C4 & ~UART0_BDH_SBR_MASK) | (uint8_t) (sbr >> 8));
	UART->BDL = (uint8_t) sbr;

#elif defined (CPU_MKL17Z256VFM4)

	uint32_t temp = UART->BAUD;

	/* Acceptable baud rate, check if OSR is between 4x and 7x oversampling.
	 * If so, then "BOTHEDGE" sampling must be turned on */
	if ((osr > 3) && (osr < 8))
	{
		temp |= LPUART_BAUD_BOTHEDGE_MASK;
	}

	/* program the osr value (bit value is one less than actual value) */
	temp &= ~LPUART_BAUD_OSR_MASK;
	temp |= LPUART_BAUD_OSR(osr - 1);

	/* write the sbr value to the BAUD registers */
	temp &= ~LPUART_BAUD_SBR_MASK;
	UART->BAUD = temp | LPUART_BAUD_SBR(sbr);

#endif

    return STATUS_OK;
}

#if defined (BRIDGE)

status_t UART2_SendBuffer(uint8_t const * txBuff, size_t txSize, uart_tx_callback_t f, void * state)
{
	/* Debug: only send log entries from threads with lower priority
	 * than the UART irq priority. */
//	assert(GET_IPSR() == 0 || GET_IPSR() > 15); // cannot send from exceptions!!

	/* Check that we're not busy.*/
	if(!isInitialized2) return ERROR_NOT_INITIALIZED;
	if(isTxOnGoing2) return STATUS_BUSY;

	/* Verify arguments. */
	if(!txBuff || !txSize) return ERROR_INVALID_ARGUMENT;

	/* Set Tx Busy Status. */
	isTxOnGoing2 = true;
	myTxCallback2 = f;
	myTxCallbackState2 = state;

	/* Set up this channel's control which includes enabling the DMA interrupt */
	DMA0->DMA[DMA_CHANNEL_UART2_TX].SAR = (uint32_t)txBuff; 	// set source address

	/* Set up this channel's control which includes enabling the DMA interrupt */
    DMA0->DMA[DMA_CHANNEL_UART2_TX].DSR_BCR = DMA_DSR_BCR_BCR(txSize); // set transfer count

	/* Enable the DMA peripheral request */
    DMA0->DMA[DMA_CHANNEL_UART2_TX].DCR |= DMA_DCR_ERQ_MASK;

	return STATUS_OK;
}

bool UART2_IsTxBusy(void)
{
	return isTxOnGoing2;
}


#endif

status_t UART_SendBuffer(uint8_t const * txBuff, size_t txSize, uart_tx_callback_t f, void * state)
{
	/* Debug: only send log entries from threads with lower priority
	 * than the UART irq priority. */
//	assert(GET_IPSR() == 0 || GET_IPSR() > 15); // cannot send from exceptions!!

	/* Check that we're not busy.*/
	if(!isInitialized) return ERROR_NOT_INITIALIZED;
	if(isTxOnGoing) return STATUS_BUSY;

	/* Verify arguments. */
	if(!txBuff || !txSize) return ERROR_INVALID_ARGUMENT;

	/* Set Tx Busy Status. */
	isTxOnGoing = true;
	myTxCallback = f;
	myTxCallbackState = state;

	/* Set up this channel's control which includes enabling the DMA interrupt */
	DMA0->DMA[DMA_CHANNEL_UART_TX].SAR = (uint32_t)txBuff; 	// set source address

	/* Set up this channel's control which includes enabling the DMA interrupt */
    DMA0->DMA[DMA_CHANNEL_UART_TX].DSR_BCR = DMA_DSR_BCR_BCR(txSize); // set transfer count

	/* Enable the DMA peripheral request */
    DMA0->DMA[DMA_CHANNEL_UART_TX].DCR |= DMA_DCR_ERQ_MASK;

	return STATUS_OK;
}

bool UART_IsTxBusy(void)
{
	return isTxOnGoing;
}

/*******************************************************************************
 * Debug Console Functions
 ******************************************************************************/

static int PutChar(void * buf, int a)
{
	(void) buf; // buf not used here.
	if (myDebugConsole_WritePtr < (myDebugConsole_Buffer + DEBUGCONSOLE_BUFFERSIZE))
	{
		*(myDebugConsole_WritePtr++) = (uint8_t) a;
	}
	return 0;
}

static inline status_t vprint(const char *fmt_s, va_list * ap)
{
	assert(ap != 0);
	if (!isInitialized) return ERROR_NOT_INITIALIZED;

    myDebugConsole_WritePtr = myDebugConsole_Buffer;
    PrintfFormattedData(&PutChar, 0, fmt_s, ap);

	if (*myDebugConsole_WritePtr != (uint8_t) '\n') PutChar(0, '\n');

    status_t status = UART_SendBuffer(myDebugConsole_Buffer, (size_t)(myDebugConsole_WritePtr - myDebugConsole_Buffer), 0, 0);
    while(UART_IsTxBusy());
    return status;
}

__attribute__((weak)) status_t print(const char *fmt_s, ...)
{
	if (!isInitialized) return ERROR_NOT_INITIALIZED;

	va_list ap;
	va_start(ap, fmt_s);
	status_t status = vprint(fmt_s, &ap);
	va_end(ap);

	return status;
}

/*******************************************************************************
 * IRQ handler
 ******************************************************************************/
#if defined(BRIDGE)
static void TxDMACallbackFunction2(status_t status)
{
	isTxOnGoing2 = false;

	if(status < STATUS_OK)
	{
		if(myErrorCallback2)
		{
			myErrorCallback2(status);
		}
	}

	if(myTxCallback2)
	{
		myTxCallback2(status, myTxCallbackState2);
	}
}
#endif

static void TxDMACallbackFunction(status_t status)
{
	isTxOnGoing = false;

	if (status < STATUS_OK)
	{
		if (myErrorCallback)
		{
			myErrorCallback(status);
		}
	}

	if (myTxCallback)
	{
		myTxCallback(status, myTxCallbackState);
	}
}



#if defined(BRIDGE)
void UART2_IRQHandler(void)
{
	/* Get status register. */
	uint8_t s1 = UART2->S1;

	/* Handle Rx Data Register Full interrupt */
	if (s1 & UART_S1_RDRF_MASK)
	{
		/* Get data and invoke callback. */
		myRxCallback2(UART2->D);
		return;
	}

//    /* Handle idle line detect interrupt */
//    if(s1 & UART_S1_IDLE_MASK)
//    {
//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
//    	UART->S1 |= UART_S1_IDLE_MASK;
//		if(myErrorCallback)
//		{
//			myErrorCallback(...);
//		}
//    }
// TODO implementation of overrun and noise of UART2
	/* Handle receive overrun interrupt */
//	if (s1 & UART_S1_OR_MASK)
//	{
//		/* Clear the flag, or the rxDataRegFull will not be set any more. */
//		UART->S1 |= UART_S1_OR_MASK;
//		if (myErrorCallback)
//		{
//			myErrorCallback(ERROR_UART_RX_OVERRUN);
//		}
//	}
//
//	/* Handle noise interrupt */
//	if (s1 & UART_S1_NF_MASK)
//	{
//		/* Clear the flag, or the rxDataRegFull will not be set any more. */
//		UART->S1 |= UART_S1_NF_MASK;
//		if (myErrorCallback)
//		{
//			myErrorCallback(ERROR_UART_RX_NOISE);
//		}
//	}
//
//	/* Handle framing error interrupt */
//	if (s1 & UART_S1_FE_MASK)
//	{
//		/* Clear the flag, or the rxDataRegFull will not be set any more. */
//		UART->S1 |= UART_S1_FE_MASK;
//		if (myErrorCallback)
//		{
//			myErrorCallback(ERROR_UART_FRAMING_ERR);
//		}
//	}

//    /* Handle parity error interrupt */
//    if(s1 & UART_S1_PF_MASK)
//    {
//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
//    	UART->S1 |= UART_S1_PF_MASK;
//		if(myErrorCallback)
//		{
//			myErrorCallback(...);
//		}
//    }
}
#endif

/* LPUART IRQ handler for Rx callback. */
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)

void UART0_IRQHandler(void)
{
	/* Get status register. */
	uint8_t s1 = UART->S1;

	/* Handle Rx Data Register Full interrupt */
	if (s1 & UART0_S1_RDRF_MASK)
	{
		/* Get data and invoke callback. */
		myRxCallback(UART->D);
		return;
	}

//    /* Handle idle line detect interrupt */
//    if(s1 & UART0_S1_IDLE_MASK)
//    {
//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
//    	UART->S1 |= UART0_S1_IDLE_MASK;
//		if(myErrorCallback)
//		{
//			myErrorCallback(...);
//		}
//    }

	/* Handle receive overrun interrupt */
	if (s1 & UART0_S1_OR_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->S1 |= UART0_S1_OR_MASK;
		if (myErrorCallback)
		{
			myErrorCallback(ERROR_UART_RX_OVERRUN);
		}
	}

	/* Handle noise interrupt */
	if (s1 & UART0_S1_NF_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->S1 |= UART0_S1_NF_MASK;
		if (myErrorCallback)
		{
			myErrorCallback(ERROR_UART_RX_NOISE);
		}
	}

	/* Handle framing error interrupt */
	if (s1 & UART0_S1_FE_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->S1 |= UART0_S1_FE_MASK;
		if (myErrorCallback)
		{
			myErrorCallback(ERROR_UART_FRAMING_ERR);
		}
	}

//    /* Handle parity error interrupt */
//    if(s1 & UART0_S1_PF_MASK)
//    {
//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
//    	UART->S1 |= UART0_S1_PF_MASK;
//		if(myErrorCallback)
//		{
//			myErrorCallback(...);
//		}
//    }
}

#elif defined (CPU_MKL17Z256VFM4)
void LPUART0_IRQHandler(void)
{
	/* Get status register. */
	uint32_t s1 = UART->STAT;

	/* Handle Rx Data Register Full interrupt */
	if(s1 & LPUART_STAT_RDRF_MASK)
	{
		/* Get data and invoke callback. */
		myRxCallback(UART->DATA);
		return;
	}

	//    /* Handle idle line detect interrupt */
	//    if(s1 & LPUART_STAT_IDLE_MASK)
	//    {
	//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
	//    	UART->S1 |= LPUART_STAT_IDLE_MASK;
	//		if(myErrorCallback)
	//		{
	//			myErrorCallback(...);
	//		}
	//    }

	/* Handle receive overrun interrupt */
	if(s1 & LPUART_STAT_OR_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->STAT |= LPUART_STAT_OR_MASK;
		if(myErrorCallback)
		{
			myErrorCallback(ERROR_UART_RX_OVERRUN);
		}
	}

	/* Handle noise interrupt */
	if(s1 & LPUART_STAT_NF_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->STAT |= LPUART_STAT_NF_MASK;
		if(myErrorCallback)
		{
			myErrorCallback(ERROR_UART_RX_NOISE);
		}
	}

	/* Handle framing error interrupt */
	if(s1 & LPUART_STAT_FE_MASK)
	{
		/* Clear the flag, or the rxDataRegFull will not be set any more. */
		UART->STAT |= LPUART_STAT_FE_MASK;
		if(myErrorCallback)
		{
			myErrorCallback(ERROR_UART_FRAMING_ERR);
		}
	}

	//    /* Handle parity error interrupt */
	//    if(s1 & UART0_S1_PF_MASK)
	//    {
	//        /* Clear the flag, or the rxDataRegFull will not be set any more. */
	//    	UART->S1 |= UART0_S1_PF_MASK;
	//    	assert(0);
	//    }
}
#endif

#if defined (BRIDGE)
void UART2_SetRxCallback(uart_rx_callback_t f)
{
	IRQ_LOCK();

	myRxCallback2 = f;

	if(f != 0)
	{
		EnableIRQ(UART2_IRQn);
	}
	else
	{
		DisableIRQ(UART2_IRQn);
	}

	IRQ_UNLOCK();
}

void UART2_RemoveRxCallback(void)
{
	UART2_SetRxCallback(0);
}
#endif

void UART_SetRxCallback(uart_rx_callback_t f)
{
	IRQ_LOCK();

	myRxCallback = f;

	if(f != 0)
	{
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)
		EnableIRQ(UART0_IRQn);
#elif defined (CPU_MKL17Z256VFM4)
		EnableIRQ(LPUART0_IRQn);
#endif
	}
	else
	{
#if defined(CPU_MKL46Z256VLH4) || defined(CPU_MKL46Z256VLL4) || defined(CPU_MKL46Z256VMC4) || defined(CPU_MKL46Z256VMP4)
		DisableIRQ(UART0_IRQn);
#elif defined (CPU_MKL17Z256VFM4)
		DisableIRQ(LPUART0_IRQn);
#endif
	}

	IRQ_UNLOCK();
}
void UART_RemoveRxCallback(void)
{
	UART_SetRxCallback(0);
}
void UART_SetErrorCallback(uart_error_callback_t f)
{
	myErrorCallback = f;
}
void UART_RemoveErrorCallback(void)
{
	UART_SetErrorCallback(0);
}