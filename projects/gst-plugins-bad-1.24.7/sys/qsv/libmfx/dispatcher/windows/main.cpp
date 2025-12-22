/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include <windows.h>

#include <stringapiset.h>

#include <memory>
#include <new>

#include "windows/mfx_critical_section.h"
#include "windows/mfx_dispatcher.h"
#include "windows/mfx_dispatcher_log.h"
#include "windows/mfx_library_iterator.h"
#include "windows/mfx_load_dll.h"

#include "windows/mfx_vector.h"

#if defined(MEDIASDK_UWP_DISPATCHER)
    #include "windows/mfx_driver_store_loader.h"
#endif

#include <string.h> /* for memset on Linux */

#include <stdlib.h> /* for qsort on Linux */

// module-local definitions
namespace {

const struct {
    // instance implementation type
    eMfxImplType implType;
    // real implementation
    mfxIMPL impl;
    // adapter numbers
    mfxU32 adapterID;

} implTypes[] = {
    // MFX_IMPL_AUTO case
    { MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE, 0 },
    { MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE, 0 },

    // MFX_IMPL_ANY case
    { MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE, 0 },
    { MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE2, 1 },
    { MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE3, 2 },
    { MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE4, 3 },
    { MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE, 0 },
    { MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE, 0 }, // unused - was MFX_IMPL_AUDIO
};

const struct {
    // start index in implTypes table for specified implementation
    mfxU32 minIndex;
    // last index in implTypes table for specified implementation
    mfxU32 maxIndex;

} implTypesRange[] = {
    { 0, 1 }, // MFX_IMPL_AUTO
    { 1, 1 }, // MFX_IMPL_SOFTWARE
    { 0, 0 }, // MFX_IMPL_HARDWARE
    { 2, 6 }, // MFX_IMPL_AUTO_ANY
    { 2, 5 }, // MFX_IMPL_HARDWARE_ANY
    { 3, 3 }, // MFX_IMPL_HARDWARE2
    { 4, 4 }, // MFX_IMPL_HARDWARE3
    { 5, 5 }, // MFX_IMPL_HARDWARE4
    { 2, 6 }, // MFX_IMPL_RUNTIME, same as MFX_IMPL_HARDWARE_ANY
};

MFX::mfxCriticalSection dispGuard = 0;

} // namespace

using namespace MFX;

#if !defined(MEDIASDK_UWP_DISPATCHER)

//
// Implement DLL exposed functions. MFXInit and MFXClose have to do
// slightly more than other. They require to be implemented explicitly.
// All other functions are implemented implicitly.
//

typedef MFXVector<MFX_DISP_HANDLE_EX *> HandleVector;
typedef MFXVector<mfxStatus> StatusVector;

struct VectorHandleGuard {
    explicit VectorHandleGuard(HandleVector &aVector) : m_vector(aVector) {}
    ~VectorHandleGuard() {
        HandleVector::iterator it = m_vector.begin(), et = m_vector.end();
        for (; it != et; ++it) {
            delete *it;
        }
    }

    HandleVector &m_vector;

private:
    void operator=(const VectorHandleGuard &);
};

