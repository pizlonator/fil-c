/*
 * Copyright © 2018 Red Hat, Inc.
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

#include <config.h>

#include <libinput.h>

#include "libinput-util.h"
#include "litest.h"
#include "quirks.h"

static void
log_handler(struct libinput *this_is_null,
	    enum libinput_log_priority priority,
	    const char *format,
	    va_list args)
{
#if 0
	vprintf(format, args);
#endif
}

struct data_dir {
	char *dirname;
	char *filename;
};

static struct data_dir *
data_dir_new(const char *file_content)
{
	struct data_dir *dir = zalloc(sizeof(*dir));
	char dirname[PATH_MAX] = "/tmp/litest-quirk-test-XXXXXX";

	litest_assert_notnull(mkdtemp(dirname));
	dir->dirname = safe_strdup(dirname);

	if (file_content) {
		char *filename = strdup_printf("%s/testfile.quirks", dirname);

		_autofclose_ FILE *fp = fopen(filename, "w+");
#ifndef __clang_analyzer__
		litest_assert_notnull(fp);
#else
		assert(fp);
#endif
		int rc = fputs(file_content, fp); // NOLINT: unix.Stream
		litest_assert_errno_success(rc);
		dir->filename = filename;
	}

	return dir;
}

static void
data_dir_destroy(struct data_dir *dd)
{
	if (dd->filename) {
		unlink(dd->filename);
		free(dd->filename);
	}
	if (dd->dirname) {
		rmdir(dd->dirname);
		free(dd->dirname);
	}
	free(dd);
}

DEFINE_DESTROY_CLEANUP_FUNC(data_dir);

START_TEST(quirks_invalid_dir)
{
	_unref_(quirks_context) *ctx = quirks_init_subsystem("/does-not-exist",
							     NULL,
							     log_handler,
							     NULL,
							     QLOG_LIBINPUT_LOGGING);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_empty_dir)
{
	_destroy_(data_dir) *dd = data_dir_new(NULL);
	_unref_(quirks_context) *ctx = quirks_init_subsystem(dd->dirname,
							     NULL,
							     log_handler,
							     NULL,
							     QLOG_LIBINPUT_LOGGING);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_empty)
{
	const char quirks_file[] = "[Empty Section]";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_double)
{
	const char quirks_file[] = "[Section name]";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_missing_match)
{
	const char quirks_file[] =
		"[Section name]\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_missing_attr)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_match_after_attr)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"AttrSizeHint=10x10\n"
		"MatchName=mouse\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_duplicate_match)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"MatchUdevType=mouse\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_section_duplicate_attr)
{
	/* This shouldn't be allowed but the current parser
	   is happy with it */
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"AttrSizeHint=10x10\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_error_section)
{
	const char quirks_file[] =
		"[Section Missing Bracket\n"
		"MatchUdevType=mouse\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_trailing_whitespace)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse    \n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_unknown_match)
{
	const char quirks_file[] =
		"[Section name]\n"
		"Matchblahblah=mouse\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_unknown_attr)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"Attrblahblah=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_unknown_model)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"Modelblahblah=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_unknown_prefix)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"Fooblahblah=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_error_model_not_one)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=true\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
}
END_TEST

