/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Versioning and export macros
 *
 * Copyright (C) 2014 Chun-wei Fan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>

#ifndef __GIVERSIONMACROS_H__
#define __GIVERSIONMACROS_H__

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#ifndef _GI_EXTERN
#define _GI_EXTERN extern
#endif

#ifndef GI_DISABLE_DEPRECATION_WARNINGS
# define GI_DEPRECATED G_DEPRECATED _GI_EXTERN
# define GI_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) _GI_EXTERN
# define GI_UNAVAILABLE(maj,min) G_UNAVAILABLE(maj, min) _GI_EXTERN
#else
# define GI_DEPRECATED _GI_EXTERN
# define GI_DEPRECATED_FOR(f) _GI_EXTERN
# define GI_UNAVAILABLE(maj,min) _GI_EXTERN
#endif

#define GI_AVAILABLE_IN_ALL _GI_EXTERN

/* XXX: Every new stable minor release should add a set of macros here
 *
 * We are using the GLib versions here as the G-I minor versions
 * need to be in sync with the GLib minor versions.  Api's added
 * at or before 1.30 are marked as GI_AVAILABLE_IN_ALL
 */

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_32
# define GI_DEPRECATED_IN_1_32                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_32_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_32                _GI_EXTERN
# define GI_DEPRECATED_IN_1_32_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_32
# define GI_AVAILABLE_IN_1_32                 GI_UNAVAILABLE(2, 32)
#else
# define GI_AVAILABLE_IN_1_32                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_34
# define GI_DEPRECATED_IN_1_34                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_34_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_34                _GI_EXTERN
# define GI_DEPRECATED_IN_1_34_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_34
# define GI_AVAILABLE_IN_1_34                 GI_UNAVAILABLE(2, 34)
#else
# define GI_AVAILABLE_IN_1_34                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_36
# define GI_DEPRECATED_IN_1_36                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_36_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_36                _GI_EXTERN
# define GI_DEPRECATED_IN_1_36_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_36
# define GI_AVAILABLE_IN_1_36                 GI_UNAVAILABLE(2, 36)
#else
# define GI_AVAILABLE_IN_1_36                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_38
# define GI_DEPRECATED_IN_1_38                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_38_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_38                _GI_EXTERN
# define GI_DEPRECATED_IN_1_38_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_38
# define GI_AVAILABLE_IN_1_38                 GI_UNAVAILABLE(2, 38)
#else
# define GI_AVAILABLE_IN_1_38                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_40
# define GI_DEPRECATED_IN_1_40                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_40_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_40                _GI_EXTERN
# define GI_DEPRECATED_IN_1_40_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_40
# define GI_AVAILABLE_IN_1_40                 GI_UNAVAILABLE(2, 40)
#else
# define GI_AVAILABLE_IN_1_40                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_42
# define GI_DEPRECATED_IN_1_42                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_42_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_42                _GI_EXTERN
# define GI_DEPRECATED_IN_1_42_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_42
# define GI_AVAILABLE_IN_1_42                 GI_UNAVAILABLE(2, 42)
#else
# define GI_AVAILABLE_IN_1_42                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_44
# define GI_DEPRECATED_IN_1_44                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_44_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_44                _GI_EXTERN
# define GI_DEPRECATED_IN_1_44_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_44
# define GI_AVAILABLE_IN_1_44                 GI_UNAVAILABLE(2, 44)
#else
# define GI_AVAILABLE_IN_1_44                 _GI_EXTERN
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_46
# define GI_DEPRECATED_IN_1_46                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_46_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_46                _GI_EXTERN
# define GI_DEPRECATED_IN_1_46_FOR(f)         _GI_EXTERN
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_46
# define GI_AVAILABLE_IN_1_46                 GI_UNAVAILABLE(2, 46)
#else
# define GI_AVAILABLE_IN_1_46                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_60) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_60
# define GI_AVAILABLE_IN_1_60                 GI_UNAVAILABLE(2, 60)
#else
# define GI_AVAILABLE_IN_1_60                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_66) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_66
# define GI_AVAILABLE_IN_1_66                 GI_UNAVAILABLE(2, 66)
#else
# define GI_AVAILABLE_IN_1_66                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_70) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_70
# define GI_AVAILABLE_IN_1_70                 GI_UNAVAILABLE(2, 70)
#else
# define GI_AVAILABLE_IN_1_70                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_72) && GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_72
# define GI_DEPRECATED_IN_1_72                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_72_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_72                _GI_EXTERN
# define GI_DEPRECATED_IN_1_72_FOR(f)         _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_72) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_72
# define GI_AVAILABLE_IN_1_72                 GI_UNAVAILABLE(2, 72)
#else
# define GI_AVAILABLE_IN_1_72                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_76) && GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_76
# define GI_DEPRECATED_IN_1_76                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_76_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_76                _GI_EXTERN
# define GI_DEPRECATED_IN_1_76_FOR(f)         _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_76) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_76
# define GI_AVAILABLE_IN_1_76                 GI_UNAVAILABLE(2, 76)
#else
# define GI_AVAILABLE_IN_1_76                 _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_78) && GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_78
# define GI_DEPRECATED_IN_1_78                GI_DEPRECATED
# define GI_DEPRECATED_IN_1_78_FOR(f)         GI_DEPRECATED_FOR(f)
#else
# define GI_DEPRECATED_IN_1_78                _GI_EXTERN
# define GI_DEPRECATED_IN_1_78_FOR(f)         _GI_EXTERN
#endif

#if defined(GLIB_VERSION_2_78) && GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_78
# define GI_AVAILABLE_IN_1_78                 GI_UNAVAILABLE(2, 78)
#else
# define GI_AVAILABLE_IN_1_78                 _GI_EXTERN
#endif

#endif /* __GIVERSIONMACROS_H__ */
