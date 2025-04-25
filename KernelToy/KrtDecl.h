// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <guiddef.h>

DECLARE_GLOBAL_CONST_UNICODE_STRING(g_KrtDeviceName, L"\\Device\\KernelToy");

// {23CEEB18-C1AB-4F6E-BC81-F81D84B90CF0}
DEFINE_GUID(KRT_CONTROL_CLASS_GUID,
    0x23ceeb18, 0xc1ab, 0x4f6e, 0xbc, 0x81, 0xf8, 0x1d, 0x84, 0xb9, 0xc, 0xf0);
