/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include <fstream>
#include "vpl/mfx_dispatcher_vpl.h"

#if defined(_WIN32) || defined(_WIN64)
    #include "vpl/mfx_dispatcher_vpl_win.h"
#endif

#ifdef __linux__
    #include <pthread.h>
    #define strncpy_s(dst, size, src, cnt) strncpy((dst), (src), (cnt)) // NOLINT
#endif

// leave table formatting alone
// clang-format off

static const mfxChar strImplName[MFX_IMPL_NAME_LEN] = "mfxhw64";
static const mfxChar strLicense[MFX_STRFIELD_LEN]   = "";

#if defined _M_IX86
static const mfxChar strKeywords[MFX_STRFIELD_LEN] = "MSDK,x86";
#else
static const mfxChar strKeywords[MFX_STRFIELD_LEN] = "MSDK,x64";
#endif

// also used in main loader routine
const mfxIMPL msdkImplTab[MAX_NUM_IMPL_MSDK] = {
    MFX_IMPL_HARDWARE,
    MFX_IMPL_HARDWARE2,
    MFX_IMPL_HARDWARE3,
    MFX_IMPL_HARDWARE4,
};

// not relevant for 1.x runtimes (no internal memory management)
#define NUM_POOL_POLICIES_MSDK 0

static const mfxPoolPolicyDescription PoolPolicies = {
    {{ 0, 1 }},                   // struct Version
    {},                         // reserved
    NUM_POOL_POLICIES_MSDK,     // NumPoolPolicies
    nullptr,
};

// 1.x function names should match list in enum eFunc
static const mfxChar* msdkImplFuncsNames[] = {
    "MFXInit",
    "MFXClose",
    "MFXQueryIMPL",
    "MFXQueryVersion",
    "MFXJoinSession",
    "MFXDisjoinSession",
    "MFXCloneSession",
    "MFXSetPriority",
    "MFXGetPriority",
    "MFXInitEx",
    "MFXVideoCORE_SetFrameAllocator",
    "MFXVideoCORE_SetHandle",
    "MFXVideoCORE_GetHandle",
    "MFXVideoCORE_SyncOperation",
    "MFXVideoENCODE_Query",
    "MFXVideoENCODE_QueryIOSurf",
    "MFXVideoENCODE_Init",
    "MFXVideoENCODE_Reset",
    "MFXVideoENCODE_Close",
    "MFXVideoENCODE_GetVideoParam",
    "MFXVideoENCODE_GetEncodeStat",
    "MFXVideoENCODE_EncodeFrameAsync",
    "MFXVideoDECODE_Query",
    "MFXVideoDECODE_DecodeHeader",
    "MFXVideoDECODE_QueryIOSurf",
    "MFXVideoDECODE_Init",
    "MFXVideoDECODE_Reset",
    "MFXVideoDECODE_Close",
    "MFXVideoDECODE_GetVideoParam",
    "MFXVideoDECODE_GetDecodeStat",
    "MFXVideoDECODE_SetSkipMode",
    "MFXVideoDECODE_GetPayload",
    "MFXVideoDECODE_DecodeFrameAsync",
    "MFXVideoVPP_Query",
    "MFXVideoVPP_QueryIOSurf",
    "MFXVideoVPP_Init",
    "MFXVideoVPP_Reset",
    "MFXVideoVPP_Close",
    "MFXVideoVPP_GetVideoParam",
    "MFXVideoVPP_GetVPPStat",
    "MFXVideoVPP_RunFrameVPPAsync",
    "MFXVideoCORE_QueryPlatform",
};

static const mfxImplementedFunctions msdkImplFuncs = {
    sizeof(msdkImplFuncsNames) / sizeof(mfxChar*),
    (mfxChar**)msdkImplFuncsNames
};

// optional extBuf to limit threads created in MSDK session creation
// to enable, set vplParam.NumExtParam and vplParam.ExtParam before calling MFXInitEx2()
static const mfxExtThreadsParam extThreadParam = {
    {MFX_EXTBUFF_THREADS_PARAM, sizeof(mfxExtThreadsParam)},
    2,
    0,
    0,
    {},
};

static const mfxExtBuffer* extParams[1] = { 
    (mfxExtBuffer *)&extThreadParam,
};

// end table formatting
// clang-format on

LoaderCtxMSDK::LoaderCtxMSDK()
        : m_msdkAdapter(),
          m_msdkAdapterD3D9(),
          m_deviceID(0),
          m_luid(0),
