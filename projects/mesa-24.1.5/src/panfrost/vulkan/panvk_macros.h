/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_MACROS_H
#define PANVK_MACROS_H

#include <assert.h>

#ifdef HAVE_VALGRIND
#include <memcheck.h>
#include <valgrind.h>
#define VG(x) x
#else
#define VG(x)
#endif

#define panvk_stub() assert(!"stub")

#define panvk_arch_name(name, version) panvk_##version##_##name

#define panvk_arch_dispatch(arch, name, ...)                                   \
   do {                                                                        \
      switch (arch) {                                                          \
      case 6:                                                                  \
         panvk_arch_name(name, v6)(__VA_ARGS__);                               \
         break;                                                                \
      case 7:                                                                  \
         panvk_arch_name(name, v7)(__VA_ARGS__);                               \
         break;                                                                \
      default:                                                                 \
         unreachable("Unsupported architecture");                              \
      }                                                                        \
   } while (0)

#define panvk_arch_dispatch_ret(arch, name, ret, ...)                          \
   do {                                                                        \
      switch (arch) {                                                          \
      case 6:                                                                  \
         ret = panvk_arch_name(name, v6)(__VA_ARGS__);                         \
         break;                                                                \
      case 7:                                                                  \
         ret = panvk_arch_name(name, v7)(__VA_ARGS__);                         \
         break;                                                                \
      default:                                                                 \
         unreachable("Unsupported architecture");                              \
      }                                                                        \
   } while (0)

#ifdef PAN_ARCH
#if PAN_ARCH == 6
#define panvk_per_arch(name) panvk_arch_name(name, v6)
#elif PAN_ARCH == 7
#define panvk_per_arch(name) panvk_arch_name(name, v7)
#else
#error "Unsupported arch"
#endif
#endif

#endif
