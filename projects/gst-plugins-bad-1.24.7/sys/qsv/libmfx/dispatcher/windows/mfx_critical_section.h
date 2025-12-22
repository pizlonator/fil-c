/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_WINDOWS_MFX_CRITICAL_SECTION_H_
#define DISPATCHER_WINDOWS_MFX_CRITICAL_SECTION_H_

#include "vpl/mfxdefs.h"

namespace MFX {

// Just set "critical section" instance to zero for initialization.
typedef volatile mfxL32 mfxCriticalSection;

// Enter the global critical section.
void mfxEnterCriticalSection(mfxCriticalSection *pCSection);

// Leave the global critical section.
void mfxLeaveCriticalSection(mfxCriticalSection *pCSection);

class MFXAutomaticCriticalSection {
public:
    // Constructor
    explicit MFXAutomaticCriticalSection(mfxCriticalSection *pCSection) {
        m_pCSection = pCSection;
        mfxEnterCriticalSection(m_pCSection);
    }

    // Destructor
    ~MFXAutomaticCriticalSection() {
        mfxLeaveCriticalSection(m_pCSection);
    }

protected:
    // Pointer to a critical section
    mfxCriticalSection *m_pCSection;

private:
    // unimplemented by intent to make this class non-copyable
    MFXAutomaticCriticalSection(const MFXAutomaticCriticalSection &);
    void operator=(const MFXAutomaticCriticalSection &);
};

} // namespace MFX

#endif // DISPATCHER_WINDOWS_MFX_CRITICAL_SECTION_H_
