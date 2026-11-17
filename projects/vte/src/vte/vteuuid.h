/*
 * Copyright © 2023 Christian Persch
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

#include <glib.h>

#include "vteenums.h"

G_BEGIN_DECLS

typedef struct _VteUuid VteUuid;

#define VTE_TYPE_UUID (vte_uuid_get_type())

_VTE_PUBLIC
GType vte_uuid_get_type(void);

_VTE_PUBLIC
VteUuid* vte_uuid_new_v4(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VteUuid* vte_uuid_new_v5(VteUuid const* ns,
                         char const* data,
                         gssize len) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
VteUuid* vte_uuid_new_from_string(char const* str,
                                  gssize len,
                                  VteUuidFormat fmt) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteUuid* vte_uuid_dup(VteUuid const* uuid) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_uuid_free(VteUuid* uuid) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char* vte_uuid_free_to_string(VteUuid* uuid,
                              VteUuidFormat fmt,
                              gsize* len) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char* vte_uuid_to_string(VteUuid const* uuid,
                         VteUuidFormat fmt,
                         gsize* len) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_uuid_equal(VteUuid const* uuid,
                        VteUuid const* other) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_uuid_validate_string(char const* str,
                                  gssize len,
                                  VteUuidFormat fmt) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(VteUuid, vte_uuid_free)

G_END_DECLS
