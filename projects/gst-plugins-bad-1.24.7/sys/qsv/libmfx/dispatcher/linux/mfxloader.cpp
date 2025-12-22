/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "vpl/mfxvideo.h"

#include "linux/device_ids.h"
#include "linux/mfxloader.h"

namespace MFX {

#if defined(__x86_64__) || (INTPTR_MAX == INT64_MAX)
    #define LIBMFXSW "libmfxsw64.so.1"
    #define LIBMFXHW "libmfxhw64.so.1"

    #define ONEVPLSW "libvplswref64.so.1"
    #define ONEVPLHW "libmfx-gen.so.1.2"
#elif defined(__i386__) || (INTPTR_MAX == INT32_MAX)
    #define LIBMFXSW "libmfxsw32.so.1"
    #define LIBMFXHW "libmfxhw32.so.1"

    #define ONEVPLSW "libvplswref32.so.1"
    #define ONEVPLHW "libmfx-gen.so.1.2"
#else
    #error Unsupported architecture
#endif

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) e##func_name,

enum Function {
    eMFXInit,
    eMFXInitEx,
    eMFXClose,
    eMFXJoinSession,
#include "linux/mfxvideo_functions.h"
    eFunctionsNum,
    eNoMoreFunctions = eFunctionsNum
};

// new functions for API 2.x
enum Function2 {
    // 2.0
    eMFXQueryImplsDescription = 0,
    eMFXReleaseImplDescription,
    eMFXMemory_GetSurfaceForVPP,
    eMFXMemory_GetSurfaceForEncode,
    eMFXMemory_GetSurfaceForDecode,
    eMFXInitialize,

    // 2.1
    eMFXMemory_GetSurfaceForVPPOut,
    eMFXVideoDECODE_VPP_Init,
    eMFXVideoDECODE_VPP_DecodeFrameAsync,
    eMFXVideoDECODE_VPP_Reset,
    eMFXVideoDECODE_VPP_GetChannelParam,
    eMFXVideoDECODE_VPP_Close,
    eMFXVideoVPP_ProcessFrameAsync,

    eFunctionsNum2,
};

struct FunctionsTable {
    Function id;
    const char *name;
    mfxVersion version;
};

struct FunctionsTable2 {
    Function2 id;
    const char *name;
    mfxVersion version;
};