static int HandleSort(const void *plhs, const void *prhs) {
    const MFX_DISP_HANDLE_EX *lhs   = *(const MFX_DISP_HANDLE_EX **)plhs;
    const MFX_DISP_HANDLE_EX *rhs   = *(const MFX_DISP_HANDLE_EX **)prhs;
    const mfxVersion vplInitVersion = { 255, 1 };

    // prefer oneVPL runtime (API = 1.255)
    if ((lhs->actualApiVersion.Version == vplInitVersion.Version) &&
        (rhs->actualApiVersion < lhs->actualApiVersion)) {
        return -1;
    }
    if ((rhs->actualApiVersion.Version == vplInitVersion.Version) &&
        (lhs->actualApiVersion < rhs->actualApiVersion)) {
        return 1;
    }

    // prefer HW implementation
    if (lhs->implType != MFX_LIB_HARDWARE && rhs->implType == MFX_LIB_HARDWARE) {
        return 1;
    }
    if (lhs->implType == MFX_LIB_HARDWARE && rhs->implType != MFX_LIB_HARDWARE) {
        return -1;
    }

    // prefer integrated GPU
    if (lhs->mediaAdapterType != MFX_MEDIA_INTEGRATED &&
        rhs->mediaAdapterType == MFX_MEDIA_INTEGRATED) {
        return 1;
    }
    if (lhs->mediaAdapterType == MFX_MEDIA_INTEGRATED &&
        rhs->mediaAdapterType != MFX_MEDIA_INTEGRATED) {
        return -1;
    }

    // prefer dll with lower API version
    if (lhs->actualApiVersion < rhs->actualApiVersion) {
        return -1;
    }
    if (rhs->actualApiVersion < lhs->actualApiVersion) {
        return 1;
    }

    // if versions are equal prefer library with HW
    if (lhs->loadStatus == MFX_WRN_PARTIAL_ACCELERATION && rhs->loadStatus == MFX_ERR_NONE) {
        return 1;
    }
    if (lhs->loadStatus == MFX_ERR_NONE && rhs->loadStatus == MFX_WRN_PARTIAL_ACCELERATION) {
        return -1;
    }

    return 0;
}

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session) {
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    DISPATCHER_LOG_BLOCK(("MFXInitEx (impl=%s, pVer=%d.%d, ExternalThreads=%d session=0x%p\n",
                          DispatcherLog_GetMFXImplString(par.Implementation).c_str(),
                          par.Version.Major,
                          par.Version.Minor,
                          par.ExternalThreads,
                          session));

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    HandleVector allocatedHandle;
    VectorHandleGuard handleGuard(allocatedHandle);

    MFX_DISP_HANDLE_EX *pHandle;
    wchar_t dllName[MFX_MAX_DLL_PATH] = { 0 };
    MFX::MFXLibraryIterator libIterator;

    // there iterators are used only if the caller specified implicit type like AUTO
    mfxU32 curImplIdx, maxImplIdx;
    // implementation method masked from the input parameter
    // special case for audio library
    const mfxIMPL implMethod = par.Implementation & (MFX_IMPL_VIA_ANY - 1);

    // implementation interface masked from the input parameter
    mfxIMPL implInterface      = par.Implementation & -MFX_IMPL_VIA_ANY;
    mfxIMPL implInterfaceOrig  = implInterface;
    mfxVersion requiredVersion = { { MFX_VERSION_MINOR, MFX_VERSION_MAJOR } };

    mfxInitializationParam vplParam = {};
    if (implMethod == MFX_IMPL_SOFTWARE) {
        vplParam.AccelerationMode = MFX_ACCEL_MODE_NA;
    }
    else {
        // hardware - D3D11 by default
        if (implInterface == MFX_IMPL_VIA_D3D9)
            vplParam.AccelerationMode = MFX_ACCEL_MODE_VIA_D3D9;
        else
            vplParam.AccelerationMode = MFX_ACCEL_MODE_VIA_D3D11;
    }

    // check error(s)
    if (NULL == session) {
        return MFX_ERR_NULL_PTR;
    }

    if (((MFX_IMPL_AUTO > implMethod) || (MFX_IMPL_RUNTIME < implMethod))) {
        return MFX_ERR_UNSUPPORTED;
    }

    // set the minimal required version
    requiredVersion = par.Version;

    try {
        // reset the session value
        *session = 0;

        // allocate the dispatching handle and call-table
        pHandle = new MFX_DISP_HANDLE_EX(requiredVersion);
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    DISPATCHER_LOG_INFO(
        (("Required API version is %u.%u\n"), requiredVersion.Major, requiredVersion.Minor));
    // particular implementation value
    mfxIMPL curImpl;

    // Load HW library or RT from system location
    curImplIdx = implTypesRange[implMethod].minIndex;
    maxImplIdx = implTypesRange[implMethod].maxIndex;
    do {
        int currentStorage = MFX::MFX_STORAGE_ID_FIRST;
        implInterface      = implInterfaceOrig;
        do {
            // this storage will be checked below
            if (currentStorage == MFX::MFX_APP_FOLDER) {
                currentStorage += 1;
                continue;
            }

            // initialize the library iterator
            mfxRes = libIterator.Init(implTypes[curImplIdx].implType,
                                      implInterface,
                                      implTypes[curImplIdx].adapterID,
                                      currentStorage);

            // look through the list of installed SDK version,
            // looking for a suitable library with higher merit value.
            if (MFX_ERR_NONE == mfxRes) {
                if (MFX_LIB_HARDWARE == implTypes[curImplIdx].implType &&
                    (!implInterface || MFX_IMPL_VIA_ANY == implInterface)) {
                    implInterface = libIterator.GetImplementationType();
                }

                do {
                    eMfxImplType implType = implTypes[curImplIdx].implType;

                    // select a desired DLL
                    mfxRes = libIterator.SelectDLLVersion(dllName,
                                                          sizeof(dllName) / sizeof(dllName[0]),
                                                          &implType,
                                                          pHandle->apiVersion);
                    if (MFX_ERR_NONE != mfxRes) {
                        break;
                    }
                    DISPATCHER_LOG_INFO((("loading library %S\n"), dllName));
                    // try to load the selected DLL
                    curImpl = implTypes[curImplIdx].impl;
                    mfxRes  = pHandle->LoadSelectedDLL(dllName,
                                                      implType,
                                                      curImpl,
                                                      implInterface,
                                                      par,
                                                      vplParam);
                    // unload the failed DLL
                    if (MFX_ERR_NONE != mfxRes) {
                        pHandle->Close();
                        continue;
                    }

                    mfxPlatform platform = { MFX_PLATFORM_UNKNOWN, 0, MFX_MEDIA_UNKNOWN };
                    if (pHandle->callTable[eMFXVideoCORE_QueryPlatform]) {
                        mfxRes = MFXVideoCORE_QueryPlatform((mfxSession)pHandle, &platform);
                        if (MFX_ERR_NONE != mfxRes) {
                            DISPATCHER_LOG_WRN(
                                ("MFXVideoCORE_QueryPlatform failed, rejecting loaded library\n"));
                            pHandle->Close();
                            continue;
                        }
                    }
                    pHandle->mediaAdapterType = platform.MediaAdapterType;
                    DISPATCHER_LOG_INFO(
                        (("media adapter type is %d\n"), pHandle->mediaAdapterType));

                    libIterator.GetSubKeyName(
                        pHandle->subkeyName,
                        sizeof(pHandle->subkeyName) / sizeof(pHandle->subkeyName[0]));
                    pHandle->storageID = libIterator.GetStorageID();
                    allocatedHandle.push_back(pHandle);
                    pHandle = new MFX_DISP_HANDLE_EX(requiredVersion);
                } while (MFX_ERR_NONE != mfxRes);
            }

            // select another place for loading engine
            currentStorage += 1;

        } while ((MFX_ERR_NONE != mfxRes) && (MFX::MFX_STORAGE_ID_LAST >= currentStorage));

    } while ((MFX_ERR_NONE != mfxRes) && (++curImplIdx <= maxImplIdx));

    curImplIdx = implTypesRange[implMethod].minIndex;
    maxImplIdx = implTypesRange[implMethod].maxIndex;

    // Load RT from app folder (libmfxsw64 with API >= 1.10)
    do {
        implInterface = implInterfaceOrig;
        // initialize the library iterator
        mfxRes = libIterator.Init(implTypes[curImplIdx].implType,
                                  implInterface,
                                  implTypes[curImplIdx].adapterID,
                                  MFX::MFX_APP_FOLDER);

        if (MFX_ERR_NONE == mfxRes) {
            if (MFX_LIB_HARDWARE == implTypes[curImplIdx].implType &&
                (!implInterface || MFX_IMPL_VIA_ANY == implInterface)) {
                implInterface = libIterator.GetImplementationType();
            }

            do {
                eMfxImplType implType;

                // select a desired DLL
                mfxRes = libIterator.SelectDLLVersion(dllName,
                                                      sizeof(dllName) / sizeof(dllName[0]),
                                                      &implType,
                                                      pHandle->apiVersion);
                if (MFX_ERR_NONE != mfxRes) {
                    break;
                }
                DISPATCHER_LOG_INFO((("loading library %S\n"), dllName));

                // try to load the selected DLL
                curImpl = implTypes[curImplIdx].impl;
                mfxRes  = pHandle->LoadSelectedDLL(dllName,
                                                  implType,
                                                  curImpl,
                                                  implInterface,
                                                  par,
                                                  vplParam);
                // unload the failed DLL
                if (MFX_ERR_NONE != mfxRes) {
                    pHandle->Close();
                }
                else {
                    if (pHandle->actualApiVersion.Major == 1 &&
                        pHandle->actualApiVersion.Minor <= 9) {
                        // this is not RT, skip it
                        mfxRes = MFX_ERR_ABORTED;
                        break;
                    }
                    pHandle->storageID = MFX::MFX_UNKNOWN_KEY;
                    allocatedHandle.push_back(pHandle);
                    pHandle = new MFX_DISP_HANDLE_EX(requiredVersion);
                }

            } while (MFX_ERR_NONE != mfxRes);
        }
    } while ((MFX_ERR_NONE != mfxRes) && (++curImplIdx <= maxImplIdx));

    // Load HW and SW libraries using legacy default DLL search mechanism
    // set current library index again
    curImplIdx = implTypesRange[implMethod].minIndex;
    do {
        implInterface = implInterfaceOrig;

        mfxRes = MFX::mfx_get_default_dll_name(dllName,
                                               sizeof(dllName) / sizeof(dllName[0]),
                                               implTypes[curImplIdx].implType);

        if (MFX_ERR_NONE == mfxRes) {
            DISPATCHER_LOG_INFO((("loading default library %S\n"), dllName))

            // try to load the selected DLL using default DLL search mechanism
            if (MFX_LIB_HARDWARE == implTypes[curImplIdx].implType) {
                if (!implInterface) {
                    implInterface = MFX_IMPL_VIA_ANY;
                }
                mfxU32 curVendorID = 0, curDeviceID = 0;
                mfxRes = MFX::SelectImplementationType(implTypes[curImplIdx].adapterID,
                                                       &implInterface,
                                                       &curVendorID,
                                                       &curDeviceID);
                if (curVendorID != INTEL_VENDOR_ID)
                    mfxRes = MFX_ERR_UNKNOWN;
            }
            if (MFX_ERR_NONE == mfxRes) {
                // try to load the selected DLL using default DLL search mechanism
                mfxRes = pHandle->LoadSelectedDLL(dllName,
                                                  implTypes[curImplIdx].implType,
                                                  implTypes[curImplIdx].impl,
                                                  implInterface,
                                                  par,
                                                  vplParam);
            }
            // unload the failed DLL
            if ((MFX_ERR_NONE != mfxRes) && (MFX_WRN_PARTIAL_ACCELERATION != mfxRes)) {
                pHandle->Close();
            }
            else {
                pHandle->storageID = MFX::MFX_UNKNOWN_KEY;
                allocatedHandle.push_back(pHandle);
                pHandle = new MFX_DISP_HANDLE_EX(requiredVersion);
            }
        }
    } while ((MFX_ERR_NONE > mfxRes) && (++curImplIdx <= maxImplIdx));
    delete pHandle;

    if (allocatedHandle.size() == 0)
        return MFX_ERR_UNSUPPORTED;

    { // sort candidate list
        bool NeedSort                = false;
        HandleVector::iterator first = allocatedHandle.begin(), it = allocatedHandle.begin(),
                               et = allocatedHandle.end();
        for (it++; it != et; ++it)
            if (HandleSort(&(*first), &(*it)) != 0)
                NeedSort = true;

        // sort allocatedHandle so that the most preferred dll is at the beginning
        if (NeedSort)
            qsort(&(*allocatedHandle.begin()),
                  allocatedHandle.size(),
                  sizeof(MFX_DISP_HANDLE_EX *),
                  &HandleSort);
    }
    HandleVector::iterator candidate = allocatedHandle.begin();
    // check the final result of loading
    try {
        pHandle = *candidate;
        //pulling up current mediasdk version, that required to match plugin version
        //block for now to avoid KW scanning failure (UNUSED)
        //mfxVersion apiVerActual = { { 0, 0 } };
        //mfxStatus stsQueryVersion =
        //    MFXQueryVersion((mfxSession)pHandle, &apiVerActual);
    }
    catch (...) {
        DISPATCHER_LOG_ERROR((("unknown exception while loading plugins\n")))
    }

    // everything is OK. Save pointers to the output variable
    *candidate = 0; // keep this one safe from guard destructor

    //===================================

    // MFXVideoCORE_QueryPlatform call creates d3d device handle, so we have handle right after MFXInit and can't accept external handle
    // This is a workaround which calls close-init to remove that handle

    mfxFunctionPointer *actualTable = pHandle->callTable;
    mfxFunctionPointer pFunc;

    pFunc  = actualTable[eMFXClose];
    mfxRes = (*(mfxStatus(MFX_CDECL *)(mfxSession))pFunc)(pHandle->session);
    if (mfxRes != MFX_ERR_NONE)
        return mfxRes;

    pHandle->session = 0;
    bool callOldInit = !actualTable[eMFXInitEx];
    pFunc            = actualTable[(callOldInit) ? eMFXInit : eMFXInitEx];

    mfxVersion version(pHandle->apiVersion);
    if (callOldInit) {
        pHandle->loadStatus = (*(mfxStatus(MFX_CDECL *)(mfxIMPL, mfxVersion *, mfxSession *))pFunc)(
            pHandle->impl | pHandle->implInterface,
            &version,
            &pHandle->session);
    }
    else {
        mfxInitParam initPar   = par;
        initPar.Implementation = pHandle->impl | pHandle->implInterface;
        initPar.Version        = version;
        pHandle->loadStatus =
            (*(mfxStatus(MFX_CDECL *)(mfxInitParam, mfxSession *))pFunc)(initPar,
                                                                         &pHandle->session);
    }

    //===================================

    *((MFX_DISP_HANDLE_EX **)session) = pHandle;

    return pHandle->loadStatus;

} // mfxStatus MFXInitEx(mfxIMPL impl, mfxVersion *ver, mfxSession *session)

// internal function - load a specific DLL, return unsupported if it fails
// vplParam is required for API >= 2.0 (load via MFXInitialize)
mfxStatus MFXInitEx2(mfxVersion version,
                     mfxInitializationParam vplParam,
                     mfxIMPL hwImpl,
                     mfxSession *session,
                     mfxU16 *deviceID,
                     wchar_t *dllName) {
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    mfxStatus mfxRes = MFX_ERR_NONE;
    HandleVector allocatedHandle;
    VectorHandleGuard handleGuard(allocatedHandle);

    MFX_DISP_HANDLE *pHandle;

    *deviceID = 0;

    // fill minimal 1.x parameters for LoadSelectedDLL to choose correct initialization path
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

    #ifdef ONEVPL_EXPERIMENTAL
    // if GPUCopy is enabled via MFXSetConfigProperty(DeviceCopy), set corresponding
    //   flag in mfxInitParam for legacy RTs
    par.GPUCopy = vplParam.DeviceCopy;
    #endif

    // also pass extBuf array (if any) to MFXInitEx for 1.x API
    par.NumExtParam = vplParam.NumExtParam;
    par.ExtParam    = (vplParam.NumExtParam ? vplParam.ExtParam : nullptr);

    eMfxImplType implType =
        (par.Implementation == MFX_IMPL_SOFTWARE ? MFX_LIB_SOFTWARE : MFX_LIB_HARDWARE);
    mfxIMPL implInterface    = par.Implementation & -MFX_IMPL_VIA_ANY;
    const mfxIMPL implMethod = par.Implementation & (MFX_IMPL_VIA_ANY - 1);

    // check error(s)
    if (NULL == session || NULL == dllName) {
        return MFX_ERR_NULL_PTR;
    }

    try {
        // reset the session value
        *session = 0;

        // allocate the dispatching handle and call-table
        pHandle = new MFX_DISP_HANDLE(par.Version);
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE == mfxRes) {
        DISPATCHER_LOG_INFO((("loading default library %S\n"), dllName))

        if (MFX_ERR_NONE == mfxRes) {
            // try to load the selected DLL using given DLL name
            mfxRes = pHandle->LoadSelectedDLL(dllName,
                                              implType,
                                              implMethod,
                                              implInterface,
                                              par,
                                              vplParam);
        }
        // unload the failed DLL
        if (MFX_ERR_NONE != mfxRes) {
            pHandle->Close();
            delete pHandle;
            return MFX_ERR_UNSUPPORTED;
        }
        else {
            pHandle->storageID = MFX::MFX_UNKNOWN_KEY;
        }
    }

    // everything is OK. Save pointers to the output variable
    *((MFX_DISP_HANDLE **)session) = pHandle;

    return pHandle->loadStatus;
}

mfxStatus MFXClose(mfxSession session) {
    MFX::MFXAutomaticCriticalSection guard(&dispGuard);

    mfxStatus mfxRes         = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *)session;

    // check error(s)
    if (pHandle) {
        try {
            // unload the DLL library
            mfxRes = pHandle->Close();

            // it is possible, that there is an active child session.
            // can't unload library in that case.
            if (MFX_ERR_UNDEFINED_BEHAVIOR != mfxRes) {
                // release the handle
                delete pHandle;
            }
        }
        catch (...) {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }

    return mfxRes;

} // mfxStatus MFXClose(mfxSession session)

#else // relates to !defined (MEDIASDK_UWP_DISPATCHER), i.e. #else part as if MEDIASDK_UWP_DISPATCHER defined

static mfxModuleHandle hModule;

// for the UWP_DISPATCHER purposes implementation of MFXinitEx is calling
// InitialiseMediaSession() implemented in intel_gfx_api.dll
mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session) {
    #if defined(MEDIASDK_ARM_LOADER)

    return MFX_ERR_UNSUPPORTED;

    #else

    wchar_t IntelGFXAPIdllName[MFX_MAX_DLL_PATH] = { 0 };
    mfxI32 adapterNum                            = -1;

    switch (par.Implementation & 0xf) {
        case MFX_IMPL_SOFTWARE:
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
        case MFX_IMPL_SINGLE_THREAD:
        #endif
            return MFX_ERR_UNSUPPORTED;
        case MFX_IMPL_AUTO:
        case MFX_IMPL_HARDWARE:
            adapterNum = 0;
            break;
        case MFX_IMPL_HARDWARE2:
            adapterNum = 1;
            break;
        case MFX_IMPL_HARDWARE3:
            adapterNum = 2;
            break;
        case MFX_IMPL_HARDWARE4:
            adapterNum = 3;
            break;
        default:
            return GfxApiInitPriorityIntegrated(par, session, hModule);
    }

    return GfxApiInitByAdapterNum(par, adapterNum, session, hModule);

    #endif
}