START_TEST(quirks_parse_comment_inline)
{
	const char quirks_file[] =
		"[Section name] # some inline comment\n"
		"MatchUdevType=mouse\t   # another inline comment\n"
		"ModelAppleTouchpad=1#\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_comment_empty)
{
	const char quirks_file[] =
		"[Section name]\n"
		"#\n"
		"   #\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_string_quotes_single)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"AttrKeyboardIntegration='internal'\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_string_quotes_double)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"AttrKeyboardIntegration=\"internal\"\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_bustype)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchBus=usb\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchBus=bluetooth\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchBus=i2c\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchBus=rmi\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchBus=ps2\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_bustype_invalid)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchBus=venga\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_vendor)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchVendor=0x0000\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchVendor=0x0001\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchVendor=0x2343\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_vendor_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchVendor=-1\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVendor=abc\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVendor=0xFFFFF\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVendor=123\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_product)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchProduct=0x12AB\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchProduct=0x0001;0x1234;0xABCD\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchProduct=0x2343\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_product_too_many)
{
	const char prologue[] =
		"[Section name]\n"
		"MatchProduct=0x12AB\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchProduct=";
	const char epilogue[] =
		"\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchProduct=0x2343\n"
		"ModelAppleTouchpad=1\n";

	char matches[4096] = { 0 };

	for (int i = 0; i < 128; i++) {
		int len = strlen(matches);
		int remaining = sizeof(matches) - len;
		snprintf(&matches[len], remaining, "0x%04X", i);
	}

	char *quirks_file = strdup_printf("%s%s%s", prologue, matches, epilogue);
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	free(quirks_file);

	/* This test will only blow up in valgrind/asan */
	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert(ctx == NULL);
}
END_TEST

START_TEST(quirks_parse_product_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchProduct=-1\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchProduct=abc\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchProduct=0xFFFFF\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchProduct=123\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_version)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchVersion=0x0000\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchVersion=0x0001\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchVersion=0x2343\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_version_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchVersion=-1\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVersion=abc\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVersion=0xFFFFF\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchVersion=123\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_name)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchName=1235\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchName=abc\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchName=*foo\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchName=foo*\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchName=foo[]\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchName=*foo*\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_name_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchName=\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_uniq)
{
	const char quirks_file[] =
		"[Section Uniq]\n"
		"MatchUniq=1235\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section Uniq]\n"
		"MatchUniq=abc\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section Uniq]\n"
		"MatchUniq=*foo\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section Uniq]\n"
		"MatchUniq=foo*\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section Uniq]\n"
		"MatchUniq=foo[]\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section Uniq]\n"
		"MatchUniq=*foo*\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_uniq_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchUniq=\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_udev)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=touchpad\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=pointingstick\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=tablet\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=tablet-pad\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=keyboard\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchUdevType=joystick\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_udev_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchUdevType=blah\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchUdevType=\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchUdevType=123\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

START_TEST(quirks_parse_dmi)
{
	const char quirks_file[] =
		"[Section name]\n"
		"MatchDMIModalias=dmi:*\n"
		"ModelAppleTouchpad=1\n"
		"\n"
		"[Section name]\n"
		"MatchDMIModalias=dmi:*svn*pn*:\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
}
END_TEST

START_TEST(quirks_parse_dmi_invalid)
{
	const char *quirks_file[] = {
		"[Section name]\n"
		"MatchDMIModalias=\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchDMIModalias=*pn*\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchDMIModalias=dmi*pn*\n"
		"ModelAppleTouchpad=1\n",
		"[Section name]\n"
		"MatchDMIModalias=foo\n"
		"ModelAppleTouchpad=1\n",
	};

	ARRAY_FOR_EACH(quirks_file, qf) {
		_destroy_(data_dir) *dd = data_dir_new(*qf);

		_unref_(quirks_context) *ctx =
			quirks_init_subsystem(dd->dirname,
					      NULL,
					      log_handler,
					      NULL,
					      QLOG_CUSTOM_LOG_PRIORITIES);
		litest_assert(ctx == NULL);
	}
}
END_TEST

typedef bool (*qparsefunc)(struct quirks *q, enum quirk which, void *data);

/*
   Helper for generic testing, matches on a mouse device with the given
   quirk set to the given string. Creates a data directory, inits the quirks
   and calls func() to return the value in data. The func has to take the
   right data, otherwise boom. Usage:
   rc = test_attr_parse(dev, QUIRK_ATTR_SIZE_HINT,
			"10x30", quirks_get_dimensions,
			&some_struct_quirks_dimensions);
   if (rc == false) // failed to parse
   else // struct now contains the 10, 30 values
 */
static bool
test_attr_parse(struct litest_device *dev,
		enum quirk which,
		const char *str,
		qparsefunc func,
		void *data)
{
	_unref_(udev_device) *ud =
		libinput_device_get_udev_device(dev->libinput_device);
	char buf[512];
	bool result;

	snprintf(buf,
		 sizeof(buf),
		 "[Section name]\n"
		 "MatchUdevType=mouse\n"
		 "%s=%s\n",
		 quirk_get_name(which),
		 str);

	_destroy_(data_dir) *dd = data_dir_new(buf);
	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	if (ctx != NULL) {
		_unref_(quirks) *q = quirks_fetch_for_device(ctx, ud);
		litest_assert_notnull(q);
		litest_assert(func(q, which, data));
		litest_assert(quirks_has_quirk(q, which));
		result = true;
	} else {
		result = false;
	}

	return result;
}

struct qtest_dim {
	const char *str;
	bool success;
	size_t w, h;
};

START_TEST(quirks_parse_dimension_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_SIZE_HINT,
		QUIRK_ATTR_RESOLUTION_HINT,
	};
	/* clang-format off */
	struct qtest_dim test_values[] = {
		{ "10x10", true, 10, 10 },
		{ "20x30", true, 20, 30 },
		{ "-10x30", false, 0, 0 },
		{ "10:30", false, 0, 0 },
		{ "30", false, 0, 0 },
		{ "0x00", false, 0, 0 },
		{ "0xa0", false, 0, 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			struct quirk_dimensions dim;
			bool rc;

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_dimensions,
					     &dim);
			litest_assert_int_eq(rc, t->success);
			if (!rc)
				continue;

			litest_assert_int_eq(dim.x, t->w);
			litest_assert_int_eq(dim.y, t->h);
		}
	}
}
END_TEST

