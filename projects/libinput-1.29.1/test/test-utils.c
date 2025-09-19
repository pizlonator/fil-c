/*
 * Copyright © 2014 Red Hat, Inc.
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <valgrind/valgrind.h>

#include "util-bits.h"
#include "util-files.h"
#include "util-input-event.h"
#include "util-list.h"
#include "util-macros.h"
#include "util-matrix.h"
#include "util-mem.h"
#include "util-newtype.h"
#include "util-prop-parsers.h"
#include "util-range.h"
#include "util-ratelimit.h"
#include "util-stringbuf.h"
#include "util-strings.h"
#include "util-time.h"

#include "evdev-frame.h"
#include "litest-runner.h"
#include "litest.h"

#define TEST_VERSIONSORT
#include "libinput-versionsort.h"

START_TEST(auto_test)
{
	/* This one is just a compile test */
	auto tv = us2tv(0);
	tv.tv_sec = 0;
	litest_assert_int_eq(tv.tv_sec, 0);
}
END_TEST

START_TEST(mkdir_p_test)
{
	const char *testdir = "/tmp/litest_mkdir_test";
	litest_assert_neg_errno_success(mkdir_p("/"));

	rmdir(testdir);
	litest_assert_neg_errno_success(mkdir_p(testdir));
	/* EEXIST is not an error */
	litest_assert_neg_errno_success(mkdir_p(testdir));
	rmdir(testdir);

	int ret = mkdir_p("/proc/foo");
	litest_assert_msg(ret == -ENOENT || ret == -EACCES,
			  "mkdir_p(\"/proc/foo\") returned %d\n",
			  ret);
}
END_TEST

START_TEST(rmdir_r_test)
{
	const char *testdir = "/tmp/litest_rmdir_test";
	_autofree_ char *path = strdup_printf("%s/foo/bar/baz", testdir);
	mkdir_p(path);

	_autofree_ char *f1 = strdup_printf("%s/remain", testdir);
	_autofree_ char *f2 = strdup_printf("%s/foo/remove", testdir);
	_autofree_ char *f3 = strdup_printf("%s/foo/bar/to-remove", testdir);
	_autofree_ char *f4 = strdup_printf("%s/foo/bar/baz/wipeme", testdir);

	litest_assert_errno_success(close(open(f1, O_WRONLY | O_CREAT, 0644)));
	litest_assert_errno_success(close(open(f2, O_WRONLY | O_CREAT, 0644)));
	litest_assert_errno_success(close(open(f3, O_WRONLY | O_CREAT, 0644)));
	litest_assert_errno_success(close(open(f4, O_WRONLY | O_CREAT, 0644)));

	struct stat st;
	litest_assert_errno_success(stat(f1, &st));
	litest_assert_errno_success(stat(f2, &st));
	litest_assert_errno_success(stat(f3, &st));
	litest_assert_errno_success(stat(f4, &st));

	_autofree_ char *rmpath = strdup_printf("%s/foo/", testdir);
	int rc = rmdir_r(rmpath);
	litest_assert_neg_errno_success(rc);

	litest_assert_errno_success(stat(f1, &st));
	litest_assert_errno_success(stat(testdir, &st));

	rc = stat(f2, &st) < 0 ? -errno : 0;
	litest_assert_int_eq(rc, -ENOENT);
	rc = stat(f3, &st) < 0 ? -errno : 0;
	litest_assert_int_eq(rc, -ENOENT);
	rc = stat(f4, &st) < 0 ? -errno : 0;
	litest_assert_int_eq(rc, -ENOENT);
}
END_TEST

START_TEST(tmpdir_test)
{
	_autofree_ char *tmpdir_path = NULL;
	{
		_destroy_(tmpdir) *tmpdir = tmpdir_create(NULL);

		tmpdir_path = safe_strdup(tmpdir->path);

		_autofree_ char *f1 = strdup_printf("%s/wipeme", tmpdir_path);
		litest_assert_errno_success(close(open(f1, O_WRONLY | O_CREAT, 0644)));
	}
	struct stat st;
	int rc = stat(tmpdir_path, &st) < 0 ? -errno : 0;
	litest_assert_int_eq(rc, -ENOENT);
}
END_TEST

START_TEST(find_files_test)
{
	_autofree_ char *dirname = strdup("/tmp/litest_find_files_test.XXXXXX");
	mkdtemp(dirname);

	_autofree_ char *d1 = strdup_printf("%s/d1", dirname);
	_autofree_ char *d2 = strdup_printf("%s/d2", dirname);
	_autofree_ char *d3 = strdup_printf("%s/d3", dirname);

	litest_assert_neg_errno_success(mkdir_p(d1));
	litest_assert_neg_errno_success(mkdir_p(d2));
	litest_assert_neg_errno_success(mkdir_p(d3));

	/* clang-format off */
	struct f {
		const char *name;
		const char *dir1;
		const char *dir2;
		const char *dir3;
		char *expected;
	} files[] = {
		{ "10-abc.suf", d1, d2, d3 },
		{ "20-def.suf", d1, NULL, d3 },
		{ "30-ghi.suf", d1, d2, NULL },
		{ "40-jkl.suf", NULL, d2, NULL },
		{ "50-mno.suf", NULL, d2, d3 },
		{ "60-pgr.suf", NULL, NULL, d3 },
		{ "70-abc.suf", NULL, NULL, d3 },
		{ "21-xyz.fix", NULL, NULL, d3 },
		{ "35-uvw.fix", NULL, d2, d3 },
		{ "70-rst.fix", d1, NULL, d3 },
		{ NULL },
	};
	/* clang-format on */
	for (struct f *f = files; f->name; f++) {
		if (f->dir1) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir1, f->name);
			close(open(path, O_WRONLY | O_CREAT, 0644));
			f->expected = steal(&path);
		}
		if (f->dir2) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir2, f->name);
			close(open(path, O_WRONLY | O_CREAT, 0644));
			if (!f->expected)
				f->expected = steal(&path);
		}
		if (f->dir3) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir3, f->name);
			close(open(path, O_WRONLY | O_CREAT, 0644));
			if (!f->expected)
				f->expected = steal(&path);
		}
	}

	const char *dirs[] = { d1, d2, d3, NULL };
	size_t nfiles;
	_autostrvfree_ char **paths = list_files(dirs, "suf", &nfiles);
	litest_assert_int_eq(nfiles, (size_t)7);
	litest_assert_str_eq(paths[0], files[0].expected);
	litest_assert_str_eq(paths[1], files[1].expected);
	litest_assert_str_eq(paths[2], files[2].expected);
	litest_assert_str_eq(paths[3], files[3].expected);
	litest_assert_str_eq(paths[4], files[4].expected);
	litest_assert_str_eq(paths[5], files[5].expected);
	litest_assert_str_eq(paths[6], files[6].expected);
	litest_assert_str_eq(paths[7], NULL);

	for (struct f *f = files; f->name; f++) {
		if (f->dir1) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir1, f->name);
			unlink(path);
		}
		if (f->dir2) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir2, f->name);
			unlink(path);
		}
		if (f->dir3) {
			_autofree_ char *path =
				strdup_printf("%s/%s", f->dir3, f->name);
			unlink(path);
		}
		free(f->expected);
	}
	rmdir(d1);
	rmdir(d2);
	rmdir(d3);
	rmdir(dirname);

	const char *empty[] = { NULL };
	_autostrvfree_ char **empty_path = list_files(empty, "suf", &nfiles);
	litest_assert_int_eq(nfiles, (size_t)0);
	litest_assert_ptr_notnull(empty_path);
	litest_assert_ptr_null(empty_path[0]);

	_autostrvfree_ char **also_empty_path = list_files(NULL, "suf", &nfiles);
	litest_assert_int_eq(nfiles, (size_t)0);
	litest_assert_ptr_notnull(also_empty_path);
	litest_assert_ptr_null(also_empty_path[0]);
}
END_TEST

START_TEST(array_for_each)
{
	int ai[6];
	char ac[10];
	struct as {
		int a;
		char b;
		int *ptr;
	} as[32];

	for (size_t i = 0; i < 6; i++)
		ai[i] = 20 + i;
	for (size_t i = 0; i < 10; i++)
		ac[i] = 100 + i;
	for (size_t i = 0; i < 32; i++) {
		as[i].a = 10 + i;
		as[i].b = 20 + i;
		as[i].ptr = (int *)0xab + i;
	}

	int iexpected = 20;
	ARRAY_FOR_EACH(ai, entry) {
		litest_assert_int_eq(*entry, iexpected);
		++iexpected;
	}
	litest_assert_int_eq(iexpected, 26);

	int cexpected = 100;
	ARRAY_FOR_EACH(ac, entry) {
		litest_assert_int_eq(*entry, cexpected);
		++cexpected;
	}
	litest_assert_int_eq(cexpected, 110);

	struct as sexpected = {
		.a = 10,
		.b = 20,
		.ptr = (int *)0xab,
	};
	ARRAY_FOR_EACH(as, entry) {
		litest_assert_int_eq(entry->a, sexpected.a);
		litest_assert_int_eq(entry->b, sexpected.b);
		litest_assert_ptr_eq(entry->ptr, sexpected.ptr);
		++sexpected.a;
		++sexpected.b;
		++sexpected.ptr;
	}
	litest_assert_int_eq(sexpected.a, 42);
}
END_TEST

START_TEST(bitfield_helpers)
{
	/* This value has a bit set on all of the word boundaries we want to
	 * test: 0, 1, 7, 8, 31, 32, and 33
	 */
	unsigned char read_bitfield[] = { 0x83, 0x1, 0x0, 0x80, 0x3 };
	unsigned char write_bitfield[ARRAY_LENGTH(read_bitfield)] = { 0 };
	size_t i;

	/* Now check that the bitfield we wrote to came out to be the same as
	 * the bitfield we were writing from */
	for (i = 0; i < ARRAY_LENGTH(read_bitfield) * 8; i++) {
		switch (i) {
		case 0:
		case 1:
		case 7:
		case 8:
		case 31:
		case 32:
		case 33:
			litest_assert(bit_is_set(read_bitfield, i));
			set_bit(write_bitfield, i);
			break;
		default:
			litest_assert(!bit_is_set(read_bitfield, i));
			clear_bit(write_bitfield, i);
			break;
		}
	}

	litest_assert_int_eq(
		memcmp(read_bitfield, write_bitfield, sizeof(read_bitfield)),
		0);
}
END_TEST

