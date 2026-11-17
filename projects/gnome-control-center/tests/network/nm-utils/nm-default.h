/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2015 Red Hat, Inc.
 */

#ifndef __NM_DEFAULT_H__
#define __NM_DEFAULT_H__

#define NM_NETWORKMANAGER_COMPILATION_WITH_GLIB                 (1 <<  0)
#define NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB        (1 <<  1)
#define NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_PROG       (1 <<  2)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM                (1 <<  3)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_PRIVATE        (1 <<  4)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE           (1 <<  5)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_INTERNAL  (1 <<  6)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_PRIVATE   (1 <<  7)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_UTIL           (1 <<  8)
#define NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_GLIB           (1 <<  9)
#define NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON               (1 << 10)
#define NM_NETWORKMANAGER_COMPILATION_WITH_SYSTEMD              (1 << 11)

#define NM_NETWORKMANAGER_COMPILATION_LIBNM_CORE     ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_PRIVATE \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_INTERNAL \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_LIBNM          ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_PRIVATE \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_INTERNAL \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_LIBNM_UTIL     ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_UTIL \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_LIBNM_GLIB     ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_LIBNM_UTIL \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_GLIB \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_CLIENT         ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_PROG \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_DAEMON         ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_PROG \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_INTERNAL \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_SYSTEMD        ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_DAEMON \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_SYSTEMD \
                                                     )

#define NM_NETWORKMANAGER_COMPILATION_GLIB           ( 0 \
                                                     | NM_NETWORKMANAGER_COMPILATION_WITH_GLIB \
                                                     )

#ifndef NETWORKMANAGER_COMPILATION
#error Define NETWORKMANAGER_COMPILATION accordingly
#endif

#ifndef G_LOG_DOMAIN
#if defined(NETWORKMANAGER_COMPILATION_TEST)
#define G_LOG_DOMAIN "test"
#elif NETWORKMANAGER_COMPILATION & NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON
#define G_LOG_DOMAIN "NetworkManager"
#else
#error Need to define G_LOG_DOMAIN
#endif
#elif defined (NETWORKMANAGER_COMPILATION_TEST) || (NETWORKMANAGER_COMPILATION & NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON)
#error Do not define G_LOG_DOMAIN with NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON
#endif

/*****************************************************************************/

/* always include these headers for our internal source files. */

#ifndef ___CONFIG_H__
#define ___CONFIG_H__
#include <config.h>
#endif

/* for internal compilation we don't want the deprecation macros
 * to be in effect. Define the widest range of versions to effectively
 * disable deprecation checks */
#define NM_VERSION_MIN_REQUIRED  NM_VERSION_0_9_8

#ifndef NM_MORE_ASSERTS
#define NM_MORE_ASSERTS 0
#endif

#if NM_MORE_ASSERTS == 0
/* The cast macros like NM_TYPE() are implemented via G_TYPE_CHECK_INSTANCE_CAST()
 * and _G_TYPE_CIC(). The latter, by default performs runtime checks of the type
 * by calling g_type_check_instance_cast().
 * This check has a certain overhead without being helpful.
 *
 * Example 1:
 *     static void foo (NMType *obj)
 *     {
 *         access_obj_without_check (obj);
 *     }
 *     foo ((NMType *) obj);
 *     // There is no runtime check and passing an invalid pointer
 *     // leads to a crash.
 *
 * Example 2:
 *     static void foo (NMType *obj)
 *     {
 *         access_obj_without_check (obj);
 *     }
 *     foo (NM_TYPE (obj));
 *     // There is a runtime check which prints a g_warning(), but that doesn't
 *     // avoid the crash as NM_TYPE() cannot do anything then passing on the
 *     // invalid pointer.
 *
 * Example 3:
 *     static void foo (NMType *obj)
 *     {
 *         g_return_if_fail (NM_IS_TYPE (obj));
 *         access_obj_without_check (obj);
 *     }
 *     foo ((NMType *) obj);
 *     // There is a runtime check which prints a g_critical() which also avoids
 *     // the crash. That is actually helpful to catch bugs and avoid crashes.
 *
 * Example 4:
 *     static void foo (NMType *obj)
 *     {
 *         g_return_if_fail (NM_IS_TYPE (obj));
 *         access_obj_without_check (obj);
 *     }
 *     foo (NM_TYPE (obj));
 *     // The runtime check is performed twice, with printing a g_warning() and
 *     // a g_critical() and avoiding the crash.
 *
 * Example 3 is how it should be done. Type checks in NM_TYPE() are pointless.
 * Disable them for our production builds.
 */
