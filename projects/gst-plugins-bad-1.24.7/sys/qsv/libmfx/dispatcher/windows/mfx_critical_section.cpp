/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "windows/mfx_critical_section.h"

#include <windows.h>
// SDK re-declares the following functions with different call declarator.
// We don't need them. Just redefine them to nothing.
#define _interlockedbittestandset     fake_set
#define _interlockedbittestandreset   fake_reset
#define _interlockedbittestandset64   fake_set64
#define _interlockedbittestandreset64 fake_reset64
#include <intrin.h>

#define MFX_WAIT() SwitchToThread()

// static section of the file
namespace {

enum { MFX_SC_IS_FREE = 0, MFX_SC_IS_TAKEN = 1 };

} // namespace

namespace MFX {

mfxU32 mfxInterlockedCas32(mfxCriticalSection *pCSection,
                           mfxU32 value_to_exchange,
                           mfxU32 value_to_compare) {
    return _InterlockedCompareExchange(pCSection, value_to_exchange, value_to_compare);
}

mfxU32 mfxInterlockedXchg32(mfxCriticalSection *pCSection, mfxU32 value) {
    return _InterlockedExchange(pCSection, value);
}

void mfxEnterCriticalSection(mfxCriticalSection *pCSection) {
    while (MFX_SC_IS_TAKEN == mfxInterlockedCas32(pCSection, MFX_SC_IS_TAKEN, MFX_SC_IS_FREE)) {
        MFX_WAIT();
    }
} // void mfxEnterCriticalSection(mfxCriticalSection *pCSection)

void mfxLeaveCriticalSection(mfxCriticalSection *pCSection) {
    mfxInterlockedXchg32(pCSection, MFX_SC_IS_FREE);
} // void mfxLeaveCriticalSection(mfxCriticalSection *pCSection)

} // namespace MFX