// for the UWP_DISPATCHER purposes implementation of MFXClose is calling
// DisposeMediaSession() implemented in intel_gfx_api.dll
mfxStatus MFXClose(mfxSession session) {
    if (NULL == session) {
        return MFX_ERR_INVALID_HANDLE;
    }

    mfxStatus sts = MFX_ERR_NONE;

    #if defined(MEDIASDK_ARM_LOADER)

    sts = MFX_ERR_UNSUPPORTED;

    #else

    sts = GfxApiClose(session, hModule);

    #endif

    session = (mfxSession)NULL;
    return sts;
}

    #undef FUNCTION
    #define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)               \
        return_value func_name formal_param_list {                                                \
            mfxStatus mfxRes = MFX_ERR_INVALID_HANDLE;                                            \
                                                                                                  \
            _mfxSession *pHandle = (_mfxSession *)session;                                        \
                                                                                                  \
            /* get the function's address and make a call */                                      \
            if (pHandle) {                                                                        \
                mfxFunctionPointer pFunc = pHandle->callPlugInsTable[e##func_name];               \
                if (pFunc) {                                                                      \
                    /* pass down the call */                                                      \
                    mfxRes = (*(mfxStatus(MFX_CDECL *) formal_param_list)pFunc)actual_param_list; \
                }                                                                                 \
            }                                                                                     \
            return mfxRes;                                                                        \
        }

FUNCTION(mfxStatus,
         MFXVideoUSER_Load,
         (mfxSession session, const mfxPluginUID *uid, mfxU32 version),
         (session, uid, version))
FUNCTION(
    mfxStatus,
    MFXVideoUSER_LoadByPath,
    (mfxSession session, const mfxPluginUID *uid, mfxU32 version, const mfxChar *path, mfxU32 len),
    (session, uid, version, path, len))
FUNCTION(mfxStatus,
         MFXVideoUSER_UnLoad,
         (mfxSession session, const mfxPluginUID *uid),
         (session, uid))
FUNCTION(mfxStatus,
         MFXAudioUSER_Load,
         (mfxSession session, const mfxPluginUID *uid, mfxU32 version),
         (session, uid, version))
FUNCTION(mfxStatus,
         MFXAudioUSER_UnLoad,
         (mfxSession session, const mfxPluginUID *uid),
         (session, uid))

#endif //!defined(MEDIASDK_UWP_DISPATCHER)

mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session) {
    mfxStatus mfxRes              = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle      = (MFX_DISP_HANDLE *)session;
    MFX_DISP_HANDLE *pChildHandle = (MFX_DISP_HANDLE *)child_session;

    // get the function's address and make a call
    if ((pHandle) && (pChildHandle) &&
        (pHandle->actualApiVersion == pChildHandle->actualApiVersion)) {
        /* check whether it is audio session or video */
        int tableIndex = eMFXJoinSession;
        mfxFunctionPointer pFunc;
        pFunc = pHandle->callTable[tableIndex];

        if (pFunc) {
            // pass down the call
            mfxRes =
                (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxSession))pFunc)(pHandle->session,
                                                                         pChildHandle->session);
        }
    }

    return mfxRes;

} // mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)

