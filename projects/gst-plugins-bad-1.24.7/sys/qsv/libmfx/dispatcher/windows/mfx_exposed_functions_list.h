/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

//
// WARNING:
// this file doesn't contain an include guard by design.
// The file may be included into a source file many times.
// That is why this header doesn't contain any include directive.
// Please, do no try to fix it.
//
// NOLINT(build/header_guard)

// Use define API_VERSION to set the API of functions listed further
// When new functions are added new section with functions declarations must be started with updated define

//
// API version 1.0 functions
//

// API version where a function is added. Minor value should precedes the major value
#define API_VERSION \
    {               \
        { 0, 1 }    \
    }

// CORE interface functions
FUNCTION(mfxStatus,
         MFXVideoCORE_SetFrameAllocator,
         (mfxSession session, mfxFrameAllocator *allocator),
         (session, allocator))
FUNCTION(mfxStatus,
         MFXVideoCORE_SetHandle,
         (mfxSession session, mfxHandleType type, mfxHDL hdl),
         (session, type, hdl))
FUNCTION(mfxStatus,
         MFXVideoCORE_GetHandle,
         (mfxSession session, mfxHandleType type, mfxHDL *hdl),
         (session, type, hdl))

FUNCTION(mfxStatus,
         MFXVideoCORE_SyncOperation,
         (mfxSession session, mfxSyncPoint syncp, mfxU32 wait),
         (session, syncp, wait))

// ENCODE interface functions
FUNCTION(mfxStatus,
         MFXVideoENCODE_Query,
         (mfxSession session, mfxVideoParam *in, mfxVideoParam *out),
         (session, in, out))
FUNCTION(mfxStatus,
         MFXVideoENCODE_QueryIOSurf,
         (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request),
         (session, par, request))
FUNCTION(mfxStatus, MFXVideoENCODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus,
         MFXVideoENCODE_GetVideoParam,
         (mfxSession session, mfxVideoParam *par),
         (session, par))
FUNCTION(mfxStatus,
         MFXVideoENCODE_GetEncodeStat,
         (mfxSession session, mfxEncodeStat *stat),
         (session, stat))
FUNCTION(mfxStatus,
         MFXVideoENCODE_EncodeFrameAsync,
         (mfxSession session,
          mfxEncodeCtrl *ctrl,
          mfxFrameSurface1 *surface,
          mfxBitstream *bs,
          mfxSyncPoint *syncp),
         (session, ctrl, surface, bs, syncp))

// DECODE interface functions
FUNCTION(mfxStatus,
         MFXVideoDECODE_Query,
         (mfxSession session, mfxVideoParam *in, mfxVideoParam *out),
         (session, in, out))
FUNCTION(mfxStatus,
         MFXVideoDECODE_DecodeHeader,
         (mfxSession session, mfxBitstream *bs, mfxVideoParam *par),
         (session, bs, par))
FUNCTION(mfxStatus,
         MFXVideoDECODE_QueryIOSurf,
         (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request),
         (session, par, request))
FUNCTION(mfxStatus, MFXVideoDECODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus,
         MFXVideoDECODE_GetVideoParam,
         (mfxSession session, mfxVideoParam *par),
         (session, par))
FUNCTION(mfxStatus,
         MFXVideoDECODE_GetDecodeStat,
         (mfxSession session, mfxDecodeStat *stat),
         (session, stat))
FUNCTION(mfxStatus,
         MFXVideoDECODE_SetSkipMode,
         (mfxSession session, mfxSkipMode mode),
         (session, mode))
FUNCTION(mfxStatus,
         MFXVideoDECODE_GetPayload,
         (mfxSession session, mfxU64 *ts, mfxPayload *payload),
         (session, ts, payload))
FUNCTION(mfxStatus,
         MFXVideoDECODE_DecodeFrameAsync,
         (mfxSession session,
          mfxBitstream *bs,
          mfxFrameSurface1 *surface_work,
          mfxFrameSurface1 **surface_out,
          mfxSyncPoint *syncp),
         (session, bs, surface_work, surface_out, syncp))

// VPP interface functions
FUNCTION(mfxStatus,
         MFXVideoVPP_Query,
         (mfxSession session, mfxVideoParam *in, mfxVideoParam *out),
         (session, in, out))
FUNCTION(mfxStatus,
         MFXVideoVPP_QueryIOSurf,
         (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request),
         (session, par, request))
FUNCTION(mfxStatus, MFXVideoVPP_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Close, (mfxSession session), (session))

FUNCTION(mfxStatus,
         MFXVideoVPP_GetVideoParam,
         (mfxSession session, mfxVideoParam *par),
         (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_GetVPPStat, (mfxSession session, mfxVPPStat *stat), (session, stat))
FUNCTION(mfxStatus,
         MFXVideoVPP_RunFrameVPPAsync,
         (mfxSession session,
          mfxFrameSurface1 *in,
          mfxFrameSurface1 *out,
          mfxExtVppAuxData *aux,
          mfxSyncPoint *syncp),
         (session, in, out, aux, syncp))

#undef API_VERSION

#define API_VERSION \
    {               \
        { 19, 1 }   \
    }

FUNCTION(mfxStatus,
         MFXVideoCORE_QueryPlatform,
         (mfxSession session, mfxPlatform *platform),
         (session, platform))

#undef API_VERSION
