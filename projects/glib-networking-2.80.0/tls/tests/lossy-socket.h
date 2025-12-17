/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2021 Ole André Vadla Ravnås <oleavr@frida.re>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

#pragma once

G_BEGIN_DECLS

typedef enum {
  IO_KEEP,
  IO_DROP
} IODecision;

typedef enum {
  IO_IN,
  IO_OUT
} IODirection;

typedef struct _IODetails IODetails;

typedef IODecision (*IOPredicateFunc) (const IODetails *io,
                                       gpointer         user_data);

struct _IODetails
{
  IODirection direction;
  guint serial;
};

#define LOSSY_TYPE_SOCKET (lossy_socket_get_type ())

G_DECLARE_FINAL_TYPE (LossySocket, lossy_socket, LOSSY, SOCKET, GObject)

GDatagramBased *lossy_socket_new (GDatagramBased  *base_socket,
                                  IOPredicateFunc  predicate_func,
                                  gpointer         predicate_data);

G_END_DECLS
