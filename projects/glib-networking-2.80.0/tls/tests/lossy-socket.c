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
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 */

#include "config.h"

#include "lossy-socket.h"

struct _LossySocket
{
  GObject parent_instance;

  GDatagramBased *base_socket;

  IOPredicateFunc predicate_func;
  gpointer predicate_data;

  gint next_rx_serial;
  gint next_tx_serial;
};

static void lossy_socket_datagram_based_iface_init (GDatagramBasedInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (LossySocket,
                               lossy_socket,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_DATAGRAM_BASED,
                                                      lossy_socket_datagram_based_iface_init))

static gint
lossy_socket_receive_messages (GDatagramBased  *datagram_based,
                               GInputMessage   *messages,
                               guint            num_messages,
                               gint             flags,
                               gint64           timeout,
                               GCancellable    *cancellable,
                               GError         **error)
{
  LossySocket *self = LOSSY_SOCKET (datagram_based);
  gint ret;
  gboolean skip;

  do
    {
      IODetails d;

      skip = FALSE;

      ret = g_datagram_based_receive_messages (self->base_socket, messages,
                                               num_messages, flags, timeout,
                                               cancellable, error);
      if (ret <= 0)
        break;

      d.direction = IO_IN;
      d.serial = self->next_rx_serial++;

      if (self->predicate_func (&d, self->predicate_data) == IO_DROP)
        {
          messages->bytes_received = 0;
          messages->flags = 0;

          if (timeout == 0)
            {
              ret = -1;
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
                                   "Operation would block");
            }
          else
            {
              skip = TRUE;
            }
        }
    }
  while (skip);

  return ret;
}

static gint
lossy_socket_send_messages (GDatagramBased  *datagram_based,
                            GOutputMessage  *messages,
                            guint            num_messages,
                            gint             flags,
                            gint64           timeout,
                            GCancellable    *cancellable,
                            GError         **error)
{
  LossySocket *self = LOSSY_SOCKET (datagram_based);
  IODetails d;

  d.direction = IO_OUT;
  d.serial = self->next_tx_serial++;

  if (self->predicate_func (&d, self->predicate_data) == IO_DROP)
    {
      guint i, j;

      for (i = 0; i < num_messages; i++)
        {
          GOutputMessage *m = &messages[i];

          for (j = 0; j < m->num_vectors; j++)
            m->bytes_sent += m->vectors[j].size;
        }

      return num_messages;
    }

  return g_datagram_based_send_messages (self->base_socket, messages,
                                         num_messages, flags, timeout,
                                         cancellable, error);
}

static GSource *
lossy_socket_create_source (GDatagramBased *datagram_based,
                            GIOCondition    condition,
                            GCancellable   *cancellable)
{
  LossySocket *self = LOSSY_SOCKET (datagram_based);

  return g_datagram_based_create_source (self->base_socket, condition,
                                         cancellable);
}

static GIOCondition
lossy_socket_condition_check (GDatagramBased *datagram_based,
                              GIOCondition    condition)
{
  LossySocket *self = LOSSY_SOCKET (datagram_based);

  return g_datagram_based_condition_check (self->base_socket, condition);
}

static gboolean
lossy_socket_condition_wait (GDatagramBased  *datagram_based,
                             GIOCondition     condition,
                             gint64           timeout,
                             GCancellable    *cancellable,
                             GError         **error)
{
  LossySocket *self = LOSSY_SOCKET (datagram_based);

  return g_datagram_based_condition_wait (self->base_socket, condition, timeout,
                                          cancellable, error);
}

static void
lossy_socket_init (LossySocket *self)
{
  self->next_rx_serial = 1;
  self->next_tx_serial = 1;
}

static void
lossy_socket_dispose (GObject *object)
{
  LossySocket *self = LOSSY_SOCKET (object);

  g_clear_object (&self->base_socket);

  G_OBJECT_CLASS (lossy_socket_parent_class)->dispose (object);
}

static void
lossy_socket_class_init (LossySocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = lossy_socket_dispose;
}

static void
lossy_socket_datagram_based_iface_init (GDatagramBasedInterface *iface)
{
  iface->receive_messages = lossy_socket_receive_messages;
  iface->send_messages = lossy_socket_send_messages;
  iface->create_source = lossy_socket_create_source;
  iface->condition_check = lossy_socket_condition_check;
  iface->condition_wait = lossy_socket_condition_wait;
}

GDatagramBased *
lossy_socket_new (GDatagramBased  *base_socket,
                  IOPredicateFunc  predicate_func,
                  gpointer         predicate_data)
{
  LossySocket *s;

  s = g_object_new (LOSSY_TYPE_SOCKET, NULL);
  s->base_socket = g_object_ref (base_socket);
  s->predicate_func = predicate_func;
  s->predicate_data = predicate_data;

  return G_DATAGRAM_BASED (s);
}
