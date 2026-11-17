/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"

G_BEGIN_DECLS

_VTE_PUBLIC
char *vte_get_user_shell(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
const char *vte_get_features (void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VteFeatureFlags vte_get_feature_flags(void) _VTE_CXX_NOEXCEPT;

#define VTE_TEST_FLAGS_NONE (G_GUINT64_CONSTANT(0))
#define VTE_TEST_FLAGS_ALL (~G_GUINT64_CONSTANT(0))

_VTE_PUBLIC
void vte_set_test_flags(guint64 flags) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
guint64 vte_get_test_flags(void) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_NAME_PREFIX:
 *
 * The string prefix that any termprop's name must start with to be installed
 * by vte_install_termprop().
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_NAME_PREFIX "vte.ext."

_VTE_PUBLIC
int vte_install_termprop(char const* name,
                         VtePropertyType type,
                         VtePropertyFlags flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
int vte_install_termprop_alias(char const* name,
                               char const* target_name) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
char const** vte_get_termprops(gsize* length) _VTE_CXX_NOEXCEPT G_GNUC_MALLOC;

_VTE_PUBLIC
gboolean vte_query_termprop(char const* name,
                            char const** resolved_name,
                            int* prop,
                            VtePropertyType* type,
                            VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_query_termprop_by_id(int prop,
                                  char const** name,
                                  VtePropertyType* type,
                                  VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_CURRENT_DIRECTORY_URI:
 *
 * A %VTE_PROPERTY_URI termprop that stores the current directory
 * URI as set by OSC 7.
 * Use this with vte_terminal_ref_termprop_uri() instead of using
 * vte_terminal_get_current_directory_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_DIRECTORY_URI "vte.cwd"

/**
 * VTE_TERMPROP_CURRENT_FILE_URI:
 *
 * A %VTE_PROPERTY_URI termprop that stores the current file URI
 * as set by OSC 6.
 * Use this with vte_terminal_ref_termprop_uri() instead of using
 * vte_terminal_get_current_file_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_FILE_URI "vte.cwf"

/**
 * VTE_TERMPROP_XTERM_TITLE:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the xterm window title
 * as set by OSC 0 and OSC 2.
 * Use this with vte_terminal_get_termprop_string() instead of using
 * vte_terminal_get_window_title().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_XTERM_TITLE "xterm.title"

/**
 * VTE_TERMPROP_CONTAINER_NAME:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the name of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_NAME "vte.container.name"

/**
 * VTE_TERMPROP_CONTAINER_RUNTIME:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the runtime of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_RUNTIME "vte.container.runtime"

/**
 * VTE_TERMPROP_CONTAINER_UID:
 *
 * A %VTE_PROPERTY_UINT termprop that stores the user ID of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_UID "vte.container.uid"

/**
 * VTE_TERMPROP_SHELL_PRECMD:
 *
 * A %VTE_PROPERTY_VALUELESS termprop that signals that the shell
 * is going to prompt.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_PRECMD "vte.shell.precmd"

/**
 * VTE_TERMPROP_SHELL_PREEXEC:
 *
 * A %VTE_PROPERTY_VALUELESS termprop that signals that the shell
 * is preparing to execute the command entered at the prompt.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_PREEXEC "vte.shell.preexec"

/**
 * VTE_TERMPROP_SHELL_POSTEXEC:
 *
 * An ephemeral %VTE_PROPERTY_UINT termprop that signals that the shell
 * has executed the commands entered at the prompt and these commands
 * have returned. The termprop value is the exit code.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_POSTEXEC "vte.shell.postexec"

G_END_DECLS
