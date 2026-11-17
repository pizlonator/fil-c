/*
 * Copyright © 2014 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(4,0,0)
#define _VTE_GTK 4
#elif GTK_CHECK_VERSION(3,90,0)
#error gtk+ version not supported
#elif GTK_CHECK_VERSION(3,0,0)
#define _VTE_GTK 3
#else
#error gtk+ version unknown
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 6)
#define _VTE_GNUC_PACKED __attribute__((__packed__))
#else
#define _VTE_GNUC_PACKED
#endif  /* !__GNUC__ */

#ifdef VTE_COMPILATION
#define _VTE_GNUC_NONNULL(...)
#else
#if defined(__GNUC__)
#define _VTE_GNUC_NONNULL(...) __attribute__((__nonnull__(__VA_ARGS__)))
#else
#define _VTE_GNUC_NONNULL(...)
#endif
#endif

#define _VTE_PUBLIC __attribute__((__visibility__("default"))) extern

#if defined(VTE_COMPILATION) && defined(__cplusplus)
#if __cplusplus >= 201103L
#define _VTE_CXX_NOEXCEPT noexcept
#endif
#endif
#ifndef _VTE_CXX_NOEXCEPT
#define _VTE_CXX_NOEXCEPT
#endif
