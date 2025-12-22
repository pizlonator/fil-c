/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_VPL_MFX_DISPATCHER_VPL_H_
#define DISPATCHER_VPL_MFX_DISPATCHER_VPL_H_

#include <algorithm>
#include <cstdlib>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "vpl/mfxdispatcher.h"
#include "vpl/mfxvideo.h"

#include "./mfx_dispatcher_vpl_log.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    // use wide char on Windows
    #define MAKE_STRING(x) L##x
typedef std::wstring STRING_TYPE;
typedef wchar_t CHAR_TYPE;
#else
    #include <dirent.h>
    #include <dlfcn.h>
    #include <string.h>
    #include <unistd.h>

    // use standard char on Linux
    #define MAKE_STRING(x) x
typedef std::string STRING_TYPE;
typedef char CHAR_TYPE;
#endif

#if defined(_WIN32) || defined(_WIN64)
    #if defined _M_IX86
        // Windows x86
        #define MSDK_LIB_NAME L"libmfxhw32."
    #else
        // Windows x64
        #define MSDK_LIB_NAME L"libmfxhw64."
    #endif
    #define ONEVPL_PRIORITY_PATH_VAR L"ONEVPL_PRIORITY_PATH"
#elif defined(__linux__)
    // Linux x64
    #define MSDK_LIB_NAME            "libmfxhw64."
    #define ONEVPL_PRIORITY_PATH_VAR "ONEVPL_PRIORITY_PATH"
#endif

#define MSDK_MIN_VERSION_MAJOR 1
#define MSDK_MIN_VERSION_MINOR 0

#define MAX_MSDK_ACCEL_MODES 16 // see mfxcommon.h --> mfxAccelerationMode

#define MAX_WINDOWS_ADAPTER_ID 3 // check adapterID in range [0,3]

#define MAX_NUM_IMPL_MSDK 4

#define MAX_VPL_SEARCH_PATH 4096

#define MAX_ENV_VAR_LEN 32768

#define DEVICE_ID_UNKNOWN   0xffffffff
#define ADAPTER_IDX_UNKNOWN 0xffffffff

#define TAB_SIZE(type, tab) (sizeof(tab) / sizeof(type))
#define MAKE_MFX_VERSION(major, minor) \
    {                                  \
        { (minor), (major) }           \
    }

// internal function to load dll by full path, fail if unsuccessful
mfxStatus MFXInitEx2(mfxVersion version,
                     mfxInitializationParam vplParam,
                     mfxIMPL hwImpl,
                     mfxSession *session,
                     mfxU16 *deviceID,
                     CHAR_TYPE *dllName);

typedef void(MFX_CDECL *VPLFunctionPtr)(void);

extern const mfxIMPL msdkImplTab[MAX_NUM_IMPL_MSDK];

enum LibType {
    LibTypeUnknown = -1,

    LibTypeVPL = 0,
    LibTypeMSDK,

    NumLibTypes
};

enum VPLFunctionIdx {
    // 2.0
    IdxMFXQueryImplsDescription = 0,
    IdxMFXReleaseImplDescription,
    IdxMFXMemory_GetSurfaceForVPP,
    IdxMFXMemory_GetSurfaceForEncode,
    IdxMFXMemory_GetSurfaceForDecode,
    IdxMFXInitialize,

    // 2.1
    IdxMFXMemory_GetSurfaceForVPPOut,
    IdxMFXVideoDECODE_VPP_Init,
    IdxMFXVideoDECODE_VPP_DecodeFrameAsync,
    IdxMFXVideoDECODE_VPP_Reset,
    IdxMFXVideoDECODE_VPP_GetChannelParam,
    IdxMFXVideoDECODE_VPP_Close,
    IdxMFXVideoVPP_ProcessFrameAsync,

    NumVPLFunctions
};

// select MSDK functions for 1.x style caps query
enum MSDKCompatFunctionIdx {
    IdxMFXInitEx = 0,
    IdxMFXClose,

    NumMSDKFunctions
};

// both Windows and Linux use char* for function names
struct VPLFunctionDesc {
    const char *pName;
    mfxVersion apiVersion;
};

// DX adapter info
struct DXGI1DeviceInfo {
    mfxU32 vendorID;
    mfxU32 deviceID;
    mfxU64 luid;
};

// priority of runtime loading, based on oneAPI-spec
enum LibPriority {
    LIB_PRIORITY_SPECIAL = 0, // highest priority regardless of other priority rules