static mfxStatus AllocateCloneHandle(MFX_DISP_HANDLE *parentHandle, MFX_DISP_HANDLE **cloneHandle) {
    MFX_DISP_HANDLE *ph = parentHandle;
    MFX_DISP_HANDLE *ch = nullptr;

    if (!ph || !ph->hModule)
        return MFX_ERR_NULL_PTR;

    // get full path to the DLL of the parent session
    wchar_t dllName[MFX::msdk_disp_path_len];
    DWORD nSize = GetModuleFileNameW((HMODULE)(ph->hModule), dllName, msdk_disp_path_len);
    if (nSize == 0 || nSize == msdk_disp_path_len)
        return MFX_ERR_UNSUPPORTED;

    try {
        // requested version matches original session
        ch = new MFX_DISP_HANDLE(ph->apiVersion);
    }
    catch (...) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    // initialization param structs are not used when bCloneSession == true
    mfxInitParam par                = {};
    mfxInitializationParam vplParam = {};

    // initialization extBufs are not saved at this level
    // the RT should save these when the parent session is created and may use
    //   them when creating the cloned session
    par.NumExtParam = 0;

    // load the selected DLL, fill out function pointer tables and other state
    mfxStatus sts = ch->LoadSelectedDLL(dllName,
                                        ph->implType,
                                        ph->impl,
                                        ph->implInterface,
                                        par,
                                        vplParam,
                                        true);

    // unload the failed DLL
    if (sts) {
        ch->Close();
        delete ch;
        return MFX_ERR_UNSUPPORTED;
    }
    else {
        ch->storageID = MFX::MFX_UNKNOWN_KEY;
    }

    *cloneHandle = ch;

    return MFX_ERR_NONE;
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone) {
    mfxStatus mfxRes         = MFX_ERR_INVALID_HANDLE;
    MFX_DISP_HANDLE *pHandle = (MFX_DISP_HANDLE *)session;
    mfxVersion apiVersion;
    mfxIMPL impl;

    if (!clone)
        return MFX_ERR_NULL_PTR;
    *clone = NULL;

    // check error(s)
    if (pHandle) {
        // initialize the clone session
        // for runtimes with 1.x API, call MFXInit followed by MFXJoinSession
        // for runtimes with 2.x API, use RT implementation of MFXCloneSession (passthrough)
        apiVersion = pHandle->actualApiVersion;

        if (apiVersion.Major == 1) {
            impl   = pHandle->impl | pHandle->implInterface;
            mfxRes = MFXInit(impl, &apiVersion, clone);
            if (MFX_ERR_NONE != mfxRes) {
                return mfxRes;
            }

            // join the sessions
            mfxRes = MFXJoinSession(session, *clone);
            if (MFX_ERR_NONE != mfxRes) {
                MFXClose(*clone);
                *clone = NULL;
                return mfxRes;
            }
        }
        else if (apiVersion.Major == 2) {
            int tableIndex = eMFXCloneSession;
            mfxFunctionPointer pFunc;
            pFunc = pHandle->callTable[tableIndex];
            if (!pFunc)
                return MFX_ERR_UNSUPPORTED;

            // allocate new dispatcher-level session object and initialize state
            //   (function pointer tables, impl type, etc.)
            MFX_DISP_HANDLE *cloneHandle;
            mfxRes = AllocateCloneHandle(pHandle, &cloneHandle);
            if (mfxRes != MFX_ERR_NONE)
                return mfxRes;

            // call RT implementation of MFXCloneSession
            mfxSession cloneRT;
            mfxRes = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxSession *))pFunc)(pHandle->session,
                                                                                &cloneRT);

            if (mfxRes != MFX_ERR_NONE || cloneRT == NULL) {
                // RT call failed, delete cloned session
                delete cloneHandle;
                return MFX_ERR_UNSUPPORTED;
            }
            cloneHandle->session = cloneRT;

            // get version of cloned session
            mfxVersion cloneVersion = {};
            mfxRes                  = MFXQueryVersion((mfxSession)cloneHandle, &cloneVersion);
            if (mfxRes != MFX_ERR_NONE) {
                MFXClose(cloneHandle);
                return MFX_ERR_UNSUPPORTED;
            }

            cloneHandle->actualApiVersion = cloneVersion;
            *clone                        = (mfxSession)cloneHandle;
        }
        else {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return mfxRes;

} // mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)

