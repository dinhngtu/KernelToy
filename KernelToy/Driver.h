// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "stdafx.h"

typedef struct _DATA_PROBE {
    ULONG Data[16];
} DATA_PROBE, * PDATA_PROBE;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD KrtpDriverUnload;

VOID KrtpEnableLogging();