struct qtest_range {
	const char *str;
	bool success;
	int hi, lo;
};

START_TEST(quirks_parse_range_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_TOUCH_SIZE_RANGE,
		QUIRK_ATTR_PRESSURE_RANGE,
	};
	/* clang-format off */
	struct qtest_range test_values[] = {
		{ "20:10", true, 20, 10 },
		{ "30:5", true, 30, 5 },
		{ "30:-10", true, 30, -10 },
		{ "-30:-100", true, -30, -100 },

		{ "5:10", false, 0, 0 },
		{ "5:5", false, 0, 0 },
		{ "-10:5", false, 0, 0 },
		{ "-10:-5", false, 0, 0 },
		{ "10x30", false, 0, 0 },
		{ "30x10", false, 0, 0 },
		{ "30", false, 0, 0 },
		{ "0x00", false, 0, 0 },
		{ "0xa0", false, 0, 0 },
		{ "0x10:0x5", false, 0, 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			struct quirk_range r;
			bool rc;

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_range,
					     &r);
			litest_assert_int_eq(rc, t->success);
			if (!rc)
				continue;

			litest_assert_int_eq(r.lower, t->lo);
			litest_assert_int_eq(r.upper, t->hi);
		}
	}
}
END_TEST

struct qtest_uint {
	const char *str;
	bool success;
	uint32_t val;
};

START_TEST(quirks_parse_uint_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_PALM_SIZE_THRESHOLD,
		QUIRK_ATTR_PALM_PRESSURE_THRESHOLD,
		QUIRK_ATTR_THUMB_PRESSURE_THRESHOLD,
	};
	/* clang-format off */
	struct qtest_uint test_values[] = {
		{ "10", true, 10 },
		{ "0", true, 0 },
		{ "5", true, 5 },
		{ "65535", true, 65535 },
		{ "4294967295", true, 4294967295 },
		{ "-10", false, 0 },
		{ "0x10", false, 0 },
		{ "0xab", false, 0 },
		{ "ab", false, 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			uint32_t v;
			bool rc;

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_uint32,
					     &v);
			litest_assert_int_eq(rc, t->success);
			if (!rc)
				continue;

			litest_assert_int_eq(v, t->val);
		}
	}
}
END_TEST

struct qtest_double {
	const char *str;
	bool success;
	double val;
};

START_TEST(quirks_parse_double_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_TRACKPOINT_MULTIPLIER,
	};
	/* clang-format off */
	struct qtest_double test_values[] = {
		{ "10", true, 10.0 },
		{ "10.0", true, 10.0 },
		{ "-10.0", true, -10.0 },
		{ "0", true, 0.0 },
		{ "0.0", true, 0.0 },
		{ "5.1", true, 5.1 },
		{ "-5.9", true, -5.9 },
		{ "65535", true, 65535 },
		{ "4294967295", true, 4294967295 },
		{ "4294967295.123", true, 4294967295.123 },
		/* our safe_atoi parses hex even though we don't really want
		 * to */
		{ "0x10", false, 0 },
		{ "0xab", false, 0 },
		{ "ab", false, 0 },
		{ "10:5", false, 0 },
		{ "10x5", false, 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			double v;
			bool rc;

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_double,
					     &v);
			litest_assert_int_eq(rc, t->success);
			if (!rc)
				continue;

			litest_assert_double_eq(v, t->val);
		}
	}
}
END_TEST