    LIB_PRIORITY_01 = 1,
    LIB_PRIORITY_02 = 2,
    LIB_PRIORITY_03 = 3,
    LIB_PRIORITY_04 = 4,
    LIB_PRIORITY_05 = 5,

    LIB_PRIORITY_LEGACY_DRIVERSTORE = 10000,
    LIB_PRIORITY_LEGACY,
};

enum CfgPropState {
    CFG_PROP_STATE_NOT_SET = 0,
    CFG_PROP_STATE_SUPPORTED,
    CFG_PROP_STATE_UNSUPPORTED,
};

enum PropRanges {
    PROP_RANGE_DEC_W = 0,
    PROP_RANGE_DEC_H,
    PROP_RANGE_ENC_W,
    PROP_RANGE_ENC_H,
    PROP_RANGE_VPP_W,
    PROP_RANGE_VPP_H,

    NUM_PROP_RANGES
};

// must match eProp_TotalProps, is checked with static_assert in _config.cpp
//   (should throw error at compile time if !=)
#define NUM_TOTAL_FILTER_PROPS 56

// typedef child structures for easier reading
typedef struct mfxDecoderDescription::decoder DecCodec;
typedef struct mfxDecoderDescription::decoder::decprofile DecProfile;
typedef struct mfxDecoderDescription::decoder::decprofile::decmemdesc DecMemDesc;

typedef struct mfxEncoderDescription::encoder EncCodec;
typedef struct mfxEncoderDescription::encoder::encprofile EncProfile;
typedef struct mfxEncoderDescription::encoder::encprofile::encmemdesc EncMemDesc;

typedef struct mfxVPPDescription::filter VPPFilter;
typedef struct mfxVPPDescription::filter::memdesc VPPMemDesc;
typedef struct mfxVPPDescription::filter::memdesc::format VPPFormat;

// flattened version of single enc/dec/vpp configs
// each struct contains all _settable_ props
//   i.e. not implied values like NumCodecs
struct DecConfig {
    mfxU32 CodecID;
    mfxU16 MaxcodecLevel;
    mfxU32 Profile;
    mfxResourceType MemHandleType;
    mfxRange32U Width;
    mfxRange32U Height;
    mfxU32 ColorFormat;
};

struct EncConfig {
    mfxU32 CodecID;
    mfxU16 MaxcodecLevel;
    mfxU16 BiDirectionalPrediction;
    mfxU16 ReportedStats;
    mfxU32 Profile;
    mfxResourceType MemHandleType;
    mfxRange32U Width;
    mfxRange32U Height;
    mfxU32 ColorFormat;
};

struct VPPConfig {
    mfxU32 FilterFourCC;
    mfxU16 MaxDelayInFrames;
    mfxResourceType MemHandleType;
    mfxRange32U Width;
    mfxRange32U Height;
    mfxU32 InFormat;
    mfxU32 OutFormat;
};

// special props which are passed in via MFXSetConfigProperty()
// these are updated with every call to ValidateConfig() and may
//   be used in MFXCreateSession()
struct SpecialConfig {
    bool bIsSet_deviceHandleType;
    mfxHandleType deviceHandleType;

    bool bIsSet_deviceHandle;
    mfxHDL deviceHandle;

    bool bIsSet_accelerationMode;
    mfxAccelerationMode accelerationMode;

    bool bIsSet_ApiVersion;
    mfxVersion ApiVersion;

    bool bIsSet_dxgiAdapterIdx;
    mfxU32 dxgiAdapterIdx;

    bool bIsSet_NumThread;
    mfxU32 NumThread;

    bool bIsSet_DeviceCopy;
    mfxU16 DeviceCopy;

    bool bIsSet_ExtBuffer;
    std::vector<mfxExtBuffer *> ExtBuffers;
};

// config class implementation
class ConfigCtxVPL {
public:
    ConfigCtxVPL();
    ~ConfigCtxVPL();

    // set a single filter property (KV pair)
    mfxStatus SetFilterProperty(const mfxU8 *name, mfxVariant value);

    static bool CheckLowLatencyConfig(std::list<ConfigCtxVPL *> configCtxList,
                                      SpecialConfig *specialConfig);

    // compare library caps vs. set of configuration filters
    static mfxStatus ValidateConfig(const mfxImplDescription *libImplDesc,
                                    const mfxImplementedFunctions *libImplFuncs,
#ifdef ONEVPL_EXPERIMENTAL
                                    const mfxExtendedDeviceId *libImplExtDevID,
#endif
                                    std::list<ConfigCtxVPL *> configCtxList,
                                    LibType libType,
                                    SpecialConfig *specialConfig);