#define VERSION(major, minor) \
    {                         \
        { minor, major }      \
    }

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    { e##func_name, #func_name, API_VERSION },

static const FunctionsTable g_mfxFuncTable[] = {
    { eMFXInit, "MFXInit", VERSION(1, 0) },
    { eMFXInitEx, "MFXInitEx", VERSION(1, 14) },
    { eMFXClose, "MFXClose", VERSION(1, 0) },
    { eMFXJoinSession, "MFXJoinSession", VERSION(1, 1) },
#include "linux/mfxvideo_functions.h" // NOLINT(build/include)
    { eNoMoreFunctions }
};

static const FunctionsTable2 g_mfxFuncTable2[] = {
    { eMFXQueryImplsDescription, "MFXQueryImplsDescription", VERSION(2, 0) },
    { eMFXReleaseImplDescription, "MFXReleaseImplDescription", VERSION(2, 0) },
    { eMFXMemory_GetSurfaceForVPP, "MFXMemory_GetSurfaceForVPP", VERSION(2, 0) },
    { eMFXMemory_GetSurfaceForEncode, "MFXMemory_GetSurfaceForEncode", VERSION(2, 0) },
    { eMFXMemory_GetSurfaceForDecode, "MFXMemory_GetSurfaceForDecode", VERSION(2, 0) },
    { eMFXInitialize, "MFXInitialize", VERSION(2, 0) },

    { eMFXMemory_GetSurfaceForVPPOut, "MFXMemory_GetSurfaceForVPPOut", VERSION(2, 1) },
    { eMFXVideoDECODE_VPP_Init, "MFXVideoDECODE_VPP_Init", VERSION(2, 1) },
    { eMFXVideoDECODE_VPP_DecodeFrameAsync, "MFXVideoDECODE_VPP_DecodeFrameAsync", VERSION(2, 1) },
    { eMFXVideoDECODE_VPP_Reset, "MFXVideoDECODE_VPP_Reset", VERSION(2, 1) },
    { eMFXVideoDECODE_VPP_GetChannelParam, "MFXVideoDECODE_VPP_GetChannelParam", VERSION(2, 1) },
    { eMFXVideoDECODE_VPP_Close, "MFXVideoDECODE_VPP_Close", VERSION(2, 1) },
    { eMFXVideoVPP_ProcessFrameAsync, "MFXVideoVPP_ProcessFrameAsync", VERSION(2, 1) },
};

class LoaderCtx {
public:
    mfxStatus Init(mfxInitParam &par,
                   mfxInitializationParam &vplParam,
                   mfxU16 *pDeviceID,
                   char *dllName,
                   bool bCloneSession = false);
    mfxStatus Close();

    inline void *getFunction(Function func) const {
        return m_table[func];
    }

    inline void *getFunction2(Function2 func) const {
        return m_table2[func];
    }

    inline mfxSession getSession() const {
        return m_session;
    }

    inline mfxIMPL getImpl() const {
        return m_implementation;
    }

    inline mfxVersion getVersion() const {
        return m_version;
    }

    inline void *getHandle() const {
        return m_dlh.get();
    }

    inline const char *getLibPath() const {
        return m_libToLoad.c_str();
    }

    // special operations to set session pointer and version from MFXCloneSession()
    inline void setSession(const mfxSession session) {
        m_session = session;
    }

    inline void setVersion(const mfxVersion version) {
        m_version = version;
    }

private:
    std::shared_ptr<void> m_dlh;
    mfxVersion m_version{};
    mfxIMPL m_implementation{};
    mfxSession m_session = nullptr;
    void *m_table[eFunctionsNum]{};
    void *m_table2[eFunctionsNum2]{};
    std::string m_libToLoad;
};

std::shared_ptr<void> make_dlopen(const char *filename, int flags) {
    return std::shared_ptr<void>(dlopen(filename, flags), [](void *handle) {
        if (handle)
            dlclose(handle);
    });
}

mfxStatus LoaderCtx::Init(mfxInitParam &par,
                          mfxInitializationParam &vplParam,
                          mfxU16 *pDeviceID,
                          char *dllName,
                          bool bCloneSession) {
    mfxStatus mfx_res = MFX_ERR_NONE;

    std::vector<std::string> libs;
    std::vector<Device> devices;
    eMFXHWType msdk_platform;

    // query graphics device_id
    // if it is found on list of legacy devices, load MSDK RT
    // otherwise load oneVPL RT
    mfxU16 deviceID = 0;
    mfx_res         = get_devices(devices);
    if (mfx_res == MFX_ERR_NOT_FOUND) {
        // query failed
        msdk_platform = MFX_HW_UNKNOWN;
    }
    else {
        // query succeeded:
        //   may be a valid platform from listLegalDevIDs[] or MFX_HW_UNKNOWN
        //   if underlying device_id is unrecognized (i.e. new platform)
        msdk_platform = devices[0].platform;
        deviceID      = devices[0].device_id;
    }

    if (pDeviceID)
        *pDeviceID = deviceID;

    if (dllName) {
        // attempt to load only this DLL, fail if unsuccessful
        // this may also be used later by MFXCloneSession()
        m_libToLoad = dllName;
        libs.emplace_back(m_libToLoad);
    }
    else {
        mfxIMPL implType = MFX_IMPL_BASETYPE(par.Implementation);
        // add HW lib
        if (implType == MFX_IMPL_AUTO || implType == MFX_IMPL_AUTO_ANY ||
            (implType & MFX_IMPL_HARDWARE) || (implType & MFX_IMPL_HARDWARE_ANY)) {
            if (msdk_platform == MFX_HW_UNKNOWN) {
                // if not on list of known MSDK platforms, prefer oneVPL
                libs.emplace_back(ONEVPLHW);
                libs.emplace_back(MFX_MODULES_DIR "/" ONEVPLHW);
            }

            // use MSDK (fallback if oneVPL is not installed)
            libs.emplace_back(LIBMFXHW);
            libs.emplace_back(MFX_MODULES_DIR "/" LIBMFXHW);
        }

        // add SW lib (oneVPL only)
        if (implType == MFX_IMPL_AUTO || implType == MFX_IMPL_AUTO_ANY ||
            (implType & MFX_IMPL_SOFTWARE)) {
            libs.emplace_back(ONEVPLSW);
            libs.emplace_back(MFX_MODULES_DIR "/" ONEVPLSW);
        }
    }

    // fail if libs is empty (invalid Implementation)
    mfx_res = MFX_ERR_UNSUPPORTED;

    for (auto &lib : libs) {
        std::shared_ptr<void> hdl = make_dlopen(lib.c_str(), RTLD_LOCAL | RTLD_NOW);
        if (hdl) {
            do {
                /* Loading functions table */
                bool wrong_version = false;
                for (int i = 0; i < eFunctionsNum; ++i) {
                    assert(i == g_mfxFuncTable[i].id);
                    m_table[i] = dlsym(hdl.get(), g_mfxFuncTable[i].name);
                    if (!m_table[i] && ((g_mfxFuncTable[i].version <= par.Version))) {
                        wrong_version = true;
                        break;
                    }
                }

                // if version >= 2.0, load these functions as well
                if (par.Version.Major >= 2) {
                    for (int i = 0; i < eFunctionsNum2; ++i) {
                        assert(i == g_mfxFuncTable2[i].id);
                        m_table2[i] = dlsym(hdl.get(), g_mfxFuncTable2[i].name);
                        if (!m_table2[i] && (g_mfxFuncTable2[i].version <= par.Version)) {
                            wrong_version = true;
                            break;
                        }
                    }
                }

                if (wrong_version) {
                    mfx_res = MFX_ERR_UNSUPPORTED;
                    break;
                }

                if (bCloneSession == true) {
                    // success - exit loop since caller will create session with MFXCloneSession()
                    mfx_res = MFX_ERR_NONE;
                    break;
                }

                if (par.Version.Major >= 2) {
                    // for API >= 2.0 call MFXInitialize instead of MFXInitEx
                    mfx_res =
                        ((decltype(MFXInitialize) *)m_table2[eMFXInitialize])(vplParam, &m_session);
                }
                else {
                    if (m_table[eMFXInitEx]) {
                        // initialize with MFXInitEx if present (API >= 1.14)
                        mfx_res = ((decltype(MFXInitEx) *)m_table[eMFXInitEx])(par, &m_session);
                    }
                    else {
                        // initialize with MFXInit for API < 1.14
                        mfx_res = ((decltype(MFXInit) *)m_table[eMFXInit])(par.Implementation,
                                                                           &(par.Version),
                                                                           &m_session);
                    }
                }

                if (MFX_ERR_NONE != mfx_res) {
                    break;
                }

                // Below we just get some data and double check that we got what we have expected
                // to get. Some of these checks are done inside mediasdk init function
                mfx_res =
                    ((decltype(MFXQueryVersion) *)m_table[eMFXQueryVersion])(m_session, &m_version);
                if (MFX_ERR_NONE != mfx_res) {
                    break;
                }

                if (m_version < par.Version) {
                    mfx_res = MFX_ERR_UNSUPPORTED;
                    break;
                }

                mfx_res = ((decltype(MFXQueryIMPL) *)m_table[eMFXQueryIMPL])(m_session,
                                                                             &m_implementation);
                if (MFX_ERR_NONE != mfx_res) {
                    mfx_res = MFX_ERR_UNSUPPORTED;
                    break;
                }
            } while (false);

            if (MFX_ERR_NONE == mfx_res) {
                m_dlh = std::move(hdl);
                break;
            }
            else {
                Close();
            }
        }
    }

    return mfx_res;
}

mfxStatus LoaderCtx::Close() {
    auto proc         = (decltype(MFXClose) *)m_table[eMFXClose];
    mfxStatus mfx_res = (proc) ? (*proc)(m_session) : MFX_ERR_NONE;

    m_implementation = {};
    m_version        = {};
    m_session        = nullptr;
    std::fill(std::begin(m_table), std::end(m_table), nullptr);
    return mfx_res;
}

} // namespace MFX