struct qtest_str {
	const char *str;
	enum quirk where;
};

START_TEST(quirks_parse_string_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_TPKBCOMBO_LAYOUT,
		QUIRK_ATTR_LID_SWITCH_RELIABILITY,
		QUIRK_ATTR_KEYBOARD_INTEGRATION,
	};
	/* clang-format off */
	struct qtest_str test_values[] = {
		{ "below", QUIRK_ATTR_TPKBCOMBO_LAYOUT },
		{ "reliable", QUIRK_ATTR_LID_SWITCH_RELIABILITY },
		{ "write_open", QUIRK_ATTR_LID_SWITCH_RELIABILITY },
		{ "internal", QUIRK_ATTR_KEYBOARD_INTEGRATION },
		{ "external", QUIRK_ATTR_KEYBOARD_INTEGRATION },

		{ "10", 0 },
		{ "-10", 0 },
		{ "0", 0 },
		{ "", 0 },
		{ "banana", 0 },
		{ "honk honk", 0 },
		{ "0x12", 0 },
		{ "0xa", 0 },
		{ "0.0", 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			bool rc;
			char *do_not_use; /* freed before we can use it */

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_string,
					     &do_not_use);
			if (*a == t->where)
				litest_assert_int_eq(rc, true);
			else
				litest_assert_int_eq(rc, false);
		}
	}
}
END_TEST

struct qtest_bool {
	const char *str;
	bool success;
	bool val;
};

START_TEST(quirks_parse_bool_attr)
{
	struct litest_device *dev = litest_current_device();
	enum quirk attrs[] = {
		QUIRK_ATTR_USE_VELOCITY_AVERAGING,
		QUIRK_ATTR_TABLET_SMOOTHING,
	};
	/* clang-format off */
	struct qtest_bool test_values[] = {
		{ "0", true, false },
		{ "1", true, true },
		{ "2", false, false },
		{ "-1", false, false },
		{ "a", false, false },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(attrs, a) {
		ARRAY_FOR_EACH(test_values, t) {
			bool v;
			bool rc;

			rc = test_attr_parse(dev,
					     *a,
					     t->str,
					     (qparsefunc)quirks_get_bool,
					     &v);
			litest_assert(rc == t->success);
			if (!rc)
				continue;

			litest_assert(v == t->val);
		}
	}
}
END_TEST

START_TEST(quirks_parse_integration_attr)
{
	struct litest_device *dev = litest_current_device();
	char *do_not_use; /* freed before we can use it */
	bool rc = test_attr_parse(dev,
				  QUIRK_ATTR_KEYBOARD_INTEGRATION,
				  "internal",
				  (qparsefunc)quirks_get_string,
				  &do_not_use);
	litest_assert(rc);
	rc = test_attr_parse(dev,
			     QUIRK_ATTR_KEYBOARD_INTEGRATION,
			     "external",
			     (qparsefunc)quirks_get_string,
			     &do_not_use);
	litest_assert(rc);
	rc = test_attr_parse(dev,
			     QUIRK_ATTR_TRACKPOINT_INTEGRATION,
			     "internal",
			     (qparsefunc)quirks_get_string,
			     &do_not_use);
	litest_assert(rc);
	rc = test_attr_parse(dev,
			     QUIRK_ATTR_TRACKPOINT_INTEGRATION,
			     "external",
			     (qparsefunc)quirks_get_string,
			     &do_not_use);
	litest_assert(rc);
}
END_TEST

START_TEST(quirks_model_one)
{
	struct litest_device *dev = litest_current_device();
	_unref_(udev_device) *ud =
		libinput_device_get_udev_device(dev->libinput_device);
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=1\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);
	bool isset;

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);

	_unref_(quirks) *q = quirks_fetch_for_device(ctx, ud);
	litest_assert_notnull(q);

	litest_assert(quirks_get_bool(q, QUIRK_MODEL_APPLE_TOUCHPAD, &isset));
	litest_assert(isset == true);
}
END_TEST

