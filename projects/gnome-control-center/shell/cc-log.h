/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-log.h
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * Copyright 2021 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#ifndef CC_LOG_LEVEL_TRACE
# define CC_LOG_LEVEL_TRACE ((GLogLevelFlags)(1 << G_LOG_LEVEL_USER_SHIFT))
# define CC_LOG_DETAILED ((GLogLevelFlags)(1 << (G_LOG_LEVEL_USER_SHIFT + 1)))
#endif

#define CC_DEBUG_MSG(fmt, ...)                          \
  cc_log (G_LOG_DOMAIN,                                 \
          G_LOG_LEVEL_DEBUG | CC_LOG_DETAILED,          \
          NULL, __FILE__, G_STRINGIFY (__LINE__),       \
          G_STRFUNC, fmt, ##__VA_ARGS__)
#define CC_TRACE_MSG(fmt, ...)                          \
  cc_log (G_LOG_DOMAIN,                                 \
          CC_LOG_LEVEL_TRACE | CC_LOG_DETAILED,         \
          NULL, __FILE__, G_STRINGIFY (__LINE__),       \
          G_STRFUNC, fmt, ##__VA_ARGS__)
#define CC_TRACE(fmt, ...)                              \
  cc_log (G_LOG_DOMAIN,                                 \
          CC_LOG_LEVEL_TRACE,                           \
          NULL, __FILE__, G_STRINGIFY (__LINE__),       \
          G_STRFUNC, fmt, ##__VA_ARGS__)
#define CC_TODO(_msg)                                   \
  g_log_structured (G_LOG_DOMAIN, CC_LOG_LEVEL_TRACE,   \
                    "MESSAGE", " TODO: %s():%d: %s",    \
                    G_STRFUNC, __LINE__, _msg)
#define CC_ENTRY                                        \
  g_log_structured (G_LOG_DOMAIN, CC_LOG_LEVEL_TRACE,   \
                    "MESSAGE", "ENTRY: %s():%d",        \
                    G_STRFUNC, __LINE__)
#define CC_EXIT                                         \
  G_STMT_START {                                        \
    g_log_structured (G_LOG_DOMAIN, CC_LOG_LEVEL_TRACE, \
                      "MESSAGE", "EXIT: %s():%d",       \
                      G_STRFUNC, __LINE__);             \
    return;                                             \
  } G_STMT_END
#define CC_RETURN(_r)                                   \
  G_STMT_START {                                        \
    g_log_structured (G_LOG_DOMAIN, CC_LOG_LEVEL_TRACE, \
                      "MESSAGE", "EXIT: %s():%d ",      \
                      G_STRFUNC, __LINE__);             \
    return _r;                                          \
  } G_STMT_END

void cc_log_init               (void);
void cc_log_increase_verbosity (void);
int  cc_log_get_verbosity      (void);
void cc_log                    (const char     *domain,
                                GLogLevelFlags  log_level,
                                const char     *value,
                                const char     *file,
                                const char     *line,
                                const char     *func,
                                const char     *message_format,
                                ...) G_GNUC_PRINTF (7, 8);
void cc_log_anonymize_value    (GString        *str,
                                const char     *value);

G_END_DECLS