START_TEST(bitmask_test)
{
	{
		bitmask_t mask1 = bitmask_from_u32(0x12345678U);
		litest_assert(bitmask_as_u32(mask1) == 0x12345678U);

		bitmask_t mask2 = bitmask_from_u32(0);
		litest_assert_int_eq(bitmask_as_u32(mask2), 0U);

		bitmask_t mask3 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert_int_eq(bitmask_as_u32(mask3), 0xFFFFFFFFU);
	}
	{
		bitmask_t mask1 = bitmask_new();
		litest_assert(bitmask_is_empty(mask1));

		bitmask_t mask2 = bitmask_from_u32(0x00000001U);
		litest_assert(!bitmask_is_empty(mask2));

		bitmask_t mask3 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(!bitmask_is_empty(mask3));
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits1 = bitmask_from_u32(0x00000003U);
		litest_assert(bitmask_any(mask1, bits1));

		bitmask_t mask2 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits2 = bitmask_from_u32(0x000000F0U);
		litest_assert(!bitmask_any(mask2, bits2));

		bitmask_t mask3 = bitmask_from_u32(0x00000000U);
		bitmask_t bits3 = bitmask_from_u32(0x00000001U);
		litest_assert(!bitmask_any(mask3, bits3));

		bitmask_t mask4 = bitmask_from_u32(0xFFFFFFFFU);
		bitmask_t bits4 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(bitmask_any(mask4, bits4));

		bitmask_t mask5 = bitmask_from_u32(0x10000000U);
		bitmask_t bits5 = bitmask_from_u32(0x10000000U);
		litest_assert(bitmask_any(mask5, bits5));
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits1 = bitmask_from_u32(0x00000003U);
		litest_assert(bitmask_all(mask1, bits1));
		litest_assert(!bitmask_all(bits1, mask1));

		bitmask_t mask2 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits2 = bitmask_from_u32(0x0000000FU);
		litest_assert(bitmask_all(mask2, bits2));
		litest_assert(bitmask_all(bits2, mask2));

		bitmask_t mask3 = bitmask_from_u32(0x00000000U);
		bitmask_t bits3 = bitmask_from_u32(0x00000000U);
		litest_assert(!bitmask_all(mask3, bits3)); /* zero is special */

		bitmask_t mask4 = bitmask_from_u32(0xFFFFFFFFU);
		bitmask_t bits4 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(bitmask_all(mask4, bits4));

		bitmask_t mask5 = bitmask_from_u32(0x10000000U);
		bitmask_t bits5 = bitmask_from_u32(0x10000000U);
		litest_assert(bitmask_all(mask5, bits5));
	}
	{

		bitmask_t mask1 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits1 = bitmask_from_u32(0x000000F0U);
		litest_assert(!bitmask_merge(&mask1, bits1));
		litest_assert_int_eq(mask1.mask, 0x000000FFU);

		bitmask_t mask2 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits2 = bitmask_from_u32(0x0000000FU);
		litest_assert(bitmask_merge(&mask2, bits2));
		litest_assert_int_eq(mask2.mask, 0x0000000FU);

		bitmask_t mask3 = bitmask_new();
		bitmask_t bits3 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(!bitmask_merge(&mask3, bits3));
		litest_assert_int_eq(mask3.mask, 0xFFFFFFFFU);

		bitmask_t mask4 = bitmask_from_u32(0x80000000U);
		bitmask_t bits4 = bitmask_from_u32(0x00000001U);
		litest_assert(!bitmask_merge(&mask4, bits4));
		litest_assert_int_eq(mask4.mask, 0x80000001U);
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0x000000FFU);
		bitmask_t bits1 = bitmask_from_u32(0x0000000FU);
		litest_assert(bitmask_clear(&mask1, bits1));
		litest_assert_int_eq(mask1.mask, 0x000000F0U);

		bitmask_t mask2 = bitmask_from_u32(0x0000000FU);
		bitmask_t bits2 = bitmask_from_u32(0x0000000FU);
		litest_assert(bitmask_clear(&mask2, bits2));
		litest_assert_int_eq(mask2.mask, 0x00000000U);

		bitmask_t mask3 = bitmask_from_u32(0xFFFFFFFFU);
		bitmask_t bits3 = bitmask_from_u32(0x00000000U);
		litest_assert(!bitmask_clear(&mask3, bits3)); /* zero is special */
		litest_assert_int_eq(mask3.mask, 0xFFFFFFFFU);

		bitmask_t mask4 = bitmask_from_u32(0xFFFFFFFFU);
		bitmask_t bits4 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(bitmask_clear(&mask4, bits4));
		litest_assert_int_eq(mask4.mask, 0x0U);
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0x00000001U);
		litest_assert(bitmask_bit_is_set(mask1, 0));
		litest_assert(!bitmask_bit_is_set(mask1, 1));

		bitmask_t mask2 = bitmask_from_u32(0x80000000U);
		litest_assert(bitmask_bit_is_set(mask2, 31));
		litest_assert(!bitmask_bit_is_set(mask2, 0));

		bitmask_t mask3 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(bitmask_bit_is_set(mask3, 0));
		litest_assert(bitmask_bit_is_set(mask3, 31));
		litest_assert(bitmask_bit_is_set(mask3, 16));

		bitmask_t mask4 = bitmask_new();
		litest_assert(!bitmask_bit_is_set(mask4, 0));
		litest_assert(!bitmask_bit_is_set(mask4, 1));
	}
	{
		bitmask_t mask1 = bitmask_new();
		litest_assert(!bitmask_set_bit(&mask1, 0));
		litest_assert_int_eq(mask1.mask, 0x00000001U);

		litest_assert(bitmask_set_bit(&mask1, 0));
		litest_assert_int_eq(mask1.mask, 0x00000001U);

		litest_assert(!bitmask_set_bit(&mask1, 31));
		litest_assert_int_eq(mask1.mask, 0x80000001U);

		bitmask_t mask2 = bitmask_from_u32(0x0000000FU);
		litest_assert(!bitmask_set_bit(&mask2, 4));
		litest_assert_int_eq(mask2.mask, 0x0000001FU);
		litest_assert(bitmask_set_bit(&mask2, 4));
		litest_assert_int_eq(mask2.mask, 0x0000001FU);
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert(bitmask_clear_bit(&mask1, 0));
		litest_assert_int_eq(mask1.mask, 0xFFFFFFFEU);

		litest_assert(!bitmask_clear_bit(&mask1, 0));
		litest_assert_int_eq(mask1.mask, 0xFFFFFFFEU);

		litest_assert(bitmask_clear_bit(&mask1, 31));
		litest_assert_int_eq(mask1.mask, 0x7FFFFFFEU);

		bitmask_t mask2 = bitmask_from_u32(0x0000001FU);
		litest_assert(bitmask_clear_bit(&mask2, 4));
		litest_assert_int_eq(mask2.mask, 0x0000000FU);
		litest_assert(!bitmask_clear_bit(&mask2, 4));
		litest_assert_int_eq(mask2.mask, 0x0000000FU);
	}
	{
		bitmask_t mask1 = bitmask_from_bit(0);
		litest_assert_int_eq(mask1.mask, 0x00000001U);

		bitmask_t mask2 = bitmask_from_bit(31);
		litest_assert_int_eq(mask2.mask, 0x80000000U);

		bitmask_t mask3 = bitmask_from_bit(16);
		litest_assert_int_eq(mask3.mask, 0x00010000U);
	}
	{
		bitmask_t mask1 = bitmask_from_u32(0x12345678U);
		litest_assert_int_eq(mask1.mask, 0x12345678U);

		bitmask_t mask2 = bitmask_from_u32(0);
		litest_assert_int_eq(mask2.mask, 0U);

		bitmask_t mask3 = bitmask_from_u32(0xFFFFFFFFU);
		litest_assert_int_eq(mask3.mask, 0xFFFFFFFFU);
	}
	{
		bitmask_t mask1 = bitmask_from_bits(1, 2, 5);
		litest_assert_int_eq(mask1.mask, bit(1) | bit(2) | bit(5));

		bitmask_t mask2 = bitmask_from_bits(0);
		litest_assert_int_eq(mask2.mask, bit(0));
	}
	{
		bitmask_t mask1 = bitmask_from_masks(0x1, 0x2, 0x8);
		litest_assert_int_eq(mask1.mask, 0x0000000BU);

		bitmask_t mask2 = bitmask_from_masks(0x0);
		litest_assert_int_eq(mask2.mask, 0x00000000U);
	}
}
END_TEST

START_TEST(matrix_helpers)
{
	struct matrix m1, m2, m3;
	float f[6] = { 1, 2, 3, 4, 5, 6 };
	int x, y;
	int row, col;

	matrix_init_identity(&m1);

	for (row = 0; row < 3; row++) {
		for (col = 0; col < 3; col++) {
			litest_assert_int_eq(m1.val[row][col], (row == col) ? 1 : 0);
		}
	}
	litest_assert(matrix_is_identity(&m1));

	matrix_from_farray6(&m2, f);
	litest_assert_int_eq(m2.val[0][0], 1);
	litest_assert_int_eq(m2.val[0][1], 2);
	litest_assert_int_eq(m2.val[0][2], 3);
	litest_assert_int_eq(m2.val[1][0], 4);
	litest_assert_int_eq(m2.val[1][1], 5);
	litest_assert_int_eq(m2.val[1][2], 6);
	litest_assert_int_eq(m2.val[2][0], 0);
	litest_assert_int_eq(m2.val[2][1], 0);
	litest_assert_int_eq(m2.val[2][2], 1);

	x = 100;
	y = 5;
	matrix_mult_vec(&m1, &x, &y);
	litest_assert_int_eq(x, 100);
	litest_assert_int_eq(y, 5);

	matrix_mult(&m3, &m1, &m1);
	litest_assert(matrix_is_identity(&m3));

	matrix_init_scale(&m2, 2, 4);
	litest_assert_int_eq(m2.val[0][0], 2);
	litest_assert_int_eq(m2.val[0][1], 0);
	litest_assert_int_eq(m2.val[0][2], 0);
	litest_assert_int_eq(m2.val[1][0], 0);
	litest_assert_int_eq(m2.val[1][1], 4);
	litest_assert_int_eq(m2.val[1][2], 0);
	litest_assert_int_eq(m2.val[2][0], 0);
	litest_assert_int_eq(m2.val[2][1], 0);
	litest_assert_int_eq(m2.val[2][2], 1);

	matrix_mult_vec(&m2, &x, &y);
	litest_assert_int_eq(x, 200);
	litest_assert_int_eq(y, 20);

	matrix_init_translate(&m2, 10, 100);
	litest_assert_int_eq(m2.val[0][0], 1);
	litest_assert_int_eq(m2.val[0][1], 0);
	litest_assert_int_eq(m2.val[0][2], 10);
	litest_assert_int_eq(m2.val[1][0], 0);
	litest_assert_int_eq(m2.val[1][1], 1);
	litest_assert_int_eq(m2.val[1][2], 100);
	litest_assert_int_eq(m2.val[2][0], 0);
	litest_assert_int_eq(m2.val[2][1], 0);
	litest_assert_int_eq(m2.val[2][2], 1);

	matrix_mult_vec(&m2, &x, &y);
	litest_assert_int_eq(x, 210);
	litest_assert_int_eq(y, 120);

	matrix_to_farray6(&m2, f);
	litest_assert_int_eq(f[0], 1);
	litest_assert_int_eq(f[1], 0);
	litest_assert_int_eq(f[2], 10);
	litest_assert_int_eq(f[3], 0);
	litest_assert_int_eq(f[4], 1);
	litest_assert_int_eq(f[5], 100);
}
END_TEST

START_TEST(ratelimit_helpers)
{
	struct ratelimit rl;
	unsigned int i, j;

	/* 10 attempts every 1000ms */
	ratelimit_init(&rl, ms2us(1000), 10);

	for (j = 0; j < 3; ++j) {
		/* a burst of 9 attempts must succeed */
		for (i = 0; i < 9; ++i) {
			litest_assert_enum_eq(ratelimit_test(&rl), RATELIMIT_PASS);
		}

		/* the 10th attempt reaches the threshold */
		litest_assert_enum_eq(ratelimit_test(&rl), RATELIMIT_THRESHOLD);

		/* ..then further attempts must fail.. */
		litest_assert_enum_eq(ratelimit_test(&rl), RATELIMIT_EXCEEDED);

		/* ..regardless of how often we try. */
		for (i = 0; i < 100; ++i) {
			litest_assert_enum_eq(ratelimit_test(&rl), RATELIMIT_EXCEEDED);
		}

		/* ..even after waiting 20ms */
		msleep(100);
		for (i = 0; i < 100; ++i) {
			litest_assert_enum_eq(ratelimit_test(&rl), RATELIMIT_EXCEEDED);
		}

		/* but after 1000ms the counter is reset */
		msleep(950); /* +50ms to account for time drifts */
	}
}
END_TEST

struct parser_test {
	char *tag;
	int expected_value;
};

START_TEST(dpi_parser)
{
	/* clang-format off */
	struct parser_test tests[] = {
		{ "450 *1800 3200", 1800 },
		{ "*450 1800 3200", 450 },
		{ "450 1800 *3200", 3200 },
		{ "450 1800 3200", 3200 },
		{ "450 1800 failboat", 0 },
		{ "450 1800 *failboat", 0 },
		{ "0 450 1800 *3200", 0 },
		{ "450@37 1800@12 *3200@6", 3200 },
		{ "450@125 1800@125   *3200@125  ", 3200 },
		{ "450@125 *1800@125  3200@125", 1800 },
		{ "*this @string fails", 0 },
		{ "12@34 *45@", 0 },
		{ "12@a *45@", 0 },
		{ "12@a *45@25", 0 },
		{ "                                      * 12, 450, 800", 0 },
		{ "                                      *12, 450, 800", 12 },
		{ "*12, *450, 800", 12 },
		{ "*-23412, 450, 800", 0 },
		{ "112@125, 450@125, 800@125, 900@-125", 0 },
		{ "", 0 },
		{ "   ", 0 },
		{ "* ", 0 },
		{ NULL, 0 },
	};
	/* clang-format on */
	int i, dpi;

	for (i = 0; tests[i].tag != NULL; i++) {
		dpi = parse_mouse_dpi_property(tests[i].tag);
		litest_assert_int_eq(dpi, tests[i].expected_value);
	}

	dpi = parse_mouse_dpi_property(NULL);
	litest_assert_int_eq(dpi, 0);
}
END_TEST