START_TEST(quirks_model_zero)
{
	struct litest_device *dev = litest_current_device();
	_unref_(udev_device) *ud =
		libinput_device_get_udev_device(dev->libinput_device);
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=0\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);
	bool isset;

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);

	_unref_(quirks) *q = quirks_fetch_for_device(ctx, ud);
	litest_assert_notnull(q);

	litest_assert(quirks_get_bool(q, QUIRK_MODEL_APPLE_TOUCHPAD, &isset));
	litest_assert(isset == false);
}
END_TEST

START_TEST(quirks_model_override)
{
	struct litest_device *dev = litest_current_device();
	_unref_(udev_device) *ud =
		libinput_device_get_udev_device(dev->libinput_device);
	bool isset;
	bool set = litest_test_param_get_bool(test_env->params, "enable_model");

	/* Test model quirks override by setting, then unsetting (or the
	   other way round) */
	_autofree_ char *quirks_file = strdup_printf(
		"[first]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=%d\n"
		"\n"
		"[second]\n"
		"MatchUdevType=mouse\n"
		"ModelAppleTouchpad=%d\n",
		set ? 0 : 1,
		set ? 1 : 0);
	litest_assert_ptr_notnull(quirks_file);

	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	_unref_(quirks_context) *ctx =
		quirks_init_subsystem(dd->dirname,
				      NULL,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);

	_unref_(quirks) *q = quirks_fetch_for_device(ctx, ud);
	litest_assert_notnull(q);

	litest_assert(quirks_get_bool(q, QUIRK_MODEL_APPLE_TOUCHPAD, &isset));
	litest_assert(isset == set);
}
END_TEST

START_TEST(quirks_model_alps)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct quirks *q;
	bool exists, value = false;

	q = dev->quirks;
	exists = quirks_get_bool(q, QUIRK_MODEL_ALPS_SERIAL_TOUCHPAD, &value);

	if (strstr(libinput_device_get_name(device), "ALPS")) {
		litest_assert(exists);
		litest_assert(value);
	} else {
		litest_assert(!exists);
		litest_assert(!value);
	}
}
END_TEST

START_TEST(quirks_model_wacom)
{
	struct litest_device *dev = litest_current_device();
	struct quirks *q;
	bool exists, value = false;

	q = dev->quirks;
	exists = quirks_get_bool(q, QUIRK_MODEL_WACOM_TOUCHPAD, &value);

	if (libevdev_get_id_vendor(dev->evdev) == VENDOR_ID_WACOM) {
		litest_assert(exists);
		litest_assert(value);
	} else {
		litest_assert(!exists);
		litest_assert(!value);
	}
}
END_TEST

START_TEST(quirks_model_apple)
{
	struct litest_device *dev = litest_current_device();
	struct quirks *q;
	bool exists, value = false;

	q = dev->quirks;
	exists = quirks_get_bool(q, QUIRK_MODEL_APPLE_TOUCHPAD, &value);

	if (libevdev_get_id_vendor(dev->evdev) == VENDOR_ID_APPLE) {
		litest_assert(exists);
		litest_assert(value);
	} else {
		litest_assert(!exists);
		litest_assert(!value);
	}
}
END_TEST

START_TEST(quirks_model_synaptics_serial)
{
	struct litest_device *dev = litest_current_device();
	struct quirks *q;
	bool exists, value = false;

	q = dev->quirks;
	exists = quirks_get_bool(q, QUIRK_MODEL_SYNAPTICS_SERIAL_TOUCHPAD, &value);

	if (libevdev_get_id_vendor(dev->evdev) == VENDOR_ID_SYNAPTICS_SERIAL &&
	    libevdev_get_id_product(dev->evdev) == PRODUCT_ID_SYNAPTICS_SERIAL) {
		litest_assert(exists);
		litest_assert(value);
	} else {
		litest_assert(!exists);
		litest_assert(!value);
	}
}
END_TEST

