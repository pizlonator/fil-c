/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "windows/mfx_dispatcher.h"

//
// implement a table with functions names
//
#ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    { #func_name, API_VERSION },

const FUNCTION_DESCRIPTION APIFunc[eVideoFuncTotal] = {
    { "MFXInit", { { 0, 1 } } },         { "MFXClose", { { 0, 1 } } },
    { "MFXQueryIMPL", { { 0, 1 } } },    { "MFXQueryVersion", { { 0, 1 } } },

    { "MFXJoinSession", { { 1, 1 } } },  { "MFXDisjoinSession", { { 1, 1 } } },
    { "MFXCloneSession", { { 1, 1 } } }, { "MFXSetPriority", { { 1, 1 } } },
    { "MFXGetPriority", { { 1, 1 } } },

    { "MFXInitEx", { { 14, 1 } } },

#include "windows/mfx_exposed_functions_list.h"
};

// new functions for API >= 2.0
const FUNCTION_DESCRIPTION APIVideoFunc2[eVideoFunc2Total] = {
    { "MFXQueryImplsDescription", { { 0, 2 } } },
    { "MFXReleaseImplDescription", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForVPP", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForEncode", { { 0, 2 } } },
    { "MFXMemory_GetSurfaceForDecode", { { 0, 2 } } },
    { "MFXInitialize", { { 0, 2 } } },

    { "MFXMemory_GetSurfaceForVPPOut", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_Init", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_DecodeFrameAsync", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_Reset", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_GetChannelParam", { { 1, 2 } } },
    { "MFXVideoDECODE_VPP_Close", { { 1, 2 } } },
    { "MFXVideoVPP_ProcessFrameAsync", { { 1, 2 } } },
};

// static section of the file
namespace {

//
// declare pseudo-functions.
// they are used as default values for call-tables.
//

mfxStatus pseudoMFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session) {
    // touch unreferenced parameters
    (void)impl;
    (void)ver;
    (void)session;

    return MFX_ERR_UNKNOWN;

} // mfxStatus pseudoMFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session)

mfxStatus pseudoMFXClose(mfxSession session) {
    // touch unreferenced parameters
    (void)session;

    return MFX_ERR_UNKNOWN;

} // mfxStatus pseudoMFXClose(mfxSession session)

mfxStatus pseudoMFXJoinSession(mfxSession session, mfxSession child_session) {
    // touch unreferenced parameters
    (void)session;
    (void)child_session;

    return MFX_ERR_UNKNOWN;

} // mfxStatus pseudoMFXJoinSession(mfxSession session, mfxSession child_session)

mfxStatus pseudoMFXCloneSession(mfxSession session, mfxSession *clone) {
    // touch unreferenced parameters
    (void)session;
    (void)clone;

    return MFX_ERR_UNKNOWN;

} // mfxStatus pseudoMFXCloneSession(mfxSession session, mfxSession *clone)

void SuppressWarnings(...) {
    // this functions is suppose to suppress warnings.
    // Actually it does nothing.

} // void SuppressWarnings(...)

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    return_value pseudo##func_name formal_param_list {                          \
        SuppressWarnings actual_param_list;                                     \
        return MFX_ERR_UNKNOWN;                                                 \
    }

#include "windows/mfx_exposed_functions_list.h" // NOLINT(build/include)

} // namespace
