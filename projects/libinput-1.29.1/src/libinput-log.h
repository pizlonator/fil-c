/*
 * Copyright © 2013 Jonas Ådahl
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "config.h"

#include <stdbool.h>

#include "util-ratelimit.h"

#include "libinput.h"

#define log_debug(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_DEBUG, __VA_ARGS__)
#define log_info(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_INFO, __VA_ARGS__)
#define log_error(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_ERROR, __VA_ARGS__)
#define log_bug_kernel(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_ERROR, "kernel bug: " __VA_ARGS__)
#define log_bug_libinput(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_ERROR, "libinput bug: " __VA_ARGS__)
#define log_bug_client(li_, ...) log_msg((li_), LIBINPUT_LOG_PRIORITY_ERROR, "client bug: " __VA_ARGS__)

#define log_debug_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_DEBUG, __VA_ARGS__)
#define log_info_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_INFO, __VA_ARGS__)
#define log_error_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_ERROR, __VA_ARGS__)
#define log_bug_kernel_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_ERROR, "kernel bug: " __VA_ARGS__)
#define log_bug_libinput_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_ERROR, "libinput bug: " __VA_ARGS__)
#define log_bug_client_ratelimit(li_, r_, ...) log_msg_ratelimit((li_), (r_), LIBINPUT_LOG_PRIORITY_ERROR, "client bug: " __VA_ARGS__)

bool
log_is_logged(const struct libinput *libinput, enum libinput_log_priority priority);

void
log_msg_ratelimit(struct libinput *libinput,
		  struct ratelimit *ratelimit,
		  enum libinput_log_priority priority,
		  const char *format,
		  ...) LIBINPUT_ATTRIBUTE_PRINTF(4, 5);

void
log_msg(struct libinput *libinput,
	enum libinput_log_priority priority,
	const char *format,
	...) LIBINPUT_ATTRIBUTE_PRINTF(3, 4);

void
log_msg_va(struct libinput *libinput,
	   enum libinput_log_priority priority,
	   const char *format,
	   va_list args) LIBINPUT_ATTRIBUTE_PRINTF(3, 0);