START_TEST(wheel_click_parser)
{
	/* clang-format off */
	struct parser_test tests[] = {
		{ "1", 1 },
		{ "10", 10 },
		{ "-12", -12 },
		{ "360", 360 },

		{ "0", 0 },
		{ "-0", 0 },
		{ "a", 0 },
		{ "10a", 0 },
		{ "10-", 0 },
		{ "sadfasfd", 0 },
		{ "361", 0 },
		{ NULL, 0 },
	};
	/* clang-format on */

	int i, angle;

	for (i = 0; tests[i].tag != NULL; i++) {
		angle = parse_mouse_wheel_click_angle_property(tests[i].tag);
		litest_assert_int_eq(angle, tests[i].expected_value);
	}
}
END_TEST

START_TEST(wheel_click_count_parser)
{
	/* clang-format off */
	struct parser_test tests[] = {
		{ "1", 1 },
		{ "10", 10 },
		{ "-12", -12 },
		{ "360", 360 },

		{ "0", 0 },
		{ "-0", 0 },
		{ "a", 0 },
		{ "10a", 0 },
		{ "10-", 0 },
		{ "sadfasfd", 0 },
		{ "361", 0 },
		{ NULL, 0 }
	};
	/* clang-format on */

	int i, angle;

	for (i = 0; tests[i].tag != NULL; i++) {
		angle = parse_mouse_wheel_click_count_property(tests[i].tag);
		litest_assert_int_eq(angle, tests[i].expected_value);
	}

	angle = parse_mouse_wheel_click_count_property(NULL);
	litest_assert_int_eq(angle, 0);
}
END_TEST

START_TEST(dimension_prop_parser)
{
	/* clang-format off */
	struct parser_test_dimension {
		char *tag;
		bool success;
		size_t x, y;
	} tests[] = {
		{ "10x10", true, 10, 10 },
		{ "1x20", true, 1, 20 },
		{ "1x8000", true, 1, 8000 },
		{ "238492x428210", true, 238492, 428210 },
		{ "0x0", false, 0, 0 },
		{ "-10x10", false, 0, 0 },
		{ "-1", false, 0, 0 },
		{ "1x-99", false, 0, 0 },
		{ "0", false, 0, 0 },
		{ "100", false, 0, 0 },
		{ "", false, 0, 0 },
		{ "abd", false, 0, 0 },
		{ "xabd", false, 0, 0 },
		{ "0xaf", false, 0, 0 },
		{ "0x0x", false, 0, 0 },
		{ "x10", false, 0, 0 },
		{ NULL, false, 0, 0 },
	};
	/* clang-format on */
	int i;
	size_t x, y;
	bool success;

	for (i = 0; tests[i].tag != NULL; i++) {
		x = y = 0xad;
		success = parse_dimension_property(tests[i].tag, &x, &y);
		litest_assert(success == tests[i].success);
		if (success) {
			litest_assert_int_eq(x, tests[i].x);
			litest_assert_int_eq(y, tests[i].y);
		} else {
			litest_assert_int_eq(x, 0xadU);
			litest_assert_int_eq(y, 0xadU);
		}
	}

	success = parse_dimension_property(NULL, &x, &y);
	litest_assert(success == false);
}
END_TEST

START_TEST(reliability_prop_parser)
{
	/* clang-format off */
	struct parser_test_reliability {
		char *tag;
		bool success;
		enum switch_reliability reliability;
	} tests[] = {
		{ "reliable", true, RELIABILITY_RELIABLE },
		{ "unreliable", true, RELIABILITY_UNRELIABLE },
		{ "write_open", true, RELIABILITY_WRITE_OPEN },
		{ "", false, 0 },
		{ "0", false, 0 },
		{ "1", false, 0 },
		{ NULL, false, 0, },
	};
	/* clang-format on */
	enum switch_reliability r;
	bool success;
	int i;

	for (i = 0; tests[i].tag != NULL; i++) {
		r = 0xaf;
		success = parse_switch_reliability_property(tests[i].tag, &r);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(r, tests[i].reliability);
		else
			litest_assert_int_eq(r, 0xafU);
	}

	success = parse_switch_reliability_property(NULL, &r);
	litest_assert(success == true);
	litest_assert_enum_eq(r, RELIABILITY_RELIABLE);

	success = parse_switch_reliability_property("foo", NULL);
	litest_assert(success == false);
}
END_TEST

