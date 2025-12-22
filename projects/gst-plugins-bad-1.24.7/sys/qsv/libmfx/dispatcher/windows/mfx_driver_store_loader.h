/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_WINDOWS_MFX_DRIVER_STORE_LOADER_H_
#define DISPATCHER_WINDOWS_MFX_DRIVER_STORE_LOADER_H_

#include <windows.h>

#include <cfgmgr32.h>

// support building in MinGW environments with older versions of cfgmgr32
#ifdef __MINGW32__
    #if !defined(CM_GETIDLIST_FILTER_PRESENT)
        #define CM_GETIDLIST_FILTER_PRESENT 0x00000100
    #endif
    #if !defined(CM_GETIDLIST_FILTER_CLASS)
        #define CM_GETIDLIST_FILTER_CLASS 0x00000200
    #endif
#endif

#include <devguid.h>

#include "windows/mfx_dispatcher_defs.h"

namespace MFX {

typedef CONFIGRET(WINAPI *Func_CM_Get_Device_ID_List_SizeW)(PULONG pulLen,
                                                            PCWSTR pszFilter,
                                                            ULONG ulFlags);
typedef CONFIGRET(WINAPI *Func_CM_Get_Device_ID_ListW)(PCWSTR pszFilter,
                                                       PZZWSTR Buffer,
                                                       ULONG BufferLen,
                                                       ULONG ulFlags);
typedef CONFIGRET(WINAPI *Func_CM_Locate_DevNodeW)(PDEVINST pdnDevInst,
                                                   DEVINSTID_W pDeviceID,
                                                   ULONG ulFlags);
typedef CONFIGRET(WINAPI *Func_CM_Open_DevNode_Key)(DEVINST dnDevNode,
                                                    REGSAM samDesired,
                                                    ULONG ulHardwareProfile,
                                                    REGDISPOSITION Disposition,
                                                    PHKEY phkDevice,
                                                    ULONG ulFlags);

class DriverStoreLoader {
public:
    DriverStoreLoader(void);
    ~DriverStoreLoader(void);

    bool GetDriverStorePath(wchar_t *path,
                            DWORD dwPathSize,
                            mfxU32 deviceID,
                            const wchar_t *driverKey);

protected:
    bool LoadCfgMgr();
    bool LoadCmFuncs();

    mfxModuleHandle m_moduleCfgMgr;
    Func_CM_Get_Device_ID_List_SizeW m_pCM_Get_Device_ID_List_Size;
    Func_CM_Get_Device_ID_ListW m_pCM_Get_Device_ID_List;
    Func_CM_Locate_DevNodeW m_pCM_Locate_DevNode;
    Func_CM_Open_DevNode_Key m_pCM_Open_DevNode_Key;

private:
    // unimplemented by intent to make this class non-copyable
    DriverStoreLoader(const DriverStoreLoader &);
    void operator=(const DriverStoreLoader &);
};

} // namespace MFX

#endif // DISPATCHER_WINDOWS_MFX_DRIVER_STORE_LOADER_H_