// internal function - load a specific DLL, return unsupported if it fails
// vplParam is required for API >= 2.0 (load via MFXInitialize)
mfxStatus MFXInitEx2(mfxVersion version,
                     mfxInitializationParam vplParam,
                     mfxIMPL hwImpl,
                     mfxSession *session,
                     mfxU16 *deviceID,
                     char *dllName) {
    if (!session)
        return MFX_ERR_NULL_PTR;

    *deviceID = 0;

    // fill minimal 1.x parameters for Init to choose correct initialization path
    mfxInitParam par = {};
    par.Version      = version;

    // select first adapter if not specified
    // only relevant for MSDK-via-MFXLoad path
    if (!hwImpl)
        hwImpl = MFX_IMPL_HARDWARE;

    switch (vplParam.AccelerationMode) {
        case MFX_ACCEL_MODE_NA:
            par.Implementation = MFX_IMPL_SOFTWARE;
            break;
        case MFX_ACCEL_MODE_VIA_D3D9:
            par.Implementation = hwImpl | MFX_IMPL_VIA_D3D9;
            break;
        case MFX_ACCEL_MODE_VIA_D3D11:
            par.Implementation = hwImpl | MFX_IMPL_VIA_D3D11;
            break;
        case MFX_ACCEL_MODE_VIA_VAAPI:
            par.Implementation = hwImpl | MFX_IMPL_VIA_VAAPI;
            break;
        default:
            par.Implementation = hwImpl;
            break;
    }

    // also pass extBuf array (if any) to MFXInitEx for 1.x API
    par.NumExtParam = vplParam.NumExtParam;
    par.ExtParam    = (vplParam.NumExtParam ? vplParam.ExtParam : nullptr);

#ifdef ONEVPL_EXPERIMENTAL
    // if GPUCopy is enabled via MFXSetConfigProperty(DeviceCopy), set corresponding
    //   flag in mfxInitParam for legacy RTs
    par.GPUCopy = vplParam.DeviceCopy;
#endif

    try {
        std::unique_ptr<MFX::LoaderCtx> loader;

        loader.reset(new MFX::LoaderCtx{});

        mfxStatus mfx_res = loader->Init(par, vplParam, deviceID, dllName);
        if (MFX_ERR_NONE == mfx_res) {
            *session = (mfxSession)loader.release();
        }
        else {
            *session = nullptr;
        }

        return mfx_res;
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session) {
    mfxInitParam par{};

    par.Implementation = impl;
    if (ver) {
        par.Version = *ver;
    }
    else {
        par.Version = VERSION(MFX_VERSION_MAJOR, MFX_VERSION_MINOR);
    }

    return MFXInitEx(par, session);
}

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session) {
    if (!session)
        return MFX_ERR_NULL_PTR;

    const mfxIMPL implMethod        = par.Implementation & (MFX_IMPL_VIA_ANY - 1);
    mfxInitializationParam vplParam = {};
    if (implMethod == MFX_IMPL_SOFTWARE) {
        vplParam.AccelerationMode = MFX_ACCEL_MODE_NA;
    }
    else {
        vplParam.AccelerationMode = MFX_ACCEL_MODE_VIA_VAAPI;
    }

    try {
        std::unique_ptr<MFX::LoaderCtx> loader;

        loader.reset(new MFX::LoaderCtx{});

        mfxStatus mfx_res = loader->Init(par, vplParam, nullptr, nullptr);
        if (MFX_ERR_NONE == mfx_res) {
            *session = (mfxSession)loader.release();
        }
        else {
            *session = nullptr;
        }

        return mfx_res;
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }
}