START_TEST(calibration_prop_parser)
{
#define DEFAULT_VALUES { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 }
	const float untouched[6] = DEFAULT_VALUES;
	/* clang-format off */
	struct parser_test_calibration {
		char *prop;
		bool success;
		float values[6];
	} tests[] = {
		{ "", false, DEFAULT_VALUES },
		{ "banana", false, DEFAULT_VALUES },
		{ "1 2 3 a 5 6", false, DEFAULT_VALUES },
		{ "2", false, DEFAULT_VALUES },
		{ "2 3 4 5 6", false, DEFAULT_VALUES },
		{ "1 2 3 4 5 6", true, DEFAULT_VALUES },
		{ "6.00012 3.244 4.238 5.2421 6.0134 8.860", true,
			{ 6.00012, 3.244, 4.238, 5.2421, 6.0134, 8.860 }},
		{ "0xff 2 3 4 5 6", false, DEFAULT_VALUES },
		{ NULL, false, DEFAULT_VALUES },
	};
	/* clang-format on */
	bool success;
	float calibration[6];
	int rc;
	int i;

	for (i = 0; tests[i].prop != NULL; i++) {
		memcpy(calibration, untouched, sizeof(calibration));

		success = parse_calibration_property(tests[i].prop, calibration);
		litest_assert_int_eq(success, tests[i].success);
		if (success)
			rc = memcmp(tests[i].values, calibration, sizeof(calibration));
		else
			rc = memcmp(untouched, calibration, sizeof(calibration));
		litest_assert_int_eq(rc, 0);
	}

	memcpy(calibration, untouched, sizeof(calibration));

	success = parse_calibration_property(NULL, calibration);
	litest_assert(success == false);
	rc = memcmp(untouched, calibration, sizeof(calibration));
	litest_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(range_prop_parser)
{
	/* clang-format off */
	struct parser_test_range {
		char *tag;
		bool success;
		int hi, lo;
	} tests[] = {
		{ "10:8", true, 10, 8 },
		{ "100:-1", true, 100, -1 },
		{ "-203813:-502023", true, -203813, -502023 },
		{ "238492:28210", true, 238492, 28210 },
		{ "none", true, 0, 0 },
		{ "0:0", false, 0, 0 },
		{ "", false, 0, 0 },
		{ "abcd", false, 0, 0 },
		{ "10:30:10", false, 0, 0 },
		{ NULL, false, 0, 0 },
	};
	/* clang-format on */
	int i;
	int hi, lo;
	bool success;

	for (i = 0; tests[i].tag != NULL; i++) {
		hi = lo = 0xad;
		success = parse_range_property(tests[i].tag, &hi, &lo);
		litest_assert(success == tests[i].success);
		if (success) {
			litest_assert_int_eq(hi, tests[i].hi);
			litest_assert_int_eq(lo, tests[i].lo);
		} else {
			litest_assert_int_eq(hi, 0xad);
			litest_assert_int_eq(lo, 0xad);
		}
	}

	success = parse_range_property(NULL, NULL, NULL);
	litest_assert(success == false);
}
END_TEST

START_TEST(boolean_prop_parser)
{
	/* clang-format off */
	struct parser_test_range {
		char *tag;
		bool success;
		bool b;
	} tests[] = {
		{ "0", true, false },
		{ "1", true, true },
		{ "-1", false, false },
		{ "2", false, false },
		{ "abcd", false, false },
		{ NULL, false, false },
	};
	/* clang-format on */
	int i;
	bool success, b;

	for (i = 0; tests[i].tag != NULL; i++) {
		b = false;
		success = parse_boolean_property(tests[i].tag, &b);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(b, tests[i].b);
		else
			litest_assert_int_eq(b, false);
	}

	success = parse_boolean_property(NULL, NULL);
	litest_assert(success == false);
}
END_TEST

START_TEST(evcode_prop_parser)
{
	/* clang-format off */
	struct parser_test_tuple {
		const char *prop;
		bool success;
		size_t nevents;
		struct input_event events[20];
	} tests[] = {
		{ "+EV_KEY", true, 1, {{ .type = EV_KEY, .code = 0xffff, .value = 1 }} },
		{ "-EV_ABS;", true, 1, {{ .type = EV_ABS, .code = 0xffff, .value = 0 }} },
		{ "+ABS_X;", true, 1, {{ .type = EV_ABS, .code = ABS_X, .value = 1 }} },
		{ "-SW_TABLET_MODE;", true, 1, {{ .type = EV_SW, .code = SW_TABLET_MODE, .value = 0 }} },
		{ "+EV_SW", true, 1, {{ .type = EV_SW, .code = 0xffff, .value = 1 }} },
		{ "-ABS_Y", true, 1, {{ .type = EV_ABS, .code = ABS_Y, .value = 0 }} },
		{ "+EV_ABS:0x00", true, 1, {{ .type = EV_ABS, .code = ABS_X, .value = 1 }} },
		{ "-EV_ABS:01", true, 1, {{ .type = EV_ABS, .code = ABS_Y, .value = 0 }} },
		{ "+ABS_TILT_X;-ABS_TILT_Y;", true, 2,
			{{ .type = EV_ABS, .code = ABS_TILT_X, .value = 1 },
			 { .type = EV_ABS, .code = ABS_TILT_Y, .value = 0}} },
		{ "+BTN_TOOL_DOUBLETAP;+EV_KEY;-KEY_A", true, 3,
			{{ .type = EV_KEY, .code = BTN_TOOL_DOUBLETAP, .value = 1 } ,
			 { .type = EV_KEY, .code = 0xffff, .value = 1 },
			 { .type = EV_KEY, .code = KEY_A, .value = 0 }} },
		{ "+REL_Y;-ABS_Z;+BTN_STYLUS", true, 3,
			{{ .type = EV_REL, .code = REL_Y, .value = 1},
			 { .type = EV_ABS, .code = ABS_Z, .value = 0},
			 { .type = EV_KEY, .code = BTN_STYLUS, .value = 1 }} },
		{ "-REL_Y;+EV_KEY:0x123;-BTN_STYLUS", true, 3,
			{{ .type = EV_REL, .code = REL_Y, .value = 0 },
			 { .type = EV_KEY, .code = 0x123, .value = 1 },
			 { .type = EV_KEY, .code = BTN_STYLUS, .value = 0 }} },
		{ .prop = "", .success = false },
		{ .prop = "+", .success = false },
		{ .prop = "-", .success = false },
		{ .prop = "!", .success = false },
		{ .prop = "+EV_FOO", .success = false },
		{ .prop = "+EV_KEY;-EV_FOO", .success = false },
		{ .prop = "+BTN_STYLUS;-EV_FOO", .success = false },
		{ .prop = "-BTN_UNKNOWN", .success = false },
		{ .prop = "+BTN_UNKNOWN;+EV_KEY", .success = false },
		{ .prop = "-PR_UNKNOWN", .success = false },
		{ .prop = "-BTN_STYLUS;+PR_UNKNOWN;-ABS_X", .success = false },
		{ .prop = "-EV_REL:0xffff", .success = false },
		{ .prop = "-EV_REL:0x123.", .success = false },
		{ .prop = "-EV_REL:ffff", .success = false },
		{ .prop = "-EV_REL:blah", .success = false },
		{ .prop = "+KEY_A:0x11", .success = false },
		{ .prop = "+EV_KEY:0x11 ", .success = false },
		{ .prop = "+EV_KEY:0x11not", .success = false },
		{ .prop = "none", .success = false },
		{ .prop = NULL },
	};
	/* clang-format on */
	struct parser_test_tuple *t;

	for (int i = 0; tests[i].prop; i++) {
		bool success;
		struct input_event events[32];
		size_t nevents = ARRAY_LENGTH(events);

		t = &tests[i];
		success = parse_evcode_property(t->prop, events, &nevents);
		litest_assert(success == t->success);
		if (!success)
			continue;

		litest_assert_int_eq(nevents, t->nevents);
		for (size_t j = 0; j < nevents; j++) {
			unsigned int type = events[j].type;
			unsigned int code = events[j].code;
			int value = events[j].value;
			litest_assert_int_eq(t->events[j].type, type);
			litest_assert_int_eq(t->events[j].code, code);
			litest_assert_int_eq(t->events[j].value, value);
		}
	}
}
END_TEST

START_TEST(input_prop_parser)
{
	/* clang-format off */
	struct parser_test_val {
		const char *prop;
		bool success;
		size_t nvals;
		struct input_prop values[20];
	} tests[] = {
		{ "+INPUT_PROP_BUTTONPAD", true, 1, {{ INPUT_PROP_BUTTONPAD, true }}},
		{ "+INPUT_PROP_BUTTONPAD;-INPUT_PROP_POINTER", true, 2,
			{ { INPUT_PROP_BUTTONPAD, true },
			  { INPUT_PROP_POINTER, false }}},
		{ "+INPUT_PROP_BUTTONPAD;-0x00;+0x03", true, 3,
			{ { INPUT_PROP_BUTTONPAD, true },
			  { INPUT_PROP_POINTER, false },
			  { INPUT_PROP_SEMI_MT, true }}},
		{ .prop = "", .success = false },
		{ .prop = "0xff", .success = false },
		{ .prop = "INPUT_PROP", .success = false },
		{ .prop = "INPUT_PROP_FOO", .success = false },
		{ .prop = "INPUT_PROP_FOO;INPUT_PROP_FOO", .success = false },
		{ .prop = "INPUT_PROP_POINTER;INPUT_PROP_FOO", .success = false },
		{ .prop = "none", .success = false },
		{ .prop = NULL },
	};
	/* clang-format on */
	struct parser_test_val *t;

	for (int i = 0; tests[i].prop; i++) {
		bool success;
		struct input_prop props[32];
		size_t nprops = ARRAY_LENGTH(props);

		t = &tests[i];
		success = parse_input_prop_property(t->prop, props, &nprops);
		litest_assert(success == t->success);
		if (!success)
			continue;

		litest_assert_int_eq(nprops, t->nvals);
		for (size_t j = 0; j < t->nvals; j++) {
			litest_assert_int_eq(t->values[j].prop, props[j].prop);
			litest_assert_int_eq(t->values[j].enabled, props[j].enabled);
		}
	}
}
END_TEST

START_TEST(evdev_abs_parser)
{
	/* clang-format off */
	struct test {
		uint32_t which;
		const char *prop;
		int min, max, res, fuzz, flat;

	} tests[] = {
		{ .which = (ABS_MASK_MIN|ABS_MASK_MAX),
		  .prop = "1:2",
		  .min = 1, .max = 2 },
		{ .which = (ABS_MASK_MIN|ABS_MASK_MAX),
		  .prop = "1:2:",
		  .min = 1, .max = 2 },
		{ .which = (ABS_MASK_MIN|ABS_MASK_MAX|ABS_MASK_RES),
		  .prop = "10:20:30",
		  .min = 10, .max = 20, .res = 30 },
		{ .which = (ABS_MASK_RES),
		  .prop = "::100",
		  .res = 100 },
		{ .which = (ABS_MASK_MIN),
		  .prop = "10:",
		  .min = 10 },
		{ .which = (ABS_MASK_MAX|ABS_MASK_RES),
		  .prop = ":10:1001",
		  .max = 10, .res = 1001 },
		{ .which = (ABS_MASK_MIN|ABS_MASK_MAX|ABS_MASK_RES|ABS_MASK_FUZZ),
		  .prop = "1:2:3:4",
		  .min = 1, .max = 2, .res = 3, .fuzz = 4},
		{ .which = (ABS_MASK_MIN|ABS_MASK_MAX|ABS_MASK_RES|ABS_MASK_FUZZ|ABS_MASK_FLAT),
		  .prop = "1:2:3:4:5",
		  .min = 1, .max = 2, .res = 3, .fuzz = 4, .flat = 5},
		{ .which = (ABS_MASK_MIN|ABS_MASK_RES|ABS_MASK_FUZZ|ABS_MASK_FLAT),
		  .prop = "1::3:4:50",
		  .min = 1, .res = 3, .fuzz = 4, .flat = 50},
		{ .which = ABS_MASK_FUZZ|ABS_MASK_FLAT,
		  .prop = ":::5:60",
		  .fuzz = 5, .flat = 60},
		{ .which = ABS_MASK_FUZZ,
		  .prop = ":::5:",
		  .fuzz = 5 },
		{ .which = ABS_MASK_RES, .prop = "::12::",
		  .res = 12 },
		/* Malformed property but parsing this one makes us more
		 * future proof */
		{ .which = (ABS_MASK_RES|ABS_MASK_FUZZ|ABS_MASK_FLAT),
		  .prop = "::12:1:2:3:4:5:6",
		  .res = 12, .fuzz = 1, .flat = 2 },
		{ .which = 0, .prop = ":::::" },
		{ .which = 0, .prop = ":" },
		{ .which = 0, .prop = "" },
		{ .which = 0, .prop = ":asb::::" },
		{ .which = 0, .prop = "foo" },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(tests, t) {
		struct input_absinfo abs;
		uint32_t mask;

		mask = parse_evdev_abs_prop(t->prop, &abs);
		litest_assert_int_eq(mask, t->which);

		if (t->which & ABS_MASK_MIN)
			litest_assert_int_eq(abs.minimum, t->min);
		if (t->which & ABS_MASK_MAX)
			litest_assert_int_eq(abs.maximum, t->max);
		if (t->which & ABS_MASK_RES)
			litest_assert_int_eq(abs.resolution, t->res);
		if (t->which & ABS_MASK_FUZZ)
			litest_assert_int_eq(abs.fuzz, t->fuzz);
		if (t->which & ABS_MASK_FLAT)
			litest_assert_int_eq(abs.flat, t->flat);
	}
}
END_TEST

START_TEST(time_conversion)
{
	litest_assert_int_eq(us(10), 10U);
	litest_assert_int_eq(ns2us(10000), 10U);
	litest_assert_int_eq(ms2us(10), 10000U);
	litest_assert_int_eq(s2us(1), 1000000U);
	litest_assert_int_eq(h2us(2), s2us(2 * 60 * 60));
	litest_assert_int_eq(us2ms(10000), 10U);
}
END_TEST

START_TEST(human_time)
{
	/* clang-format off */
	struct ht_tests {
		uint64_t interval;
		unsigned int value;
		const char *unit;
	} tests[] = {
		{ 0, 0, "us" },
		{ 123, 123, "us" },
		{ ms2us(5), 5, "ms" },
		{ ms2us(100), 100, "ms" },
		{ s2us(5), 5, "s" },
		{ s2us(100), 100, "s" },
		{ s2us(120), 2, "min" },
		{ 5 * s2us(60), 5, "min" },
		{ 120 * s2us(60), 2, "h" },
		{ 5 * 60 * s2us(60), 5, "h" },
		{ 48 * 60 * s2us(60), 2, "d" },
		{ 1000 * 24 * 60 * s2us(60), 1000, "d" },
		{ 0, 0, NULL },
	};
	/* clang-format on */
	for (int i = 0; tests[i].unit != NULL; i++) {
		struct human_time ht;

		ht = to_human_time(tests[i].interval);
		litest_assert_int_eq(ht.value, tests[i].value);
		litest_assert_str_eq(ht.unit, tests[i].unit);
	}
}
END_TEST

struct atoi_test {
	char *str;
	bool success;
	int val;
};

START_TEST(safe_atoi_test)
{
	/* clang-format off */
	struct atoi_test tests[] = {
		{ "10", true, 10 },
		{ "20", true, 20 },
		{ "-1", true, -1 },
		{ "2147483647", true, 2147483647 },
		{ "-2147483648", true, -2147483648 },
		{ "4294967295", false, 0 },
		{ "0x0", false, 0 },
		{ "-10x10", false, 0 },
		{ "1x-99", false, 0 },
		{ "", false, 0 },
		{ "abd", false, 0 },
		{ "xabd", false, 0 },
		{ "0xaf", false, 0 },
		{ "0x0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */
	int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atoi(tests[i].str, &v);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xad);
	}
}
END_TEST

START_TEST(safe_atoi_base_16_test)
{
	/* clang-format off */
	struct atoi_test tests[] = {
		{ "10", true, 0x10 },
		{ "20", true, 0x20 },
		{ "-1", true, -1 },
		{ "0x10", true, 0x10 },
		{ "0xff", true, 0xff },
		{ "abc", true, 0xabc },
		{ "-10", true, -0x10 },
		{ "0x0", true, 0 },
		{ "0", true, 0 },
		{ "0x-99", false, 0 },
		{ "0xak", false, 0 },
		{ "0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */

	int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atoi_base(tests[i].str, &v, 16);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xad);
	}
}
END_TEST

START_TEST(safe_atoi_base_8_test)
{
	/* clang-format off */
	struct atoi_test tests[] = {
		{ "7", true, 07 },
		{ "10", true, 010 },
		{ "20", true, 020 },
		{ "-1", true, -1 },
		{ "010", true, 010 },
		{ "0ff", false, 0 },
		{ "abc", false, 0},
		{ "0xabc", false, 0},
		{ "-10", true, -010 },
		{ "0", true, 0 },
		{ "00", true, 0 },
		{ "0x0", false, 0 },
		{ "0x-99", false, 0 },
		{ "0xak", false, 0 },
		{ "0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */

	int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atoi_base(tests[i].str, &v, 8);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xad);
	}
}
END_TEST

struct atou_test {
	char *str;
	bool success;
	unsigned int val;
};

START_TEST(safe_atou_test)
{
	/* clang-format off */
	struct atou_test tests[] = {
		{ "10", true, 10 },
		{ "20", true, 20 },
		{ "-1", false, 0 },
		{ "2147483647", true, 2147483647 },
		{ "-2147483648", false, 0},
		{ "0x0", false, 0 },
		{ "-10x10", false, 0 },
		{ "1x-99", false, 0 },
		{ "", false, 0 },
		{ "abd", false, 0 },
		{ "xabd", false, 0 },
		{ "0xaf", false, 0 },
		{ "0x0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */
	unsigned int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atou(tests[i].str, &v);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xadU);
	}
}
END_TEST

START_TEST(safe_atou_base_16_test)
{
	/* clang-format off */
	struct atou_test tests[] = {
		{ "10", true, 0x10 },
		{ "20", true, 0x20 },
		{ "-1", false, 0 },
		{ "0x10", true, 0x10 },
		{ "0xff", true, 0xff },
		{ "abc", true, 0xabc },
		{ "-10", false, 0 },
		{ "0x0", true, 0 },
		{ "0", true, 0 },
		{ "0x-99", false, 0 },
		{ "0xak", false, 0 },
		{ "0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */

	unsigned int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atou_base(tests[i].str, &v, 16);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xadU);
	}
}
END_TEST

START_TEST(safe_atou_base_8_test)
{
	/* clang-format off */
	struct atou_test tests[] = {
		{ "7", true, 07 },
		{ "10", true, 010 },
		{ "20", true, 020 },
		{ "-1", false, 0 },
		{ "010", true, 010 },
		{ "0ff", false, 0 },
		{ "abc", false, 0},
		{ "0xabc", false, 0},
		{ "-10", false, 0 },
		{ "0", true, 0 },
		{ "00", true, 0 },
		{ "0x0", false, 0 },
		{ "0x-99", false, 0 },
		{ "0xak", false, 0 },
		{ "0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */

	unsigned int v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atou_base(tests[i].str, &v, 8);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xadU);
	}
}
END_TEST

struct atou64_test {
	char *str;
	bool success;
	unsigned long val;
};

START_TEST(safe_atou64_test)
{
	/* clang-format off */
	struct atou64_test tests[] = {
		{ "10", true, 10 },
		{ "20", true, 20 },
		{ "-1", false, 0 },
		{ "9999999999", true, 9999999999 },
		{ "2147483647", true, 2147483647 },
		{ "-2147483648", false, 0},
		{ "0x0", false, 0 },
		{ "-10x10", false, 0 },
		{ "1x-99", false, 0 },
		{ "", false, 0 },
		{ "abd", false, 0 },
		{ "xabd", false, 0 },
		{ "0xaf", false, 0 },
		{ "0x0x", false, 0 },
		{ "x10", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */
	uint64_t v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atou64(tests[i].str, &v);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_int_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xadU);
	}
}
END_TEST

START_TEST(safe_atod_test)
{
	/* clang-format off */
	struct atod_test {
		char *str;
		bool success;
		double val;
	} tests[] = {
		{ "10", true, 10 },
		{ "20", true, 20 },
		{ "-1", true, -1 },
		{ "2147483647", true, 2147483647 },
		{ "-2147483648", true, -2147483648 },
		{ "4294967295", true, 4294967295 },
		{ "0x0", false, 0 },
		{ "0x10", false, 0 },
		{ "0xaf", false, 0 },
		{ "x80", false, 0 },
		{ "0.0", true, 0.0 },
		{ "0.1", true, 0.1 },
		{ "1.2", true, 1.2 },
		{ "-324.9", true, -324.9 },
		{ "9324.9", true, 9324.9 },
		{ "NAN", false, 0 },
		{ "INFINITY", false, 0 },
		{ "-10x10", false, 0 },
		{ "1x-99", false, 0 },
		{ "", false, 0 },
		{ "abd", false, 0 },
		{ "xabd", false, 0 },
		{ "0x0x", false, 0 },
		{ NULL, false, 0 },
	};
	/* clang-format on */
	double v;
	bool success;

	for (int i = 0; tests[i].str != NULL; i++) {
		v = 0xad;
		success = safe_atod(tests[i].str, &v);
		litest_assert(success == tests[i].success);
		if (success)
			litest_assert_double_eq(v, tests[i].val);
		else
			litest_assert_int_eq(v, 0xad);
	}
}
END_TEST

START_TEST(strsplit_test)
{
	/* clang-format off */
	struct strsplit_test {
		const char *string;
		const char *delim;
		const char *results[10];
		const size_t nresults;
	} tests[] = {
		{ "one two three", " ", { "one", "two", "three", NULL }, 3 },
		{ "one two\tthree", " \t", { "one", "two", "three", NULL }, 3 },
		{ "one", " ", { "one", NULL }, 1 },
		{ "one two ", " ", { "one", "two", NULL }, 2 },
		{ "one  two", " ", { "one", "two", NULL }, 2 },
		{ " one two", " ", { "one", "two", NULL }, 2 },
		{ "one", "\t \r", { "one", NULL }, 1 },
		{ "one two three", " t", { "one", "wo", "hree", NULL }, 3 },
		{ " one two three", "te", { " on", " ", "wo ", "hr", NULL }, 4 },
		{ "one", "ne", { "o", NULL }, 1 },
		{ "onene", "ne", { "o", NULL }, 1 },
		{ "+1-2++3--4++-+5-+-", "+-", { "1", "2", "3", "4", "5", NULL }, 5 },
		/* special cases */
		{ "", " ", { NULL }, 0 },
		{ " ", " ", { NULL }, 0 },
		{ "     ", " ", { NULL }, 0 },
		{ "oneoneone", "one", { NULL} , 0 },
		{ NULL, NULL, { NULL }, 0},
	};
	/* clang-format on */
	struct strsplit_test *t = tests;

	while (t->string) {
		size_t nelem;
		char **strv = strv_from_string(t->string, t->delim, &nelem);

		for (size_t idx = 0; idx < t->nresults; idx++)
			litest_assert_str_eq(t->results[idx], strv[idx]);

		litest_assert_int_eq(nelem, t->nresults);

		/* When there are no elements validate return value is Null,
		   otherwise validate result array is Null terminated. */
		if (t->nresults == 0)
			litest_assert_ptr_eq(strv, NULL);
		else
			litest_assert_ptr_eq(strv[t->nresults], NULL);

		strv_free(strv);
		t++;
	}
}
END_TEST

struct strv_test_data {
	const char *terminate_at;
	unsigned char bitmask[1];
};

static int
strv_test_set_bitmask(const char *str, size_t index, void *data)
{
	struct strv_test_data *td = data;

	if (streq(str, td->terminate_at))
		return index + 1;

	set_bit(td->bitmask, index);

	return 0;
}

START_TEST(strv_for_each_test)
{
	/* clang-format off */
	struct test_data {
		const char *terminator;
		int index;
		unsigned int bitmask;
	} test_data[] = {
		{ "one", 1, 0x0 },
		{ "two", 2, 0x1 },
		{ "three", 3, 0x3 },
		{ "four", 4, 0x7 },
		{ "five", 5, 0xf },
		{ "does-not-exist", 0, 0x1f },
		{ NULL, 0, 0x1f },
		{ NULL, 0 },
	};
	/* clang-format on */
	const char *array[] = { "one", "two", "three", "four", "five", NULL };
	struct test_data *t = test_data;

	while (t->terminator || t->bitmask) {
		const int max = 3;
		struct strv_test_data td = {
			.terminate_at = t->terminator,
			.bitmask = { 0 },
		};

		int rc = strv_for_each(array, strv_test_set_bitmask, &td);
		litest_assert_int_eq(rc, t->index);
		litest_assert_int_eq(td.bitmask[0], t->bitmask);

		struct strv_test_data tdmax = {
			.terminate_at = t->terminator,
			.bitmask = { 0 },
		};

		rc = strv_for_each_n(array, max, strv_test_set_bitmask, &tdmax);
		if (max < t->index)
			litest_assert_int_eq(rc, 0);
		else
			litest_assert_int_eq(rc, t->index);
		litest_assert_int_eq(tdmax.bitmask[0], t->bitmask & ((1 << max) - 1));

		t++;
	}
}
END_TEST

__attribute__((format(printf, 1, 0))) static char **
test_strv_appendv(char *format, ...)
{
	va_list args;
	va_start(args, format);
	char **strv = NULL;
	strv = strv_append_vprintf(strv, "%s %d", args);
	va_end(args);
	return strv;
}

START_TEST(strv_append_test)
{
	{
		char *test_strv1[] = { "a", "b", "c", NULL };
		char **test_strv2 = NULL;

		litest_assert_int_eq(strv_len(test_strv1), 4U);
		litest_assert_int_eq(strv_len(test_strv2), 0U);
	}
	{
		char **strv = NULL;
		char *dup = safe_strdup("test");
		strv = strv_append_take(strv, &dup);
		litest_assert_ptr_null(dup);
		litest_assert_ptr_notnull(strv);
		litest_assert_str_eq(strv[0], "test");
		litest_assert_ptr_eq(strv[1], NULL);
		litest_assert_int_eq(strv_len(strv), 2U);

		char *dup2 = safe_strdup("test2");
		strv = strv_append_take(strv, &dup2);
		litest_assert_ptr_null(dup2);
		litest_assert_str_eq(strv[1], "test2");
		litest_assert_ptr_eq(strv[2], NULL);
		litest_assert_int_eq(strv_len(strv), 3U);

		strv = strv_append_take(strv, NULL);
		litest_assert_int_eq(strv_len(strv), 3U);
		strv_free(strv);
	}
	{
		char **strv = NULL;
		strv = strv_append_strdup(strv, "banana");
		litest_assert(strv != NULL);
		litest_assert_str_eq(strv[0], "banana");
		litest_assert_ptr_null(strv[1]);
		litest_assert_int_eq(strv_len(strv), 2U);
		strv_free(strv);
	}
	{
		char **strv = test_strv_appendv("%s %d", "apple", 2);
		litest_assert_ptr_notnull(strv);
		litest_assert_str_eq(strv[0], "apple 2");
		litest_assert_ptr_null(strv[1]);
		litest_assert_int_eq(strv_len(strv), 2U);
		strv_free(strv);
	}
	{
		char **strv = NULL;
		strv = strv_append_printf(strv, "coco%s", "nut");
		litest_assert_ptr_notnull(strv);
		litest_assert_str_eq(strv[0], "coconut");
		litest_assert_ptr_null(strv[1]);
		litest_assert_int_eq(strv_len(strv), 2U);
		strv_free(strv);
	}
}
END_TEST

START_TEST(strv_find_test)
{
	char *strv[] = { "a", "b", "c", NULL };

	bool rc;
	size_t index;

	rc = strv_find(strv, "a", &index);
	litest_assert(rc);
	litest_assert_int_eq(index, 0U);

	rc = strv_find(strv, "b", &index);
	litest_assert(rc);
	litest_assert_int_eq(index, 1U);

	rc = strv_find(strv, "a", NULL);
	litest_assert(rc);

	index = 0xffff;
	rc = strv_find(strv, "d", &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find(strv, "d", NULL);
	litest_assert(!rc);

	rc = strv_find(NULL, "a", &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find(NULL, NULL, &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find(strv, NULL, &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);
}
END_TEST

START_TEST(strv_find_substring_test)
{
	char *strv[] = { "a", "bc", "cccc", NULL };

	bool rc;
	size_t index;

	rc = strv_find_substring(strv, "a", &index);
	litest_assert(rc);
	litest_assert_int_eq(index, 0U);

	rc = strv_find_substring(strv, "b", &index);
	litest_assert(rc);
	litest_assert_int_eq(index, 1U);

	rc = strv_find_substring(strv, "c", &index);
	litest_assert(rc);
	litest_assert_int_eq(index, 1U);

	rc = strv_find_substring(strv, "a", NULL);
	litest_assert(rc);

	index = 0xffff;
	rc = strv_find_substring(strv, "d", &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find_substring(strv, "d", NULL);
	litest_assert(!rc);

	rc = strv_find_substring(NULL, "a", &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find_substring(NULL, NULL, &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);

	rc = strv_find_substring(strv, NULL, &index);
	litest_assert(!rc);
	litest_assert_int_eq(index, 0xffffU);
}
END_TEST

START_TEST(double_array_from_string_test)
{
	/* clang-format off */
	struct double_array_from_string_test {
		const char *string;
		const char *delim;
		const double array[10];
		const size_t len;
		const bool result;
	} tests[] = {
		{ "1 2 3", " ", { 1, 2, 3 }, 3 },
		{ "1", " ", { 1 }, 1 },
		{ "1,2.5,", ",", { 1, 2.5 }, 2 },
		{ "1.0  2", " ", { 1, 2.0 }, 2 },
		{ " 1 2", " ", { 1, 2 }, 2 },
		{ " ; 1;2  3.5  ;;4.1", "; ", { 1, 2, 3.5, 4.1 }, 4 },
		/* special cases */
		{ "1 two", " ", { 0 }, 0 },
		{ "one two", " ", { 0 }, 0 },
		{ "one 2", " ", { 0 }, 0 },
		{ "", " ", { 0 }, 0 },
		{ " ", " ", { 0 }, 0 },
		{ "    ", " ", { 0 }, 0 },
		{ "", " ", { 0 }, 0 },
		{ "oneoneone", "one", { 0 }, 0 },
		{ NULL, NULL, { 0 }, 0 },
	};
	/* clang-format on */
	struct double_array_from_string_test *t = tests;

	while (t->string) {
		size_t len;
		double *array = double_array_from_string(t->string, t->delim, &len);
		litest_assert_int_eq(len, t->len);

		for (size_t idx = 0; idx < len; idx++) {
			litest_assert_ptr_notnull(array);
			litest_assert_double_eq(array[idx], t->array[idx]);
		}

		free(array);
		t++;
	}
}
END_TEST

START_TEST(strargv_test)
{
	/* clang-format off */
	struct argv_test {
		int argc;
		char *argv[10];
		int expected;
	} tests[] = {
		{ 0, {NULL}, 0 },
		{ 1, {"hello", "World"}, 1 },
		{ 2, {"hello", "World"}, 2 },
		{ 2, {"", " "}, 2 },
		{ 2, {"", NULL}, 0 },
		{ 2, {NULL, NULL}, 0 },
		{ 1, {NULL, NULL}, 0 },
		{ 3, {"hello", NULL, "World"}, 0 },
	};
	/* clang-format on */

	ARRAY_FOR_EACH(tests, t) {
		char **strv = strv_from_argv(t->argc, t->argv);

		if (t->expected == 0) {
			litest_assert(strv == NULL);
		} else {
			int count = 0;
			char **s = strv;
			while (*s) {
				litest_assert_str_eq(*s, t->argv[count]);
				count++;
				s++;
			}
			litest_assert_int_eq(t->expected, count);
			strv_free(strv);
		}
	}
}
END_TEST

START_TEST(kvsplit_double_test)
{
	/* clang-format off */
	struct kvsplit_dbl_test {
		const char *string;
		const char *psep;
		const char *kvsep;
		ssize_t nresults;
		struct {
			double a;
			double b;
		} results[32];
	} tests[] = {
		{ "1:2;3:4;5:6", ";", ":", 3, { {1, 2}, {3, 4}, {5, 6}}},
		{ "1.0x2.3 -3.2x4.5 8.090909x-6.00", " ", "x", 3, { {1.0, 2.3}, {-3.2, 4.5}, {8.090909, -6}}},

		{ "1:2", "x", ":", 1, {{1, 2}}},
		{ "1:2", ":", "x", -1, {}},
		{ "1:2", NULL, "x", -1, {}},
		{ "1:2", "", "x", -1, {}},
		{ "1:2", "x", NULL, -1, {}},
		{ "1:2", "x", "", -1, {}},
		{ "a:b", "x", ":", -1, {}},
		{ "", " ", "x", -1, {}},
		{ "1.2.3.4.5", ".", "", -1, {}},
		{ NULL },
	};
	/* clang-format on */
	struct kvsplit_dbl_test *t = tests;

	while (t->string) {
		struct key_value_double *result = NULL;
		ssize_t npairs;

		npairs = kv_double_from_string(t->string, t->psep, t->kvsep, &result);
		litest_assert_int_eq(npairs, t->nresults);

		for (ssize_t i = 0; i < npairs; i++) {
			litest_assert_double_eq(t->results[i].a, result[i].key);
			litest_assert_double_eq(t->results[i].b, result[i].value);
		}

		free(result);
		t++;
	}
}
END_TEST

START_TEST(strjoin_test)
{
	/* clang-format off */
	struct strjoin_test {
		char *strv[10];
		const char *joiner;
		const char *result;
	} tests[] = {
		{ { "one", "two", "three", NULL }, " ", "one two three" },
		{ { "one", NULL }, "x", "one" },
		{ { "one", "two", NULL }, "x", "onextwo" },
		{ { "one", "two", NULL }, ",", "one,two" },
		{ { "one", "two", NULL }, ", ", "one, two" },
		{ { "one", "two", NULL }, "one", "oneonetwo" },
		{ { "one", "two", NULL }, NULL, NULL },
		{ { "", "", "", NULL }, " ", "  " },
		{ { "a", "b", "c", NULL }, "", "abc" },
		{ { "", "b", "c", NULL }, "x", "xbxc" },
		{ { "", "", "", NULL }, "", "" },
		{ { NULL }, NULL, NULL },
	};
	/* clang-format on */
	struct strjoin_test *t = tests;
	struct strjoin_test nulltest = { { NULL }, "x", NULL };

	while (t->strv[0]) {
		char *str;
		str = strv_join(t->strv, t->joiner);
		if (t->result == NULL)
			litest_assert(str == NULL);
		else
			litest_assert_str_eq(str, t->result);
		free(str);
		t++;
	}

	litest_assert(strv_join(nulltest.strv, "x") == NULL);
}
END_TEST

START_TEST(strstrip_test)
{
	/* clang-format off */
	struct strstrip_test {
		const char *string;
		const char *expected;
		const char *what;
	} tests[] = {
		{ "foo",		"foo",		"1234" },
		{ "\"bar\"",		"bar",		"\"" },
		{ "'bar'",		"bar",		"'" },
		{ "\"bar\"",		"\"bar\"",	"'" },
		{ "'bar'",		"'bar'",	"\"" },
		{ "\"bar\"",		"bar",		"\"" },
		{ "\"\"",		"",		"\"" },
		{ "\"foo\"bar\"",	"foo\"bar",	"\"" },
		{ "\"'foo\"bar\"",	"foo\"bar",	"\"'" },
		{ "abcfooabcbarbca",	"fooabcbar",	"abc" },
		{ "xxxxfoo",		"foo",		"x" },
		{ "fooyyyy",		"foo",		"y" },
		{ "xxxxfooyyyy",	"foo",		"xy" },
		{ "x xfooy y",		" xfooy ",	"xy" },
		{ " foo\n",		"foo",		" \n" },
		{ "",			"",		"abc" },
		{ "",			"",		"" },
		{ NULL , NULL, NULL },
	};
	/* clang-format on */
	struct strstrip_test *t = tests;

	while (t->string) {
		char *str;
		str = strstrip(t->string, t->what);
		litest_assert_str_eq(str, t->expected);
		free(str);
		t++;
	}
}
END_TEST

START_TEST(strendswith_test)
{
	/* clang-format off */
	struct strendswith_test {
		const char *string;
		const char *suffix;
		bool expected;
	} tests[] = {
		{ "foobar", "bar", true },
		{ "foobar", "foo", false },
		{ "foobar", "foobar", true },
		{ "foo", "foobar", false },
		{ "foobar", "", false },
		{ "", "", false },
		{ "", "foo", false },
		{ NULL, NULL, false },
	};
	/* clang-format on */

	for (struct strendswith_test *t = tests; t->string; t++) {
		litest_assert_int_eq(strendswith(t->string, t->suffix), t->expected);
	}
}
END_TEST

START_TEST(strstartswith_test)
{
	/* clang-format off */
	struct strstartswith_test {
		const char *string;
		const char *suffix;
		bool expected;
	} tests[] = {
		{ "foobar", "foo", true },
		{ "foobar", "bar", false },
		{ "foobar", "foobar", true },
		{ "foo", "foobar", false },
		{ "foo", "", false },
		{ "", "", false },
		{ "foo", "", false },
		{ NULL, NULL, false },
	};
	/* clang-format on */

	for (struct strstartswith_test *t = tests; t->string; t++) {
		litest_assert_int_eq(strstartswith(t->string, t->suffix), t->expected);
	}
}
END_TEST

START_TEST(strsanitize_test)
{
	/* clang-format off */
	struct strsanitize_test {
		const char *string;
		const char *expected;
	} tests[] = {
		{ "foobar", "foobar" },
		{ "", "" },
		{ "%", "%%" },
		{ "%%%%", "%%%%%%%%" },
		{ "x %s", "x %%s" },
		{ "x %", "x %%" },
		{ "%sx", "%%sx" },
		{ "%s%s", "%%s%%s" },
		{ NULL, NULL },
	};
	/* clang-format on */

	for (struct strsanitize_test *t = tests; t->string; t++) {
		char *sanitized = str_sanitize(t->string);
		litest_assert_str_eq(sanitized, t->expected);
		free(sanitized);
	}
}
END_TEST

START_TEST(list_test_insert)
{
	struct list_test {
		int val;
		struct list node;
	} tests[] = {
		{ .val = 1 },
		{ .val = 2 },
		{ .val = 3 },
		{ .val = 4 },
	};
	struct list_test *t;
	struct list head;
	int val;

	list_init(&head);

	ARRAY_FOR_EACH(tests, t) {
		list_insert(&head, &t->node);
	}

	val = 4;
	list_for_each(t, &head, node) {
		litest_assert_int_eq(t->val, val);
		val--;
	}

	litest_assert_int_eq(val, 0);
}
END_TEST

START_TEST(list_test_append)
{
	struct list_test {
		int val;
		struct list node;
	} tests[] = {
		{ .val = 1 },
		{ .val = 2 },
		{ .val = 3 },
		{ .val = 4 },
	};
	struct list_test *t;
	struct list head;
	int val;

	list_init(&head);

	ARRAY_FOR_EACH(tests, t) {
		list_append(&head, &t->node);
	}

	val = 1;
	list_for_each(t, &head, node) {
		litest_assert_int_eq(t->val, val);
		val++;
	}
	litest_assert_int_eq(val, 5);
}
END_TEST

START_TEST(list_test_chain)
{
	struct list_test {
		int val;
		struct list node;
	} tests[] = {
		{ .val = 1 },
		{ .val = 2 },
		{ .val = 3 },
		{ .val = 4 },
	};
	struct list l1, l2;
	struct list_test *t;
	int val;

	list_init(&l1);
	list_init(&l2);

	list_chain(&l1, &l2);
	litest_assert(list_empty(&l2));

	list_append(&l2, &tests[0].node);
	list_append(&l2, &tests[1].node);
	list_chain(&l1, &l2);
	litest_assert(list_empty(&l2));

	val = 1;
	list_for_each_safe(t, &l1, node) {
		litest_assert_int_eq(t->val, val);
		val++;
		list_remove(&t->node);
	}
	litest_assert_int_eq(val, 3);

	list_append(&l1, &tests[0].node);
	list_append(&l1, &tests[1].node);
	list_append(&l2, &tests[2].node);
	list_append(&l2, &tests[3].node);

	list_chain(&l1, &l2);
	litest_assert(list_empty(&l2));

	val = 1;
	list_for_each(t, &l1, node) {
		litest_assert_int_eq(t->val, val);
		val++;
	}
	litest_assert_int_eq(val, 5);
}
END_TEST

START_TEST(list_test_foreach)
{
	struct list_test {
		int val;
		struct list node;
	} tests[] = {
		{ .val = 1 },
		{ .val = 2 },
		{ .val = 3 },
		{ .val = 4 },
	};
	struct list_test *t;
	struct list head;

	list_init(&head);

	ARRAY_FOR_EACH(tests, t) {
		list_append(&head, &t->node);
	}

	/* Make sure both loop macros are a single line statement */
	if (false)
		list_for_each(t, &head, node) {
			litest_abort_msg("We should not get here");
		}

	if (false)
		list_for_each_safe(t, &head, node) {
			litest_abort_msg("We should not get here");
		}
}
END_TEST

START_TEST(list_test_first_last)
{
	struct list_test {
		int val;
		struct list node;
	} tests[] = {
		{ .val = 1 },
		{ .val = 2 },
		{ .val = 3 },
		{ .val = 4 },
	};
	struct list head;

	list_init(&head);

	ARRAY_FOR_EACH(tests, t) {
		list_append(&head, &t->node);
	}

	struct list_test *first;
	struct list_test *last;

	first = list_first_entry(&head, first, node);
	last = list_last_entry(&head, last, node);
	litest_assert_ptr_eq(first, &tests[0]);
	litest_assert_ptr_eq(last, &tests[3]);

	struct list_test *second;
	struct list_test *penultimate;

	second = list_first_entry(&first->node, first, node);
	penultimate = list_last_entry(&last->node, last, node);
	litest_assert_ptr_eq(second, &tests[1]);
	litest_assert_ptr_eq(penultimate, &tests[2]);

	/* Now remove nodes */

	/* No change expected */
	list_remove(&tests[2].node);
	first = list_first_entry(&head, first, node);
	last = list_last_entry(&head, last, node);
	litest_assert_ptr_eq(first, &tests[0]);
	litest_assert_ptr_eq(last, &tests[3]);

	list_remove(&tests[3].node);
	first = list_first_entry(&head, first, node);
	last = list_last_entry(&head, last, node);
	litest_assert_ptr_eq(first, &tests[0]);
	litest_assert_ptr_eq(last, &tests[1]);

	list_remove(&tests[0].node);
	first = list_first_entry(&head, first, node);
	last = list_last_entry(&head, last, node);
	litest_assert_ptr_eq(first, &tests[1]);
	litest_assert_ptr_eq(last, &tests[1]);
}
END_TEST

START_TEST(strverscmp_test)
{
	litest_assert_int_eq(libinput_strverscmp("", ""), 0);
	litest_assert_int_gt(libinput_strverscmp("0.0.1", ""), 0);
	litest_assert_int_lt(libinput_strverscmp("", "0.0.1"), 0);
	litest_assert_int_eq(libinput_strverscmp("0.0.1", "0.0.1"), 0);
	litest_assert_int_eq(libinput_strverscmp("0.0.1", "0.0.2"), -1);
	litest_assert_int_eq(libinput_strverscmp("0.0.2", "0.0.1"), 1);
	litest_assert_int_eq(libinput_strverscmp("0.0.1", "0.1.0"), -1);
	litest_assert_int_eq(libinput_strverscmp("0.1.0", "0.0.1"), 1);
}
END_TEST

START_TEST(streq_test)
{
	litest_assert(streq("", "") == true);
	litest_assert(streq(NULL, NULL) == true);
	litest_assert(streq("0.0.1", "") == false);
	litest_assert(streq("foo", NULL) == false);
	litest_assert(streq(NULL, "foo") == false);
	litest_assert(streq("0.0.1", "0.0.1") == true);
}
END_TEST

START_TEST(strneq_test)
{
	litest_assert(strneq("", "", 1) == true);
	litest_assert(strneq(NULL, NULL, 1) == true);
	litest_assert(strneq("0.0.1", "", 6) == false);
	litest_assert(strneq("foo", NULL, 5) == false);
	litest_assert(strneq(NULL, "foo", 5) == false);
	litest_assert(strneq("0.0.1", "0.0.1", 6) == true);
}
END_TEST

START_TEST(basename_test)
{
	struct test {
		const char *path;
		const char *expected;
	} tests[] = {
		{ "a", "a" },
		{ "foo.c", "foo.c" },
		{ "foo", "foo" },
		{ "/path/to/foo.h", "foo.h" },
		{ "../bar.foo", "bar.foo" },
		{ "./bar.foo.baz", "bar.foo.baz" },
		{ "./", NULL },
		{ "/", NULL },
		{ "/bar/", NULL },
		{ "/bar", "bar" },
		{ "", NULL },
	};

	ARRAY_FOR_EACH(tests, t) {
		const char *result = safe_basename(t->path);
		if (t->expected == NULL)
			litest_assert(result == NULL);
		else
			litest_assert_str_eq(result, t->expected);
	}
}
END_TEST

START_TEST(trunkname_test)
{
	struct test {
		const char *path;
		const char *expected;
	} tests[] = {
		{ "foo.c", "foo" },
		{ "/path/to/foo.h", "foo" },
		{ "/path/to/foo", "foo" },
		{ "../bar.foo", "bar" },
		{ "./bar.foo.baz", "bar.foo" },
		{ "./", "" },
		{ "/", "" },
		{ "/bar/", "" },
		{ "/bar", "bar" },
		{ "", "" },
	};

	ARRAY_FOR_EACH(tests, t) {
		char *result = trunkname(t->path);
		litest_assert_str_eq(result, t->expected);
		free(result);
	}
}
END_TEST

START_TEST(absinfo_normalize_value_test)
{
	struct input_absinfo abs = {
		.minimum = 0,
		.maximum = 100,
	};

	litest_assert_double_eq(absinfo_normalize_value(&abs, -100), 0.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, -1), 0.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 0), 0.0);
	litest_assert_double_gt(absinfo_normalize_value(&abs, 1), 0.0);
	litest_assert_double_lt(absinfo_normalize_value(&abs, 99), 1.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 100), 1.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 101), 1.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 200), 1.0);

	abs.minimum = -50;
	abs.maximum = 50;

	litest_assert_double_eq(absinfo_normalize_value(&abs, -51), 0.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, -50), 0.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 0), 0.5);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 50), 1.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 51), 1.0);

	abs.minimum = -50;
	abs.maximum = 0;

	litest_assert_double_eq(absinfo_normalize_value(&abs, -51), 0.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, -50), 0.0);
	litest_assert_double_gt(absinfo_normalize_value(&abs, -49), 0.0);
	litest_assert_double_lt(absinfo_normalize_value(&abs, -1), 1.0);
	litest_assert_double_eq(absinfo_normalize_value(&abs, 0), 1.0);
}
END_TEST

