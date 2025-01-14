/*************************************************************************//**
 * @file
 * @brief       This file is part of the AFBR-S50 Explorer Demo Application.
 * @details     This file contains the hardware API of the Explorer Application.
 *
 * @copyright
 *
 * Copyright (c) 2023, Broadcom Inc.
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

/*******************************************************************************
 * Include Files
 ******************************************************************************/

#include "core_device.h"
#include "core_cfg.h"
#include "core_flash.h"
#include "core_utils.h"
#include <assert.h>
#include "driver/s2pi.h"
#include "debug.h"
#include "explorer_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

static explorer_t explorerArray[EXPLORER_DEVICE_COUNT] = { 0 };
static explorer_t * explorerIDMap[S2PI_SLAVE_COUNT + 1] = { 0 };

/*******************************************************************************
 * Local Functions
 ******************************************************************************/

static status_t CheckConnectedDevice(uint8_t slave)
{
    status_t status = STATUS_OK;

    uint8_t data[17U] = {0};
    for (uint8_t i = 1; i < 17U; ++i) data[i] = i;

    for (uint8_t n = 0; n < 2; n++)
    {
        data[0] = 0x04;
        status = S2PI_TransferFrame(slave, data, data, 17U, 0, 0);
        if (status < STATUS_OK)
        {
            return status;
        }

        ltc_t start;
        Time_GetNow(&start);
        do
        {
            status = S2PI_GetStatus(slave);
            if (Time_CheckTimeoutMSec(&start, 100))
            {
                status = ERROR_TIMEOUT;
            }
        }
        while (status == STATUS_BUSY);

        if (status < STATUS_OK)
        {
            S2PI_Abort(slave);
            return status;
        }
    }

    bool hasData = true;
    for (uint8_t i = 1; i < 17U; ++i)
    {
        uint8_t j = ~i; // devices w/ inverted MISO
        if ((data[i] != i) && (data[i] != j))
            hasData = false;
    }

    if (hasData) return STATUS_OK;

    return ERROR_ARGUS_NOT_CONNECTED;
}

static status_t FindConnectedDevices(int8_t * slave, uint32_t * maxBaudRate)
{
    assert(slave != 0);
    assert(maxBaudRate != 0);

    status_t status = STATUS_OK;
    uint8_t r_max = 0;

    if (*slave == 0)
    {
        return ERROR_ARGUS_NOT_CONNECTED;
    }
    else if (*slave > 0)
    {
        return CheckConnectedDevice(*slave);
    }
    else
    {
        /* Reduce baud rate for search. */
        while (((*maxBaudRate) >> r_max) > 100000U) r_max++;
        S2PI_SetBaudRate(*slave, (*maxBaudRate) >> r_max);

        /* Auto detect slave. */
        *slave = 0;
        for (uint8_t s = 1; s <= 4; s++)
        {
            status = CheckConnectedDevice(s);
            if (status == STATUS_OK)
            {
                *slave = s;
                break;
            }
            else if (status != ERROR_ARGUS_NOT_CONNECTED)
            {
                return status;
            }
        }
        if (*slave == 0)
        {
            return ERROR_ARGUS_NOT_CONNECTED;
        }
    }

    /* Auto detect max baud rate. */
    for (uint8_t r = 0; r < r_max; ++r)
    {
        S2PI_SetBaudRate(*slave, (*maxBaudRate) >> r);
        status = CheckConnectedDevice(*slave);
        if (status == STATUS_OK)
        {
            (*maxBaudRate) = (*maxBaudRate) >> r;
            return STATUS_OK;
        }
        else if (status != ERROR_ARGUS_NOT_CONNECTED)
        {
            return status;
        }
    }

    return ERROR_ARGUS_NOT_CONNECTED;
}

/*******************************************************************************
 * Functions
 ******************************************************************************/

argus_hnd_t * ExplorerApp_GetArgusPtr(sci_device_t deviceID)
{
    assert(deviceID <= EXPLORER_DEVICE_ID_MAX);
    explorer_t * explorer = ExplorerApp_GetExplorerPtr(deviceID);
//    assert(explorer != NULL && explorer->Argus != NULL);
    return explorer != NULL ? explorer->Argus : NULL;
}

explorer_t * ExplorerApp_GetExplorerPtr(sci_device_t deviceID)
{
    assert(deviceID <= EXPLORER_DEVICE_ID_MAX);
    return (deviceID <= EXPLORER_DEVICE_ID_MAX) ? explorerIDMap[deviceID] : NULL;
}