mfxStatus MFXClose(mfxSession session) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    try {
        std::unique_ptr<MFX::LoaderCtx> loader((MFX::LoaderCtx *)session);
        mfxStatus mfx_res = loader->Close();

        if (mfx_res == MFX_ERR_UNDEFINED_BEHAVIOR) {
            // It is possible, that there is an active child session.
            // Can't unload library in this case.
            loader.release();
        }
        return mfx_res;
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }
}

// passthrough functions to implementation
mfxStatus MFXMemory_GetSurfaceForVPP(mfxSession session, mfxFrameSurface1 **surface) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXMemory_GetSurfaceForVPP) *)loader->getFunction2(
        MFX::eMFXMemory_GetSurfaceForVPP);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), surface);
}

mfxStatus MFXMemory_GetSurfaceForVPPOut(mfxSession session, mfxFrameSurface1 **surface) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXMemory_GetSurfaceForVPPOut) *)loader->getFunction2(
        MFX::eMFXMemory_GetSurfaceForVPPOut);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), surface);
}

mfxStatus MFXMemory_GetSurfaceForEncode(mfxSession session, mfxFrameSurface1 **surface) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXMemory_GetSurfaceForEncode) *)loader->getFunction2(
        MFX::eMFXMemory_GetSurfaceForEncode);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), surface);
}