    // parse deviceID for x86 devices
    static bool ParseDeviceIDx86(mfxChar *cDeviceID, mfxU32 &deviceID, mfxU32 &adapterIdx);

    // loader object this config is associated with - needed to
    //   rebuild valid implementation list after each calling
    //   MFXSetConfigFilterProperty()
    class LoaderCtxVPL *m_parentLoader;

private:
    static __inline std::string GetNextProp(std::list<std::string> &s) {
        if (s.empty())
            return "";
        std::string t = s.front();
        s.pop_front();
        return t;
    }

    mfxStatus ValidateAndSetProp(mfxI32 idx, mfxVariant value);
    mfxStatus SetFilterPropertyDec(std::list<std::string> &propParsedString, mfxVariant value);
    mfxStatus SetFilterPropertyEnc(std::list<std::string> &propParsedString, mfxVariant value);
    mfxStatus SetFilterPropertyVPP(std::list<std::string> &propParsedString, mfxVariant value);

    static mfxStatus GetFlatDescriptionsDec(const mfxImplDescription *libImplDesc,
                                            std::list<DecConfig> &decConfigList);

    static mfxStatus GetFlatDescriptionsEnc(const mfxImplDescription *libImplDesc,
                                            std::list<EncConfig> &encConfigList);

    static mfxStatus GetFlatDescriptionsVPP(const mfxImplDescription *libImplDesc,
                                            std::list<VPPConfig> &vppConfigList);

    static mfxStatus CheckPropsGeneral(const mfxVariant cfgPropsAll[],
                                       const mfxImplDescription *libImplDesc);

    static mfxStatus CheckPropsDec(const mfxVariant cfgPropsAll[],
                                   std::list<DecConfig> decConfigList);

    static mfxStatus CheckPropsEnc(const mfxVariant cfgPropsAll[],
                                   std::list<EncConfig> encConfigList);

    static mfxStatus CheckPropsVPP(const mfxVariant cfgPropsAll[],
                                   std::list<VPPConfig> vppConfigList);

    static mfxStatus CheckPropString(const mfxChar *implString, const std::string filtString);

#ifdef ONEVPL_EXPERIMENTAL
    static mfxStatus CheckPropsExtDevID(const mfxVariant cfgPropsAll[],
                                        const mfxExtendedDeviceId *libImplExtDevID);

#endif

    mfxVariant m_propVar[NUM_TOTAL_FILTER_PROPS];

    // special containers for properties which are passed by pointer
    //   (save a copy of the whole object based on property name)
    mfxRange32U m_propRange32U[NUM_PROP_RANGES];
    std::string m_implName;
    std::string m_implLicense;
    std::string m_implKeywords;
    std::string m_deviceIdStr;
    std::string m_implFunctionName;

    mfxU8 m_extDevLUID8U[8];
    std::string m_extDevNameStr;

    std::vector<mfxU8> m_extBuf;

    __inline bool SetExtBuf(mfxExtBuffer *extBuf) {
        if (!extBuf)
            return false;

        mfxU32 BufferSz = extBuf->BufferSz;
        if (BufferSz > 0) {
            m_extBuf.resize(BufferSz);
            std::copy((mfxU8 *)extBuf, (mfxU8 *)extBuf + BufferSz, m_extBuf.begin());
            return true;
        }
        return false;
    }

    __inline bool GetExtBuf(mfxExtBuffer **extBuf) {
        if (!extBuf)
            return false;

        *extBuf = nullptr;
        if (!m_extBuf.empty()) {
            *extBuf = (mfxExtBuffer *)m_extBuf.data();
            return true;
        }
        return false;
    }

    __inline void ClearExtBuf() {
        m_extBuf.clear();
    }
};

// MSDK compatibility loader implementation
class LoaderCtxMSDK {
public:
    LoaderCtxMSDK();
    ~LoaderCtxMSDK();

    // public function to be called by VPL dispatcher
    // do not allocate any new memory here, so no need for a matching Release functions
    mfxStatus QueryMSDKCaps(STRING_TYPE libNameFull,
                            mfxImplDescription **implDesc,
                            mfxImplementedFunctions **implFuncs,
                            mfxU32 adapterID,
                            bool bSkipD3D9Check);