#ifdef ONEVPL_EXPERIMENTAL
          m_extDeviceID(),
#endif
          m_libNameFull(),
          m_id(),
          m_accelMode(),
          m_loaderDeviceID(0) {
}

LoaderCtxMSDK::~LoaderCtxMSDK() {}

mfxStatus LoaderCtxMSDK::OpenSession(mfxSession *session,
                                     STRING_TYPE libNameFull,
                                     mfxAccelerationMode accelMode,
                                     mfxIMPL hwImpl) {
    // require API 1.0 or later (both MFXInit and MFXInitEx supported)
    mfxVersion reqVersion;
    reqVersion.Major = MSDK_MIN_VERSION_MAJOR;
    reqVersion.Minor = MSDK_MIN_VERSION_MINOR;

    // set acceleration mode - will be mapped to 1.x API
    mfxInitializationParam vplParam = {};
    vplParam.AccelerationMode       = accelMode;

#ifdef __linux__
    vplParam.ExtParam    = (mfxExtBuffer **)&extParams;
    vplParam.NumExtParam = 1;
#else
    vplParam.ExtParam    = nullptr;
    vplParam.NumExtParam = 0;
#endif

    return MFXInitEx2(reqVersion,
                      vplParam,
                      hwImpl,
                      session,
                      &m_loaderDeviceID,
                      (CHAR_TYPE *)libNameFull.c_str());
}

// safe to call more than once (sets/checks for null session)
void LoaderCtxMSDK::CloseSession(mfxSession *session) {
    if (*session)
        MFXClose(*session);

    *session = nullptr;
}

// map mfxIMPL (1.x) to mfxAccelerationMode (2.x)
mfxAccelerationMode LoaderCtxMSDK::CvtAccelType(mfxIMPL implType, mfxIMPL implMethod) {
    if (implType == MFX_IMPL_HARDWARE) {
        switch (implMethod) {
            case MFX_IMPL_VIA_D3D9:
                return MFX_ACCEL_MODE_VIA_D3D9;
            case MFX_IMPL_VIA_D3D11:
                return MFX_ACCEL_MODE_VIA_D3D11;
            case MFX_IMPL_VIA_VAAPI:
                return MFX_ACCEL_MODE_VIA_VAAPI;
        }
    }

    return MFX_ACCEL_MODE_NA;
}

mfxStatus LoaderCtxMSDK::GetDefaultAccelType(mfxU32 adapterID, mfxIMPL *implDefault, mfxU64 *luid) {
#ifdef __linux__
    // VAAPI only
    *implDefault = MFX_IMPL_VIA_VAAPI;
    *luid        = 0;
    return MFX_ERR_NONE;
#else
    // Windows - D3D11 only
    mfxU32 VendorID = 0, DeviceID = 0;
    mfxIMPL implTest;
    mfxStatus sts;

    // check whether adapterID supports D3D11 and has correct VendorID
    implTest = MFX_IMPL_VIA_D3D11;
    sts      = MFX::SelectImplementationType(adapterID, &implTest, &VendorID, &DeviceID, luid);

    if (sts != MFX_ERR_NONE || VendorID != 0x8086) {
        implTest = MFX_IMPL_UNSUPPORTED;
        return MFX_ERR_UNSUPPORTED;
    }

    *implDefault = implTest;

    return MFX_ERR_NONE;
#endif
}

