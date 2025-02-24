/*************************************************************************//**
 * @file
 * @brief       This file is part of the MKL46z/MKL17z platform layer.
 * @details     This file provides generic board abstraction.
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
 *****************************************************************************/

#ifndef BOARD_H
#define BOARD_H

/*!***************************************************************************
 * @defgroup    bsp Board Support Package
 * @ingroup     platform
 * @brief       Board Support Package
 * @details     Board/Platform Depended Definitions.
 * @addtogroup  bsp
 * @{
 *****************************************************************************/

#include "api/argus_status.h"

/*! Define the default SPI slave for device.
 *  The slave is used for SPI initialization only! */
#ifndef SPI_DEFAULT_SLAVE
#define SPI_DEFAULT_SLAVE 1
#endif


/*! Define the SPI baud rate (to be used in the SPI module).
 *  This is dependent of the available peripheral. */
#ifndef SPI_BAUD_RATE
#endif

/*! Define the maximum SPI baud rate (to be used in the SPI module).
 *  This is dependent of the available peripheral. */
#ifndef SPI_MAX_BAUDRATE
#define SPI_MAX_BAUDRATE 21000000
#endif

/*! Define the current SPI baud rate (to be used in the SPI module).
 *  This is dependent of the available peripheral. */
#ifndef SPI_BAUDRATE
#define SPI_BAUDRATE SPI_MAX_BAUDRATE
#endif


/*!***************************************************************************
 * @brief   Initializes the board and its peripherals.
 * @return  Returns the \link #status_t status\endlink (#STATUS_OK on success).
 *****************************************************************************/
status_t Board_Init(void);

/*!***************************************************************************
 * @brief   Enforce system reset!
 * @details Calls the NVIC reset function.
 *****************************************************************************/
void Board_Reset(void);

/*!***************************************************************************
 * @brief   Checks the reason of the latest system reset and does a printout
 *****************************************************************************/
void Board_CheckReset(void);

/*! @} */
#endif /* BOARD_H */