START_TEST(range_test)
{
	struct range incl = range_init_inclusive(1, 100);
	litest_assert_int_eq(incl.lower, 1);
	litest_assert_int_eq(incl.upper, 101);

	struct range excl = range_init_exclusive(1, 100);
	litest_assert_int_eq(excl.lower, 1);
	litest_assert_int_eq(excl.upper, 100);

	struct range zero = range_init_exclusive(0, 0);
	litest_assert_int_eq(zero.lower, 0);
	litest_assert_int_eq(zero.upper, 0);

	struct range empty = range_init_empty();
	litest_assert_int_eq(empty.lower, 0);
	litest_assert_int_eq(empty.upper, -1);

	litest_assert(range_is_valid(&incl));
	litest_assert(range_is_valid(&excl));
	litest_assert(!range_is_valid(&zero));
	litest_assert(!range_is_valid(&empty));

	int expected = 1;
	int r = 0;
	range_for_each(&incl, r) {
		litest_assert_int_eq(r, expected);
		expected++;
	}
	litest_assert_int_eq(r, 101);
}
END_TEST

START_TEST(stringbuf_test)
{
	struct stringbuf buf;
	struct stringbuf *b = &buf;
	int rc;

	stringbuf_init(b);
	litest_assert_int_eq(b->len, 0u);

	rc = stringbuf_append_string(b, "foo");
	litest_assert_neg_errno_success(rc);
	rc = stringbuf_append_string(b, "bar");
	litest_assert_neg_errno_success(rc);
	rc = stringbuf_append_string(b, "baz");
	litest_assert_neg_errno_success(rc);
	litest_assert_str_eq(b->data, "foobarbaz");
	litest_assert_int_eq(b->len, strlen("foobarbaz"));

	rc = stringbuf_ensure_space(b, 500);
	litest_assert_neg_errno_success(rc);
	litest_assert_int_ge(b->sz, 500u);

	rc = stringbuf_ensure_size(b, 0);
	litest_assert_neg_errno_success(rc);
	rc = stringbuf_ensure_size(b, 1024);
	litest_assert_neg_errno_success(rc);
	litest_assert_int_ge(b->sz, 1024u);

	char *data = stringbuf_steal(b);
	litest_assert_str_eq(data, "foobarbaz");
	litest_assert_int_eq(b->sz, 0u);
	litest_assert_int_eq(b->len, 0u);
	litest_assert_ptr_null(b->data);
	free(data);

	const char *str = "1234567890";
	rc = stringbuf_append_string(b, str);
	litest_assert_neg_errno_success(rc);
	litest_assert_str_eq(b->data, str);
	litest_assert_int_eq(b->len, 10u);
	stringbuf_reset(b);

	/* intentional double-reset */
	stringbuf_reset(b);

	int pipefd[2];
	rc = pipe2(pipefd, O_CLOEXEC | O_NONBLOCK);
	litest_assert_neg_errno_success(rc);

	str = "foo bar baz";
	char *compare = NULL;
	for (int i = 0; i < 100; i++) {
		rc = write(pipefd[1], str, strlen(str));
		litest_assert_neg_errno_success(rc);

		rc = stringbuf_append_from_fd(b, pipefd[0], 64);
		litest_assert_neg_errno_success(rc);

		char *expected = strdup_printf("%s%s", compare ? compare : "", str);
		litest_assert_ptr_notnull(expected);
		litest_assert_str_eq(b->data, expected);

		free(compare);
		compare = expected;
	}
	free(compare);
	close(pipefd[0]);
	close(pipefd[1]);
	stringbuf_reset(b);

	rc = pipe2(pipefd, O_CLOEXEC | O_NONBLOCK);
	litest_assert_neg_errno_success(rc);

	const size_t stride = 256;
	const size_t maxsize = 4096;

	for (size_t i = 0; i < maxsize; i += stride) {
		char buf[stride];
		memset(buf, i / stride, sizeof(buf));
		rc = write(pipefd[1], buf, sizeof(buf));
		litest_assert_neg_errno_success(rc);
	}

	stringbuf_append_from_fd(b, pipefd[0], 0);
	litest_assert_int_eq(b->len, maxsize);
	litest_assert_int_ge(b->sz, maxsize);

	for (size_t i = 0; i < maxsize; i += stride) {
		char buf[stride];
		memset(buf, i / stride, sizeof(buf));
		litest_assert_int_eq(memcmp(buf, b->data + i, sizeof(buf)), 0);
	}

	close(pipefd[0]);
	close(pipefd[1]);
	stringbuf_reset(b);
}
END_TEST

