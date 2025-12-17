/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlsbio.c
 *
 * Copyright (C) 2015 NICE s.r.l.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Authors: Ignacio Casal Quinteiro
 */

#include "gtlsbio.h"

#include <string.h>

typedef struct {
  GIOStream *io_stream;
  GDatagramBased *socket;
  GCancellable *read_cancellable;
  GCancellable *write_cancellable;
  GError **read_error;
  GError **write_error;
  gboolean reached_eof;
} GTlsBio;

typedef struct {
  gboolean done;
  gboolean timed_out;
} WaitData;

static void
free_gbio (gpointer user_data)
{
  GTlsBio *bio = (GTlsBio *)user_data;

  if (bio->io_stream)
    g_object_unref (bio->io_stream);
  else
    g_object_unref (bio->socket);
  g_free (bio);
}

static int
gtls_bio_create (BIO *bio)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  bio->init = 0;
  bio->num = 0;
  bio->ptr = NULL;
  bio->flags = 0;
#else
  BIO_set_init (bio, 0);
  BIO_set_data (bio, NULL);
  BIO_clear_flags (bio, ~0);
#endif
  return 1;
}

static int
gtls_bio_destroy (BIO *bio)
{
  if (!bio)
    return 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  if (bio->shutdown)
    {
      if (bio->ptr)
        {
          free_gbio (bio->ptr);
          bio->ptr = NULL;
        }
      bio->init = 0;
      bio->flags = 0;
    }
#else
  if (BIO_get_shutdown (bio))
    {
      if (BIO_get_data (bio))
        {
          free_gbio (BIO_get_data (bio));
          BIO_set_data (bio, NULL);
        }
      BIO_clear_flags (bio, ~0);
      BIO_set_init (bio, 0);
    }
#endif

    return 1;
}

static long
gtls_bio_ctrl (BIO  *b,
               int   cmd,
               long  num,
               void *ptr)
{
  GTlsBio *gbio;
  long ret = 1;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)b->ptr;
#else
  gbio = BIO_get_data (b);
#endif

  switch (cmd)
    {
    case BIO_CTRL_GET_CLOSE:
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
      ret = b->shutdown;
#else
      ret = BIO_get_shutdown (b);
#endif
      break;
    case BIO_CTRL_SET_CLOSE:
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
      b->shutdown = (int)num;
#else
      BIO_set_shutdown (b, (int)num);
#endif
      break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      ret = 1;
      break;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
      ret = 0;
      break;
    case BIO_CTRL_DGRAM_QUERY_MTU:
      ret = 1400; /* Same as the GnuTLS backend */
      break;
#ifdef BIO_CTRL_EOF
    case BIO_CTRL_EOF:
      ret = gbio->reached_eof ? 1 : 0;
      break;
#endif
    default:
      g_debug ("Got unsupported command: %d", cmd);
      ret = 0;
      break;
    }

  return ret;
}

static int
gtls_bio_write (BIO        *bio,
                const char *in,
                int         inl)
{
  GTlsBio *gbio;
  gssize written;
  GError *error = NULL;

  if (
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
      !bio->init ||
#else
      !BIO_get_init (bio) ||
#endif
      !in || inl == 0)
    return 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif

  BIO_clear_retry_flags (bio);
  if (gbio->io_stream)
    {
      written = g_pollable_stream_write (g_io_stream_get_output_stream (gbio->io_stream),
                                         in, inl,
                                         FALSE,
                                         gbio->write_cancellable,
                                         &error);
    }
  else
    {
      GOutputVector vector = { in, inl };
      GOutputMessage message = { NULL, &vector, 1, 0, NULL, 0 };

      written = g_datagram_based_send_messages (gbio->socket,
                                                &message, 1, 0,
                                                0,
                                                gbio->write_cancellable,
                                                &error);

      if (written > 0)
        written = message.bytes_sent;
    }

  if (written == -1)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        BIO_set_retry_write (bio);

      g_clear_error (gbio->write_error);
      g_propagate_error (gbio->write_error, error);
    }

  return written;
}