mfxStatus MFXMemory_GetSurfaceForDecode(mfxSession session, mfxFrameSurface1 **surface) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXMemory_GetSurfaceForDecode) *)loader->getFunction2(
        MFX::eMFXMemory_GetSurfaceForDecode);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), surface);
}

mfxStatus MFXVideoDECODE_VPP_Init(mfxSession session,
                                  mfxVideoParam *decode_par,
                                  mfxVideoChannelParam **vpp_par_array,
                                  mfxU32 num_vpp_par) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc =
        (decltype(MFXVideoDECODE_VPP_Init) *)loader->getFunction2(MFX::eMFXVideoDECODE_VPP_Init);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), decode_par, vpp_par_array, num_vpp_par);
}

mfxStatus MFXVideoDECODE_VPP_DecodeFrameAsync(mfxSession session,
                                              mfxBitstream *bs,
                                              mfxU32 *skip_channels,
                                              mfxU32 num_skip_channels,
                                              mfxSurfaceArray **surf_array_out) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXVideoDECODE_VPP_DecodeFrameAsync) *)loader->getFunction2(
        MFX::eMFXVideoDECODE_VPP_DecodeFrameAsync);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), bs, skip_channels, num_skip_channels, surf_array_out);
}

mfxStatus MFXVideoDECODE_VPP_Reset(mfxSession session,
                                   mfxVideoParam *decode_par,
                                   mfxVideoChannelParam **vpp_par_array,
                                   mfxU32 num_vpp_par) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc =
        (decltype(MFXVideoDECODE_VPP_Reset) *)loader->getFunction2(MFX::eMFXVideoDECODE_VPP_Reset);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), decode_par, vpp_par_array, num_vpp_par);
}

mfxStatus MFXVideoDECODE_VPP_GetChannelParam(mfxSession session,
                                             mfxVideoChannelParam *par,
                                             mfxU32 channel_id) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXVideoDECODE_VPP_GetChannelParam) *)loader->getFunction2(
        MFX::eMFXVideoDECODE_VPP_GetChannelParam);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), par, channel_id);
}

mfxStatus MFXVideoDECODE_VPP_Close(mfxSession session) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc =
        (decltype(MFXVideoDECODE_VPP_Close) *)loader->getFunction2(MFX::eMFXVideoDECODE_VPP_Close);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession());
}

mfxStatus MFXVideoVPP_ProcessFrameAsync(mfxSession session,
                                        mfxFrameSurface1 *in,
                                        mfxFrameSurface1 **out) {
    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

    auto proc = (decltype(MFXVideoVPP_ProcessFrameAsync) *)loader->getFunction2(
        MFX::eMFXVideoVPP_ProcessFrameAsync);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), in, out);
}

mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session) {
    if (!session || !child_session) {
        return MFX_ERR_INVALID_HANDLE;
    }

    MFX::LoaderCtx *loader       = (MFX::LoaderCtx *)session;
    MFX::LoaderCtx *child_loader = (MFX::LoaderCtx *)child_session;

    if (loader->getVersion().Version != child_loader->getVersion().Version) {
        return MFX_ERR_INVALID_HANDLE;
    }

    auto proc = (decltype(MFXJoinSession) *)loader->getFunction(MFX::eMFXJoinSession);
    if (!proc) {
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*proc)(loader->getSession(), child_loader->getSession());
}

