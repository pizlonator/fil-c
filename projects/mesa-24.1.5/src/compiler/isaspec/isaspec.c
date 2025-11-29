/*
 * Copyright © 2020 Google, Inc.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "isaspec.h"

#include <stdarg.h>
#include <stdlib.h>

#include "util/u_string.h"

void
isa_print(struct isa_print_state *state, const char *fmt, ...)
{
	char *buffer;
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(&buffer, fmt, args);
	va_end(args);

	if (ret != -1) {
		const size_t len = strlen(buffer);

		for (size_t i = 0; i < len; i++) {
			const char c = buffer[i];

			fputc(c, state->out);
			state->line_column++;

			if (c == '\n') {
				state->line_column = 0;
			}
		}

		free(buffer);

		return;
	}
}