static int
gtls_bio_read (BIO  *bio,
               char *out,
               int   outl)
{
  GTlsBio *gbio;
  gssize read;
  GError *error = NULL;

  if (
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
      !bio->init ||
#else
      !BIO_get_init (bio) ||
#endif
      !out || outl == 0)
    return 0;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif

  BIO_clear_retry_flags (bio);
  if (gbio->io_stream)
    {
      read = g_pollable_stream_read (g_io_stream_get_input_stream (gbio->io_stream),
                                     out, outl,
                                     FALSE,
                                     gbio->read_cancellable,
                                     &error);
    }
  else
    {
      GInputVector vector = { out, outl };
      GInputMessage message = { NULL, &vector, 1, 0, 0, NULL, NULL };

      read = g_datagram_based_receive_messages (gbio->socket,
                                                &message, 1, 0,
                                                0,
                                                gbio->read_cancellable,
                                                &error);

      if (read > 0)
        read = message.bytes_received;
    }

  if (read == -1)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        BIO_set_retry_read (bio);

      g_clear_error (gbio->read_error);
      g_propagate_error (gbio->read_error, error);
    }
  else if (read == 0)
    gbio->reached_eof = TRUE;

  return read;
}

static int
gtls_bio_puts(BIO        *bio,
              const char *str)
{
  return gtls_bio_write (bio, str, (int)strlen (str));
}

