/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include <tchar.h>

#include "windows/mfx_dispatcher_log.h"
#include "windows/mfx_driver_store_loader.h"
#include "windows/mfx_load_dll.h"
#include "windows/mfx_vector.h"

namespace MFX {

inline bool IsIntelDeviceInstanceID(const wchar_t *DeviceID) {
    return wcsstr(DeviceID, L"VEN_8086") || wcsstr(DeviceID, L"ven_8086");
}

inline bool ExctractDeviceID(const wchar_t *descrString, mfxU32 &deviceID) {
    const wchar_t *begin = wcsstr(descrString, L"DEV_");

    if (!begin) {
        begin = wcsstr(descrString, L"dev_");
        if (!begin) {
            DISPATCHER_LOG_WRN(("exctracting device id: failed to find device id substring\n"));
            return false;
        }
    }

    begin += wcslen(L"DEV_");
    deviceID = wcstoul(begin, NULL, 16);
    if (!deviceID) {
        DISPATCHER_LOG_WRN(("exctracting device id: failed to convert device id str to int\n"));
        return false;
    }

    return true;
}

DriverStoreLoader::DriverStoreLoader(void)
        : m_moduleCfgMgr(NULL),
          m_pCM_Get_Device_ID_List_Size(NULL),
          m_pCM_Get_Device_ID_List(NULL),
          m_pCM_Locate_DevNode(NULL),
          m_pCM_Open_DevNode_Key(NULL) {}

DriverStoreLoader::~DriverStoreLoader(void) {}

bool DriverStoreLoader::GetDriverStorePath(wchar_t *path,
                                           DWORD dwPathSize,
                                           mfxU32 deviceID,
                                           const wchar_t *driverKey) {
    if (path == NULL || dwPathSize == 0) {
        return false;
    }

    // Obtain a PnP handle to the Intel graphics adapter
    CONFIGRET result       = CR_SUCCESS;
    ULONG DeviceIDListSize = 0;
    MFXVector<WCHAR> DeviceIDList;
    wchar_t DisplayGUID[40];
    DEVINST DeviceInst;

    DISPATCHER_LOG_INFO(("Looking for MediaSDK in DriverStore\n"));

    if (!LoadCfgMgr() || !LoadCmFuncs()) {
        return false;
    }

    if (StringFromGUID2(GUID_DEVCLASS_DISPLAY, DisplayGUID, sizeof(DisplayGUID)) == 0) {
        DISPATCHER_LOG_WRN(("Couldn't prepare string from GUID\n"));
        return false;
    }

    do {
        result =
            m_pCM_Get_Device_ID_List_Size(&DeviceIDListSize,
                                          DisplayGUID,
                                          CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
        if (result != CR_SUCCESS) {
            break;
        }

        try {
            DeviceIDList.resize(DeviceIDListSize);
        }
        catch (...) {
            return false;
        }
        result = m_pCM_Get_Device_ID_List(DisplayGUID,
                                          DeviceIDList.data(),
                                          DeviceIDListSize,
                                          CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);

    } while (result == CR_BUFFER_SMALL);

    if (result != CR_SUCCESS) {
        return false;
    }

    //Look for MediaSDK record
    wchar_t *begin = DeviceIDList.data();
    wchar_t *end   = begin + DeviceIDList.size();
    size_t len     = 0;

    for (; (begin < end) && (len = wcslen(begin)) > 0; begin += len + 1) {
        if (IsIntelDeviceInstanceID(begin)) {
            mfxU32 curDeviceID = 0;
            if (!ExctractDeviceID(begin, curDeviceID) || curDeviceID != deviceID) {
                continue;
            }

            result = m_pCM_Locate_DevNode(&DeviceInst, begin, CM_LOCATE_DEVNODE_NORMAL);
            if (result != CR_SUCCESS) {
                continue;
            }

            HKEY hKey_sw;
            result = m_pCM_Open_DevNode_Key(DeviceInst,
                                            KEY_READ,
                                            0,
                                            RegDisposition_OpenExisting,
                                            &hKey_sw,
                                            CM_REGISTRY_SOFTWARE);
            if (result != CR_SUCCESS) {
                continue;
            }

            ULONG nError;

            DWORD pathSize = dwPathSize;

            nError = RegQueryValueExW(hKey_sw, driverKey, 0, NULL, (LPBYTE)path, &pathSize);

            RegCloseKey(hKey_sw);

            if (ERROR_SUCCESS == nError) {
                if (path[wcslen(path) - 1] != '/' && path[wcslen(path) - 1] != '\\') {
                    wcscat_s(path, MFX_MAX_DLL_PATH, L"\\");
                }
                DISPATCHER_LOG_INFO(("DriverStore path is found\n"));
                return true;
            }
        }
    }

    DISPATCHER_LOG_INFO(("DriverStore path isn't found\n"));
    return false;

} // bool DriverStoreLoader::GetDriverStorePath(wchar_t * path, DWORD dwPathSize, wchar_t *driverKey)

bool DriverStoreLoader::LoadCfgMgr() {
    if (!m_moduleCfgMgr) {
        m_moduleCfgMgr = mfx_dll_load(L"cfgmgr32.dll");

        if (!m_moduleCfgMgr) {
            DISPATCHER_LOG_WRN(("cfgmgr32.dll couldn't be loaded\n"));
            return false;
        }
    }

    return true;

} // bool DriverStoreLoader::LoadCfgMgr()

bool DriverStoreLoader::LoadCmFuncs() {
    if (!m_pCM_Get_Device_ID_List || !m_pCM_Get_Device_ID_List_Size || !m_pCM_Locate_DevNode ||
        !m_pCM_Open_DevNode_Key) {
        m_pCM_Get_Device_ID_List =
            (Func_CM_Get_Device_ID_ListW)mfx_dll_get_addr((HMODULE)m_moduleCfgMgr,
                                                          "CM_Get_Device_ID_ListW");
        m_pCM_Get_Device_ID_List_Size =
            (Func_CM_Get_Device_ID_List_SizeW)mfx_dll_get_addr((HMODULE)m_moduleCfgMgr,
                                                               "CM_Get_Device_ID_List_SizeW");
        m_pCM_Locate_DevNode   = (Func_CM_Locate_DevNodeW)mfx_dll_get_addr((HMODULE)m_moduleCfgMgr,
                                                                         "CM_Locate_DevNodeW");
        m_pCM_Open_DevNode_Key = (Func_CM_Open_DevNode_Key)mfx_dll_get_addr((HMODULE)m_moduleCfgMgr,
                                                                            "CM_Open_DevNode_Key");

        if (!m_pCM_Get_Device_ID_List || !m_pCM_Get_Device_ID_List_Size || !m_pCM_Locate_DevNode ||
            !m_pCM_Open_DevNode_Key) {
            DISPATCHER_LOG_WRN(("One of cfgmgr32.dll function isn't found\n"));
            return false;
        }
    }

    return true;

} // bool DriverStoreLoader::LoadCmFuncs()

} // namespace MFX