mfxStatus MFXInit(mfxIMPL impl, mfxVersion *pVer, mfxSession *session) {
    mfxInitParam par = {};

    par.Implementation = impl;
    if (pVer) {
        par.Version = *pVer;
    }
    else {
        par.Version.Major = DEFAULT_API_VERSION_MAJOR;
        par.Version.Minor = DEFAULT_API_VERSION_MINOR;
    }
    par.ExternalThreads = 0;

    return MFXInitEx(par, session);
}

// passthrough functions to implementation
mfxStatus MFXMemory_GetSurfaceForVPP(mfxSession session, mfxFrameSurface1 **surface) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXMemory_GetSurfaceForVPP];
    if (pFunc) {
        session = pHandle->session;
        sts = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxFrameSurface1 **))pFunc)(session, surface);
    }

    return sts;
}

mfxStatus MFXMemory_GetSurfaceForVPPOut(mfxSession session, mfxFrameSurface1 **surface) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXMemory_GetSurfaceForVPPOut];
    if (pFunc) {
        session = pHandle->session;
        sts = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxFrameSurface1 **))pFunc)(session, surface);
    }

    return sts;
}

mfxStatus MFXMemory_GetSurfaceForEncode(mfxSession session, mfxFrameSurface1 **surface) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXMemory_GetSurfaceForEncode];
    if (pFunc) {
        session = pHandle->session;
        sts = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxFrameSurface1 **))pFunc)(session, surface);
    }

    return sts;
}

