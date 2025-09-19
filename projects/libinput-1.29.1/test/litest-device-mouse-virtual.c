/*
 * Copyright © 2025 Ryan Hendrickson
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

#include "litest-int.h"
#include "litest.h"

static struct input_id input_id = {
	.bustype = 0x11,
	.vendor = 0x1,
	.product = 0x2,
};

/* clang-format off */
static int events[] = {
	EV_REL, REL_WHEEL,
	EV_REL, REL_WHEEL_HI_RES,
	-1, -1,
};
/* clang-format on */

static const char quirk_file[] =
	"[litest Virtual Mouse is virtual]\n"
	"MatchName=litest Virtual Mouse\n"
	"AttrIsVirtual=1\n";

TEST_DEVICE(LITEST_MOUSE_VIRTUAL,
	    .features = LITEST_WHEEL |
			LITEST_IGNORED, /* Only needed for mouse wheel tests */
	    .interface = NULL,

	    .name = "Virtual Mouse",
	    .id = &input_id,
	    .events = events,
	    .absinfo = NULL,
	    .quirk_file = quirk_file, )
