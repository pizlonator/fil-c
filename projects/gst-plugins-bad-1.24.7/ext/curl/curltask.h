/*
 * GstCurlHttpSrc
 * Copyright 2014 British Broadcasting Corporation - Research and Development
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

#ifndef CURLTASK_H_
#define CURLTASK_H_

#include <curl/curl.h>

#define GSTCURL_ERROR_PRINT(...) GST_CAT_ERROR (gst_curl_loop_debug, __VA_ARGS__)
#define GSTCURL_WARNING_PRINT(...) GST_CAT_WARNING (gst_curl_loop_debug, __VA_ARGS__)
#define GSTCURL_INFO_PRINT(...) GST_CAT_INFO (gst_curl_loop_debug, __VA_ARGS__)
#define GSTCURL_DEBUG_PRINT(...) GST_CAT_DEBUG (gst_curl_loop_debug, __VA_ARGS__)
#define GSTCURL_TRACE_PRINT(...) GST_CAT_TRACE (gst_curl_loop_debug, __VA_ARGS__)

#define gst_curl_setopt_str(s,handle,type,option) \
  if(option != NULL) { \
    if(curl_easy_setopt(handle,type,option) != CURLE_OK) { \
      GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
    } \
  } \

#define gst_curl_setopt_int(s,handle, type, option) \
  if((option >= GSTCURL_HANDLE_MIN_##type) && (option <= GSTCURL_HANDLE_MAX_##type)) { \
    if(curl_easy_setopt(handle,type,option) != CURLE_OK) { \
      GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
    } \
  } \

#define gst_curl_setopt_bool(s,handle, type, option) \
  if(curl_easy_setopt(handle,type,((option != 0)?1L:0L)) != CURLE_OK) { \
    GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
  } \

#define gst_curl_setopt_str_default(s,handle,type,option) \
  if((option == NULL) && (GSTCURL_HANDLE_DEFAULT_##type != NULL)) { \
    if(curl_easy_setopt(handle,type,GSTCURL_HANDLE_DEFAULT_##type) != CURLE_OK) { \
      GST_WARNING_OBJECT(s, "Cannot set unsupported option %s,", #type ); \
    } \
  } \
  else { \
    if(curl_easy_setopt(handle,type,option) != CURLE_OK) { \
      GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
    } \
  } \

#define gst_curl_setopt_int_default(s,handle,type,option) \
  if((option < GSTCURL_HANDLE_MIN_##type) || (option > GSTCURL_HANDLE_MAX_##type)) { \
    GST_WARNING_OBJECT(s, "Value of %ld out of acceptable range for %s", option, \
                       #type ); \
    if(curl_easy_setopt(handle,type,GSTCURL_HANDLE_DEFAULT_##type) != CURLE_OK) { \
      GST_WARNING_OBJECT(s, "Cannot set unsupported option %s,", #type ); \
    } \
  } \
  else { \
    if(curl_easy_setopt(handle,type,option) != CURLE_OK) { \
      GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
    } \
  } \

#define gst_curl_setopt_generic(s, handle, type, option) \
  if (curl_easy_setopt (handle, type, option) != CURLE_OK) { \
    GST_WARNING_OBJECT (s, "Cannot set unsupported option %s", #type ); \
  } \

#define GSTCURL_ASSERT_MUTEX(x) if(g_atomic_pointer_get(&x->p) == NULL) GSTCURL_DEBUG_PRINT("ASSERTION: No valid mutex handle in GMutex %p", x);

/* As gboolean is either 0x0 or 0xffffffff, this sanitises things for curl. */
#define GSTCURL_BINARYBOOL(x) ((x != 0)?1:0)

/*
 * Function definitions
 */

#endif /* CURLTASK_H_ */
