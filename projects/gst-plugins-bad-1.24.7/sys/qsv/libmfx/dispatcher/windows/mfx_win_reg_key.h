/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_WINDOWS_MFX_WIN_REG_KEY_H_
#define DISPATCHER_WINDOWS_MFX_WIN_REG_KEY_H_

#include <windows.h>
#include "vpl/mfxcommon.h"
#include "windows/mfx_dispatcher_log.h"

#if !defined(MEDIASDK_UWP_DISPATCHER)
namespace MFX {

template <class T>
struct RegKey {};
template <>
struct RegKey<bool> {
    enum { type = REG_DWORD };
};
template <>
struct RegKey<mfxU32> {
    enum { type = REG_DWORD };
};
template <>
struct RegKey<mfxVersion> {
    enum { type = REG_DWORD };
};
template <>
struct RegKey<char *> {
    enum { type = REG_SZ };
};
template <>
struct RegKey<wchar_t *> {
    enum { type = REG_SZ };
};

class WinRegKey {
public:
    // Default constructor
    WinRegKey(void);
    // Destructor
    ~WinRegKey(void);

    // Open a registry key
    bool Open(HKEY hRootKey, const wchar_t *pSubKey, REGSAM samDesired);
    bool Open(WinRegKey &rootKey, const wchar_t *pSubKey, REGSAM samDesired);

    // Query value
    bool QueryInfo(LPDWORD lpcSubkeys);

    bool QueryValueSize(const wchar_t *pValueName, DWORD type, LPDWORD pcbData);
    bool Query(const wchar_t *pValueName, DWORD type, LPBYTE pData, LPDWORD pcbData);

    bool Query(const wchar_t *pValueName, wchar_t *pData, mfxU32 &nData) {
        DWORD dw = (DWORD)nData;
        if (!Query(pValueName, RegKey<wchar_t *>::type, (LPBYTE)pData, &dw)) {
            return false;
        }
        nData = dw;
        return true;
    }

    // Enumerate value names
    bool EnumValue(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName, LPDWORD pType);
    bool EnumKey(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName);

protected:
    // Release the object
    void Release(void);

    HKEY m_hKey; // (HKEY) handle to the opened key

private:
    // unimplemented by intent to make this class non-copyable
    WinRegKey(const WinRegKey &);
    void operator=(const WinRegKey &);
};

template <class T>
inline bool QueryKey(WinRegKey &key, const wchar_t *pValueName, T &data) {
    DWORD size = sizeof(data);
    return key.Query(pValueName, RegKey<T>::type, (LPBYTE)&data, &size);
}

template <>
inline bool QueryKey<bool>(WinRegKey &key, const wchar_t *pValueName, bool &data) {
    mfxU32 value = 0;
    bool bRes    = QueryKey(key, pValueName, value);
    data         = (1 == value);
    return bRes;
}

} // namespace MFX
#endif // #if !defined(MEDIASDK_UWP_DISPATCHER)

#endif // DISPATCHER_WINDOWS_MFX_WIN_REG_KEY_H_
