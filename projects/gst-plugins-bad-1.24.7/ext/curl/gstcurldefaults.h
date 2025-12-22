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

#ifndef GSTCURLDEFAULTS_H_
#define GSTCURLDEFAULTS_H_

/*
 * This file contains a list of all the default values used. These are used to
 * initialise an object in its init call.
 *
 * Must all conform to GSTCURL_HANDLE_DEFAULT_##type for macro sillyness in
 * curltask.h, where "type" is the CURLOPT_<something> string.
 */
/* Defaults from http://curl.haxx.se/libcurl/c/curl_easy_setopt.html */
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_URL ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_USERNAME ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_PASSWORD ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_PROXY ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_PROXYUSERNAME ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_PROXYPASSWORD ((void *)0)
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_USERAGENT "GStreamer curlhttpsrc libcurl"
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_ACCEPT_ENCODING FALSE
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_FOLLOWLOCATION 1L
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_MAXREDIRS -1
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_TCP_KEEPALIVE 1L
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_TIMEOUT 0
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_SSL_VERIFYPEER 1
#define GSTCURL_HANDLE_DEFAULT_CURLOPT_CAINFO ((void *)0)


/* Defaults from http://curl.haxx.se/libcurl/c/curl_multi_setopt.html */
#define GSTCURL_HANDLE_DEFAULT_CURLMOPT_PIPELINING 1L
#define GSTCURL_HANDLE_DEFAULT_CURLMOPT_MAXCONNECTS 255L
#define GSTCURL_HANDLE_DEFAULT_CURLMOPT_MAX_HOST_CONNECTIONS 0L
#define GSTCURL_HANDLE_DEFAULT_CURLMOPT_MAX_PIPELINE_LENGTH 5L
#define GSTCURL_HANDLE_DEFAULT_CURLMOPT_MAX_TOTAL_CONNECTIONS 255L

/* Not a CURLOPT, is something I've implemented which curl doesn't */
#define GSTCURL_HANDLE_DEFAULT_RETRIES -1

/*
 * Now set acceptable ranges. Defaults can lie outside the range, in which case
 * it is expected that the programmer will use the gst_curl_setopt and not the
 * gst_curl_setopt_default macro, as if the value supplied lies outside of the
 * default range, it won't bother to set it. If the _default macro is used,
 * then the offending value is replaced by the default type above.
 */
#define GSTCURL_HANDLE_MIN_CURLOPT_FOLLOWLOCATION 0L
#define GSTCURL_HANDLE_MAX_CURLOPT_FOLLOWLOCATION 1L
#define GSTCURL_HANDLE_MIN_CURLOPT_MAXREDIRS -1
#define GSTCURL_HANDLE_MAX_CURLOPT_MAXREDIRS 255
#define GSTCURL_HANDLE_MIN_CURLOPT_TCP_KEEPALIVE 0L
#define GSTCURL_HANDLE_MAX_CURLOPT_TCP_KEEPALIVE 1L
#define GSTCURL_HANDLE_MIN_CURLOPT_TIMEOUT 0
#define GSTCURL_HANDLE_MAX_CURLOPT_TIMEOUT 3600
#define GSTCURL_HANDLE_MIN_CURLOPT_SSL_VERIFYPEER 0
#define GSTCURL_HANDLE_MAX_CURLOPT_SSL_VERIFYPEER 1
#define GSTCURL_HANDLE_MIN_CURLOPT_HTTP_VERSION CURL_HTTP_VERSION_1_0
#ifdef CURL_VERSION_HTTP2
#define GSTCURL_HANDLE_MAX_CURLOPT_HTTP_VERSION CURL_HTTP_VERSION_2_0
#else
#define GSTCURL_HANDLE_MAX_CURLOPT_HTTP_VERSION CURL_HTTP_VERSION_1_1
#endif

#define GSTCURL_HANDLE_MIN_CURLMOPT_PIPELINING 0L
#define GSTCURL_HANDLE_MAX_CURLMOPT_PIPELINING 1L
#define GSTCURL_HANDLE_MIN_CURLMOPT_MAXCONNECTS 32L
#define GSTCURL_HANDLE_MAX_CURLMOPT_MAXCONNECTS 255L
#define GSTCURL_HANDLE_MIN_CURLMOPT_MAX_HOST_CONNECTIONS 1L
#define GSTCURL_HANDLE_MAX_CURLMOPT_MAX_HOST_CONNECTIONS 127L
#define GSTCURL_HANDLE_MIN_CURLMOPT_MAX_PIPELINE_LENGTH 1L
#define GSTCURL_HANDLE_MAX_CURLMOPT_MAX_PIPELINE_LENGTH 200L
#define GSTCURL_HANDLE_MIN_CURLMOPT_MAX_TOTAL_CONNECTIONS 32L
#define GSTCURL_HANDLE_MAX_CURLMOPT_MAX_TOTAL_CONNECTIONS 255L

#define GSTCURL_HANDLE_MIN_RETRIES -1
#define GSTCURL_HANDLE_MAX_RETRIES 9999

/* Because g_param_spec_int requires min/max bounding... */
#define GSTCURL_MIN_REDIRECTIONS -1
#define GSTCURL_MAX_REDIRECTIONS 255
#define GSTCURL_MIN_CONNECTION_TIME 2
#define GSTCURL_MAX_CONNECTION_TIME 60
#define GSTCURL_MIN_CONNECTIONS_SERVER 1
#define GSTCURL_MAX_CONNECTIONS_SERVER 60
#define GSTCURL_MIN_CONNECTIONS_PROXY 1
#define GSTCURL_MAX_CONNECTIONS_PROXY 60
#define GSTCURL_MIN_CONNECTIONS_GLOBAL 1
#define GSTCURL_MAX_CONNECTIONS_GLOBAL 255
#define GSTCURL_DEFAULT_CONNECTION_TIME 30
#define GSTCURL_DEFAULT_CONNECTIONS_SERVER 5
#define GSTCURL_DEFAULT_CONNECTIONS_PROXY 30
#define GSTCURL_DEFAULT_CONNECTIONS_GLOBAL 255
#define GSTCURL_INFO_RESPONSE(x) ((x >= 100) && (x <= 199))
#define GSTCURL_SUCCESS_RESPONSE(x) ((x >= 200) && (x <=299))
#define GSTCURL_REDIRECT_RESPONSE(x) ((x >= 300) && (x <= 399))
#define GSTCURL_CLIENT_ERR_RESPONSE(x) ((x >= 400) && (x <= 499))
#define GSTCURL_SERVER_ERR_RESPONSE(x) ((x >= 500) && (x <= 599))
#define GSTCURL_FUNCTIONTRACE 0

#endif /* GSTCURLDEFAULTS_H_ */