mfxStatus MFXMemory_GetSurfaceForDecode(mfxSession session, mfxFrameSurface1 **surface) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXMemory_GetSurfaceForDecode];
    if (pFunc) {
        session = pHandle->session;
        sts = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxFrameSurface1 **))pFunc)(session, surface);
    }

    return sts;
}

mfxStatus MFXVideoDECODE_VPP_Init(mfxSession session,
                                  mfxVideoParam *decode_par,
                                  mfxVideoChannelParam **vpp_par_array,
                                  mfxU32 num_vpp_par) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoDECODE_VPP_Init];
    if (pFunc) {
        session = pHandle->session;
        sts =
            (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxVideoParam *, mfxVideoChannelParam **, mfxU32))
                 pFunc)(session, decode_par, vpp_par_array, num_vpp_par);
    }

    return sts;
}

mfxStatus MFXVideoDECODE_VPP_DecodeFrameAsync(mfxSession session,
                                              mfxBitstream *bs,
                                              mfxU32 *skip_channels,
                                              mfxU32 num_skip_channels,
                                              mfxSurfaceArray **surf_array_out) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoDECODE_VPP_DecodeFrameAsync];
    if (pFunc) {
        session = pHandle->session;
        sts     = (*(mfxStatus(
            MFX_CDECL *)(mfxSession, mfxBitstream *, mfxU32 *, mfxU32, mfxSurfaceArray **))
                   pFunc)(session, bs, skip_channels, num_skip_channels, surf_array_out);
    }

    return sts;
}