static mfxStatus AllocateCloneLoader(MFX::LoaderCtx *parentLoader, MFX::LoaderCtx **cloneLoader) {
    // initialization param structs are not used when bCloneSession == true
    mfxInitParam par                = {};
    mfxInitializationParam vplParam = {};
    mfxU16 deviceID                 = 0;

    // initialization extBufs are not saved at this level
    // the RT should save these when the parent session is created and may use
    //   them when creating the cloned session
    par.NumExtParam = 0;

    try {
        std::unique_ptr<MFX::LoaderCtx> cl;

        cl.reset(new MFX::LoaderCtx{});

        mfxStatus mfx_res =
            cl->Init(par, vplParam, &deviceID, (char *)parentLoader->getLibPath(), true);
        if (MFX_ERR_NONE == mfx_res) {
            *cloneLoader = cl.release();
        }
        else {
            *cloneLoader = nullptr;
        }

        return mfx_res;
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone) {
    if (!session || !clone)
        return MFX_ERR_INVALID_HANDLE;

    MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;
    mfxVersion version     = loader->getVersion();
    *clone                 = nullptr;

    // initialize the clone session
    // for runtimes with 1.x API, call MFXInit followed by MFXJoinSession
    // for runtimes with 2.x API, use RT implementation of MFXCloneSession (passthrough)
    if (version.Major == 1) {
        mfxStatus mfx_res = MFXInit(loader->getImpl(), &version, clone);
        if (MFX_ERR_NONE != mfx_res) {
            return mfx_res;
        }

        // join the sessions
        mfx_res = MFXJoinSession(session, *clone);
        if (MFX_ERR_NONE != mfx_res) {
            MFXClose(*clone);
            *clone = nullptr;
            return mfx_res;
        }
    }
    else if (version.Major == 2) {
        MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;

        // MFXCloneSession not included in function pointer search during init
        // for bwd-compat, check for it here and fail gracefully if missing
        void *libHandle = loader->getHandle();
        auto proc       = (decltype(MFXCloneSession) *)(dlsym(libHandle, "MFXCloneSession"));
        if (!proc)
            return MFX_ERR_UNSUPPORTED;

        // allocate new dispatcher-level session object and copy
        //   state from parent session (function pointer tables, impl type, etc.)
        MFX::LoaderCtx *cloneLoader;
        mfxStatus mfx_res = AllocateCloneLoader(loader, &cloneLoader);
        if (mfx_res != MFX_ERR_NONE)
            return mfx_res;

        // call RT implementation of MFXCloneSession
        mfxSession cloneRT;
        mfx_res = (*proc)(loader->getSession(), &cloneRT);

        if (mfx_res != MFX_ERR_NONE || cloneRT == NULL) {
            // RT call failed, delete cloned loader (no valid session created)
            delete cloneLoader;
            return MFX_ERR_UNSUPPORTED;
        }
        cloneLoader->setSession(cloneRT);

        // get version of cloned session
        mfxVersion cloneVersion = {};
        mfx_res                 = MFXQueryVersion((mfxSession)cloneLoader, &cloneVersion);
        cloneLoader->setVersion(cloneVersion);
        if (mfx_res != MFX_ERR_NONE) {
            MFXClose((mfxSession)cloneLoader);
            return mfx_res;
        }

        *clone = (mfxSession)cloneLoader;
    }
    else {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)    \
    return_value MFX_CDECL func_name formal_param_list {                           \
        /* get the function's address and make a call */                           \
        if (!session)                                                              \
            return MFX_ERR_INVALID_HANDLE;                                         \
                                                                                   \
        MFX::LoaderCtx *loader = (MFX::LoaderCtx *)session;                        \
                                                                                   \
        auto proc = (decltype(func_name) *)loader->getFunction(MFX::e##func_name); \
        if (!proc)                                                                 \
            return MFX_ERR_INVALID_HANDLE;                                         \
                                                                                   \
        /* get the real session pointer */                                         \
        session = loader->getSession();                                            \
        /* pass down the call */                                                   \
        return (*proc)actual_param_list;                                           \
    }

#include "linux/mfxvideo_functions.h" // NOLINT(build/include)

#ifdef __cplusplus
}
#endif
