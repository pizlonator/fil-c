/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"
#include "xcursor.h"
#include "wayland-cursor.h"
#include "wayland-client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <os-compatibility.h>
#include <glib.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct shm_pool {
	struct wl_shm_pool *pool;
	int fd;
	unsigned int size;
	unsigned int used;
	char *data;
};

static struct shm_pool *
shm_pool_create(struct wl_shm *shm, int size)
{
	struct shm_pool *pool;

	pool = malloc(sizeof *pool);
	if (!pool)
		return NULL;

	pool->fd = os_create_anonymous_file (size);
	if (pool->fd < 0)
		goto err_free;

	pool->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			  pool->fd, 0);

	if (pool->data == MAP_FAILED)
		goto err_close;

	pool->pool = wl_shm_create_pool(shm, pool->fd, size);
	pool->size = size;
	pool->used = 0;

	return pool;

err_close:
	close(pool->fd);
err_free:
	free(pool);
	return NULL;
}

static int
shm_pool_resize(struct shm_pool *pool, int size)
{
	if (ftruncate(pool->fd, size) < 0)
		return 0;

#ifdef HAVE_POSIX_FALLOCATE
	errno = posix_fallocate(pool->fd, 0, size);
	if (errno != 0)
		return 0;
#endif

	wl_shm_pool_resize(pool->pool, size);

	munmap(pool->data, pool->size);

	pool->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			  pool->fd, 0);
	if (pool->data == (void *)-1)
		return 0;
	pool->size = size;

	return 1;
}

static int
shm_pool_allocate(struct shm_pool *pool, int size)
{
	int offset;

	if (pool->used + size > pool->size)
		if (!shm_pool_resize(pool, 2 * pool->size + size))
			return -1;

	offset = pool->used;
	pool->used += size;

	return offset;
}

static void
shm_pool_destroy(struct shm_pool *pool)
{
	munmap(pool->data, pool->size);
	wl_shm_pool_destroy(pool->pool);
	close(pool->fd);
	free(pool);
}


struct wl_cursor_theme {
	unsigned int cursor_count;
	struct wl_cursor **cursors;
	struct wl_shm *shm;
	struct shm_pool *pool;
	int size;
        char *path;
};

struct cursor_image {
	struct wl_cursor_image image;
	struct wl_cursor_theme *theme;
	struct wl_buffer *buffer;
	int offset; /* data offset of this image in the shm pool */
};

struct cursor {
	struct wl_cursor cursor;
	uint32_t total_delay; /* length of the animation in ms */
};

/** Get an shm buffer for a cursor image
 *
 * \param image The cursor image
 * \return An shm buffer for the cursor image. The user should not destroy
 * the returned buffer.
 */
struct wl_buffer *
wl_cursor_image_get_buffer(struct wl_cursor_image *_img)
{
	struct cursor_image *image = (struct cursor_image *) _img;
	struct wl_cursor_theme *theme = image->theme;

	if (!image->buffer) {
		image->buffer =
			wl_shm_pool_create_buffer(theme->pool->pool,
						  image->offset,
						  _img->width, _img->height,
						  _img->width * 4,
						  WL_SHM_FORMAT_ARGB8888);
	};

	return image->buffer;
}

static void
wl_cursor_image_destroy(struct wl_cursor_image *_img)
{
	struct cursor_image *image = (struct cursor_image *) _img;

	if (image->buffer)
		wl_buffer_destroy(image->buffer);

	free(image);
}

static void
wl_cursor_destroy(struct wl_cursor *cursor)
{
	unsigned int i;

	for (i = 0; i < cursor->image_count; i++)
		wl_cursor_image_destroy(cursor->images[i]);

	free(cursor->images);
	free(cursor->name);
	free(cursor);
}