static int
gtls_bio_gets(BIO  *bio,
              char *buf,
              int   len)
{
  return -1;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
static BIO_METHOD methods_gtls = {
  BIO_TYPE_SOURCE_SINK,
  "gtls",
  gtls_bio_write,
  gtls_bio_read,
  gtls_bio_puts,
  gtls_bio_gets,
  gtls_bio_ctrl,
  gtls_bio_create,
  gtls_bio_destroy
};
#else
static BIO_METHOD *methods_gtls = NULL;
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
static BIO_METHOD *
BIO_s_gtls (void)
{
  return &methods_gtls;
}
#else
static const BIO_METHOD *
BIO_s_gtls (void)
{
  if (!methods_gtls)
    {
      methods_gtls = BIO_meth_new (BIO_TYPE_SOURCE_SINK | BIO_get_new_index (), "gtls");
      if (!methods_gtls ||
          !BIO_meth_set_write (methods_gtls, gtls_bio_write) ||
          !BIO_meth_set_read (methods_gtls, gtls_bio_read) ||
          !BIO_meth_set_puts (methods_gtls, gtls_bio_puts) ||
          !BIO_meth_set_gets (methods_gtls, gtls_bio_gets) ||
          !BIO_meth_set_ctrl (methods_gtls, gtls_bio_ctrl) ||
          !BIO_meth_set_create (methods_gtls, gtls_bio_create) ||
          !BIO_meth_set_destroy (methods_gtls, gtls_bio_destroy))
        return NULL;
    }
  return methods_gtls;
}
#endif

static BIO *
g_tls_bio_alloc (GTlsBio **out_gbio)
{
  BIO *ret;
  GTlsBio *gbio;

  ret = BIO_new(BIO_s_gtls ());
  if (!ret)
    return NULL;

  gbio = g_new0 (GTlsBio, 1);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  ret->ptr = gbio;
  ret->init = 1;
#else
  BIO_set_data (ret, gbio);
  BIO_set_init (ret, 1);
#endif

  *out_gbio = gbio;
  return ret;
}

BIO *
g_tls_bio_new_from_iostream (GIOStream *io_stream)
{
  BIO *ret;
  GTlsBio *gbio;

  ret = g_tls_bio_alloc (&gbio);
  gbio->io_stream = g_object_ref (io_stream);

  return ret;
}

BIO *
g_tls_bio_new_from_datagram_based (GDatagramBased *socket)
{
  BIO *ret;
  GTlsBio *gbio;

  ret = g_tls_bio_alloc (&gbio);
  gbio->socket = g_object_ref (socket);

  return ret;
}

void
g_tls_bio_set_read_cancellable (BIO          *bio,
                                GCancellable *cancellable)
{
  GTlsBio *gbio;

  g_return_if_fail (bio);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif
  gbio->read_cancellable = cancellable;
}

void
g_tls_bio_set_read_error (BIO     *bio,
                          GError **error)
{
  GTlsBio *gbio;

  g_return_if_fail (bio);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif
  gbio->read_error = error;
}

void
g_tls_bio_set_write_cancellable (BIO          *bio,
                                 GCancellable *cancellable)
{
  GTlsBio *gbio;

  g_return_if_fail (bio);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif
  gbio->write_cancellable = cancellable;
}

void
g_tls_bio_set_write_error (BIO     *bio,
                           GError **error)
{
  GTlsBio *gbio;

  g_return_if_fail (bio);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif
  gbio->write_error = error;
}

static gboolean
on_pollable_source_ready (GObject *pollable_stream,
                          gpointer user_data)
{
  WaitData *wait_data = user_data;

  wait_data->done = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
on_datagram_source_ready (GDatagramBased *datagram_based,
                          GIOCondition condition,
                          gpointer user_data)
{
  WaitData *wait_data = user_data;

  wait_data->done = TRUE;

  return G_SOURCE_REMOVE;
}

static gboolean
on_timeout_source_ready (gpointer user_data)
{
  WaitData *wait_data = user_data;

  wait_data->done = TRUE;
  wait_data->timed_out = TRUE;

  return G_SOURCE_REMOVE;
}

gboolean
g_tls_bio_wait_available (BIO          *bio,
                          GIOCondition  condition,
                          gint64        timeout,
                          GCancellable *cancellable)
{
  GTlsBio *gbio;
  WaitData wait_data;
  GMainContext *ctx;
  GSource *io_source, *timeout_source;

  g_return_val_if_fail (bio, FALSE);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined (LIBRESSL_VERSION_NUMBER)
  gbio = (GTlsBio *)bio->ptr;
#else
  gbio = BIO_get_data (bio);
#endif

  wait_data.done = FALSE;
  wait_data.timed_out = FALSE;

  ctx = g_main_context_new ();
  g_main_context_push_thread_default (ctx);

  if (gbio->io_stream)
    {
      if (condition & G_IO_IN)
        io_source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (g_io_stream_get_input_stream (gbio->io_stream)),
                                                           cancellable);
      else
        io_source = g_pollable_output_stream_create_source (G_POLLABLE_OUTPUT_STREAM (g_io_stream_get_output_stream (gbio->io_stream)),
                                                            cancellable);
      g_source_set_callback (io_source, (GSourceFunc)on_pollable_source_ready, &wait_data, NULL);
    }
  else
    {
      io_source = g_datagram_based_create_source (gbio->socket, condition, cancellable);
      g_source_set_callback (io_source, (GSourceFunc)on_datagram_source_ready, &wait_data, NULL);
    }
  g_source_attach (io_source, ctx);

  if (timeout >= 0)
    {
      timeout_source = g_timeout_source_new (timeout / 1000);
      g_source_set_callback (timeout_source, (GSourceFunc)on_timeout_source_ready, &wait_data, NULL);
      g_source_attach (timeout_source, ctx);
    }
  else
    {
      timeout_source = NULL;
    }

  while (!wait_data.done)
    g_main_context_iteration (ctx, TRUE);

  if (timeout_source)
    {
      g_source_destroy (timeout_source);
      g_source_unref (timeout_source);
    }

  g_source_destroy (io_source);
  g_source_unref (io_source);

  g_main_context_pop_thread_default (ctx);
  g_main_context_unref (ctx);

  return !wait_data.timed_out;
}