mfxStatus MFXVideoDECODE_VPP_Reset(mfxSession session,
                                   mfxVideoParam *decode_par,
                                   mfxVideoChannelParam **vpp_par_array,
                                   mfxU32 num_vpp_par) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoDECODE_VPP_Reset];
    if (pFunc) {
        session = pHandle->session;
        sts =
            (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxVideoParam *, mfxVideoChannelParam **, mfxU32))
                 pFunc)(session, decode_par, vpp_par_array, num_vpp_par);
    }

    return sts;
}

mfxStatus MFXVideoDECODE_VPP_GetChannelParam(mfxSession session,
                                             mfxVideoChannelParam *par,
                                             mfxU32 channel_id) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoDECODE_VPP_GetChannelParam];
    if (pFunc) {
        session = pHandle->session;
        sts     = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxVideoChannelParam *, mfxU32))pFunc)(
            session,
            par,
            channel_id);
    }

    return sts;
}

mfxStatus MFXVideoDECODE_VPP_Close(mfxSession session) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoDECODE_VPP_Close];
    if (pFunc) {
        session = pHandle->session;
        sts     = (*(mfxStatus(MFX_CDECL *)(mfxSession))pFunc)(session);
    }

    return sts;
}

mfxStatus MFXVideoVPP_ProcessFrameAsync(mfxSession session,
                                        mfxFrameSurface1 *in,
                                        mfxFrameSurface1 **out) {
    mfxStatus sts = MFX_ERR_INVALID_HANDLE;

    if (!session)
        return MFX_ERR_INVALID_HANDLE;

    struct _mfxSession *pHandle = (struct _mfxSession *)session;

    mfxFunctionPointer pFunc;
    pFunc = pHandle->callVideoTable2[eMFXVideoVPP_ProcessFrameAsync];
    if (pFunc) {
        session = pHandle->session;
        sts = (*(mfxStatus(MFX_CDECL *)(mfxSession, mfxFrameSurface1 *, mfxFrameSurface1 **))pFunc)(
            session,
            in,
            out);
    }

    return sts;
}

