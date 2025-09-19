/*
 * Copyright © 2024 Red Hat, Inc.
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

#include "config.h"

#include "util-files.h"
#include "util-list.h"
#include "util-macros.h"
#include "util-strings.h"

#include "libinput-versionsort.h"

struct file {
	struct list link;
	char *name;
	char *directory;
};

static void
file_destroy(struct file *f)
{
	list_remove(&f->link);
	free(f->name);
	free(f->directory);
	free(f);
}

DEFINE_DESTROY_CLEANUP_FUNC(file);

/**
 * Appends to the given list all files files in the given directory that end
 * with the given with the given suffix.
 */
static void
filenames(const char *directory, const char *suffix, struct list *list)
{
	_autofree_ struct dirent **namelist = NULL;

	int ndev = scandir(directory, &namelist, NULL, versionsort);
	if (ndev <= 0)
		return;

	for (int i = 0; i < ndev; i++) {
		_autofree_ struct dirent *entry = namelist[i];
		if (!strendswith(entry->d_name, suffix))
			continue;

		struct file *f = zalloc(sizeof(*f));
		f->name = safe_strdup(entry->d_name);
		f->directory = safe_strdup(directory);
		list_append(list, &f->link);
	}
}

static int
filenamesort(const void *a, const void *b)
{
	const struct file *f1 = *(const struct file **)a;
	const struct file *f2 = *(const struct file **)b;

	return strverscmp(f1->name, f2->name);
}

char **
list_files(const char **directories, const char *suffix, size_t *nfiles_out)
{
	struct list files = LIST_INIT(files);

	if (!directories) {
		if (nfiles_out)
			*nfiles_out = 0;
		return zalloc(1 * sizeof(char *));
	}

	const char **d = directories;
	while (*d) {
		struct list new_files = LIST_INIT(new_files);
		filenames(*d, suffix, &new_files);

		struct file *old_file;
		list_for_each_safe(old_file, &files, link) {
			struct file *new_file;
			list_for_each_safe(new_file, &new_files, link) {
				if (streq(old_file->name, new_file->name)) {
					file_destroy(new_file);
					break;
				}
			}
		}
		struct file *new_file;
		list_for_each_safe(new_file, &new_files, link) {
			list_remove(&new_file->link);
			list_append(&files, &new_file->link);
		}
		d++;
	}

	size_t nfiles = 0;
	struct file *f;
	list_for_each(f, &files, link) {
		nfiles++;
	}
	/* Allocating +1 conveniently handles the directories[0] = NULL case */
	_autofree_ struct file **fs = zalloc((nfiles + 1) * sizeof(*fs));
	size_t idx = 0;
	list_for_each_safe(f, &files, link) {
		fs[idx++] = f;
		list_remove(&f->link);
		list_init(&f->link); // So we can file_destroy it later
	}

	qsort(fs, nfiles, sizeof(*fs), filenamesort);

	char **paths = zalloc((nfiles + 1) * sizeof(*paths));
	for (size_t i = 0; i < nfiles; i++) {
		_destroy_(file) *f = fs[i];
		paths[i] = strdup_printf("%s/%s", f->directory, f->name);
	}

	if (nfiles_out)
		*nfiles_out = nfiles;

	return steal(&paths);
}
