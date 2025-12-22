/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#pragma once
#include <cstdio>
#include <cstring>
#include "vpl/mfxdefs.h"

#if defined(MFX_DISPATCHER_LOG)
    #include <string.h>
    #include <string>
#endif

#define MAX_PLUGIN_PATH 4096
#define MAX_PLUGIN_NAME 4096

#if _MSC_VER < 1400
    #define wcscpy_s(to, to_size, from) \
        (void)(to_size);                \
        wcscpy(to, from)
    #define wcscat_s(to, to_size, from) \
        (void)(to_size);                \
        wcscat(to, from)
#endif

// declare library module's handle
typedef void *mfxModuleHandle;

typedef void(MFX_CDECL *mfxFunctionPointer)(void);

// Tracer uses lib loading from Program Files logic (via Dispatch reg key) to make dispatcher load tracer dll.
// With DriverStore loading put at 1st place, dispatcher loads real lib before it finds tracer dll.
// This workaround explicitly checks tracer presence in Dispatch reg key and loads tracer dll before the search for lib in all other places.
#define MFX_TRACER_WA_FOR_DS 1
