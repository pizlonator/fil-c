/*############################################################################
  # Copyright (C) Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef DISPATCHER_LINUX_MFXLOADER_H_
#define DISPATCHER_LINUX_MFXLOADER_H_

#include <limits.h>

#include <list>
#include <sstream>
#include <string>

#include "vpl/mfxdefs.h"

inline bool operator<(const mfxVersion &lhs, const mfxVersion &rhs) {
    return (lhs.Major < rhs.Major || (lhs.Major == rhs.Major && lhs.Minor < rhs.Minor));
}

inline bool operator<=(const mfxVersion &lhs, const mfxVersion &rhs) {
    return (lhs < rhs || (lhs.Major == rhs.Major && lhs.Minor == rhs.Minor));
}

#endif // DISPATCHER_LINUX_MFXLOADER_H_
