/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "vpl/mfx_dispatcher_vpl.h"

#if defined(_WIN32) || defined(_WIN64)
    #include "vpl/mfx_dispatcher_vpl_win.h"

    #if defined _M_IX86
        // Windows x86
        #define LIB_ONEVPL L"libmfx32-gen.dll"
        #define LIB_MSDK   L"libmfxhw32.dll"
    #else
        // Windows x64
        #define LIB_ONEVPL L"libmfx64-gen.dll"
        #define LIB_MSDK   L"libmfxhw64.dll"
    #endif
#elif defined(__linux__)
    // Linux x64
    #define LIB_ONEVPL "libmfx-gen.so.1.2"
    #define LIB_MSDK   "libmfxhw64.so.1"
#endif

// For Windows:
//  VPL - load from Driver Store, look only for libmfx64-gen.dll (32)
//  MSDK - load from Driver Store, look only for libmfxhw64.dll (32)
//  MSDK - fallback, load from %windir%\system32 or %windir%\syswow64

// For Linux:
//  VPL - load from system paths in LoadLibsFromMultipleDirs(), look only for libmfx-gen.so.1.2
//  MSDK - load from system paths in LoadLibsFromMultipleDirs(), look only for libmfxhw64.so.1

// library names
static const CHAR_TYPE *libNameVPL  = LIB_ONEVPL;
static const CHAR_TYPE *libNameMSDK = LIB_MSDK;

// required exports
static const char *reqFuncVPL  = "MFXInitialize";
static const char *reqFuncMSDK = "MFXInitEx";

LibInfo *LoaderCtxVPL::AddSingleLibrary(STRING_TYPE libPath, LibType libType) {
    LibInfo *libInfo = nullptr;

#if defined(_WIN32) || defined(_WIN64)
    // try to open library
    mfxModuleHandle hLib = MFX::mfx_dll_load(libPath.c_str());
    if (!hLib)
        return nullptr;

    // check for required entrypoint function
    const char *reqFunc  = (libType == LibTypeVPL ? reqFuncVPL : reqFuncMSDK);
    VPLFunctionPtr pProc = (VPLFunctionPtr)MFX::mfx_dll_get_addr(hLib, reqFunc);
    MFX::mfx_dll_free(hLib);

    // entrypoint function missing - invalid library
    if (!pProc)
        return nullptr;
#else
    // try to open library
    void *hLib = dlopen(libPath.c_str(), RTLD_LOCAL | RTLD_NOW);
    if (!hLib)
        return nullptr;

    // check for required entrypoint function
    const char *reqFunc  = (libType == LibTypeVPL ? reqFuncVPL : reqFuncMSDK);
    VPLFunctionPtr pProc = (VPLFunctionPtr)dlsym(hLib, reqFunc);
    dlclose(hLib);

    // entrypoint function missing - invalid library
    if (!pProc)
        return nullptr;
#endif

    // create new LibInfo and add to list
    libInfo = new LibInfo;
    if (!libInfo)
        return nullptr;

    libInfo->libNameFull = libPath;
    libInfo->libType     = libType;
    libInfo->libPriority = (libType == LibTypeVPL ? LIB_PRIORITY_01 : LIB_PRIORITY_LEGACY);

    return libInfo;
}