mfxStatus LoaderCtxMSDK::QueryAPIVersion(STRING_TYPE libNameFull, mfxVersion *msdkVersion) {
    mfxStatus sts;
    mfxSession session = nullptr;

    mfxVersion reqVersion;
    reqVersion.Major = MSDK_MIN_VERSION_MAJOR;
    reqVersion.Minor = MSDK_MIN_VERSION_MINOR;

    // try creating a session with each adapter in order to get MSDK API version
    // stop with first successful session creation
    for (mfxU32 adapterID = 0; adapterID < MAX_NUM_IMPL_MSDK; adapterID++) {
        // try HW session, default acceleration mode
        mfxIMPL hwImpl      = msdkImplTab[adapterID];
        mfxIMPL implDefault = MFX_IMPL_UNSUPPORTED;
        mfxU64 luid;

        // if not a valid HW device, try next adapter
        sts = GetDefaultAccelType(adapterID, &implDefault, &luid);
        if (sts != MFX_ERR_NONE)
            continue;

        // set acceleration mode - will be mapped to 1.x API
        mfxInitializationParam vplParam = {};
        vplParam.AccelerationMode =
            (mfxAccelerationMode)CvtAccelType(MFX_IMPL_HARDWARE, implDefault & 0xFF00);

#ifdef __linux__
        vplParam.ExtParam    = (mfxExtBuffer **)&extParams;
        vplParam.NumExtParam = 1;
#else
        vplParam.ExtParam    = nullptr;
        vplParam.NumExtParam = 0;
#endif

        mfxU16 deviceID;
        sts = MFXInitEx2(reqVersion,
                         vplParam,
                         hwImpl,
                         &session,
                         &deviceID,
                         (CHAR_TYPE *)libNameFull.c_str());

        if (sts == MFX_ERR_NONE) {
            sts = MFXQueryVersion(session, msdkVersion);
            MFXClose(session);

            if (sts == MFX_ERR_NONE)
                return sts;
        }
    }

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus LoaderCtxMSDK::QueryMSDKCaps(STRING_TYPE libNameFull,
                                       mfxImplDescription **implDesc,
                                       mfxImplementedFunctions **implFuncs,
                                       mfxU32 adapterID,
                                       bool bSkipD3D9Check) {
#ifdef DISABLE_MSDK_COMPAT
    // disable support for legacy MSDK
    return MFX_ERR_UNSUPPORTED;
#endif

    mfxStatus sts;
    mfxSession session = nullptr;

    m_libNameFull = libNameFull;
    m_deviceID    = 0;

#ifdef __linux__
    // require pthreads to be linked in for MSDK RT to load
    pthread_key_t pkey;
    if (pthread_key_create(&pkey, NULL) == 0) {
        pthread_key_delete(pkey);
    }

    mfxU32 vendorID = 0;
    mfxU16 deviceID = 0;
    sts             = GetRenderNodeDescription(adapterID, vendorID, deviceID);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    // on Linux read deviceID from the render node path
    m_deviceID = deviceID;
#endif

    // try HW session, default acceleration mode
    mfxIMPL hwImpl      = msdkImplTab[adapterID];
    mfxIMPL implDefault = MFX_IMPL_UNSUPPORTED;

    sts = GetDefaultAccelType(adapterID, &implDefault, &m_luid);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    sts = OpenSession(&session,
                      m_libNameFull,
                      (mfxAccelerationMode)CvtAccelType(MFX_IMPL_HARDWARE, implDefault & 0xFF00),
                      hwImpl);

    // adapter unsupported
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    // return list of implemented functions
    *implFuncs = (mfxImplementedFunctions *)(&msdkImplFuncs);

    // clear new 2.0 style description struct
    memset(&m_id, 0, sizeof(mfxImplDescription));
    *implDesc = &m_id;

    // fill in top-level capabilities
    m_id.Version.Version = MFX_IMPLDESCRIPTION_VERSION;
    m_id.Impl            = MFX_IMPL_TYPE_HARDWARE;

    // query API version
    sts = MFXQueryVersion(session, &m_id.ApiVersion);
    if (sts != MFX_ERR_NONE) {
        CloseSession(&session);
        return sts;
    }

    // set default acceleration mode
    m_id.AccelerationMode = CvtAccelType(MFX_IMPL_HARDWARE, implDefault & 0xFF00);

    // fill in acceleration description struct
    mfxAccelerationModeDescription *accelDesc = &(m_id.AccelerationModeDescription);
    accelDesc->Version.Version                = MFX_ACCELERATIONMODESCRIPTION_VERSION;

    // fill in mode description with just the single (default) mode
    accelDesc->NumAccelerationModes = 1;
    accelDesc->Mode                 = m_accelMode;
    accelDesc->Mode[0]              = m_id.AccelerationMode;

    // return HW accelerator - required by MFXCreateSession
    m_msdkAdapter = hwImpl;

    // map MFX HW number to VendorImplID
    m_id.VendorImplID = 0;
    switch (hwImpl) {
        case MFX_IMPL_HARDWARE:
            m_id.VendorImplID = 0;
            break;
        case MFX_IMPL_HARDWARE2:
            m_id.VendorImplID = 1;
            break;
        case MFX_IMPL_HARDWARE3:
            m_id.VendorImplID = 2;
            break;
        case MFX_IMPL_HARDWARE4:
            m_id.VendorImplID = 3;
            break;
    }

    // fill in strings
    strncpy_s(m_id.ImplName, sizeof(m_id.ImplName), strImplName, sizeof(strImplName));
    m_id.ImplName[sizeof(m_id.ImplName) - 1] = 0;
    strncpy_s(m_id.License, sizeof(m_id.License), strLicense, sizeof(strLicense));
    m_id.License[sizeof(m_id.License) - 1] = 0;
    strncpy_s(m_id.Keywords, sizeof(m_id.Keywords), strKeywords, sizeof(strKeywords));
    m_id.Keywords[sizeof(m_id.Keywords) - 1] = 0;

    m_id.VendorID    = 0x8086;
    m_id.NumExtParam = 0;

    // fill in pool policies
    m_id.PoolPolicies = PoolPolicies;

    // fill in device description
    mfxDeviceDescription *Dev = &(m_id.Dev);
    memset(Dev, 0, sizeof(mfxDeviceDescription)); // initially empty
    Dev->MediaAdapterType = MFX_MEDIA_UNKNOWN;

    // query for underlying deviceID (requires API >= 1.19)
    // for Linux, we may already have the deviceID from parsing render node path earlier
    if (m_deviceID == 0) {
        if (IsVersionSupported(MAKE_MFX_VERSION(1, 19), m_id.ApiVersion)) {
            mfxPlatform platform = {};

            sts = MFXVideoCORE_QueryPlatform(session, &platform);
            if (sts == MFX_ERR_NONE)
                m_deviceID = platform.DeviceId;

            // mfxPlatform::MediaAdapterType was added in API 1.31
            if (IsVersionSupported(MAKE_MFX_VERSION(1, 31), m_id.ApiVersion)) {
                Dev->MediaAdapterType = platform.MediaAdapterType;
            }
        }
    }

    // if QueryPlatform did not return deviceID, we may have received
    //   it from the loader (MFXInitEx2)
    if (m_deviceID == 0)
        m_deviceID = m_loaderDeviceID;

    // store DeviceID as "DevID" (hex) / "AdapterIdx" (dec) to match GPU RT
    Dev->Version.Version = MFX_DEVICEDESCRIPTION_VERSION;
    snprintf(Dev->DeviceID, sizeof(Dev->DeviceID), "%x/%d", m_deviceID, m_id.VendorImplID);
    Dev->NumSubDevices = 0;

    CloseSession(&session);

#if defined(_WIN32) || defined(_WIN64)
    if (bSkipD3D9Check == false) {
        mfxIMPL implD3D9;
        m_msdkAdapterD3D9 = MFX_IMPL_UNSUPPORTED;

        sts = CheckD3D9Support(m_luid, libNameFull, &implD3D9);
        if (sts == MFX_ERR_NONE) {
            m_msdkAdapterD3D9 = implD3D9;

            accelDesc->Mode[accelDesc->NumAccelerationModes] = MFX_ACCEL_MODE_VIA_D3D9;
            accelDesc->NumAccelerationModes++;
        }
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus LoaderCtxMSDK::CheckD3D9Support(mfxU64 luid, STRING_TYPE libNameFull, mfxIMPL *implD3D9) {
#if defined(_WIN32) || defined(_WIN64)
    mfxU32 VendorID = 0, DeviceID = 0;
    mfxIMPL implTest = MFX_IMPL_VIA_D3D9;

    mfxStatus sts;
    mfxSession session = nullptr;

    mfxVersion reqVersion;
    reqVersion.Major = MSDK_MIN_VERSION_MAJOR;
    reqVersion.Minor = MSDK_MIN_VERSION_MINOR;

    *implD3D9 = MFX_IMPL_UNSUPPORTED;

    mfxU32 idx;
    for (idx = 0; idx < MAX_NUM_IMPL_MSDK; idx++) {
        mfxU64 luidD3D9 = 0;
        sts = MFX::SelectImplementationType(idx, &implTest, &VendorID, &DeviceID, &luidD3D9);

        if (sts != MFX_ERR_NONE || VendorID != 0x8086 || luid != luidD3D9)
            continue;

        // matching LUID - try creating a D3D9 session
        mfxInitializationParam vplParam = {};
        vplParam.AccelerationMode       = MFX_ACCEL_MODE_VIA_D3D9;

        // thread limit not enabled on Windows
        vplParam.ExtParam    = nullptr;
        vplParam.NumExtParam = 0;

        mfxU16 deviceID;
        sts = MFXInitEx2(reqVersion,
                         vplParam,
                         msdkImplTab[idx],
                         &session,
                         &deviceID,
                         (CHAR_TYPE *)libNameFull.c_str());

        if (sts == MFX_ERR_NONE) {
            *implD3D9 = msdkImplTab[idx];
            MFXClose(session);
            return MFX_ERR_NONE;
        }

        break; // D3D9 not supported
    }

    // this adapter (input luid) does not support D3D9
    return MFX_ERR_UNSUPPORTED;
#else
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxU32 read_device_file(std::string &path) {
    mfxU32 result = 0;
    std::string line;
    std::ifstream dev_str(path);
    if (!dev_str.is_open()) {
        return 0;
    }
    std::getline(dev_str, line);
    dev_str.close();
    try {
        result = std::stoul(line, 0, 16);
    }
    catch (std::invalid_argument &) {
        return 0;
    }
    catch (std::out_of_range &) {
        return 0;
    }
    return result;
}

mfxStatus LoaderCtxMSDK::GetRenderNodeDescription(mfxU32 adapterID,
                                                  mfxU32 &vendorID,
                                                  mfxU16 &deviceID) {
    vendorID = 0;
    deviceID = 0;

#if defined(__linux__)
    mfxU32 DRMRenderNodeNum = 128 + adapterID;
    std::string nodeStr     = std::to_string(DRMRenderNodeNum);

    std::string vendorPath = "/sys/class/drm/renderD" + nodeStr + "/device/vendor";
    std::string devPath    = "/sys/class/drm/renderD" + nodeStr + "/device/device";

    vendorID = read_device_file(vendorPath);

    if (vendorID != 0x8086)
        return MFX_ERR_UNSUPPORTED;

    deviceID = read_device_file(devPath);

    if (deviceID == 0)
        return MFX_ERR_UNSUPPORTED;

    return MFX_ERR_NONE;
#else
    return MFX_ERR_UNSUPPORTED;
#endif
}

// avoid confusing #ifdef indentation
// clang-format off

#ifdef ONEVPL_EXPERIMENTAL

mfxStatus LoaderCtxMSDK::QueryExtDeviceID(mfxExtendedDeviceId *extDeviceID,
                                          mfxU32 adapterID,
                                          mfxU16 deviceID,
                                          mfxU64 luid) {
    // fill extended device ID struct (API >= 2.6)
    memset(extDeviceID, 0, sizeof(mfxExtendedDeviceId));

    // common properties
    extDeviceID->Version.Version = MFX_EXTENDEDDEVICEID_VERSION;
    extDeviceID->VendorID = 0x8086;
    extDeviceID->DeviceID = deviceID;

    // default - no PCI info
    // additional dependencies required to obtain these props
    extDeviceID->PCIDomain   = 0xFFFFFFFF;
    extDeviceID->PCIBus      = 0xFFFFFFFF;
    extDeviceID->PCIDevice   = 0xFFFFFFFF;
    extDeviceID->PCIFunction = 0xFFFFFFFF;

    // default - no LUID
    extDeviceID->LUIDDeviceNodeMask = 0;
    extDeviceID->LUIDValid = 0;
    for (mfxU32 idx = 0; idx < 8; idx++)
        extDeviceID->DeviceLUID[idx] = 0;

    // default - no DRM node
    extDeviceID->DRMRenderNodeNum  = 0;
    extDeviceID->DRMPrimaryNodeNum = 0x7FFFFFFF;

    // default - no PCI RevisionID
    extDeviceID->RevisionID        = 0xFFFF;

    snprintf(extDeviceID->DeviceName, sizeof(extDeviceID->DeviceName), "%s", strImplName);

#if defined(_WIN32) || defined(_WIN64)
    // fill in Windows-specific properties
    if (luid) {
        extDeviceID->LUIDDeviceNodeMask = 1;
        extDeviceID->LUIDValid          = 1;

        // map 64-bit LUID into mfxU8[8]
        mfxU64 luidArr = luid;
        for (mfxU32 idx = 0; idx < 8; idx++) {
            extDeviceID->DeviceLUID[idx] = mfxU8((luidArr)&0xFF);
            luidArr >>= 8;
        }
    }
#elif defined(__linux__)
    extDeviceID->DRMPrimaryNodeNum = adapterID;
    extDeviceID->DRMRenderNodeNum  = 128 + adapterID;
#endif

    return MFX_ERR_NONE;
}
#endif // ONEVPL_EXPERIMENTAL

// clang-format on