START_TEST(multivalue_test)
{
	{
		struct multivalue v = multivalue_new_string("test");
		litest_assert_int_eq(v.type, 's');
		litest_assert_str_eq(v.value.s, "test");

		const char *s;
		multivalue_extract_typed(&v, 's', &s);
		litest_assert_str_eq(s, "test");
		litest_assert_ptr_eq(s, (const char *)v.value.s);
		multivalue_extract(&v, &s);
		litest_assert_str_eq(s, "test");
		litest_assert_ptr_eq(s, (const char *)v.value.s);

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_str_eq(copy.value.s, v.value.s);
		char *p1 = copy.value.s;
		char *p2 = v.value.s;
		litest_assert_ptr_ne(p1, p2);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "test");
		free(str);
	}

	{
		struct multivalue v = multivalue_new_char('x');
		litest_assert_int_eq(v.type, 'c');
		litest_assert_int_eq(v.value.c, 'x');

		char c;
		multivalue_extract_typed(&v, 'c', &c);
		litest_assert_int_eq(c, 'x');
		multivalue_extract(&v, &c);
		litest_assert_int_eq(c, 'x');

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_int_eq(copy.value.c, v.value.c);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "x");
		free(str);
	}

	{
		struct multivalue v = multivalue_new_u32(0x1234);
		litest_assert_int_eq(v.type, 'u');
		litest_assert_int_eq(v.value.u, 0x1234u);

		uint32_t c;
		multivalue_extract_typed(&v, 'u', &c);
		litest_assert_int_eq(c, 0x1234u);
		multivalue_extract(&v, &c);
		litest_assert_int_eq(c, 0x1234u);

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_int_eq(copy.value.u, v.value.u);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "4660");
		free(str);
	}

	{
		struct multivalue v = multivalue_new_i32(-123);
		litest_assert_int_eq(v.type, 'i');
		litest_assert_int_eq(v.value.i, -123);

		int32_t c;
		multivalue_extract_typed(&v, 'i', &c);
		litest_assert_int_eq(c, -123);
		multivalue_extract(&v, &c);
		litest_assert_int_eq(c, -123);

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_int_eq(copy.value.i, v.value.i);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "-123");
		free(str);
	}

	{
		struct multivalue v = multivalue_new_bool(true);
		litest_assert_int_eq(v.type, 'b');
		litest_assert_int_eq(v.value.b, true);

		bool c;
		multivalue_extract_typed(&v, 'b', &c);
		litest_assert_int_eq(c, true);
		multivalue_extract(&v, &c);
		litest_assert_int_eq(c, true);

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_int_eq(copy.value.b, v.value.b);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "true");
		free(str);
	}

	{
		struct multivalue v = multivalue_new_double(0.1234);
		litest_assert_int_eq(v.type, 'd');
		litest_assert_double_eq(v.value.d, 0.1234);

		double c;
		multivalue_extract_typed(&v, 'd', &c);
		litest_assert_double_eq(c, 0.1234);
		multivalue_extract(&v, &c);
		litest_assert_double_eq(c, 0.1234);

		struct multivalue copy = multivalue_copy(&v);
		litest_assert_int_eq(copy.type, v.type);
		litest_assert_double_eq(copy.value.d, v.value.d);

		char *str = multivalue_as_str(&v);
		litest_assert_str_eq(str, "0.123400");
		free(str);
	}
}
END_TEST