#ifndef G_DISABLE_CAST_CHECKS
#define G_DISABLE_CAST_CHECKS
#endif
#endif

#if NM_MORE_ASSERTS == 0
#ifndef G_DISABLE_CAST_CHECKS
/* Unless compiling with G_DISABLE_CAST_CHECKS, glib performs type checking
 * during G_VARIANT_TYPE() via g_variant_type_checked_(). This is not necesary
 * because commonly this cast is needed during something like
 *
 *   g_variant_builder_init (&props, G_VARIANT_TYPE ("a{sv}"));
 *
 * Note that in if the variant type would be invalid, the check still
 * wouldn't make the buggy code magically work. Instead of passing a
 * bogus type string (bad), it would pass %NULL to g_variant_builder_init()
 * (also bad).
 *
 * Also, a function like g_variant_builder_init() already validates
 * the input type via something like
 *
 *   g_return_if_fail (g_variant_type_is_container (type));
 *
 * So, by having G_VARIANT_TYPE() also validate the type, we validate
 * twice, whereas the first validation is rather pointless because it
 * doesn't prevent the function to be called with invalid arguments.
 *
 * Just patch G_VARIANT_TYPE() to perform no check.
 */
#undef G_VARIANT_TYPE
#define G_VARIANT_TYPE(type_string) ((const GVariantType *) (type_string))
#endif
#endif

#include <stdlib.h>

/*****************************************************************************/

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_GLIB

#include <glib.h>

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_PROG
#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB
#error Cannot define NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_PROG and NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB
#endif
#include <glib/gi18n.h>
#elif (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_GLIB_I18N_LIB
#include <glib/gi18n-lib.h>
#endif

/*****************************************************************************/

#if NM_MORE_ASSERTS == 0

/* glib assertions (g_return_*(), g_assert*()) contain a textual representation
 * of the checked statement. This part of the assertion blows up the size of the
 * binary. Unless we compile a debug-build with NM_MORE_ASSERTS, drop these
 * parts. Note that the failed assertion still prints the file and line where the
 * assertion fails. That shall suffice. */

static inline void
_nm_g_return_if_fail_warning (const char *log_domain,
                              const char *file,
                              int line)
{
	char file_buf[256 + 15];

	g_snprintf (file_buf, sizeof (file_buf), "((%s:%d))", file, line);
	g_return_if_fail_warning (log_domain, file_buf, "<dropped>");
}

#define g_return_if_fail_warning(log_domain, pretty_function, expression) \
	_nm_g_return_if_fail_warning (log_domain, __FILE__, __LINE__)

#define g_assertion_message_expr(domain, file, line, func, expr) \
	g_assertion_message_expr(domain, file, line, "<unknown-fcn>", (expr) ? "<dropped>" : NULL)

#undef g_return_val_if_reached
#define g_return_val_if_reached(val) \
    G_STMT_START { \
        g_log (G_LOG_DOMAIN, \
               G_LOG_LEVEL_CRITICAL, \
               "file %s: line %d (%s): should not be reached", \
               __FILE__, \
               __LINE__, \
               "<dropped>"); \
        return (val); \
    } G_STMT_END

#undef g_return_if_reached
#define g_return_if_reached() \
    G_STMT_START { \
        g_log (G_LOG_DOMAIN, \
               G_LOG_LEVEL_CRITICAL, \
               "file %s: line %d (%s): should not be reached", \
               __FILE__, \
               __LINE__, \
               "<dropped>"); \
        return; \
    } G_STMT_END

#define NM_ASSERT_G_RETURN_EXPR(expr) "<dropped>"
#define NM_ASSERT_NO_MSG 1

#else

#define NM_ASSERT_G_RETURN_EXPR(expr) ""expr""
#define NM_ASSERT_NO_MSG 0

#endif

/*****************************************************************************/

#include "nm-utils/nm-macros-internal.h"
#include "nm-utils/nm-shared-utils.h"

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_UTIL
/* no hash-utils in legacy code. */
#else
#include "nm-utils/nm-hash-utils.h"
#endif

/*****************************************************************************/

#if (NETWORKMANAGER_COMPILATION) & (NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_UTIL)
#include "nm-version.h"
#endif

/*****************************************************************************/

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_DAEMON
#include "nm-types.h"
#include "nm-logging.h"
#endif

#if ((NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM) && !((NETWORKMANAGER_COMPILATION) & (NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_PRIVATE | NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_CORE_INTERNAL))
#include "NetworkManager.h"
#endif

#endif /* NM_NETWORKMANAGER_COMPILATION_WITH_GLIB */

/*****************************************************************************/

#endif /* __NM_DEFAULT_H__ */
