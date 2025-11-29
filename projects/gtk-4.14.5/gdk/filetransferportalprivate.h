/*
 * Copyright (C) 2018 Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once


void     file_transfer_portal_register              (void);

void     file_transfer_portal_register_files        (const char           **files,
                                                     gboolean               writable,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               data);
gboolean file_transfer_portal_register_files_finish (GAsyncResult          *result,
                                                     char                 **key,
                                                     GError               **error);

void     file_transfer_portal_retrieve_files        (const char            *key,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               data);
gboolean file_transfer_portal_retrieve_files_finish (GAsyncResult          *result,
                                                     char                ***files,
                                                     GError               **error);