DECLARE_NEWTYPE(newint, int);
DECLARE_NEWTYPE(newdouble, double);

START_TEST(newtype_test)
{
	{
		newint_t n1 = newint_from_int(1);
		newint_t n2 = newint_from_int(2);

		litest_assert_int_eq(newint(n1), 1);
		litest_assert_int_eq(newint_as_int(n1), 1);
		litest_assert_int_eq(newint(n2), 2);
		litest_assert_int_eq(newint_as_int(n2), 2);

		litest_assert_int_eq(newint_cmp(n1, n2), -1);
		litest_assert_int_eq(newint_cmp(n1, n1), 0);
		litest_assert_int_eq(newint_cmp(n2, n1), 1);

		newint_t copy = newint_copy(n1);
		litest_assert_int_eq(newint_cmp(n1, copy), 0);

		newint_t min = newint_min(n1, n2);
		newint_t max = newint_max(n1, n2);
		litest_assert_int_eq(newint_cmp(min, n1), 0);
		litest_assert_int_eq(newint_cmp(max, n2), 0);

		litest_assert(newint_gt(n1, 0));
		litest_assert(newint_eq(n1, 1));
		litest_assert(newint_ge(n1, 1));
		litest_assert(newint_le(n1, 1));
		litest_assert(newint_ne(n1, 2));
		litest_assert(newint_lt(n1, 2));

		litest_assert(!newint_gt(n1, 1));
		litest_assert(!newint_eq(n1, 0));
		litest_assert(!newint_ge(n1, 2));
		litest_assert(!newint_le(n1, 0));
		litest_assert(!newint_ne(n1, 1));
		litest_assert(!newint_lt(n1, 1));
	}
	{
		newdouble_t n1 = newdouble_from_double(1.2);
		newdouble_t n2 = newdouble_from_double(2.3);

		litest_assert_double_eq(newdouble(n1), 1.2);
		litest_assert_double_eq(newdouble_as_double(n1), 1.2);
		litest_assert_double_eq(newdouble(n2), 2.3);
		litest_assert_double_eq(newdouble_as_double(n2), 2.3);

		litest_assert_int_eq(newdouble_cmp(n1, n2), -1);
		litest_assert_int_eq(newdouble_cmp(n1, n1), 0);
		litest_assert_int_eq(newdouble_cmp(n2, n1), 1);

		newdouble_t copy = newdouble_copy(n1);
		litest_assert_int_eq(newdouble_cmp(n1, copy), 0);

		newdouble_t min = newdouble_min(n1, n2);
		newdouble_t max = newdouble_max(n1, n2);
		litest_assert_int_eq(newdouble_cmp(min, n1), 0);
		litest_assert_int_eq(newdouble_cmp(max, n2), 0);

		litest_assert(newdouble_gt(n1, 0.0));
		litest_assert(newdouble_eq(n1, 1.2));
		litest_assert(newdouble_ge(n1, 1.2));
		litest_assert(newdouble_le(n1, 1.2));
		litest_assert(newdouble_ne(n1, 2.3));
		litest_assert(newdouble_lt(n1, 2.3));

		litest_assert(!newdouble_gt(n1, 1.2));
		litest_assert(!newdouble_eq(n1, 0.0));
		litest_assert(!newdouble_ge(n1, 2.3));
		litest_assert(!newdouble_le(n1, 0.0));
		litest_assert(!newdouble_ne(n1, 1.2));
		litest_assert(!newdouble_lt(n1, 1.2));
	}
}
END_TEST