static struct wl_cursor *
wl_cursor_create_from_xcursor_images(struct wl_cursor_theme *theme,
                                     const char *name,
                                     unsigned int size,
                                     unsigned int scale)
{
        char *path;
        XcursorImages *images;
        struct wl_cursor *cursor;
	struct cursor_image *image;
	int i, nbytes;
        unsigned int load_size;
        int load_scale = 1;

        load_size = size * scale;

        path = g_strconcat (theme->path, "/", name, NULL);
        images = xcursor_load_images (path, load_size);

        if (!images)
          {
            g_free (path);
            return NULL;
          }

        if (images->images[0]->width != load_size ||
            images->images[0]->height != load_size)
          {
	        xcursor_images_destroy (images);
                images = xcursor_load_images (path, size);
                load_scale = scale;
          }

        g_free (path);

	cursor = malloc(sizeof *cursor);
	if (!cursor) {
	        xcursor_images_destroy (images);
		return NULL;
        }

	cursor->images =
		malloc(images->nimage * sizeof cursor->images[0]);
	if (!cursor->images) {
		free(cursor);
	        xcursor_images_destroy (images);
		return NULL;
	}

	cursor->name = strdup(name);
        cursor->size = load_size;

	for (i = 0; i < images->nimage; i++) {
		image = malloc(sizeof *image);
		if (image == NULL)
			break;

		image->theme = theme;
		image->buffer = NULL;

		image->image.width = images->images[i]->width * load_scale;
		image->image.height = images->images[i]->height * load_scale;
		image->image.hotspot_x = images->images[i]->xhot * load_scale;
		image->image.hotspot_y = images->images[i]->yhot * load_scale;
		image->image.delay = images->images[i]->delay;

		nbytes = image->image.width * image->image.height * 4;
		image->offset = shm_pool_allocate(theme->pool, nbytes);
		if (image->offset < 0) {
			free(image);
			break;
		}

                if (load_scale == 1) {
		    /* copy pixels to shm pool */
                    memcpy(theme->pool->data + image->offset,
                           images->images[i]->pixels, nbytes);
                }
                else {
                    /* scale image up while copying it */
                    for (int y = 0; y < image->image.height; y++) {
                        char *p = theme->pool->data + image->offset + y * image->image.width * 4;
                        char *q = ((char *)images->images[i]->pixels) + (y / load_scale) * images->images[i]->width * 4;
                        for (int x = 0; x < image->image.width; x++) {
                            p[4 * x] = q[4 * (x/load_scale)];
                            p[4 * x + 1] = q[4 * (x/load_scale) + 1];
                            p[4 * x + 2] = q[4 * (x/load_scale) + 2];
                            p[4 * x + 3] = q[4 * (x/load_scale) + 3];
                        }
                    }
                }
		cursor->images[i] = (struct wl_cursor_image *) image;
	}
	cursor->image_count = i;

	if (cursor->image_count == 0) {
		free(cursor->name);
		free(cursor->images);
		free(cursor);
	        xcursor_images_destroy (images);
		return NULL;
	}

	xcursor_images_destroy (images);

	return cursor;
}

static void
load_cursor(struct wl_cursor_theme *theme,
            const char             *name,
            unsigned int            size,
            unsigned int            scale)
{
	struct wl_cursor *cursor;

        cursor = wl_cursor_create_from_xcursor_images(theme, name, size, scale);

	if (cursor) {
		theme->cursor_count++;
		theme->cursors =
			realloc(theme->cursors,
				theme->cursor_count * sizeof theme->cursors[0]);

		if (theme->cursors == NULL) {
			theme->cursor_count--;
			free(cursor);
		} else {
			theme->cursors[theme->cursor_count - 1] = cursor;
		}
	}
}

/** Load a cursor theme to memory shared with the compositor
 *
 * \param name The name of the cursor theme to load. If %NULL, the default
 * theme will be loaded.
 * \param size Desired size of the cursor images.
 * \param shm The compositor's shm interface.
 *
 * \return An object representing the theme that should be destroyed with
 * wl_cursor_theme_destroy() or %NULL on error. If no theme with the given
 * name exists, a default theme will be loaded.
 */
struct wl_cursor_theme *
wl_cursor_theme_create(const char *path, int size, struct wl_shm *shm)
{
	struct wl_cursor_theme *theme;

	theme = malloc(sizeof *theme);
	if (!theme)
		return NULL;

	theme->path = strdup (path);
	theme->size = size;
	theme->cursor_count = 0;
	theme->cursors = NULL;

	theme->pool = shm_pool_create(shm, size * size * 4);
	if (!theme->pool) {
                free (theme->path);
                free (theme);
		return NULL;
        }

	return theme;
}

/** Destroys a cursor theme object
 *
 * \param theme The cursor theme to be destroyed
 */
void
wl_cursor_theme_destroy(struct wl_cursor_theme *theme)
{
	unsigned int i;

	for (i = 0; i < theme->cursor_count; i++)
		wl_cursor_destroy(theme->cursors[i]);

	shm_pool_destroy(theme->pool);

	free(theme->cursors);
        free(theme->path);
	free(theme);
}

/** Get the cursor for a given name from a cursor theme
 *
 * \param theme The cursor theme
 * \param name Name of the desired cursor
 * \return The theme's cursor of the given name or %NULL if there is no
 * such cursor
 */
struct wl_cursor *
wl_cursor_theme_get_cursor(struct wl_cursor_theme *theme,
			   const char *name,
                           unsigned int scale)
{
	unsigned int i;
        unsigned int size;

        size = theme->size * scale;

	for (i = 0; i < theme->cursor_count; i++) {
                if (size == theme->cursors[i]->size &&
		    strcmp(name, theme->cursors[i]->name) == 0)
		        return theme->cursors[i];
        }

        load_cursor (theme, name, theme->size, scale);

        if (i < theme->cursor_count) {
                if (size == theme->cursors[i]->size &&
                    strcmp (name, theme->cursors[theme->cursor_count - 1]->name) == 0)
                        return theme->cursors[theme->cursor_count - 1];
        }

	return NULL;
}
