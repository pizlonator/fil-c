/*
 * GstCurlHttpSrc
 * Copyright 2017 British Broadcasting Corporation - Research and Development
 *
 * Author: Sam Hurst <samuelh@rd.bbc.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gstcurlqueue.h"

/**
 * gst_curl_http_src_add_queue_item:
 *
 * Function to add an item to a queue. If the queue is empty (i.e. NULL), then
 * it creates the new head of the queue, otherwise it scans to the end and adds
 * the entry there.
 * @param queue The queue to add an item to. Can be NULL.
 * @param s The item to be added to the queue.
 * @return Returns TRUE (0) on success, FALSE (!0) is an error.
 */
gboolean
gst_curl_http_src_add_queue_item (GstCurlHttpSrcQueueElement ** queue,
    GstCurlHttpSrc * s)
{
  GstCurlHttpSrcQueueElement *insert_point;

  if (*queue == NULL) {
    /* Queue is currently empty, so create a new item on the head */
    *queue = (GstCurlHttpSrcQueueElement *)
        g_malloc (sizeof (GstCurlHttpSrcQueueElement));
    if (*queue == NULL) {
      return FALSE;
    }
    insert_point = *queue;
  } else {
    insert_point = *queue;
    while (insert_point->next != NULL) {
      insert_point = insert_point->next;
    }
    insert_point->next = (GstCurlHttpSrcQueueElement *)
        g_malloc (sizeof (GstCurlHttpSrcQueueElement));
    if (insert_point->next == NULL) {
      return FALSE;
    }
    insert_point = insert_point->next;
  }

  insert_point->p = s;
  g_atomic_int_set (&insert_point->running, 0);
  insert_point->next = NULL;
  s->connection_status = GSTCURL_CONNECTED;
  return TRUE;
}

/**
 * gst_curl_http_src_remove_queue_item:
 *
 * Function to remove an item from a queue.
 * @param queue The queue to remove an item from.
 * @param s The item to be removed.
 * @return Returns TRUE if item removed, FALSE if item couldn't be found.
 */
gboolean
gst_curl_http_src_remove_queue_item (GstCurlHttpSrcQueueElement ** queue,
    GstCurlHttpSrc * s)
{
  GstCurlHttpSrcQueueElement *prev_qelement, *this_qelement;

  prev_qelement = NULL;
  this_qelement = *queue;
  while (this_qelement && (this_qelement->p != s)) {
    prev_qelement = this_qelement;
    this_qelement = this_qelement->next;
  }
  if (this_qelement == NULL) {
    /* Reached end of list without finding anything */
    return FALSE;
  }

  /* First queue item matched. */
  if (prev_qelement == NULL) {
    /* First and only element? If so, free the element and make queue NULL */
    if (this_qelement->next == NULL) {
      g_free (*queue);
      *queue = NULL;
      return TRUE;
    } else {
      *queue = this_qelement->next;
    }
  } else {
    prev_qelement->next = this_qelement->next;
  }
  g_free (this_qelement);
  s->connection_status = GSTCURL_NOT_CONNECTED;
  return TRUE;
}

/**
 * gst_curl_http_src_remove_queue_handle:
 *
 * Convenience function to remove an item from a queue by it's contained curl
 * handle. Only ever called from within the multi loop when the CURL handle
 * returns, so it's safe to assume that the transfer completed and the result
 * can be set as GSTCURL_RETURN_DONE (which doesn't necessarily mean that the
 * transfer was a success, just that CURL is finished with it)
 * @param queue The queue to remove an item from.
 * @param s The item to be removed.
 * @return Returns TRUE if item removed, FALSE if item couldn't be found.
 */
gboolean
gst_curl_http_src_remove_queue_handle (GstCurlHttpSrcQueueElement ** queue,
    CURL * handle, CURLcode result)
{
  GstCurlHttpSrcQueueElement *prev_qelement, *this_qelement;

  prev_qelement = NULL;
  this_qelement = *queue;
  while (this_qelement && (this_qelement->p->curl_handle != handle)) {
    prev_qelement = this_qelement;
    this_qelement = this_qelement->next;
  }
  if (this_qelement == NULL) {
    /* Reached end of list without finding anything */
    return FALSE;
  }

  /*GST_DEBUG_OBJECT (this_qelement->p,
     "Removing queue item via curl handle for URI %s",
     this_qelement->p->uri); */
  /* First, signal the transfer owner thread to wake up */
  g_mutex_lock (&this_qelement->p->buffer_mutex);
  g_cond_signal (&this_qelement->p->buffer_cond);
  if (this_qelement->p->state != GSTCURL_UNLOCK) {
    this_qelement->p->state = GSTCURL_DONE;
  } else {
    this_qelement->p->pending_state = GSTCURL_DONE;
  }
  this_qelement->p->connection_status = GSTCURL_NOT_CONNECTED;
  this_qelement->p->curl_result = result;
  g_mutex_unlock (&this_qelement->p->buffer_mutex);

  /* First queue item matched. */
  if (prev_qelement == NULL) {
    /* First and only element? If so, free the element and make queue NULL */
    if (this_qelement->next == NULL) {
      g_free (*queue);
      *queue = NULL;
      return TRUE;
    } else {
      *queue = this_qelement->next;
    }
  } else {
    prev_qelement->next = this_qelement->next;
  }
  g_free (this_qelement);
  return TRUE;
}