struct sunref {};
struct sdestroy {};
struct sfree {};

static void
sunref_unref(struct sunref *s)
{
	free(s);
}
static void
sdestroy_destroy(struct sdestroy *s)
{
	free(s);
}
static void
sfree_free(struct sfree *s)
{
	free(s);
}

DEFINE_UNREF_CLEANUP_FUNC(sunref);
DEFINE_DESTROY_CLEANUP_FUNC(sdestroy);
DEFINE_FREE_CLEANUP_FUNC(sfree);

START_TEST(attribute_cleanup)
{
	/* These tests will likely only show up in valgrind,
	 * the various asserts are just to shut up the compiler
	 * about unused variables
	 */
	{
		_autofree_ char *autofree = zalloc(64);
		litest_assert(autofree);
	}
	{
		_autofree_ char *stolen = zalloc(64);
		free(steal(&stolen));
	}
	{
		_autoclose_ int fd = open("/proc/self/cmdline", O_RDONLY);
		litest_assert_int_ge(fd, 0);

		_autoclose_ int badfd = -1;
		litest_assert_int_eq(badfd, -1);

		_autoclose_ int stealfd = open("/proc/self/cmdline", O_RDONLY);
		steal_fd(&stealfd);
		litest_assert_int_eq(stealfd, -1);
	}
	{
		_autostrvfree_ char **strv = zalloc(3 * sizeof(*strv));
		for (int i = 0; i < 2; i++) {
			strv[i] = strdup_printf("element %d", i);
		}

		_autostrvfree_ char **badstrv = NULL;
		litest_assert_ptr_null(badstrv);
	}
	{
		_autofclose_ FILE *fp = fopen("/proc/self/cmdline", "r");
		litest_assert_ptr_notnull(fp);

		_autofclose_ FILE *badfd = NULL;
		litest_assert_ptr_null(badfd);
	}
	{
		_unref_(sunref) *s = zalloc(sizeof(*s));
	}
	{
		_destroy_(sdestroy) *s = zalloc(sizeof(*s));
	}
	{
		_free_(sfree) *s = zalloc(sizeof(*s));
	}
}
END_TEST

START_TEST(macros_expand)
{
#define _A1(_1) _1, #_1
#define _A2(_1, _2) _1, _2
#define A(...) _VARIABLE_MACRO(_A, __VA_ARGS__)
	char buf[64];
	snprintf(buf, sizeof(buf), "%d:%s", A(0));
	litest_assert_str_eq(buf, "0:0");
	snprintf(buf, sizeof(buf), "%d:%s", A(100));
	litest_assert_str_eq(buf, "100:100");
	snprintf(buf, sizeof(buf), "%d:%s", A(100, "hundred"));
	litest_assert_str_eq(buf, "100:hundred");
#undef _A1
#undef _A2
#undef A
}
END_TEST

START_TEST(evdev_frames)
{
#define U(u_) evdev_usage_from_uint32_t(u_)
	{
		evdev_frame_unref(NULL); /* unref on NULL is permitted */
	}
	{
		_unref_(evdev_frame) *frame = evdev_frame_new(3);
		litest_assert_int_eq(evdev_frame_get_count(frame), 1U); /* SYN_REPORT */

		litest_assert_ptr_eq(evdev_frame_ref(frame), frame);
		litest_assert_ptr_eq(evdev_frame_unref(frame), NULL);
	}
	{
		_unref_(evdev_frame) *frame = evdev_frame_new(3);
		struct evdev_event toobig[] = {
			{
				.usage = U(EVDEV_ABS_X),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_Y),
				.value = 2,
			},
			{
				.usage = U(EVDEV_ABS_Z),
				.value = 3,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
		};

		int rc = evdev_frame_set(frame, toobig, ARRAY_LENGTH(toobig));
		litest_assert_int_eq(rc, -ENOMEM);
	}
	{
		struct evdev_event events[] = {
			{
				.usage = U(EVDEV_ABS_X),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_Y),
				.value = 2,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
		};

		_unref_(evdev_frame) *frame = evdev_frame_new(3);
		int rc = evdev_frame_set(frame, events, ARRAY_LENGTH(events));
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame),
				     ARRAY_LENGTH(events));
		litest_assert_int_eq(frame->max_size, ARRAY_LENGTH(events));

		size_t nevents;
		rc = memcmp(evdev_frame_get_events(frame, &nevents),
			    events,
			    sizeof(events));
		litest_assert_int_eq(rc, 0);
		litest_assert_int_eq(nevents, ARRAY_LENGTH(events));

		/* Already full, can't append */
		rc = evdev_frame_append(frame, events, 1);
		litest_assert_int_eq(rc, -ENOMEM);
	}
	{
		struct evdev_event events[] = {
			{
				.usage = U(EVDEV_ABS_X),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_Y),
				.value = 2,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
		};

		_unref_(evdev_frame) *frame = evdev_frame_new(3);
		int rc = evdev_frame_set(frame, events, 1);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame),
				     2U); /* we appended SYN_REPORT */
		rc = evdev_frame_append(frame, events + 1, 1);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame),
				     3U); /* we appended SYN_REPORT */
		rc = evdev_frame_append(frame, events + 2, 1);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame),
				     3U); /* SYN_REPORT already there */
	}
	{
		struct evdev_event interrupted[] = {
			{
				.usage = U(EVDEV_ABS_X),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_Y),
				.value = 2,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
			{
				.usage = U(EVDEV_ABS_RX),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_RY),
				.value = 2,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
		};

		_unref_(evdev_frame) *frame = evdev_frame_new(5);
		int rc = evdev_frame_set(frame, interrupted, ARRAY_LENGTH(interrupted));
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame), 3U);

		rc = evdev_frame_set(frame, &interrupted[2], 1);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame), 1U);

		rc = evdev_frame_set(frame,
				     &interrupted[1],
				     ARRAY_LENGTH(interrupted) - 1);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame), 2U);

		/* We never appended a timestamp */
		litest_assert_int_eq(evdev_frame_get_time(frame), 0U);
	}
	{
		struct evdev_event events[] = {
			{
				.usage = U(EVDEV_ABS_X),
				.value = 1,
			},
			{
				.usage = U(EVDEV_ABS_Y),
				.value = 2,
			},
			{
				.usage = U(EVDEV_SYN_REPORT),
				.value = 0,
			},
		};

		_unref_(evdev_frame) *frame = evdev_frame_new(3);
		int rc = evdev_frame_append_one(frame, U(EVDEV_ABS_X), 1);
		litest_assert_neg_errno_success(rc);
		rc = evdev_frame_append_one(frame, U(EVDEV_ABS_Y), 2);
		litest_assert_neg_errno_success(rc);
		rc = evdev_frame_append_one(frame, U(EV_SYN), 0);
		litest_assert_neg_errno_success(rc);

		litest_assert_int_eq(evdev_frame_get_count(frame),
				     ARRAY_LENGTH(events));
		litest_assert_int_eq(frame->max_size, ARRAY_LENGTH(events));

		size_t nevents;
		rc = memcmp(evdev_frame_get_events(frame, &nevents),
			    events,
			    sizeof(events));
		litest_assert_int_eq(rc, 0);
		litest_assert_int_eq(nevents, ARRAY_LENGTH(events));

		/* Already full, can't append */
		rc = evdev_frame_append_one(frame, U(EVDEV_ABS_Z), 1);
		litest_assert_int_eq(rc, -ENOMEM);

		/* Appending SYN_REPORT is a noop */
		rc = evdev_frame_append_one(frame, U(EVDEV_SYN_REPORT), 0);
		litest_assert_neg_errno_success(rc);
		litest_assert_int_eq(evdev_frame_get_count(frame),
				     ARRAY_LENGTH(events));
		litest_assert_int_eq(frame->max_size, ARRAY_LENGTH(events));
	}
}
END_TEST

int
main(void)
{
	struct litest_runner *runner = litest_runner_new();

	/* not worth forking the tests here */
	litest_runner_set_num_parallel(runner, 0);

#define ADD_TEST(func_) do { \
	struct litest_runner_test_description tdesc =  { \
		.func = func_, \
	};\
	snprintf(tdesc.name, sizeof(tdesc.name), # func_); \
	litest_runner_add_test(runner, &tdesc); \
} while(0)

	ADD_TEST(auto_test);
	ADD_TEST(mkdir_p_test);
	ADD_TEST(rmdir_r_test);
	ADD_TEST(tmpdir_test);
	ADD_TEST(find_files_test);

	ADD_TEST(array_for_each);

	ADD_TEST(bitfield_helpers);
	ADD_TEST(bitmask_test);
	ADD_TEST(matrix_helpers);
	ADD_TEST(ratelimit_helpers);
	ADD_TEST(dpi_parser);
	ADD_TEST(wheel_click_parser);
	ADD_TEST(wheel_click_count_parser);
	ADD_TEST(dimension_prop_parser);
	ADD_TEST(reliability_prop_parser);
	ADD_TEST(calibration_prop_parser);
	ADD_TEST(range_prop_parser);
	ADD_TEST(boolean_prop_parser);
	ADD_TEST(evcode_prop_parser);
	ADD_TEST(input_prop_parser);
	ADD_TEST(evdev_abs_parser);
	ADD_TEST(safe_atoi_test);
	ADD_TEST(safe_atoi_base_16_test);
	ADD_TEST(safe_atoi_base_8_test);
	ADD_TEST(safe_atou_test);
	ADD_TEST(safe_atou_base_16_test);
	ADD_TEST(safe_atou_base_8_test);
	ADD_TEST(safe_atou64_test);
	ADD_TEST(safe_atod_test);
	ADD_TEST(strsplit_test);
	ADD_TEST(strv_for_each_test);
	ADD_TEST(strv_append_test);
	ADD_TEST(strv_find_test);
	ADD_TEST(strv_find_substring_test);
	ADD_TEST(double_array_from_string_test);
	ADD_TEST(strargv_test);
	ADD_TEST(kvsplit_double_test);
	ADD_TEST(strjoin_test);
	ADD_TEST(strstrip_test);
	ADD_TEST(strendswith_test);
	ADD_TEST(strstartswith_test);
	ADD_TEST(strsanitize_test);
	ADD_TEST(time_conversion);
	ADD_TEST(human_time);

	ADD_TEST(list_test_insert);
	ADD_TEST(list_test_append);
	ADD_TEST(list_test_foreach);
	ADD_TEST(list_test_first_last);
	ADD_TEST(list_test_chain);
	ADD_TEST(strverscmp_test);
	ADD_TEST(streq_test);
	ADD_TEST(strneq_test);
	ADD_TEST(trunkname_test);
	ADD_TEST(basename_test);

	ADD_TEST(absinfo_normalize_value_test);

	ADD_TEST(range_test);
	ADD_TEST(stringbuf_test);
	ADD_TEST(multivalue_test);

	ADD_TEST(newtype_test);
	ADD_TEST(attribute_cleanup);
	ADD_TEST(macros_expand);

	ADD_TEST(evdev_frames);

	enum litest_runner_result result = litest_runner_run_tests(runner);
	litest_runner_destroy(runner);

	if (result == LITEST_SKIP)
		return 77;

	return result - LITEST_PASS;
}