    static mfxStatus QueryAPIVersion(STRING_TYPE libNameFull, mfxVersion *msdkVersion);

#ifdef ONEVPL_EXPERIMENTAL
    static mfxStatus QueryExtDeviceID(mfxExtendedDeviceId *extDeviceID,
                                      mfxU32 adapterID,
                                      mfxU16 deviceID,
                                      mfxU64 luid);
#endif

    // required by MFXCreateSession
    mfxIMPL m_msdkAdapter;
    mfxIMPL m_msdkAdapterD3D9;

    mfxU16 m_deviceID;
    mfxU64 m_luid;

#ifdef ONEVPL_EXPERIMENTAL
    mfxExtendedDeviceId m_extDeviceID;
#endif

private:
    // session management
    mfxStatus OpenSession(mfxSession *session,
                          STRING_TYPE libNameFull,
                          mfxAccelerationMode accelMode,
                          mfxIMPL hwImpl);
    void CloseSession(mfxSession *session);

    // utility functions
    static mfxAccelerationMode CvtAccelType(mfxIMPL implType, mfxIMPL implMethod);
    static mfxStatus GetDefaultAccelType(mfxU32 adapterID, mfxIMPL *implDefault, mfxU64 *luid);
    static mfxStatus CheckD3D9Support(mfxU64 luid, STRING_TYPE libNameFull, mfxIMPL *implD3D9);
    static mfxStatus GetRenderNodeDescription(mfxU32 adapterID, mfxU32 &vendorID, mfxU16 &deviceID);

    // internal state variables
    STRING_TYPE m_libNameFull;
    mfxImplDescription m_id; // base description struct
    mfxAccelerationMode m_accelMode[MAX_MSDK_ACCEL_MODES];
    mfxU16 m_loaderDeviceID;

    __inline bool IsVersionSupported(mfxVersion reqVersion, mfxVersion actualVersion) {
        if (actualVersion.Major > reqVersion.Major) {
            return true;
        }
        else if ((actualVersion.Major == reqVersion.Major) &&
                 (actualVersion.Minor >= reqVersion.Minor)) {
            return true;
        }
        return false;
    }
};

struct LibInfo {
    // during search store candidate file names
    //   and priority based on rules in spec
    STRING_TYPE libNameFull;
    mfxU32 libPriority;
    LibType libType;

    // if valid library, store file handle
    //   and table of exported functions
    void *hModuleVPL;
    VPLFunctionPtr vplFuncTable[NumVPLFunctions]; // NOLINT

    // loader context for legacy MSDK
    LoaderCtxMSDK msdkCtx[MAX_NUM_IMPL_MSDK];

    // API version of legacy MSDK
    mfxVersion msdkVersion;

    // user-friendly version of path for MFX_IMPLCAPS_IMPLPATH query
    mfxChar implCapsPath[MAX_VPL_SEARCH_PATH];

    // avoid warnings
    LibInfo()
            : libNameFull(),
              libPriority(0),
              libType(LibTypeUnknown),
              hModuleVPL(nullptr),
              vplFuncTable(),
              msdkCtx(),
              msdkVersion(),
              implCapsPath() {}

private:
    // make this class non-copyable
    LibInfo(const LibInfo &);
    void operator=(const LibInfo &);
};

struct ImplInfo {
    // library containing this implementation
    LibInfo *libInfo;

    // description of implementation
    mfxHDL implDesc;

    // list of implemented functions
    mfxHDL implFuncs;

#ifdef ONEVPL_EXPERIMENTAL
    mfxHDL implExtDeviceID;
#endif

    // used for session initialization with this implementation
    mfxInitializationParam vplParam;
    mfxVersion version;

    // if MSDK library, index of corresponding adapter (i.e. which LoaderCtxMSDK)
    mfxU32 msdkImplIdx;

    // adapter index in multi-adapter systems
    mfxU32 adapterIdx;

    // local index for libraries with more than one implementation
    mfxU32 libImplIdx;

    // index of valid libraries - updates with every call to MFXSetConfigFilterProperty()
    mfxI32 validImplIdx;

    // avoid warnings
    ImplInfo()
            : libInfo(nullptr),
              implDesc(nullptr),
              implFuncs(nullptr),
#ifdef ONEVPL_EXPERIMENTAL
              implExtDeviceID(nullptr),
#endif
              vplParam(),
              version(),
              msdkImplIdx(0),
              adapterIdx(ADAPTER_IDX_UNKNOWN),
              libImplIdx(0),
              validImplIdx(-1) {
    }
};