//
//
// implement all other calling functions.
// They just call a procedure of DLL library from the table.
//

// define for common functions (from mfxsession.h)
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)               \
    return_value func_name formal_param_list {                                                \
        mfxStatus mfxRes     = MFX_ERR_INVALID_HANDLE;                                        \
        _mfxSession *pHandle = (_mfxSession *)session;                                        \
        /* get the function's address and make a call */                                      \
        if (pHandle) {                                                                        \
            /* check whether it is audio session or video */                                  \
            int tableIndex = e##func_name;                                                    \
            mfxFunctionPointer pFunc;                                                         \
            pFunc = pHandle->callTable[tableIndex];                                           \
            if (pFunc) {                                                                      \
                /* get the real session pointer */                                            \
                session = pHandle->session;                                                   \
                /* pass down the call */                                                      \
                mfxRes = (*(mfxStatus(MFX_CDECL *) formal_param_list)pFunc)actual_param_list; \
            }                                                                                 \
        }                                                                                     \
        return mfxRes;                                                                        \
    }

FUNCTION(mfxStatus, MFXQueryIMPL, (mfxSession session, mfxIMPL *impl), (session, impl))
FUNCTION(mfxStatus, MFXQueryVersion, (mfxSession session, mfxVersion *version), (session, version))

// these functions are not necessary in LOADER part of dispatcher and
// need to be included only in in SOLID dispatcher or PROCTABLE part of dispatcher

FUNCTION(mfxStatus, MFXDisjoinSession, (mfxSession session), (session))
FUNCTION(mfxStatus, MFXSetPriority, (mfxSession session, mfxPriority priority), (session, priority))
FUNCTION(mfxStatus,
         MFXGetPriority,
         (mfxSession session, mfxPriority *priority),
         (session, priority))

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)               \
    return_value func_name formal_param_list {                                                \
        mfxStatus mfxRes     = MFX_ERR_INVALID_HANDLE;                                        \
        _mfxSession *pHandle = (_mfxSession *)session;                                        \
        /* get the function's address and make a call */                                      \
        if (pHandle) {                                                                        \
            mfxFunctionPointer pFunc = pHandle->callTable[e##func_name];                      \
            if (pFunc) {                                                                      \
                /* get the real session pointer */                                            \
                session = pHandle->session;                                                   \
                /* pass down the call */                                                      \
                mfxRes = (*(mfxStatus(MFX_CDECL *) formal_param_list)pFunc)actual_param_list; \
            }                                                                                 \
        }                                                                                     \
        return mfxRes;                                                                        \
    }

#include "windows/mfx_exposed_functions_list.h"