START_TEST(quirks_call_NULL)
{
	litest_assert(!quirks_fetch_for_device(NULL, NULL));

	litest_assert(!quirks_get_uint32(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_int32(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_range(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_dimensions(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_double(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_string(NULL, QUIRK_NONE, NULL));
	litest_assert(!quirks_get_bool(NULL, QUIRK_NONE, NULL));
}
END_TEST

START_TEST(quirks_ctx_ref)
{
	struct quirks_context *ctx, *ctx2;
	const char quirks_file[] =
		"[Section name]\n"
		"MatchUdevType=mouse\n"
		"AttrSizeHint=10x10\n";
	_destroy_(data_dir) *dd = data_dir_new(quirks_file);

	ctx = quirks_init_subsystem(dd->dirname,
				    NULL,
				    log_handler,
				    NULL,
				    QLOG_CUSTOM_LOG_PRIORITIES);
	litest_assert_notnull(ctx);
	ctx2 = quirks_context_ref(ctx);
	litest_assert_ptr_eq(ctx, ctx2);
	ctx2 = quirks_context_unref(ctx);
	litest_assert_ptr_eq(ctx2, NULL);
	ctx2 = quirks_context_unref(ctx);
	litest_assert_ptr_eq(ctx2, NULL);
}
END_TEST

TEST_COLLECTION(quirks)
{
	/* clang-format off */
	litest_add_deviceless(quirks_invalid_dir);
	litest_add_deviceless(quirks_empty_dir);

	litest_add_deviceless(quirks_section_empty);
	litest_add_deviceless(quirks_section_double);
	litest_add_deviceless(quirks_section_missing_match);
	litest_add_deviceless(quirks_section_missing_attr);
	litest_add_deviceless(quirks_section_match_after_attr);
	litest_add_deviceless(quirks_section_duplicate_match);
	litest_add_deviceless(quirks_section_duplicate_attr);

	litest_add_deviceless(quirks_parse_error_section);
	litest_add_deviceless(quirks_parse_error_trailing_whitespace);
	litest_add_deviceless(quirks_parse_error_unknown_match);
	litest_add_deviceless(quirks_parse_error_unknown_attr);
	litest_add_deviceless(quirks_parse_error_unknown_model);
	litest_add_deviceless(quirks_parse_error_unknown_prefix);
	litest_add_deviceless(quirks_parse_error_model_not_one);
	litest_add_deviceless(quirks_parse_comment_inline);
	litest_add_deviceless(quirks_parse_comment_empty);
	litest_add_deviceless(quirks_parse_string_quotes_single);
	litest_add_deviceless(quirks_parse_string_quotes_double);

	litest_add_deviceless(quirks_parse_bustype);
	litest_add_deviceless(quirks_parse_bustype_invalid);
	litest_add_deviceless(quirks_parse_vendor);
	litest_add_deviceless(quirks_parse_vendor_invalid);
	litest_add_deviceless(quirks_parse_product);
	litest_add_deviceless(quirks_parse_product_too_many);
	litest_add_deviceless(quirks_parse_product_invalid);
	litest_add_deviceless(quirks_parse_version);
	litest_add_deviceless(quirks_parse_version_invalid);
	litest_add_deviceless(quirks_parse_name);
	litest_add_deviceless(quirks_parse_name_invalid);
	litest_add_deviceless(quirks_parse_uniq);
	litest_add_deviceless(quirks_parse_uniq_invalid);
	litest_add_deviceless(quirks_parse_udev);
	litest_add_deviceless(quirks_parse_udev_invalid);
	litest_add_deviceless(quirks_parse_dmi);
	litest_add_deviceless(quirks_parse_dmi_invalid);

	litest_add_for_device(quirks_parse_dimension_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_range_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_uint_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_double_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_string_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_bool_attr, LITEST_MOUSE);
	litest_add_for_device(quirks_parse_integration_attr, LITEST_MOUSE);

	litest_add_for_device(quirks_model_one, LITEST_MOUSE);
	litest_add_for_device(quirks_model_zero, LITEST_MOUSE);
	litest_with_parameters(params, "enable_model", 'b') {
		litest_add_parametrized_for_device(quirks_model_override, LITEST_MOUSE, params);
	}

	litest_add(quirks_model_alps, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add(quirks_model_wacom, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add(quirks_model_apple, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add(quirks_model_synaptics_serial, LITEST_TOUCHPAD, LITEST_ANY);

	litest_add_deviceless(quirks_call_NULL);
	litest_add_deviceless(quirks_ctx_ref);
	/* clang-format on */
}