// loader class implementation
class LoaderCtxVPL {
public:
    LoaderCtxVPL();
    ~LoaderCtxVPL();

    // manage library implementations
    mfxStatus BuildListOfCandidateLibs();
    mfxU32 CheckValidLibraries();
    mfxStatus QueryLibraryCaps();
    mfxStatus UnloadAllLibraries();

    // query capabilities of each implementation
    mfxStatus FullLoadAndQuery();
    mfxStatus QueryImpl(mfxU32 idx, mfxImplCapsDeliveryFormat format, mfxHDL *idesc);
    mfxStatus ReleaseImpl(mfxHDL idesc);

    // update list of valid implementations based on current filter props
    mfxStatus UpdateValidImplList(void);
    mfxStatus PrioritizeImplList(void);

    // create mfxSession
    mfxStatus CreateSession(mfxU32 idx, mfxSession *session);

    // manage configuration filters
    ConfigCtxVPL *AddConfigFilter();
    mfxStatus FreeConfigFilters();

    // manage logging
    mfxStatus InitDispatcherLog();
    DispatcherLogVPL *GetLogger();

    // low latency initialization
    mfxStatus LoadLibsLowLatency();
    mfxStatus UpdateLowLatency();

    bool m_bLowLatency;
    bool m_bNeedUpdateValidImpls;
    bool m_bNeedFullQuery;
    bool m_bNeedLowLatencyQuery;
    bool m_bPriorityPathEnabled;

private:
    // helper functions
    mfxStatus LoadSingleLibrary(LibInfo *libInfo);
    mfxStatus UnloadSingleLibrary(LibInfo *libInfo);
    mfxStatus UnloadSingleImplementation(ImplInfo *implInfo);
    VPLFunctionPtr GetFunctionAddr(void *hModuleVPL, const char *pName);

    mfxU32 GetSearchPathsDriverStore(std::list<STRING_TYPE> &searchDirs, LibType libType);
    mfxU32 GetSearchPathsSystemDefault(std::list<STRING_TYPE> &searchDirs);
    mfxU32 GetSearchPathsCurrentExe(std::list<STRING_TYPE> &searchDirs);
    mfxU32 GetSearchPathsCurrentDir(std::list<STRING_TYPE> &searchDirs);
    mfxU32 GetSearchPathsLegacy(std::list<STRING_TYPE> &searchDirs);

    mfxU32 ParseEnvSearchPaths(const CHAR_TYPE *envVarName, std::list<STRING_TYPE> &searchDirs);
    mfxU32 ParseLegacySearchPaths(std::list<STRING_TYPE> &searchDirs);

    mfxStatus SearchDirForLibs(STRING_TYPE searchDir,
                               std::list<LibInfo *> &libInfoList,
                               mfxU32 priority,
                               bool bLoadVPLOnly = false);

    mfxU32 LoadAPIExports(LibInfo *libInfo, LibType libType);
    mfxStatus ValidateAPIExports(VPLFunctionPtr *vplFuncTable, mfxVersion reportedVersion);
    bool IsValidX86GPU(ImplInfo *implInfo, mfxU32 &deviceID, mfxU32 &adapterIdx);
    mfxStatus UpdateImplPath(LibInfo *libInfo);

    mfxStatus LoadLibsFromDriverStore(mfxU32 numAdapters,
                                      const std::vector<DXGI1DeviceInfo> &adapterInfo,
                                      LibType libType);
    mfxStatus LoadLibsFromSystemDir(LibType libType);
    mfxStatus LoadLibsFromMultipleDirs(LibType libType);

    LibInfo *AddSingleLibrary(STRING_TYPE libPath, LibType libType);
    mfxStatus QuerySessionLowLatency(LibInfo *libInfo, mfxU32 adapterID, mfxVersion *ver);

    std::list<LibInfo *> m_libInfoList;
    std::list<ImplInfo *> m_implInfoList;
    std::list<ConfigCtxVPL *> m_configCtxList;
    std::vector<DXGI1DeviceInfo> m_gpuAdapterInfo;

    SpecialConfig m_specialConfig;

    mfxU32 m_implIdxNext;
    bool m_bKeepCapsUntilUnload;
    CHAR_TYPE m_envVar[MAX_ENV_VAR_LEN];

    // logger object - enabled with ONEVPL_DISPATCHER_LOG environment variable
    DispatcherLogVPL m_dispLog;
};

#endif // DISPATCHER_VPL_MFX_DISPATCHER_VPL_H_