mfxStatus LoaderCtxVPL::LoadLibsFromDriverStore(mfxU32 numAdapters,
                                                const std::vector<DXGI1DeviceInfo> &adapterInfo,
                                                LibType libType) {
#if defined(_WIN32) || defined(_WIN64)
    mfxStatus sts = MFX_ERR_NONE;

    mfxI32 storageID         = MFX::MFX_UNKNOWN_KEY;
    const CHAR_TYPE *libName = nullptr;
    const char *reqFunc      = nullptr;

    if (libType == LibTypeVPL) {
        storageID = MFX::MFX_DRIVER_STORE_ONEVPL;
        libName   = libNameVPL;
        reqFunc   = reqFuncVPL;
    }
    else if (libType == LibTypeMSDK) {
        storageID = MFX::MFX_DRIVER_STORE;
        libName   = libNameMSDK;
        reqFunc   = reqFuncMSDK;
    }
    else {
        return MFX_ERR_UNSUPPORTED;
    }

    // get path to Windows driver store
    STRING_TYPE libPath;
    mfxU32 adapterID;
    for (adapterID = 0; adapterID < numAdapters; adapterID++) {
        // get driver store path for this adapter
        libPath.clear();
        sts = MFX::MFXLibraryIterator::GetDriverStoreDir(libPath,
                                                         MAX_VPL_SEARCH_PATH,
                                                         adapterInfo[adapterID].deviceID,
                                                         storageID);
        if (sts != MFX_ERR_NONE || libPath.size() == 0)
            continue;

        // try to open library
        libPath += libName;
        LibInfo *libInfo = AddSingleLibrary(libPath, libType);

        // if successful, add to list and return (stop at first
        if (libInfo) {
            m_libInfoList.push_back(libInfo);
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNSUPPORTED;
#else
    // Linux - not supported
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus LoaderCtxVPL::LoadLibsFromSystemDir(LibType libType) {
#if defined(_WIN32) || defined(_WIN64)
    mfxStatus sts = MFX_ERR_NONE;

    const CHAR_TYPE *libName = nullptr;
    const char *reqFunc      = nullptr;

    if (libType == LibTypeVPL) {
        libName = libNameVPL;
        reqFunc = reqFuncVPL;
    }
    else if (libType == LibTypeMSDK) {
        libName = libNameMSDK;
        reqFunc = reqFuncMSDK;
    }
    else {
        return MFX_ERR_UNSUPPORTED;
    }

    // get path to Windows system dir
    STRING_TYPE libPath;
    libPath.clear();

    std::list<STRING_TYPE> winSysDir;
    ParseEnvSearchPaths(L"windir", winSysDir);

    // should resolve to a single directory, otherwise something went wrong
    if (winSysDir.size() == 1) {
    #if defined _M_IX86
        libPath = winSysDir.front() + L"\\syswow64\\";
    #else
        libPath = winSysDir.front() + L"\\system32\\";
    #endif
    }
    else {
        return MFX_ERR_UNSUPPORTED;
    }

    // try to open library
    libPath += libName;
    LibInfo *libInfo = AddSingleLibrary(libPath, libType);

    // if successful, add to list and return (stop at first
    if (libInfo) {
        m_libInfoList.push_back(libInfo);
        return MFX_ERR_NONE;
    }

    return MFX_ERR_UNSUPPORTED;
#else
    // Linux - not supported
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus LoaderCtxVPL::LoadLibsFromMultipleDirs(LibType libType) {
#ifdef __linux__
    // clang-format off

    // standard paths for RT installation on Linux
    std::vector<std::string> llSearchDir = {
        "/usr/lib/x86_64-linux-gnu",
        "/lib",
        "/usr/lib",
        "/lib64",
        "/usr/lib64",
    };

    // clang-format on

    const CHAR_TYPE *libName = nullptr;

    if (libType == LibTypeVPL) {
        libName = libNameVPL;
    }
    else if (libType == LibTypeMSDK) {
        libName = libNameMSDK;

        // additional search directories for MSDK
        llSearchDir.push_back("/opt/intel/mediasdk/lib");
        llSearchDir.push_back("/opt/intel/mediasdk/lib64");
    }
    else {
        return MFX_ERR_UNSUPPORTED;
    }

    for (const auto &searchDir : llSearchDir) {
        STRING_TYPE libPath;
        libPath = searchDir;
        libPath += "/";
        libPath += libName;

        // try to open library
        LibInfo *libInfo = AddSingleLibrary(libPath, libType);

        // if successful, add to list and return (stop at first success)
        if (libInfo) {
            m_libInfoList.push_back(libInfo);
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNSUPPORTED;
#else
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus LoaderCtxVPL::LoadLibsLowLatency() {
    DISP_LOG_FUNCTION(&m_dispLog);

#if defined(_WIN32) || defined(_WIN64)
    mfxStatus sts = MFX_ERR_NONE;

    // check driver store
    mfxU32 numAdapters = 0;

    std::vector<DXGI1DeviceInfo> adapterInfo;
    bool bEnumSuccess = MFX::DXGI1Device::GetAdapterList(adapterInfo);
    numAdapters       = (mfxU32)adapterInfo.size();

    // error - no graphics adapters found
    if (!bEnumSuccess || numAdapters == 0)
        return MFX_ERR_UNSUPPORTED;

    // try loading oneVPL from driver store
    sts = LoadLibsFromDriverStore(numAdapters, adapterInfo, LibTypeVPL);
    if (sts == MFX_ERR_NONE) {
        LibInfo *libInfo = m_libInfoList.back();

        sts = LoadSingleLibrary(libInfo);
        if (sts == MFX_ERR_NONE) {
            LoadAPIExports(libInfo, LibTypeVPL);
            m_bNeedLowLatencyQuery = false;
            return MFX_ERR_NONE;
        }
        UnloadSingleLibrary(libInfo); // failed - unload and move to next location
    }

    // try loading MSDK from driver store
    sts = LoadLibsFromDriverStore(numAdapters, adapterInfo, LibTypeMSDK);
    if (sts == MFX_ERR_NONE) {
        LibInfo *libInfo = m_libInfoList.back();

        sts = LoadSingleLibrary(libInfo);
        if (sts == MFX_ERR_NONE) {
            mfxU32 numFunctions = LoadAPIExports(libInfo, LibTypeMSDK);

            if (numFunctions == NumMSDKFunctions) {
                mfxVariant var = {};
                var.Type       = MFX_VARIANT_TYPE_PTR;
                var.Data.Ptr   = (mfxHDL) "mfxhw64";

                auto it = m_configCtxList.begin();

                while (it != m_configCtxList.end()) {
                    ConfigCtxVPL *config = (*it);
                    sts = config->SetFilterProperty((const mfxU8 *)"mfxImplDescription.ImplName",
                                                    var);
                    if (sts != MFX_ERR_NONE)
                        return MFX_ERR_UNSUPPORTED;
                    it++;
                }

                m_bNeedLowLatencyQuery = false;
                return MFX_ERR_NONE;
            }
        }
        UnloadSingleLibrary(libInfo); // failed - unload and move to next location
    }

    // try loading MSDK from sysdir %windir%\system32 and %windir%\syswow64
    sts = LoadLibsFromSystemDir(LibTypeMSDK);
    if (sts == MFX_ERR_NONE) {
        LibInfo *libInfo = m_libInfoList.front();

        sts = LoadSingleLibrary(libInfo);
        if (sts == MFX_ERR_NONE) {
            mfxU32 numFunctions = LoadAPIExports(libInfo, LibTypeMSDK);

            if (numFunctions == NumMSDKFunctions) {
                mfxVariant var = {};
                var.Type       = MFX_VARIANT_TYPE_PTR;
                var.Data.Ptr   = (mfxHDL) "mfxhw64";

                auto it = m_configCtxList.begin();

                while (it != m_configCtxList.end()) {
                    ConfigCtxVPL *config = (*it);
                    sts = config->SetFilterProperty((const mfxU8 *)"mfxImplDescription.ImplName",
                                                    var);
                    if (sts != MFX_ERR_NONE)
                        return MFX_ERR_UNSUPPORTED;
                    it++;
                }
                m_bNeedLowLatencyQuery = false;
                return MFX_ERR_NONE;
            }
        }
        UnloadSingleLibrary(libInfo); // failed - unload and move to next location
    }

    return MFX_ERR_UNSUPPORTED;
#else
    mfxStatus sts = MFX_ERR_NONE;

    // try loading VPL from Linux system directories
    sts = LoadLibsFromMultipleDirs(LibTypeVPL);
    if (sts == MFX_ERR_NONE) {
        LibInfo *libInfo = m_libInfoList.back();

        sts = LoadSingleLibrary(libInfo);
        if (sts == MFX_ERR_NONE) {
            LoadAPIExports(libInfo, LibTypeVPL);
            m_bNeedLowLatencyQuery = false;
            return MFX_ERR_NONE;
        }
        UnloadSingleLibrary(libInfo); // failed - unload and move to next location
    }

    // try loading MSDK from Linux system directories
    sts = LoadLibsFromMultipleDirs(LibTypeMSDK);
    if (sts == MFX_ERR_NONE) {
        LibInfo *libInfo = m_libInfoList.back();

        sts = LoadSingleLibrary(libInfo);
        if (sts == MFX_ERR_NONE) {
            mfxU32 numFunctions = LoadAPIExports(libInfo, LibTypeMSDK);

            if (numFunctions == NumMSDKFunctions) {
                mfxVariant var = {};
                var.Type       = MFX_VARIANT_TYPE_PTR;
                var.Data.Ptr   = (mfxHDL) "mfxhw64";

                auto it = m_configCtxList.begin();

                while (it != m_configCtxList.end()) {
                    ConfigCtxVPL *config = (*it);
                    sts = config->SetFilterProperty((const mfxU8 *)"mfxImplDescription.ImplName",
                                                    var);
                    if (sts != MFX_ERR_NONE)
                        return MFX_ERR_UNSUPPORTED;
                    it++;
                }
                m_bNeedLowLatencyQuery = false;
                return MFX_ERR_NONE;
            }
        }
        UnloadSingleLibrary(libInfo); // failed - unload and move to next location
    }

    return MFX_ERR_UNSUPPORTED;
#endif
}

// try creating a session in order to get runtime API version
mfxStatus LoaderCtxVPL::QuerySessionLowLatency(LibInfo *libInfo,
                                               mfxU32 adapterID,
                                               mfxVersion *ver) {
    mfxStatus sts;
    mfxSession session = nullptr;

    mfxVersion reqVersion;
    if (libInfo->libType == LibTypeVPL) {
        reqVersion.Major = 2;
        reqVersion.Minor = 0;
    }
    else {
        reqVersion.Major = 1;
        reqVersion.Minor = 0;
    }

    // set acceleration mode
    mfxInitializationParam vplParam = {};
    vplParam.AccelerationMode       = m_specialConfig.accelerationMode;

    // set adapter ID for both MSDK and VPL
    vplParam.VendorImplID = adapterID;
    mfxIMPL hwImpl        = msdkImplTab[adapterID];

    mfxU16 deviceID;
    sts = MFXInitEx2(reqVersion,
                     vplParam,
                     hwImpl,
                     &session,
                     &deviceID,
                     (CHAR_TYPE *)libInfo->libNameFull.c_str());

    if (sts == MFX_ERR_NONE) {
        sts = MFXQueryVersion(session, ver);
        MFXClose(session);
    }

    return sts;
}