explorer_t * ExplorerApp_GetExplorerPtrFromArgus(argus_hnd_t * argus)
{
    assert(argus != NULL);

    for (uint8_t i = 0; i < EXPLORER_DEVICE_COUNT; i++)
    {
        if (explorerArray[i].Argus == argus)
            return &explorerArray[i];
    }

    assert(0);
    return NULL;
}

uint8_t ExplorerApp_GetInitializedExplorerCount()
{
    uint8_t count = 0;
    for (uint8_t idx = 0; idx < EXPLORER_DEVICE_COUNT; ++idx)
    {
        if (explorerArray[idx].Argus != NULL)
            count++;
    }
    return count;
}

explorer_t * ExplorerApp_GetInitializedExplorer(uint8_t index)
{
    assert(index < EXPLORER_DEVICE_COUNT);

    explorer_t * explorer = &explorerArray[index];
    return explorer->Argus != NULL ? explorer : NULL;
}

status_t ExplorerApp_InitDevice(explorer_t * explorer, argus_mode_t mode, bool reinit)
{
    assert(explorer != NULL);

    if (explorer->Argus == NULL)
    {
        explorer->Argus = Argus_CreateHandle();
        if (explorer->Argus == NULL)
        {
            error_log("Failed to allocate the memory for the AFBR-S50 API handle.");
            assert(0);
            return ERROR_FAIL;
        }
    }

    if (reinit)
    {
        status_t status = Argus_Deinit(explorer->Argus);
        if (status < STATUS_OK)
        {
            error_log("Failed to de-initialize the AFBR-S50 API handle, "
                      "error code: %d", status);
            return status;
        }
    }

    /* Check for connected devices. */
    int8_t slave = explorer->Configuration.SPISlave;
    uint32_t baudRate = explorer->Configuration.SPIBaudRate;
    status_t status = FindConnectedDevices(&slave, &baudRate);
    if (status < STATUS_OK)
    {
        Argus_DestroyHandle(explorer->Argus);
        explorer->Argus = NULL;
        error_log("No suitable device connected, error code: %d", status);
        return status;
    }

    /* Device initialization */
    ltc_t start = Time_Now();
    status = Argus_InitMode(explorer->Argus, slave, mode);
    uint32_t elapsed = Time_GetElapsedUSec(&start);
    print("Init Time: %d us", elapsed);
    if (status < STATUS_OK)
    {
        Argus_DestroyHandle(explorer->Argus);
        explorer->Argus = NULL;
        error_log("Failed to initialize AFBR-S50 API, error code: %d", status);
        return status;
    }

    ExplorerApp_ResetDefaultDataStreamingMode(explorer);
    ExplorerApp_DisplayUnambiguousRange(explorer->Argus);

    return STATUS_OK;
}

status_t ExplorerApp_DeviceReinit(explorer_t * explorer, argus_mode_t mode)
{
    assert(explorer != NULL);
    assert(explorer->Argus != NULL);

    status_t status = ExplorerApp_InitDevice(explorer, mode, true);
    ExplorerApp_ResetDefaultDataStreamingMode(explorer);
    ExplorerApp_DisplayUnambiguousRange(explorer->Argus);
    return status;
}

status_t ExplorerApp_InitExplorer(sci_device_t deviceID)
{
    assert(deviceID > 0u);
    status_t status;

    /* ensure the uninitialized device starts with a null mapping */
    explorerIDMap[deviceID] = NULL;

    /* find an unused memory block and allocate it for that instance. */
    explorer_t * pExplorer = NULL;
    for (uint8_t idx = 0; idx < EXPLORER_DEVICE_COUNT; idx++)
    {
        if (explorerArray[idx].Argus == NULL)
        {
            pExplorer = &explorerArray[idx];
            break;
        }
    }

    /* Make sure there is an empty Explorer object available. */
    if (pExplorer == NULL)
    {
        error_log("Failed to allocate an empty explorer object for the AFBR-S50 API instance.");
        return ERROR_FAIL;
    }


    ExplorerApp_GetDefaultConfiguration(&pExplorer->Configuration);
    pExplorer->Configuration.SPISlave = deviceID;

    /* Initialize connected devices. */
    status = ExplorerApp_InitDevice(pExplorer, 0, false);
    if (status < STATUS_OK) return status;

    status = ExplorerApp_SetConfiguration(pExplorer, &pExplorer->Configuration);
    if (status < STATUS_OK) return status;

    /* Only once all checks are completed map the Explorer device to its ID for usage
     * deviceID starts with 1, so a mapping is needed.
     * deviceID 0 is reserved for default device */
    explorerIDMap[deviceID] = pExplorer;

    if (explorerIDMap[0] == NULL)
    {
        explorerIDMap[0] = pExplorer;
    }

    return status;
}
