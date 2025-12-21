/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
 * Copyright (C) 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if 0
#ifdef HAVE_PTHREAD
#define _GNU_SOURCE
#include <pthread.h>
#endif
#endif

#include "video-converter.h"

#include <glib.h>
#include <string.h>
#include <math.h>
#include <gst/base/base.h>

#include "video-orc.h"

/**
 * SECTION:videoconverter
 * @title: GstVideoConverter
 * @short_description: Generic video conversion
 *
 * This object is used to convert video frames from one format to another.
 * The object can perform conversion of:
 *
 *  * video format
 *  * video colorspace
 *  * chroma-siting
 *  * video size
 *
 */

/*
 * (a)  unpack
 * (b)  chroma upsample
 * (c)  (convert Y'CbCr to R'G'B')
 * (d)  gamma decode
 * (e)  downscale
 * (f)  colorspace convert through XYZ
 * (g)  upscale
 * (h)  gamma encode
 * (i)  (convert R'G'B' to Y'CbCr)
 * (j)  chroma downsample
 * (k)  pack
 *
 * quality options
 *
 *  (a) range truncate, range expand
 *  (b) full upsample, 1-1 non-cosited upsample, no upsample
 *  (c) 8 bits, 16 bits
 *  (d)
 *  (e) 8 bits, 16 bits
 *  (f) 8 bits, 16 bits
 *  (g) 8 bits, 16 bits
 *  (h)
 *  (i) 8 bits, 16 bits
 *  (j) 1-1 cosited downsample, no downsample
 *  (k)
 *
 *
 *         1 : a ->   ->   ->   -> e  -> f  -> g  ->   ->   ->   -> k
 *         2 : a ->   ->   ->   -> e  -> f* -> g  ->   ->   ->   -> k
 *         3 : a ->   ->   ->   -> e* -> f* -> g* ->   ->   ->   -> k
 *         4 : a -> b ->   ->   -> e  -> f  -> g  ->   ->   -> j -> k
 *         5 : a -> b ->   ->   -> e* -> f* -> g* ->   ->   -> j -> k
 *         6 : a -> b -> c -> d -> e  -> f  -> g  -> h -> i -> j -> k
 *         7 : a -> b -> c -> d -> e* -> f* -> g* -> h -> i -> j -> k
 *
 *         8 : a -> b -> c -> d -> e* -> f* -> g* -> h -> i -> j -> k
 *         9 : a -> b -> c -> d -> e* -> f* -> g* -> h -> i -> j -> k
 *        10 : a -> b -> c -> d -> e* -> f* -> g* -> h -> i -> j -> k
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-converter", 0,
        "video-converter object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

typedef void (*GstParallelizedTaskFunc) (gpointer user_data);

typedef struct _GstParallelizedTaskRunner GstParallelizedTaskRunner;
typedef struct _GstParallelizedWorkItem GstParallelizedWorkItem;

struct _GstParallelizedWorkItem
{
  GstParallelizedTaskRunner *self;
  GstParallelizedTaskFunc func;
  gpointer user_data;
};

struct _GstParallelizedTaskRunner
{
  GstTaskPool *pool;
  gboolean own_pool;
  guint n_threads;

  GstQueueArray *tasks;
  GstQueueArray *work_items;

  GMutex lock;

  gboolean async_tasks;
};

static void
gst_parallelized_task_thread_func (gpointer data)
{
  GstParallelizedTaskRunner *runner = data;
  GstParallelizedWorkItem *work_item;

  g_mutex_lock (&runner->lock);
  work_item = gst_queue_array_pop_head (runner->work_items);
  g_mutex_unlock (&runner->lock);

  g_assert (work_item != NULL);
  g_assert (work_item->func != NULL);


  work_item->func (work_item->user_data);
  if (runner->async_tasks)
    g_free (work_item);
}

static void
gst_parallelized_task_runner_join (GstParallelizedTaskRunner * self)
{
  gboolean joined = FALSE;

  while (!joined) {
    g_mutex_lock (&self->lock);
    if (!(joined = gst_queue_array_is_empty (self->tasks))) {
      gpointer task = gst_queue_array_pop_head (self->tasks);
      g_mutex_unlock (&self->lock);
      gst_task_pool_join (self->pool, task);
    } else {
      g_mutex_unlock (&self->lock);
    }
  }
}

static void
gst_parallelized_task_runner_free (GstParallelizedTaskRunner * self)
{
  gst_parallelized_task_runner_join (self);

  gst_queue_array_free (self->work_items);
  gst_queue_array_free (self->tasks);
  if (self->own_pool)
    gst_task_pool_cleanup (self->pool);
  gst_object_unref (self->pool);
  g_mutex_clear (&self->lock);
  g_free (self);
}

static GstParallelizedTaskRunner *
gst_parallelized_task_runner_new (guint n_threads, GstTaskPool * pool,
    gboolean async_tasks)
{
  GstParallelizedTaskRunner *self;

  if (n_threads == 0)
    n_threads = g_get_num_processors ();

  self = g_new0 (GstParallelizedTaskRunner, 1);

  if (pool) {
    self->pool = g_object_ref (pool);
    self->own_pool = FALSE;

    /* No reason to split up the work between more threads than the
     * pool can spawn */
    if (GST_IS_SHARED_TASK_POOL (pool))
      n_threads =
          MIN (n_threads,
          gst_shared_task_pool_get_max_threads (GST_SHARED_TASK_POOL (pool)));
  } else {
    self->pool = gst_shared_task_pool_new ();
    self->own_pool = TRUE;
    gst_shared_task_pool_set_max_threads (GST_SHARED_TASK_POOL (self->pool),
        n_threads);
    gst_task_pool_prepare (self->pool, NULL);
  }

  self->tasks = gst_queue_array_new (n_threads);
  self->work_items = gst_queue_array_new (n_threads);

  self->n_threads = n_threads;

  g_mutex_init (&self->lock);

  /* Set when scheduling a job */
  self->async_tasks = async_tasks;

  return self;
}

static void
gst_parallelized_task_runner_finish (GstParallelizedTaskRunner * self)
{
  gst_parallelized_task_runner_join (self);
}

static void
gst_parallelized_task_runner_run (GstParallelizedTaskRunner * self,
    GstParallelizedTaskFunc func, gpointer * task_data)
{
  guint n_threads = self->n_threads;

  if (n_threads > 1 || self->async_tasks) {
    guint i = 0;
    g_mutex_lock (&self->lock);
    if (!self->async_tasks) {
      /* if not async, perform one of the functions in the current thread */
      i = 1;
    }
    for (; i < n_threads; i++) {
      gpointer task;
      GstParallelizedWorkItem *work_item;

      if (!self->async_tasks)
        work_item = g_newa (GstParallelizedWorkItem, 1);
      else
        work_item = g_new0 (GstParallelizedWorkItem, 1);

      work_item->self = self;
      work_item->func = func;
      work_item->user_data = task_data[i];
      gst_queue_array_push_tail (self->work_items, work_item);

      task =
          gst_task_pool_push (self->pool, gst_parallelized_task_thread_func,
          self, NULL);

      /* The return value of push() is unfortunately nullable, and we can't deal with that */
      g_assert (task != NULL);
      gst_queue_array_push_tail (self->tasks, task);
    }
    g_mutex_unlock (&self->lock);
  }

  if (!self->async_tasks) {
    func (task_data[0]);

    gst_parallelized_task_runner_finish (self);
  }
}

typedef struct _GstLineCache GstLineCache;

#define SCALE    (8)
#define SCALE_F  ((float) (1 << SCALE))

typedef struct _MatrixData MatrixData;

struct _MatrixData
{
  gdouble dm[4][4];
  gint im[4][4];
  gint width;
  guint64 orc_p1;
  guint64 orc_p2;
  guint64 orc_p3;
  guint64 orc_p4;
  gint64 *t_r;
  gint64 *t_g;
  gint64 *t_b;
  gint64 t_c;
  void (*matrix_func) (MatrixData * data, gpointer pixels);
};

typedef struct _GammaData GammaData;

struct _GammaData
{
  gpointer gamma_table;
  gint width;
  void (*gamma_func) (GammaData * data, gpointer dest, gpointer src);
};

typedef enum
{
  ALPHA_MODE_NONE = 0,
  ALPHA_MODE_COPY = (1 << 0),
  ALPHA_MODE_SET = (1 << 1),
  ALPHA_MODE_MULT = (1 << 2)
} AlphaMode;

typedef struct
{
  guint8 *data;
  guint stride;
  guint n_lines;
  guint idx;
  gpointer user_data;
  GDestroyNotify notify;
} ConverterAlloc;

typedef void (*FastConvertFunc) (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane);

struct _GstVideoConverter
{
  gint flags;

  GstVideoInfo in_info;
  GstVideoInfo out_info;

  gint in_x;
  gint in_y;
  gint in_width;
  gint in_height;
  gint in_maxwidth;
  gint in_maxheight;
  gint out_x;
  gint out_y;
  gint out_width;
  gint out_height;
  gint out_maxwidth;
  gint out_maxheight;

  gint current_pstride;
  gint current_width;
  gint current_height;
  GstVideoFormat current_format;
  gint current_bits;

  GstStructure *config;

  GstParallelizedTaskRunner *conversion_runner;

  guint16 **tmpline;

  gboolean fill_border;
  gpointer borderline;
  guint64 borders[4];
  guint32 border_argb;
  guint32 alpha_value;
  AlphaMode alpha_mode;

  void (*convert) (GstVideoConverter * convert, const GstVideoFrame * src,
      GstVideoFrame * dest);

  /* data for unpack */
  GstLineCache **unpack_lines;
  GstVideoFormat unpack_format;
  guint unpack_bits;
  gboolean unpack_rgb;
  gboolean identity_unpack;
  gint unpack_pstride;

  /* chroma upsample */
  GstLineCache **upsample_lines;
  GstVideoChromaResample **upsample;
  GstVideoChromaResample **upsample_p;
  GstVideoChromaResample **upsample_i;
  guint up_n_lines;
  gint up_offset;

  /* to R'G'B */
  GstLineCache **to_RGB_lines;
  MatrixData to_RGB_matrix;
  /* gamma decode */
  GammaData gamma_dec;

  /* scaling */
  GstLineCache **hscale_lines;
  GstVideoScaler **h_scaler;
  gint h_scale_format;
  GstLineCache **vscale_lines;
  GstVideoScaler **v_scaler;
  GstVideoScaler **v_scaler_p;
  GstVideoScaler **v_scaler_i;
  gint v_scale_width;
  gint v_scale_format;

  /* color space conversion */
  GstLineCache **convert_lines;
  MatrixData convert_matrix;
  gint in_bits;
  gint out_bits;

  /* alpha correction */
  GstLineCache **alpha_lines;
  void (*alpha_func) (GstVideoConverter * convert, gpointer pixels, gint width);

  /* gamma encode */
  GammaData gamma_enc;
  /* to Y'CbCr */
  GstLineCache **to_YUV_lines;
  MatrixData to_YUV_matrix;

  /* chroma downsample */
  GstLineCache **downsample_lines;
  GstVideoChromaResample **downsample;
  GstVideoChromaResample **downsample_p;
  GstVideoChromaResample **downsample_i;
  guint down_n_lines;
  gint down_offset;

  /* dither */
  GstLineCache **dither_lines;
  GstVideoDither **dither;

  /* pack */
  GstLineCache **pack_lines;
  guint pack_nlines;
  GstVideoFormat pack_format;
  guint pack_bits;
  gboolean pack_rgb;
  gboolean identity_pack;
  gint pack_pstride;
  gconstpointer pack_pal;
  gsize pack_palsize;

  const GstVideoFrame *src;
  GstVideoFrame *dest;

  /* fastpath */
  GstVideoFormat fformat[4];
  gint fin_x[4];
  gint fin_y[4];
  gint fout_x[4];
  gint fout_y[4];
  gint fout_width[4];
  gint fout_height[4];
  gint fsplane[4];
  gint ffill[4];

  struct
  {
    GstVideoScaler **scaler;
  } fh_scaler[4];
  struct
  {
    GstVideoScaler **scaler;
  } fv_scaler[4];
  FastConvertFunc fconvert[4];

  /* for parallel async running */
  gpointer tasks[4];
  gpointer tasks_p[4];
};

typedef gpointer (*GstLineCacheAllocLineFunc) (GstLineCache * cache, gint idx,
    gpointer user_data);
typedef gboolean (*GstLineCacheNeedLineFunc) (GstLineCache * cache, gint idx,
    gint out_line, gint in_line, gpointer user_data);

struct _GstLineCache
{
  gint first;
  gint backlog;
  GPtrArray *lines;

  GstLineCache *prev;
  gboolean write_input;
  gboolean pass_alloc;
  gboolean alloc_writable;

  GstLineCacheNeedLineFunc need_line;
  gint need_line_idx;
  gpointer need_line_data;
  GDestroyNotify need_line_notify;

  guint n_lines;
  guint stride;
  GstLineCacheAllocLineFunc alloc_line;
  gpointer alloc_line_data;
  GDestroyNotify alloc_line_notify;
};

static GstLineCache *
gst_line_cache_new (GstLineCache * prev)
{
  GstLineCache *result;

  result = g_new0 (GstLineCache, 1);
  result->lines = g_ptr_array_new ();
  result->prev = prev;

  return result;
}

static void
gst_line_cache_clear (GstLineCache * cache)
{
  g_return_if_fail (cache != NULL);

  g_ptr_array_set_size (cache->lines, 0);
  cache->first = 0;
}

static void
gst_line_cache_free (GstLineCache * cache)
{
  if (cache->need_line_notify)
    cache->need_line_notify (cache->need_line_data);
  if (cache->alloc_line_notify)
    cache->alloc_line_notify (cache->alloc_line_data);
  gst_line_cache_clear (cache);
  g_ptr_array_unref (cache->lines);
  g_free (cache);
}

static void
gst_line_cache_set_need_line_func (GstLineCache * cache,
    GstLineCacheNeedLineFunc need_line, gint idx, gpointer user_data,
    GDestroyNotify notify)
{
  cache->need_line = need_line;
  cache->need_line_idx = idx;
  cache->need_line_data = user_data;
  cache->need_line_notify = notify;
}

static void
gst_line_cache_set_alloc_line_func (GstLineCache * cache,
    GstLineCacheAllocLineFunc alloc_line, gpointer user_data,
    GDestroyNotify notify)
{
  cache->alloc_line = alloc_line;
  cache->alloc_line_data = user_data;
  cache->alloc_line_notify = notify;
}

/* keep this much backlog for interlaced video */
#define BACKLOG 2

static gpointer *
gst_line_cache_get_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gint n_lines)
{
  if (cache->first + cache->backlog < in_line) {
    gint to_remove =
        MIN (in_line - (cache->first + cache->backlog), cache->lines->len);
    if (to_remove > 0) {
      g_ptr_array_remove_range (cache->lines, 0, to_remove);
    }
    cache->first += to_remove;
  } else if (in_line < cache->first) {
    gst_line_cache_clear (cache);
    cache->first = in_line;
  }

  while (TRUE) {
    gint oline;

    if (cache->first <= in_line
        && in_line + n_lines <= cache->first + (gint) cache->lines->len) {
      return cache->lines->pdata + (in_line - cache->first);
    }

    if (cache->need_line == NULL)
      break;

    /* We may be able to skip ahead to the earliest line needed */
    if (cache->lines->len == 0 && cache->first + cache->backlog < in_line)
      cache->first = in_line - cache->backlog;

    oline = out_line + cache->first + cache->lines->len - in_line;

    if (!cache->need_line (cache, idx, oline, cache->first + cache->lines->len,
            cache->need_line_data))
      break;
  }
  GST_LOG ("no lines");
  return NULL;
}

static void
gst_line_cache_add_line (GstLineCache * cache, gint idx, gpointer line)
{
  if (cache->first + cache->lines->len != idx) {
    gst_line_cache_clear (cache);
    cache->first = idx;
  }
  g_ptr_array_add (cache->lines, line);
}

static gpointer
gst_line_cache_alloc_line (GstLineCache * cache, gint idx)
{
  gpointer res;

  if (cache->alloc_line)
    res = cache->alloc_line (cache, idx, cache->alloc_line_data);
  else
    res = NULL;

  return res;
}

static void video_converter_generic (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest);
static gboolean video_converter_lookup_fastpath (GstVideoConverter * convert);
static void video_converter_compute_matrix (GstVideoConverter * convert);
static void video_converter_compute_resample (GstVideoConverter * convert,
    gint idx);

static gpointer get_dest_line (GstLineCache * cache, gint idx,
    gpointer user_data);

static gboolean do_unpack_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);
static gboolean do_downsample_lines (GstLineCache * cache, gint idx,
    gint out_line, gint in_line, gpointer user_data);
static gboolean do_convert_to_RGB_lines (GstLineCache * cache, gint idx,
    gint out_line, gint in_line, gpointer user_data);
static gboolean do_convert_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);
static gboolean do_alpha_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);
static gboolean do_convert_to_YUV_lines (GstLineCache * cache, gint idx,
    gint out_line, gint in_line, gpointer user_data);
static gboolean do_upsample_lines (GstLineCache * cache, gint idx,
    gint out_line, gint in_line, gpointer user_data);
static gboolean do_vscale_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);
static gboolean do_hscale_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);
static gboolean do_dither_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data);

static ConverterAlloc *
converter_alloc_new (guint stride, guint n_lines, gpointer user_data,
    GDestroyNotify notify)
{
  ConverterAlloc *alloc;

  GST_LOG ("stride %d, n_lines %d", stride, n_lines);
  alloc = g_new0 (ConverterAlloc, 1);
  alloc->data = g_malloc (stride * n_lines);
  alloc->stride = stride;
  alloc->n_lines = n_lines;
  alloc->idx = 0;
  alloc->user_data = user_data;
  alloc->notify = notify;

  return alloc;
}

static void
converter_alloc_free (ConverterAlloc * alloc)
{
  if (alloc->notify)
    alloc->notify (alloc->user_data);
  g_free (alloc->data);
  g_free (alloc);
}

static void
setup_border_alloc (GstVideoConverter * convert, ConverterAlloc * alloc)
{
  gint i;

  if (convert->borderline) {
    for (i = 0; i < alloc->n_lines; i++)
      memcpy (&alloc->data[i * alloc->stride], convert->borderline,
          alloc->stride);
  }
}

static gpointer
get_temp_line (GstLineCache * cache, gint idx, gpointer user_data)
{
  ConverterAlloc *alloc = user_data;
  gpointer tmpline;

  GST_LOG ("get temp line %d (%p %d)", idx, alloc, alloc->idx);
  tmpline = &alloc->data[alloc->stride * alloc->idx];
  alloc->idx = (alloc->idx + 1) % alloc->n_lines;

  return tmpline;
}

static gpointer
get_border_temp_line (GstLineCache * cache, gint idx, gpointer user_data)
{
  ConverterAlloc *alloc = user_data;
  GstVideoConverter *convert = alloc->user_data;
  gpointer tmpline;

  GST_LOG ("get temp line %d (%p %d)", idx, alloc, alloc->idx);
  tmpline = &alloc->data[alloc->stride * alloc->idx] +
      (convert->out_x * convert->pack_pstride);
  alloc->idx = (alloc->idx + 1) % alloc->n_lines;

  return tmpline;
}

static gint
get_opt_int (GstVideoConverter * convert, const gchar * opt, gint def)
{
  gint res;
  if (!gst_structure_get_int (convert->config, opt, &res))
    res = def;
  return res;
}

static guint
get_opt_uint (GstVideoConverter * convert, const gchar * opt, guint def)
{
  guint res;
  if (!gst_structure_get_uint (convert->config, opt, &res))
    res = def;
  return res;
}

static gdouble
get_opt_double (GstVideoConverter * convert, const gchar * opt, gdouble def)
{
  gdouble res;
  if (!gst_structure_get_double (convert->config, opt, &res))
    res = def;
  return res;
}

static gboolean
get_opt_bool (GstVideoConverter * convert, const gchar * opt, gboolean def)
{
  gboolean res;
  if (!gst_structure_get_boolean (convert->config, opt, &res))
    res = def;
  return res;
}

static gint
get_opt_enum (GstVideoConverter * convert, const gchar * opt, GType type,
    gint def)
{
  gint res;
  if (!gst_structure_get_enum (convert->config, opt, type, &res))
    res = def;
  return res;
}

#define DEFAULT_OPT_FILL_BORDER TRUE
#define DEFAULT_OPT_ALPHA_VALUE 1.0
/* options copy, set, mult */
#define DEFAULT_OPT_ALPHA_MODE GST_VIDEO_ALPHA_MODE_COPY
#define DEFAULT_OPT_BORDER_ARGB 0xff000000
/* options full, input-only, output-only, none */
#define DEFAULT_OPT_MATRIX_MODE GST_VIDEO_MATRIX_MODE_FULL
/* none, remap */
#define DEFAULT_OPT_GAMMA_MODE GST_VIDEO_GAMMA_MODE_NONE
/* none, merge-only, fast */
#define DEFAULT_OPT_PRIMARIES_MODE GST_VIDEO_PRIMARIES_MODE_NONE
/* options full, upsample-only, downsample-only, none */
#define DEFAULT_OPT_CHROMA_MODE GST_VIDEO_CHROMA_MODE_FULL
#define DEFAULT_OPT_RESAMPLER_METHOD GST_VIDEO_RESAMPLER_METHOD_CUBIC
#define DEFAULT_OPT_CHROMA_RESAMPLER_METHOD GST_VIDEO_RESAMPLER_METHOD_LINEAR
#define DEFAULT_OPT_RESAMPLER_TAPS 0
#define DEFAULT_OPT_DITHER_METHOD GST_VIDEO_DITHER_BAYER
#define DEFAULT_OPT_DITHER_QUANTIZATION 1
#define DEFAULT_OPT_ASYNC_TASKS FALSE

#define GET_OPT_FILL_BORDER(c) get_opt_bool(c, \
    GST_VIDEO_CONVERTER_OPT_FILL_BORDER, DEFAULT_OPT_FILL_BORDER)
#define GET_OPT_ALPHA_VALUE(c) get_opt_double(c, \
    GST_VIDEO_CONVERTER_OPT_ALPHA_VALUE, DEFAULT_OPT_ALPHA_VALUE)
#define GET_OPT_ALPHA_MODE(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_ALPHA_MODE, GST_TYPE_VIDEO_ALPHA_MODE, DEFAULT_OPT_ALPHA_MODE)
#define GET_OPT_BORDER_ARGB(c) get_opt_uint(c, \
    GST_VIDEO_CONVERTER_OPT_BORDER_ARGB, DEFAULT_OPT_BORDER_ARGB)
#define GET_OPT_MATRIX_MODE(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_MATRIX_MODE, GST_TYPE_VIDEO_MATRIX_MODE, DEFAULT_OPT_MATRIX_MODE)
#define GET_OPT_GAMMA_MODE(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_GAMMA_MODE, GST_TYPE_VIDEO_GAMMA_MODE, DEFAULT_OPT_GAMMA_MODE)
#define GET_OPT_PRIMARIES_MODE(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_PRIMARIES_MODE, GST_TYPE_VIDEO_PRIMARIES_MODE, DEFAULT_OPT_PRIMARIES_MODE)
#define GET_OPT_CHROMA_MODE(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_CHROMA_MODE, GST_TYPE_VIDEO_CHROMA_MODE, DEFAULT_OPT_CHROMA_MODE)
#define GET_OPT_RESAMPLER_METHOD(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_RESAMPLER_METHOD, GST_TYPE_VIDEO_RESAMPLER_METHOD, \
    DEFAULT_OPT_RESAMPLER_METHOD)
#define GET_OPT_CHROMA_RESAMPLER_METHOD(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_CHROMA_RESAMPLER_METHOD, GST_TYPE_VIDEO_RESAMPLER_METHOD, \
    DEFAULT_OPT_CHROMA_RESAMPLER_METHOD)
#define GET_OPT_RESAMPLER_TAPS(c) get_opt_uint(c, \
    GST_VIDEO_CONVERTER_OPT_RESAMPLER_TAPS, DEFAULT_OPT_RESAMPLER_TAPS)
#define GET_OPT_DITHER_METHOD(c) get_opt_enum(c, \
    GST_VIDEO_CONVERTER_OPT_DITHER_METHOD, GST_TYPE_VIDEO_DITHER_METHOD, \
    DEFAULT_OPT_DITHER_METHOD)
#define GET_OPT_DITHER_QUANTIZATION(c) get_opt_uint(c, \
    GST_VIDEO_CONVERTER_OPT_DITHER_QUANTIZATION, DEFAULT_OPT_DITHER_QUANTIZATION)
#define GET_OPT_ASYNC_TASKS(c) get_opt_bool(c, \
    GST_VIDEO_CONVERTER_OPT_ASYNC_TASKS, DEFAULT_OPT_ASYNC_TASKS)

#define CHECK_ALPHA_COPY(c) (GET_OPT_ALPHA_MODE(c) == GST_VIDEO_ALPHA_MODE_COPY)
#define CHECK_ALPHA_SET(c) (GET_OPT_ALPHA_MODE(c) == GST_VIDEO_ALPHA_MODE_SET)
#define CHECK_ALPHA_MULT(c) (GET_OPT_ALPHA_MODE(c) == GST_VIDEO_ALPHA_MODE_MULT)

#define CHECK_MATRIX_FULL(c) (GET_OPT_MATRIX_MODE(c) == GST_VIDEO_MATRIX_MODE_FULL)
#define CHECK_MATRIX_INPUT(c) (GET_OPT_MATRIX_MODE(c) == GST_VIDEO_MATRIX_MODE_INPUT_ONLY)
#define CHECK_MATRIX_OUTPUT(c) (GET_OPT_MATRIX_MODE(c) == GST_VIDEO_MATRIX_MODE_OUTPUT_ONLY)
#define CHECK_MATRIX_NONE(c) (GET_OPT_MATRIX_MODE(c) == GST_VIDEO_MATRIX_MODE_NONE)

#define CHECK_GAMMA_NONE(c) (GET_OPT_GAMMA_MODE(c) == GST_VIDEO_GAMMA_MODE_NONE)
#define CHECK_GAMMA_REMAP(c) (GET_OPT_GAMMA_MODE(c) == GST_VIDEO_GAMMA_MODE_REMAP)

#define CHECK_PRIMARIES_NONE(c) (GET_OPT_PRIMARIES_MODE(c) == GST_VIDEO_PRIMARIES_MODE_NONE)
#define CHECK_PRIMARIES_MERGE(c) (GET_OPT_PRIMARIES_MODE(c) == GST_VIDEO_PRIMARIES_MODE_MERGE_ONLY)
#define CHECK_PRIMARIES_FAST(c) (GET_OPT_PRIMARIES_MODE(c) == GST_VIDEO_PRIMARIES_MODE_FAST)

#define CHECK_CHROMA_FULL(c) (GET_OPT_CHROMA_MODE(c) == GST_VIDEO_CHROMA_MODE_FULL)
#define CHECK_CHROMA_UPSAMPLE(c) (GET_OPT_CHROMA_MODE(c) == GST_VIDEO_CHROMA_MODE_UPSAMPLE_ONLY)
#define CHECK_CHROMA_DOWNSAMPLE(c) (GET_OPT_CHROMA_MODE(c) == GST_VIDEO_CHROMA_MODE_DOWNSAMPLE_ONLY)
#define CHECK_CHROMA_NONE(c) (GET_OPT_CHROMA_MODE(c) == GST_VIDEO_CHROMA_MODE_NONE)

static GstLineCache *
chain_unpack_line (GstVideoConverter * convert, gint idx)
{
  GstLineCache *prev;
  GstVideoInfo *info;

  info = &convert->in_info;

  convert->current_format = convert->unpack_format;
  convert->current_bits = convert->unpack_bits;
  convert->current_pstride = convert->current_bits >> 1;

  convert->unpack_pstride = convert->current_pstride;
  convert->identity_unpack = (convert->current_format == info->finfo->format);

  GST_LOG ("chain unpack line format %s, pstride %d, identity_unpack %d",
      gst_video_format_to_string (convert->current_format),
      convert->current_pstride, convert->identity_unpack);

  prev = convert->unpack_lines[idx] = gst_line_cache_new (NULL);
  prev->write_input = FALSE;
  prev->pass_alloc = FALSE;
  prev->n_lines = 1;
  prev->stride = convert->current_pstride * convert->current_width;
  gst_line_cache_set_need_line_func (prev, do_unpack_lines, idx, convert, NULL);

  return prev;
}

static GstLineCache *
chain_upsample (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  video_converter_compute_resample (convert, idx);

  if (convert->upsample_p[idx] || convert->upsample_i[idx]) {
    GST_LOG ("chain upsample");
    prev = convert->upsample_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = TRUE;
    prev->pass_alloc = TRUE;
    /* XXX: why this hardcoded value? */
    prev->n_lines = 5;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev,
        do_upsample_lines, idx, convert, NULL);
  }
  return prev;
}

static void
color_matrix_set_identity (MatrixData * m)
{
  int i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      m->dm[i][j] = (i == j);
    }
  }
}

static void
color_matrix_copy (MatrixData * d, const MatrixData * s)
{
  gint i, j;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      d->dm[i][j] = s->dm[i][j];
}

/* Perform 4x4 matrix multiplication:
 *  - @dst@ = @a@ * @b@
 *  - @dst@ may be a pointer to @a@ andor @b@
 */
static void
color_matrix_multiply (MatrixData * dst, MatrixData * a, MatrixData * b)
{
  MatrixData tmp;
  int i, j, k;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      double x = 0;
      for (k = 0; k < 4; k++) {
        x += a->dm[i][k] * b->dm[k][j];
      }
      tmp.dm[i][j] = x;
    }
  }
  color_matrix_copy (dst, &tmp);
}

static void
color_matrix_invert (MatrixData * d, MatrixData * s)
{
  MatrixData tmp;
  int i, j;
  double det;

  color_matrix_set_identity (&tmp);
  for (j = 0; j < 3; j++) {
    for (i = 0; i < 3; i++) {
      tmp.dm[j][i] =
          s->dm[(i + 1) % 3][(j + 1) % 3] * s->dm[(i + 2) % 3][(j + 2) % 3] -
          s->dm[(i + 1) % 3][(j + 2) % 3] * s->dm[(i + 2) % 3][(j + 1) % 3];
    }
  }
  det =
      tmp.dm[0][0] * s->dm[0][0] + tmp.dm[0][1] * s->dm[1][0] +
      tmp.dm[0][2] * s->dm[2][0];
  for (j = 0; j < 3; j++) {
    for (i = 0; i < 3; i++) {
      tmp.dm[i][j] /= det;
    }
  }
  color_matrix_copy (d, &tmp);
}

static void
color_matrix_offset_components (MatrixData * m, double a1, double a2, double a3)
{
  MatrixData a;

  color_matrix_set_identity (&a);
  a.dm[0][3] = a1;
  a.dm[1][3] = a2;
  a.dm[2][3] = a3;
  color_matrix_multiply (m, &a, m);
}

static void
color_matrix_scale_components (MatrixData * m, double a1, double a2, double a3)
{
  MatrixData a;

  color_matrix_set_identity (&a);
  a.dm[0][0] = a1;
  a.dm[1][1] = a2;
  a.dm[2][2] = a3;
  color_matrix_multiply (m, &a, m);
}

static void
color_matrix_debug (const MatrixData * s)
{
  GST_LOG ("[%f %f %f %f]", s->dm[0][0], s->dm[0][1], s->dm[0][2], s->dm[0][3]);
  GST_LOG ("[%f %f %f %f]", s->dm[1][0], s->dm[1][1], s->dm[1][2], s->dm[1][3]);
  GST_LOG ("[%f %f %f %f]", s->dm[2][0], s->dm[2][1], s->dm[2][2], s->dm[2][3]);
  GST_LOG ("[%f %f %f %f]", s->dm[3][0], s->dm[3][1], s->dm[3][2], s->dm[3][3]);
}

static void
color_matrix_convert (MatrixData * s)
{
  gint i, j;

  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      s->im[i][j] = rint (s->dm[i][j]);

  GST_LOG ("[%6d %6d %6d %6d]", s->im[0][0], s->im[0][1], s->im[0][2],
      s->im[0][3]);
  GST_LOG ("[%6d %6d %6d %6d]", s->im[1][0], s->im[1][1], s->im[1][2],
      s->im[1][3]);
  GST_LOG ("[%6d %6d %6d %6d]", s->im[2][0], s->im[2][1], s->im[2][2],
      s->im[2][3]);
  GST_LOG ("[%6d %6d %6d %6d]", s->im[3][0], s->im[3][1], s->im[3][2],
      s->im[3][3]);
}

static void
color_matrix_YCbCr_to_RGB (MatrixData * m, double Kr, double Kb)
{
  double Kg = 1.0 - Kr - Kb;
  MatrixData k = {
    {
          {1., 0., 2 * (1 - Kr), 0.},
          {1., -2 * Kb * (1 - Kb) / Kg, -2 * Kr * (1 - Kr) / Kg, 0.},
          {1., 2 * (1 - Kb), 0., 0.},
          {0., 0., 0., 1.},
        }
  };

  color_matrix_multiply (m, &k, m);
}

static void
color_matrix_RGB_to_YCbCr (MatrixData * m, double Kr, double Kb)
{
  double Kg = 1.0 - Kr - Kb;
  MatrixData k;
  double x;

  k.dm[0][0] = Kr;
  k.dm[0][1] = Kg;
  k.dm[0][2] = Kb;
  k.dm[0][3] = 0;

  x = 1 / (2 * (1 - Kb));
  k.dm[1][0] = -x * Kr;
  k.dm[1][1] = -x * Kg;
  k.dm[1][2] = x * (1 - Kb);
  k.dm[1][3] = 0;

  x = 1 / (2 * (1 - Kr));
  k.dm[2][0] = x * (1 - Kr);
  k.dm[2][1] = -x * Kg;
  k.dm[2][2] = -x * Kb;
  k.dm[2][3] = 0;

  k.dm[3][0] = 0;
  k.dm[3][1] = 0;
  k.dm[3][2] = 0;
  k.dm[3][3] = 1;

  color_matrix_multiply (m, &k, m);
}

static void
color_matrix_RGB_to_XYZ (MatrixData * dst, double Rx, double Ry, double Gx,
    double Gy, double Bx, double By, double Wx, double Wy)
{
  MatrixData m, im;
  double sx, sy, sz;
  double wx, wy, wz;

  color_matrix_set_identity (&m);

  m.dm[0][0] = Rx;
  m.dm[1][0] = Ry;
  m.dm[2][0] = (1.0 - Rx - Ry);
  m.dm[0][1] = Gx;
  m.dm[1][1] = Gy;
  m.dm[2][1] = (1.0 - Gx - Gy);
  m.dm[0][2] = Bx;
  m.dm[1][2] = By;
  m.dm[2][2] = (1.0 - Bx - By);

  color_matrix_invert (&im, &m);

  wx = Wx / Wy;
  wy = 1.0;
  wz = (1.0 - Wx - Wy) / Wy;

  sx = im.dm[0][0] * wx + im.dm[0][1] * wy + im.dm[0][2] * wz;
  sy = im.dm[1][0] * wx + im.dm[1][1] * wy + im.dm[1][2] * wz;
  sz = im.dm[2][0] * wx + im.dm[2][1] * wy + im.dm[2][2] * wz;

  m.dm[0][0] *= sx;
  m.dm[1][0] *= sx;
  m.dm[2][0] *= sx;
  m.dm[0][1] *= sy;
  m.dm[1][1] *= sy;
  m.dm[2][1] *= sy;
  m.dm[0][2] *= sz;
  m.dm[1][2] *= sz;
  m.dm[2][2] *= sz;

  color_matrix_copy (dst, &m);
}

static void
videoconvert_convert_init_tables (MatrixData * data)
{
  gint i, j;

  data->t_r = g_new (gint64, 256);
  data->t_g = g_new (gint64, 256);
  data->t_b = g_new (gint64, 256);

  for (i = 0; i < 256; i++) {
    gint64 r = 0, g = 0, b = 0;

    for (j = 0; j < 3; j++) {
      r = (r << 16) + data->im[j][0] * i;
      g = (g << 16) + data->im[j][1] * i;
      b = (b << 16) + data->im[j][2] * i;
    }
    data->t_r[i] = r;
    data->t_g[i] = g;
    data->t_b[i] = b;
  }
  data->t_c = ((gint64) data->im[0][3] << 32)
      + ((gint64) data->im[1][3] << 16)
      + ((gint64) data->im[2][3] << 0);
}

void
_custom_video_orc_matrix8 (guint8 * ORC_RESTRICT d1,
    const guint8 * ORC_RESTRICT s1, orc_int64 p1, orc_int64 p2, orc_int64 p3,
    orc_int64 p4, int n)
{
  gint i;
  gint r, g, b;
  gint y, u, v;
  gint a00, a01, a02, a03;
  gint a10, a11, a12, a13;
  gint a20, a21, a22, a23;

  a00 = (gint16) (p1 >> 16);
  a01 = (gint16) (p2 >> 16);
  a02 = (gint16) (p3 >> 16);
  a03 = (gint16) (p4 >> 16);
  a10 = (gint16) (p1 >> 32);
  a11 = (gint16) (p2 >> 32);
  a12 = (gint16) (p3 >> 32);
  a13 = (gint16) (p4 >> 32);
  a20 = (gint16) (p1 >> 48);
  a21 = (gint16) (p2 >> 48);
  a22 = (gint16) (p3 >> 48);
  a23 = (gint16) (p4 >> 48);

  for (i = 0; i < n; i++) {
    r = s1[i * 4 + 1];
    g = s1[i * 4 + 2];
    b = s1[i * 4 + 3];

    y = ((a00 * r + a01 * g + a02 * b) >> SCALE) + a03;
    u = ((a10 * r + a11 * g + a12 * b) >> SCALE) + a13;
    v = ((a20 * r + a21 * g + a22 * b) >> SCALE) + a23;

    d1[i * 4 + 1] = CLAMP (y, 0, 255);
    d1[i * 4 + 2] = CLAMP (u, 0, 255);
    d1[i * 4 + 3] = CLAMP (v, 0, 255);
  }
}

static void
video_converter_matrix8 (MatrixData * data, gpointer pixels)
{
  gpointer d = pixels;
  video_orc_matrix8 (d, pixels, data->orc_p1, data->orc_p2,
      data->orc_p3, data->orc_p4, data->width);
}

static void
video_converter_matrix8_table (MatrixData * data, gpointer pixels)
{
  gint i, width = data->width * 4;
  guint8 r, g, b;
  gint64 c = data->t_c;
  guint8 *p = pixels;
  gint64 x;

  for (i = 0; i < width; i += 4) {
    r = p[i + 1];
    g = p[i + 2];
    b = p[i + 3];

    x = data->t_r[r] + data->t_g[g] + data->t_b[b] + c;

    p[i + 1] = x >> (32 + SCALE);
    p[i + 2] = x >> (16 + SCALE);
    p[i + 3] = x >> (0 + SCALE);
  }
}

static void
video_converter_matrix8_AYUV_ARGB (MatrixData * data, gpointer pixels)
{
  gpointer d = pixels;

  video_orc_convert_AYUV_ARGB (d, 0, pixels, 0,
      data->im[0][0], data->im[0][2],
      data->im[2][1], data->im[1][1], data->im[1][2], data->width, 1);
}

static gboolean
is_ayuv_to_rgb_matrix (MatrixData * data)
{
  if (data->im[0][0] != data->im[1][0] || data->im[1][0] != data->im[2][0])
    return FALSE;

  if (data->im[0][1] != 0 || data->im[2][2] != 0)
    return FALSE;

  return TRUE;
}

static gboolean
is_identity_matrix (MatrixData * data)
{
  gint i, j;
  gint c = data->im[0][0];

  /* not really checking identity because of rounding errors but given
   * the conversions we do we just check for anything that looks like:
   *
   *  c 0 0 0
   *  0 c 0 0
   *  0 0 c 0
   *  0 0 0 1
   */
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (i == j) {
        if (i == 3 && data->im[i][j] != 1)
          return FALSE;
        else if (data->im[i][j] != c)
          return FALSE;
      } else if (data->im[i][j] != 0)
        return FALSE;
    }
  }
  return TRUE;
}

static gboolean
is_no_clip_matrix (MatrixData * data)
{
  gint i;
  static const guint8 test[8][3] = {
    {0, 0, 0},
    {0, 0, 255},
    {0, 255, 0},
    {0, 255, 255},
    {255, 0, 0},
    {255, 0, 255},
    {255, 255, 0},
    {255, 255, 255}
  };

  for (i = 0; i < 8; i++) {
    gint r, g, b;
    gint y, u, v;

    r = test[i][0];
    g = test[i][1];
    b = test[i][2];

    y = (data->im[0][0] * r + data->im[0][1] * g +
        data->im[0][2] * b + data->im[0][3]) >> SCALE;
    u = (data->im[1][0] * r + data->im[1][1] * g +
        data->im[1][2] * b + data->im[1][3]) >> SCALE;
    v = (data->im[2][0] * r + data->im[2][1] * g +
        data->im[2][2] * b + data->im[2][3]) >> SCALE;

    if (y != CLAMP (y, 0, 255) || u != CLAMP (u, 0, 255)
        || v != CLAMP (v, 0, 255))
      return FALSE;
  }
  return TRUE;
}

static void
video_converter_matrix16 (MatrixData * data, gpointer pixels)
{
  int i;
  int r, g, b;
  int y, u, v;
  guint16 *p = pixels;
  gint width = data->width;

  for (i = 0; i < width; i++) {
    r = p[i * 4 + 1];
    g = p[i * 4 + 2];
    b = p[i * 4 + 3];

    y = (data->im[0][0] * r + data->im[0][1] * g +
        data->im[0][2] * b + data->im[0][3]) >> SCALE;
    u = (data->im[1][0] * r + data->im[1][1] * g +
        data->im[1][2] * b + data->im[1][3]) >> SCALE;
    v = (data->im[2][0] * r + data->im[2][1] * g +
        data->im[2][2] * b + data->im[2][3]) >> SCALE;

    p[i * 4 + 1] = CLAMP (y, 0, 65535);
    p[i * 4 + 2] = CLAMP (u, 0, 65535);
    p[i * 4 + 3] = CLAMP (v, 0, 65535);
  }
}


static void
prepare_matrix (GstVideoConverter * convert, MatrixData * data)
{
  if (is_identity_matrix (data))
    return;

  color_matrix_scale_components (data, SCALE_F, SCALE_F, SCALE_F);
  color_matrix_convert (data);

  data->width = convert->current_width;

  if (convert->current_bits == 8) {
    if (!convert->unpack_rgb && convert->pack_rgb
        && is_ayuv_to_rgb_matrix (data)) {
      GST_LOG ("use fast AYUV -> RGB matrix");
      data->matrix_func = video_converter_matrix8_AYUV_ARGB;
    } else if (is_no_clip_matrix (data)) {
      GST_LOG ("use 8bit table");
      data->matrix_func = video_converter_matrix8_table;
      videoconvert_convert_init_tables (data);
    } else {
      gint a03, a13, a23;

      GST_LOG ("use 8bit matrix");
      data->matrix_func = video_converter_matrix8;

      data->orc_p1 = (((guint64) (guint16) data->im[2][0]) << 48) |
          (((guint64) (guint16) data->im[1][0]) << 32) |
          (((guint64) (guint16) data->im[0][0]) << 16);
      data->orc_p2 = (((guint64) (guint16) data->im[2][1]) << 48) |
          (((guint64) (guint16) data->im[1][1]) << 32) |
          (((guint64) (guint16) data->im[0][1]) << 16);
      data->orc_p3 = (((guint64) (guint16) data->im[2][2]) << 48) |
          (((guint64) (guint16) data->im[1][2]) << 32) |
          (((guint64) (guint16) data->im[0][2]) << 16);

      a03 = data->im[0][3] >> SCALE;
      a13 = data->im[1][3] >> SCALE;
      a23 = data->im[2][3] >> SCALE;

      data->orc_p4 = (((guint64) (guint16) a23) << 48) |
          (((guint64) (guint16) a13) << 32) | (((guint64) (guint16) a03) << 16);
    }
  } else {
    GST_LOG ("use 16bit matrix");
    data->matrix_func = video_converter_matrix16;
  }
}

static void
compute_matrix_to_RGB (GstVideoConverter * convert, MatrixData * data)
{
  GstVideoInfo *info;
  gdouble Kr = 0, Kb = 0;

  info = &convert->in_info;

  {
    const GstVideoFormatInfo *uinfo;
    gint offset[4], scale[4];

    uinfo = gst_video_format_get_info (convert->unpack_format);

    /* bring color components to [0..1.0] range */
    gst_video_color_range_offsets (info->colorimetry.range, uinfo, offset,
        scale);

    color_matrix_offset_components (data, -offset[0], -offset[1], -offset[2]);
    color_matrix_scale_components (data, 1 / ((float) scale[0]),
        1 / ((float) scale[1]), 1 / ((float) scale[2]));
  }

  if (!convert->unpack_rgb && !CHECK_MATRIX_NONE (convert)) {
    if (CHECK_MATRIX_OUTPUT (convert))
      info = &convert->out_info;

    /* bring components to R'G'B' space */
    if (gst_video_color_matrix_get_Kr_Kb (info->colorimetry.matrix, &Kr, &Kb))
      color_matrix_YCbCr_to_RGB (data, Kr, Kb);
  }
  color_matrix_debug (data);
}

static void
compute_matrix_to_YUV (GstVideoConverter * convert, MatrixData * data,
    gboolean force)
{
  GstVideoInfo *info;
  gdouble Kr = 0, Kb = 0;

  if (force || (!convert->pack_rgb && !CHECK_MATRIX_NONE (convert))) {
    if (CHECK_MATRIX_INPUT (convert))
      info = &convert->in_info;
    else
      info = &convert->out_info;

    /* bring components to YCbCr space */
    if (gst_video_color_matrix_get_Kr_Kb (info->colorimetry.matrix, &Kr, &Kb))
      color_matrix_RGB_to_YCbCr (data, Kr, Kb);
  }

  info = &convert->out_info;

  {
    const GstVideoFormatInfo *uinfo;
    gint offset[4], scale[4];

    uinfo = gst_video_format_get_info (convert->pack_format);

    /* bring color components to nominal range */
    gst_video_color_range_offsets (info->colorimetry.range, uinfo, offset,
        scale);

    color_matrix_scale_components (data, (float) scale[0], (float) scale[1],
        (float) scale[2]);
    color_matrix_offset_components (data, offset[0], offset[1], offset[2]);
  }

  color_matrix_debug (data);
}


static void
gamma_convert_u8_u16 (GammaData * data, gpointer dest, gpointer src)
{
  gint i;
  guint8 *s = src;
  guint16 *d = dest;
  guint16 *table = data->gamma_table;
  gint width = data->width * 4;

  for (i = 0; i < width; i += 4) {
    d[i + 0] = (s[i] << 8) | s[i];
    d[i + 1] = table[s[i + 1]];
    d[i + 2] = table[s[i + 2]];
    d[i + 3] = table[s[i + 3]];
  }
}

static void
gamma_convert_u16_u8 (GammaData * data, gpointer dest, gpointer src)
{
  gint i;
  guint16 *s = src;
  guint8 *d = dest;
  guint8 *table = data->gamma_table;
  gint width = data->width * 4;

  for (i = 0; i < width; i += 4) {
    d[i + 0] = s[i] >> 8;
    d[i + 1] = table[s[i + 1]];
    d[i + 2] = table[s[i + 2]];
    d[i + 3] = table[s[i + 3]];
  }
}

static void
gamma_convert_u16_u16 (GammaData * data, gpointer dest, gpointer src)
{
  gint i;
  guint16 *s = src;
  guint16 *d = dest;
  guint16 *table = data->gamma_table;
  gint width = data->width * 4;

  for (i = 0; i < width; i += 4) {
    d[i + 0] = s[i];
    d[i + 1] = table[s[i + 1]];
    d[i + 2] = table[s[i + 2]];
    d[i + 3] = table[s[i + 3]];
  }
}

static void
setup_gamma_decode (GstVideoConverter * convert)
{
  GstVideoTransferFunction func;
  guint16 *t;
  gint i;

  func = convert->in_info.colorimetry.transfer;

  convert->gamma_dec.width = convert->current_width;
  if (convert->gamma_dec.gamma_table) {
    GST_LOG ("gamma decode already set up");
  } else if (convert->current_bits == 8) {
    GST_LOG ("gamma decode 8->16: %d", func);
    convert->gamma_dec.gamma_func = gamma_convert_u8_u16;
    t = convert->gamma_dec.gamma_table = g_malloc (sizeof (guint16) * 256);

    for (i = 0; i < 256; i++)
      t[i] =
          rint (gst_video_transfer_function_decode (func, i / 255.0) * 65535.0);
  } else {
    GST_LOG ("gamma decode 16->16: %d", func);
    convert->gamma_dec.gamma_func = gamma_convert_u16_u16;
    t = convert->gamma_dec.gamma_table = g_malloc (sizeof (guint16) * 65536);

    for (i = 0; i < 65536; i++)
      t[i] =
          rint (gst_video_transfer_function_decode (func,
              i / 65535.0) * 65535.0);
  }
  convert->current_bits = 16;
  convert->current_pstride = 8;
  convert->current_format = GST_VIDEO_FORMAT_ARGB64;
}

static void
setup_gamma_encode (GstVideoConverter * convert, gint target_bits)
{
  GstVideoTransferFunction func;
  gint i;

  func = convert->out_info.colorimetry.transfer;

  convert->gamma_enc.width = convert->current_width;
  if (convert->gamma_enc.gamma_table) {
    GST_LOG ("gamma encode already set up");
  } else if (target_bits == 8) {
    guint8 *t;

    GST_LOG ("gamma encode 16->8: %d", func);
    convert->gamma_enc.gamma_func = gamma_convert_u16_u8;
    t = convert->gamma_enc.gamma_table = g_malloc (sizeof (guint8) * 65536);

    for (i = 0; i < 65536; i++)
      t[i] =
          rint (gst_video_transfer_function_encode (func, i / 65535.0) * 255.0);
  } else {
    guint16 *t;

    GST_LOG ("gamma encode 16->16: %d", func);
    convert->gamma_enc.gamma_func = gamma_convert_u16_u16;
    t = convert->gamma_enc.gamma_table = g_malloc (sizeof (guint16) * 65536);

    for (i = 0; i < 65536; i++)
      t[i] =
          rint (gst_video_transfer_function_encode (func,
              i / 65535.0) * 65535.0);
  }
}

static GstLineCache *
chain_convert_to_RGB (GstVideoConverter * convert, GstLineCache * prev,
    gint idx)
{
  gboolean do_gamma;

  do_gamma = CHECK_GAMMA_REMAP (convert);

  if (do_gamma) {
    gint scale;

    /* Set up conversion matrices if needed, but only for the first thread */
    if (idx == 0 && !convert->unpack_rgb) {
      color_matrix_set_identity (&convert->to_RGB_matrix);
      compute_matrix_to_RGB (convert, &convert->to_RGB_matrix);

      /* matrix is in 0..1 range, scale to current bits */
      GST_LOG ("chain RGB convert");
      scale = 1 << convert->current_bits;
      color_matrix_scale_components (&convert->to_RGB_matrix,
          (float) scale, (float) scale, (float) scale);

      prepare_matrix (convert, &convert->to_RGB_matrix);

      if (convert->current_bits == 8)
        convert->current_format = GST_VIDEO_FORMAT_ARGB;
      else
        convert->current_format = GST_VIDEO_FORMAT_ARGB64;
    }

    prev = convert->to_RGB_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = TRUE;
    prev->pass_alloc = FALSE;
    prev->n_lines = 1;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev,
        do_convert_to_RGB_lines, idx, convert, NULL);

    GST_LOG ("chain gamma decode");
    setup_gamma_decode (convert);
  }
  return prev;
}

static GstLineCache *
chain_hscale (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  gint method;
  guint taps;

  method = GET_OPT_RESAMPLER_METHOD (convert);
  taps = GET_OPT_RESAMPLER_TAPS (convert);

  convert->h_scaler[idx] =
      gst_video_scaler_new (method, GST_VIDEO_SCALER_FLAG_NONE, taps,
      convert->in_width, convert->out_width, convert->config);

  gst_video_scaler_get_coeff (convert->h_scaler[idx], 0, NULL, &taps);

  GST_LOG ("chain hscale %d->%d, taps %d, method %d",
      convert->in_width, convert->out_width, taps, method);

  convert->current_width = convert->out_width;
  convert->h_scale_format = convert->current_format;

  prev = convert->hscale_lines[idx] = gst_line_cache_new (prev);
  prev->write_input = FALSE;
  prev->pass_alloc = FALSE;
  prev->n_lines = 1;
  prev->stride = convert->current_pstride * convert->current_width;
  gst_line_cache_set_need_line_func (prev, do_hscale_lines, idx, convert, NULL);

  return prev;
}

static GstLineCache *
chain_vscale (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  gint method;
  guint taps, taps_i = 0;
  gint backlog = 0;

  method = GET_OPT_RESAMPLER_METHOD (convert);
  taps = GET_OPT_RESAMPLER_TAPS (convert);

  if (GST_VIDEO_INFO_IS_INTERLACED (&convert->in_info)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&convert->in_info) !=
          GST_VIDEO_INTERLACE_MODE_ALTERNATE)) {
    convert->v_scaler_i[idx] =
        gst_video_scaler_new (method, GST_VIDEO_SCALER_FLAG_INTERLACED, taps,
        convert->in_height, convert->out_height, convert->config);

    gst_video_scaler_get_coeff (convert->v_scaler_i[idx], 0, NULL, &taps_i);
    backlog = taps_i;
  }
  convert->v_scaler_p[idx] =
      gst_video_scaler_new (method, 0, taps, convert->in_height,
      convert->out_height, convert->config);
  convert->v_scale_width = convert->current_width;
  convert->v_scale_format = convert->current_format;
  convert->current_height = convert->out_height;

  gst_video_scaler_get_coeff (convert->v_scaler_p[idx], 0, NULL, &taps);

  GST_LOG ("chain vscale %d->%d, taps %d, method %d, backlog %d",
      convert->in_height, convert->out_height, taps, method, backlog);

  prev->backlog = backlog;
  prev = convert->vscale_lines[idx] = gst_line_cache_new (prev);
  prev->pass_alloc = (taps == 1);
  prev->write_input = FALSE;
  prev->n_lines = MAX (taps_i, taps);
  prev->stride = convert->current_pstride * convert->current_width;
  gst_line_cache_set_need_line_func (prev, do_vscale_lines, idx, convert, NULL);

  return prev;
}

static GstLineCache *
chain_scale (GstVideoConverter * convert, GstLineCache * prev, gboolean force,
    gint idx)
{
  gint s0, s1, s2, s3;

  s0 = convert->current_width * convert->current_height;
  s3 = convert->out_width * convert->out_height;

  GST_LOG ("in pixels %d <> out pixels %d", s0, s3);

  if (s3 <= s0 || force) {
    /* we are making the image smaller or are forced to resample */
    s1 = convert->out_width * convert->current_height;
    s2 = convert->current_width * convert->out_height;

    GST_LOG ("%d <> %d", s1, s2);

    if (s1 <= s2) {
      /* h scaling first produces less pixels */
      if (convert->current_width != convert->out_width)
        prev = chain_hscale (convert, prev, idx);
      if (convert->current_height != convert->out_height)
        prev = chain_vscale (convert, prev, idx);
    } else {
      /* v scaling first produces less pixels */
      if (convert->current_height != convert->out_height)
        prev = chain_vscale (convert, prev, idx);
      if (convert->current_width != convert->out_width)
        prev = chain_hscale (convert, prev, idx);
    }
  }
  return prev;
}

static GstLineCache *
chain_convert (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  gboolean do_gamma, do_conversion, pass_alloc = FALSE;
  gboolean same_matrix, same_primaries, same_bits;
  MatrixData p1, p2;

  same_bits = convert->unpack_bits == convert->pack_bits;
  if (CHECK_MATRIX_NONE (convert)) {
    same_matrix = TRUE;
  } else {
    same_matrix =
        convert->in_info.colorimetry.matrix ==
        convert->out_info.colorimetry.matrix;
  }

  if (CHECK_PRIMARIES_NONE (convert)) {
    same_primaries = TRUE;
  } else {
    same_primaries =
        gst_video_color_primaries_is_equivalent (convert->in_info.
        colorimetry.primaries, convert->out_info.colorimetry.primaries);
  }

  GST_LOG ("matrix %d -> %d (%d)", convert->in_info.colorimetry.matrix,
      convert->out_info.colorimetry.matrix, same_matrix);
  GST_LOG ("bits %d -> %d (%d)", convert->unpack_bits, convert->pack_bits,
      same_bits);
  GST_LOG ("primaries %d -> %d (%d)", convert->in_info.colorimetry.primaries,
      convert->out_info.colorimetry.primaries, same_primaries);

  color_matrix_set_identity (&convert->convert_matrix);

  if (!same_primaries) {
    const GstVideoColorPrimariesInfo *pi;

    /* Convert from RGB_input to RGB_output via XYZ
     *    res = XYZ_to_RGB_output ( RGB_to_XYZ_input ( input ) )
     * or in matricial form:
     *    RGB_output = XYZ_to_RGB_output_matrix * RGB_TO_XYZ_input_matrix * RGB_input
     *
     * The RGB_input is the pre-existing convert_matrix
     * The convert_matrix will become the RGB_output
     */

    /* Convert input RGB to XYZ */
    pi = gst_video_color_primaries_get_info (convert->in_info.colorimetry.
        primaries);
    /* Get the RGB_TO_XYZ_input_matrix */
    color_matrix_RGB_to_XYZ (&p1, pi->Rx, pi->Ry, pi->Gx, pi->Gy, pi->Bx,
        pi->By, pi->Wx, pi->Wy);
    GST_LOG ("to XYZ matrix");
    color_matrix_debug (&p1);
    GST_LOG ("current matrix");
    /* convert_matrix = RGB_TO_XYZ_input_matrix * input_RGB */
    color_matrix_multiply (&convert->convert_matrix, &convert->convert_matrix,
        &p1);
    color_matrix_debug (&convert->convert_matrix);

    /* Convert XYZ to output RGB */
    pi = gst_video_color_primaries_get_info (convert->out_info.colorimetry.
        primaries);
    /* Calculate the XYZ_to_RGB_output_matrix
     *  * Get the RGB_TO_XYZ_output_matrix
     *  * invert it
     *  * store in p2
     */
    color_matrix_RGB_to_XYZ (&p2, pi->Rx, pi->Ry, pi->Gx, pi->Gy, pi->Bx,
        pi->By, pi->Wx, pi->Wy);
    color_matrix_invert (&p2, &p2);
    GST_LOG ("to RGB matrix");
    color_matrix_debug (&p2);
    /* Finally:
     * convert_matrix = XYZ_to_RGB_output_matrix * RGB_TO_XYZ_input_matrix * RGB_input
     *                = XYZ_to_RGB_output_matrix * convert_matrix
     *                = p2 * convert_matrix
     */
    color_matrix_multiply (&convert->convert_matrix, &p2,
        &convert->convert_matrix);
    GST_LOG ("current matrix");
    color_matrix_debug (&convert->convert_matrix);
  }

  do_gamma = CHECK_GAMMA_REMAP (convert);
  if (!do_gamma) {

    convert->in_bits = convert->unpack_bits;
    convert->out_bits = convert->pack_bits;

    if (!same_bits || !same_matrix || !same_primaries) {
      /* no gamma, combine all conversions into 1 */
      if (convert->in_bits < convert->out_bits) {
        gint scale = 1 << (convert->out_bits - convert->in_bits);
        color_matrix_scale_components (&convert->convert_matrix,
            1 / (float) scale, 1 / (float) scale, 1 / (float) scale);
      }
      GST_LOG ("to RGB matrix");
      compute_matrix_to_RGB (convert, &convert->convert_matrix);
      GST_LOG ("current matrix");
      color_matrix_debug (&convert->convert_matrix);

      GST_LOG ("to YUV matrix");
      compute_matrix_to_YUV (convert, &convert->convert_matrix, FALSE);
      GST_LOG ("current matrix");
      color_matrix_debug (&convert->convert_matrix);
      if (convert->in_bits > convert->out_bits) {
        gint scale = 1 << (convert->in_bits - convert->out_bits);
        color_matrix_scale_components (&convert->convert_matrix,
            (float) scale, (float) scale, (float) scale);
      }
      convert->current_bits = MAX (convert->in_bits, convert->out_bits);

      do_conversion = TRUE;
      if (!same_matrix || !same_primaries) {
        if (idx == 0)
          prepare_matrix (convert, &convert->convert_matrix);
      }
      if (convert->in_bits == convert->out_bits)
        pass_alloc = TRUE;
    } else
      do_conversion = FALSE;

    convert->current_bits = convert->pack_bits;
    convert->current_format = convert->pack_format;
    convert->current_pstride = convert->current_bits >> 1;
  } else {
    /* we did gamma, just do colorspace conversion if needed */
    if (same_primaries) {
      do_conversion = FALSE;
    } else {
      if (idx == 0)
        prepare_matrix (convert, &convert->convert_matrix);
      convert->in_bits = convert->out_bits = 16;
      pass_alloc = TRUE;
      do_conversion = TRUE;
    }
  }

  if (do_conversion) {
    GST_LOG ("chain conversion");
    prev = convert->convert_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = TRUE;
    prev->pass_alloc = pass_alloc;
    prev->n_lines = 1;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev,
        do_convert_lines, idx, convert, NULL);
  }
  return prev;
}

static void
convert_set_alpha_u8 (GstVideoConverter * convert, gpointer pixels, gint width)
{
  guint8 *p = pixels;
  guint8 alpha = MIN (convert->alpha_value, 255);
  int i;

  for (i = 0; i < width; i++)
    p[i * 4] = alpha;
}

static void
convert_set_alpha_u16 (GstVideoConverter * convert, gpointer pixels, gint width)
{
  guint16 *p = pixels;
  guint16 alpha;
  int i;

  alpha = MIN (convert->alpha_value, 255);
  alpha |= alpha << 8;

  for (i = 0; i < width; i++)
    p[i * 4] = alpha;
}

static void
convert_mult_alpha_u8 (GstVideoConverter * convert, gpointer pixels, gint width)
{
  guint8 *p = pixels;
  guint alpha = convert->alpha_value;
  int i;

  for (i = 0; i < width; i++) {
    gint a = (p[i * 4] * alpha) / 255;
    p[i * 4] = CLAMP (a, 0, 255);
  }
}

static void
convert_mult_alpha_u16 (GstVideoConverter * convert, gpointer pixels,
    gint width)
{
  guint16 *p = pixels;
  guint alpha = convert->alpha_value;
  int i;

  for (i = 0; i < width; i++) {
    gint a = (p[i * 4] * alpha) / 255;
    p[i * 4] = CLAMP (a, 0, 65535);
  }
}

static GstLineCache *
chain_alpha (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  switch (convert->alpha_mode) {
    case ALPHA_MODE_NONE:
    case ALPHA_MODE_COPY:
      return prev;

    case ALPHA_MODE_SET:
      if (convert->current_bits == 8)
        convert->alpha_func = convert_set_alpha_u8;
      else
        convert->alpha_func = convert_set_alpha_u16;
      break;
    case ALPHA_MODE_MULT:
      if (convert->current_bits == 8)
        convert->alpha_func = convert_mult_alpha_u8;
      else
        convert->alpha_func = convert_mult_alpha_u16;
      break;
  }

  GST_LOG ("chain alpha mode %d", convert->alpha_mode);
  prev = convert->alpha_lines[idx] = gst_line_cache_new (prev);
  prev->write_input = TRUE;
  prev->pass_alloc = TRUE;
  prev->n_lines = 1;
  prev->stride = convert->current_pstride * convert->current_width;
  gst_line_cache_set_need_line_func (prev, do_alpha_lines, idx, convert, NULL);

  return prev;
}

static GstLineCache *
chain_convert_to_YUV (GstVideoConverter * convert, GstLineCache * prev,
    gint idx)
{
  gboolean do_gamma;

  do_gamma = CHECK_GAMMA_REMAP (convert);

  if (do_gamma) {
    gint scale;

    GST_LOG ("chain gamma encode");
    setup_gamma_encode (convert, convert->pack_bits);

    convert->current_bits = convert->pack_bits;
    convert->current_pstride = convert->current_bits >> 1;

    if (idx == 0 && !convert->pack_rgb) {
      color_matrix_set_identity (&convert->to_YUV_matrix);

      /* When gamma remap is enabled, we do
       * 1) converts to ARGB64 linear RGB
       *   - if input is 8bits, convert to ARGB and scaled to 16bits with gamma
       *     decoding at once
       *   - otherwise converted ARGB64 and gamma decoded
       * 2) scale/convert etc,
       * 3) and gamma encode
       *
       * So source data to the do_convert_to_YUV_lines() method is always
       * ARGB64
       *
       * Then, if output unpack format is 8bits, setup_gamma_encode() will scale
       * ARGB64 down to ARGB as a part of gamma encoding, otherwise it's still
       * ARGB64
       *
       * Finally this to_YUV_matrix is applied. Since compute_matrix_to_YUV()
       * expects [0, 1.0] range RGB as an input, scale down identity matrix
       * to expected scale here, otherwise offset of the matrix would be
       * very wrong
       */
      GST_LOG ("chain YUV convert");
      scale = 1 << convert->pack_bits;
      color_matrix_scale_components (&convert->to_YUV_matrix,
          1 / (float) scale, 1 / (float) scale, 1 / (float) scale);

      compute_matrix_to_YUV (convert, &convert->to_YUV_matrix, FALSE);
      prepare_matrix (convert, &convert->to_YUV_matrix);
    }
    convert->current_format = convert->pack_format;

    prev = convert->to_YUV_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = FALSE;
    prev->pass_alloc = FALSE;
    prev->n_lines = 1;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev,
        do_convert_to_YUV_lines, idx, convert, NULL);
  }

  return prev;
}

static GstLineCache *
chain_downsample (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  if (convert->downsample_p[idx] || convert->downsample_i[idx]) {
    GST_LOG ("chain downsample");
    prev = convert->downsample_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = TRUE;
    prev->pass_alloc = TRUE;
    /* XXX: why this hardcoded value? */
    prev->n_lines = 5;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev,
        do_downsample_lines, idx, convert, NULL);
  }
  return prev;
}

static GstLineCache *
chain_dither (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  gint i;
  gboolean do_dither = FALSE;
  GstVideoDitherFlags flags = 0;
  GstVideoDitherMethod method;
  guint quant[4], target_quant;

  method = GET_OPT_DITHER_METHOD (convert);
  if (method == GST_VIDEO_DITHER_NONE)
    return prev;

  target_quant = GET_OPT_DITHER_QUANTIZATION (convert);
  GST_LOG ("method %d, target-quantization %d", method, target_quant);

  if (convert->pack_pal) {
    quant[0] = 47;
    quant[1] = 47;
    quant[2] = 47;
    quant[3] = 1;
    do_dither = TRUE;
  } else {
    for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
      gint depth;

      depth = convert->out_info.finfo->depth[i];

      if (depth == 0) {
        quant[i] = 0;
        continue;
      }

      if (convert->current_bits >= depth) {
        quant[i] = 1 << (convert->current_bits - depth);
        if (target_quant > quant[i]) {
          flags |= GST_VIDEO_DITHER_FLAG_QUANTIZE;
          quant[i] = target_quant;
        }
      } else {
        quant[i] = 0;
      }
      if (quant[i] > 1)
        do_dither = TRUE;
    }
  }

  if (do_dither) {
    GST_LOG ("chain dither");

    convert->dither[idx] = gst_video_dither_new (method,
        flags, convert->pack_format, quant, convert->current_width);

    prev = convert->dither_lines[idx] = gst_line_cache_new (prev);
    prev->write_input = TRUE;
    prev->pass_alloc = TRUE;
    prev->n_lines = 1;
    prev->stride = convert->current_pstride * convert->current_width;
    gst_line_cache_set_need_line_func (prev, do_dither_lines, idx, convert,
        NULL);
  }
  return prev;
}

static GstLineCache *
chain_pack (GstVideoConverter * convert, GstLineCache * prev, gint idx)
{
  convert->pack_nlines = convert->out_info.finfo->pack_lines;
  convert->pack_pstride = convert->current_pstride;
  convert->identity_pack =
      (convert->out_info.finfo->format ==
      convert->out_info.finfo->unpack_format);
  GST_LOG ("chain pack line format %s, pstride %d, identity_pack %d (%d %d)",
      gst_video_format_to_string (convert->current_format),
      convert->current_pstride, convert->identity_pack,
      convert->out_info.finfo->format, convert->out_info.finfo->unpack_format);

  return prev;
}

static void
setup_allocators (GstVideoConverter * convert)
{
  GstLineCache *cache, *prev;
  GstLineCacheAllocLineFunc alloc_line;
  gboolean alloc_writable;
  gpointer user_data;
  GDestroyNotify notify;
  gint width;
  gint i;

  width = MAX (convert->in_maxwidth, convert->out_maxwidth);
  width += convert->out_x;

  for (i = 0; i < convert->conversion_runner->n_threads; i++) {
    /* start with using dest lines if we can directly write into it */
    if (convert->identity_pack) {
      alloc_line = get_dest_line;
      alloc_writable = TRUE;
      user_data = convert;
      notify = NULL;
    } else {
      user_data =
          converter_alloc_new (sizeof (guint16) * width * 4, 4 + BACKLOG,
          convert, NULL);
      setup_border_alloc (convert, user_data);
      notify = (GDestroyNotify) converter_alloc_free;
      alloc_line = get_border_temp_line;
      /* when we add a border, we need to write */
      alloc_writable = convert->borderline != NULL;
    }

    /* First step, try to calculate how many temp lines we need. Go backwards,
     * keep track of the maximum number of lines we need for each intermediate
     * step.  */
    for (prev = cache = convert->pack_lines[i]; cache; cache = cache->prev) {
      GST_LOG ("looking at cache %p, %d lines, %d backlog", cache,
          cache->n_lines, cache->backlog);
      prev->n_lines = MAX (prev->n_lines, cache->n_lines);
      if (!cache->pass_alloc) {
        GST_LOG ("cache %p, needs %d lines", prev, prev->n_lines);
        prev = cache;
      }
    }

    /* now walk backwards, we try to write into the dest lines directly
     * and keep track if the source needs to be writable */
    for (cache = convert->pack_lines[i]; cache; cache = cache->prev) {
      gst_line_cache_set_alloc_line_func (cache, alloc_line, user_data, notify);
      cache->alloc_writable = alloc_writable;

      /* make sure only one cache frees the allocator */
      notify = NULL;

      if (!cache->pass_alloc) {
        /* can't pass allocator, make new temp line allocator */
        user_data =
            converter_alloc_new (sizeof (guint16) * width * 4,
            cache->n_lines + cache->backlog, convert, NULL);
        notify = (GDestroyNotify) converter_alloc_free;
        alloc_line = get_temp_line;
        alloc_writable = FALSE;
      }
      /* if someone writes to the input, we need a writable line from the
       * previous cache */
      if (cache->write_input)
        alloc_writable = TRUE;
    }
    /* free leftover allocator */
    if (notify)
      notify (user_data);
  }
}

static void
setup_borderline (GstVideoConverter * convert)
{
  gint width;

  width = MAX (convert->in_maxwidth, convert->out_maxwidth);
  width += convert->out_x;

  if (convert->fill_border && (convert->out_height < convert->out_maxheight ||
          convert->out_width < convert->out_maxwidth)) {
    guint32 border_val;
    gint i, w_sub;
    const GstVideoFormatInfo *out_finfo;
    gpointer planes[GST_VIDEO_MAX_PLANES];
    gint strides[GST_VIDEO_MAX_PLANES];

    convert->borderline = g_malloc0 (sizeof (guint16) * width * 4);

    out_finfo = convert->out_info.finfo;

    if (GST_VIDEO_INFO_IS_YUV (&convert->out_info)) {
      MatrixData cm;
      gint a, r, g, b;
      gint y, u, v;

      /* Get Color matrix. */
      color_matrix_set_identity (&cm);
      compute_matrix_to_YUV (convert, &cm, TRUE);
      color_matrix_convert (&cm);

      border_val = GINT32_FROM_BE (convert->border_argb);

      b = (0xFF000000 & border_val) >> 24;
      g = (0x00FF0000 & border_val) >> 16;
      r = (0x0000FF00 & border_val) >> 8;
      a = (0x000000FF & border_val);

      y = 16 + ((r * cm.im[0][0] + g * cm.im[0][1] + b * cm.im[0][2]) >> 8);
      u = 128 + ((r * cm.im[1][0] + g * cm.im[1][1] + b * cm.im[1][2]) >> 8);
      v = 128 + ((r * cm.im[2][0] + g * cm.im[2][1] + b * cm.im[2][2]) >> 8);

      a = CLAMP (a, 0, 255);
      y = CLAMP (y, 0, 255);
      u = CLAMP (u, 0, 255);
      v = CLAMP (v, 0, 255);

      border_val = a | (y << 8) | (u << 16) | ((guint32) v << 24);
    } else {
      border_val = GINT32_FROM_BE (convert->border_argb);
    }
    if (convert->pack_bits == 8)
      video_orc_splat_u32 (convert->borderline, border_val, width);
    else
      video_orc_splat2_u64 (convert->borderline, border_val, width);

    /* convert pixels */
    for (i = 0; i < out_finfo->n_planes; i++) {
      planes[i] = &convert->borders[i];
      strides[i] = sizeof (guint64);
    }
    w_sub = 0;
    if (out_finfo->n_planes == 1) {
      /* for packed formats, convert based on subsampling so that we
       * get a complete group of pixels */
      for (i = 0; i < out_finfo->n_components; i++) {
        w_sub = MAX (w_sub, out_finfo->w_sub[i]);
      }
    }
    out_finfo->pack_func (out_finfo, GST_VIDEO_PACK_FLAG_NONE,
        convert->borderline, 0, planes, strides,
        GST_VIDEO_CHROMA_SITE_UNKNOWN, 0, 1 << w_sub);
  } else {
    convert->borderline = NULL;
  }
}

static AlphaMode
convert_get_alpha_mode (GstVideoConverter * convert)
{
  gboolean in_alpha, out_alpha;

  in_alpha = GST_VIDEO_INFO_HAS_ALPHA (&convert->in_info);
  out_alpha = GST_VIDEO_INFO_HAS_ALPHA (&convert->out_info);

  /* no output alpha, do nothing */
  if (!out_alpha)
    return ALPHA_MODE_NONE;

  if (in_alpha) {
    /* in and out */
    if (CHECK_ALPHA_COPY (convert))
      return ALPHA_MODE_COPY;

    if (CHECK_ALPHA_MULT (convert)) {
      if (GET_OPT_ALPHA_VALUE (convert) == 1.0)
        return ALPHA_MODE_COPY;
      else
        return ALPHA_MODE_MULT;
    }
  }
  /* nothing special, this is what unpack etc does automatically */
  if (GET_OPT_ALPHA_VALUE (convert) == 1.0)
    return ALPHA_MODE_NONE;

  /* everything else becomes SET */
  return ALPHA_MODE_SET;
}

/**
 * gst_video_converter_new_with_pool: (skip)
 * @in_info: a #GstVideoInfo
 * @out_info: a #GstVideoInfo
 * @config: (transfer full): a #GstStructure with configuration options
 * @pool: (nullable): a #GstTaskPool to spawn threads from
 *
 * Create a new converter object to convert between @in_info and @out_info
 * with @config.
 *
 * The optional @pool can be used to spawn threads, this is useful when
 * creating new converters rapidly, for example when updating cropping.
 *
 * Returns (nullable): a #GstVideoConverter or %NULL if conversion is not possible.
 *
 * Since: 1.20
 */
GstVideoConverter *
gst_video_converter_new_with_pool (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstStructure * config, GstTaskPool * pool)
{
  GstVideoConverter *convert;
  GstLineCache *prev;
  const GstVideoFormatInfo *fin, *fout, *finfo;
  gdouble alpha_value;
  gint n_threads, i;
  gboolean async_tasks;

  g_return_val_if_fail (in_info != NULL, NULL);
  g_return_val_if_fail (out_info != NULL, NULL);
  /* we won't ever do framerate conversion */
  g_return_val_if_fail (in_info->fps_n == out_info->fps_n, NULL);
  g_return_val_if_fail (in_info->fps_d == out_info->fps_d, NULL);
  /* we won't ever do deinterlace */
  g_return_val_if_fail (in_info->interlace_mode == out_info->interlace_mode,
      NULL);

  convert = g_new0 (GstVideoConverter, 1);

  fin = in_info->finfo;
  fout = out_info->finfo;

  convert->in_info = *in_info;
  convert->out_info = *out_info;

  /* default config */
  convert->config = gst_structure_new_empty ("GstVideoConverter");
  if (config)
    gst_video_converter_set_config (convert, config);

  convert->in_maxwidth = GST_VIDEO_INFO_WIDTH (in_info);
  convert->in_maxheight = GST_VIDEO_INFO_FIELD_HEIGHT (in_info);
  convert->out_maxwidth = GST_VIDEO_INFO_WIDTH (out_info);
  convert->out_maxheight = GST_VIDEO_INFO_FIELD_HEIGHT (out_info);

  convert->in_x = get_opt_int (convert, GST_VIDEO_CONVERTER_OPT_SRC_X, 0);
  convert->in_y = get_opt_int (convert, GST_VIDEO_CONVERTER_OPT_SRC_Y, 0);
  convert->in_x &= ~((1 << fin->w_sub[1]) - 1);
  convert->in_y &= ~((1 << fin->h_sub[1]) - 1);

  convert->in_width = get_opt_int (convert,
      GST_VIDEO_CONVERTER_OPT_SRC_WIDTH, convert->in_maxwidth - convert->in_x);
  convert->in_height = get_opt_int (convert,
      GST_VIDEO_CONVERTER_OPT_SRC_HEIGHT,
      convert->in_maxheight - convert->in_y);

  convert->in_width =
      MIN (convert->in_width, convert->in_maxwidth - convert->in_x);
  if (convert->in_width + convert->in_x < 0 ||
      convert->in_width + convert->in_x > convert->in_maxwidth) {
    convert->in_width = 0;
  }

  convert->in_height =
      MIN (convert->in_height, convert->in_maxheight - convert->in_y);
  if (convert->in_height + convert->in_y < 0 ||
      convert->in_height + convert->in_y > convert->in_maxheight) {
    convert->in_height = 0;
  }

  convert->out_x = get_opt_int (convert, GST_VIDEO_CONVERTER_OPT_DEST_X, 0);
  convert->out_y = get_opt_int (convert, GST_VIDEO_CONVERTER_OPT_DEST_Y, 0);
  convert->out_x &= ~((1 << fout->w_sub[1]) - 1);
  convert->out_y &= ~((1 << fout->h_sub[1]) - 1);

  convert->out_width = get_opt_int (convert,
      GST_VIDEO_CONVERTER_OPT_DEST_WIDTH,
      convert->out_maxwidth - convert->out_x);
  convert->out_height =
      get_opt_int (convert, GST_VIDEO_CONVERTER_OPT_DEST_HEIGHT,
      convert->out_maxheight - convert->out_y);

  if (convert->out_width > convert->out_maxwidth - convert->out_x)
    convert->out_width = convert->out_maxwidth - convert->out_x;
  convert->out_width = CLAMP (convert->out_width, 0, convert->out_maxwidth);

  /* Check if completely outside the framebuffer */
  if (convert->out_width + convert->out_x < 0 ||
      convert->out_width + convert->out_x > convert->out_maxwidth) {
    convert->out_width = 0;
  }

  /* Same for height */
  if (convert->out_height > convert->out_maxheight - convert->out_y)
    convert->out_height = convert->out_maxheight - convert->out_y;
  convert->out_height = CLAMP (convert->out_height, 0, convert->out_maxheight);

  if (convert->out_height + convert->out_y < 0 ||
      convert->out_height + convert->out_y > convert->out_maxheight) {
    convert->out_height = 0;
  }

  convert->fill_border = GET_OPT_FILL_BORDER (convert);
  convert->border_argb = get_opt_uint (convert,
      GST_VIDEO_CONVERTER_OPT_BORDER_ARGB, DEFAULT_OPT_BORDER_ARGB);

  alpha_value = GET_OPT_ALPHA_VALUE (convert);
  convert->alpha_value = 255 * alpha_value;
  convert->alpha_mode = convert_get_alpha_mode (convert);

  convert->unpack_format = in_info->finfo->unpack_format;
  finfo = gst_video_format_get_info (convert->unpack_format);
  convert->unpack_bits = GST_VIDEO_FORMAT_INFO_DEPTH (finfo, 0);
  convert->unpack_rgb = GST_VIDEO_FORMAT_INFO_IS_RGB (finfo);
  if (convert->unpack_rgb
      && in_info->colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB) {
    /* force identity matrix for RGB input */
    GST_WARNING ("invalid matrix %d for input RGB format, using RGB",
        in_info->colorimetry.matrix);
    convert->in_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
  }

  convert->pack_format = out_info->finfo->unpack_format;
  finfo = gst_video_format_get_info (convert->pack_format);
  convert->pack_bits = GST_VIDEO_FORMAT_INFO_DEPTH (finfo, 0);
  convert->pack_rgb = GST_VIDEO_FORMAT_INFO_IS_RGB (finfo);
  convert->pack_pal =
      gst_video_format_get_palette (GST_VIDEO_INFO_FORMAT (out_info),
      &convert->pack_palsize);
  if (convert->pack_rgb
      && out_info->colorimetry.matrix != GST_VIDEO_COLOR_MATRIX_RGB) {
    /* force identity matrix for RGB output */
    GST_WARNING ("invalid matrix %d for output RGB format, using RGB",
        out_info->colorimetry.matrix);
    convert->out_info.colorimetry.matrix = GST_VIDEO_COLOR_MATRIX_RGB;
  }

  n_threads = get_opt_uint (convert, GST_VIDEO_CONVERTER_OPT_THREADS, 1);
  if (n_threads == 0 || n_threads > g_get_num_processors ())
    n_threads = g_get_num_processors ();
  /* Magic number of 200 lines */
  if (MAX (convert->out_height, convert->in_height) / n_threads < 200)
    n_threads = (MAX (convert->out_height, convert->in_height) + 199) / 200;
  if (n_threads < 1)
    n_threads = 1;

  async_tasks = GET_OPT_ASYNC_TASKS (convert);
  convert->conversion_runner =
      gst_parallelized_task_runner_new (n_threads, pool, async_tasks);

  if (video_converter_lookup_fastpath (convert))
    goto done;

  if (in_info->finfo->unpack_func == NULL)
    goto no_unpack_func;

  if (out_info->finfo->pack_func == NULL)
    goto no_pack_func;

  convert->convert = video_converter_generic;

  convert->upsample_p = g_new0 (GstVideoChromaResample *, n_threads);
  convert->upsample_i = g_new0 (GstVideoChromaResample *, n_threads);
  convert->downsample_p = g_new0 (GstVideoChromaResample *, n_threads);
  convert->downsample_i = g_new0 (GstVideoChromaResample *, n_threads);
  convert->v_scaler_p = g_new0 (GstVideoScaler *, n_threads);
  convert->v_scaler_i = g_new0 (GstVideoScaler *, n_threads);
  convert->h_scaler = g_new0 (GstVideoScaler *, n_threads);
  convert->unpack_lines = g_new0 (GstLineCache *, n_threads);
  convert->pack_lines = g_new0 (GstLineCache *, n_threads);
  convert->upsample_lines = g_new0 (GstLineCache *, n_threads);
  convert->to_RGB_lines = g_new0 (GstLineCache *, n_threads);
  convert->hscale_lines = g_new0 (GstLineCache *, n_threads);
  convert->vscale_lines = g_new0 (GstLineCache *, n_threads);
  convert->convert_lines = g_new0 (GstLineCache *, n_threads);
  convert->alpha_lines = g_new0 (GstLineCache *, n_threads);
  convert->to_YUV_lines = g_new0 (GstLineCache *, n_threads);
  convert->downsample_lines = g_new0 (GstLineCache *, n_threads);
  convert->dither_lines = g_new0 (GstLineCache *, n_threads);
  convert->dither = g_new0 (GstVideoDither *, n_threads);

  if (convert->in_width > 0 && convert->out_width > 0 && convert->in_height > 0
      && convert->out_height > 0) {
    for (i = 0; i < n_threads; i++) {
      convert->current_format = GST_VIDEO_INFO_FORMAT (in_info);
      convert->current_width = convert->in_width;
      convert->current_height = convert->in_height;

      /* unpack */
      prev = chain_unpack_line (convert, i);
      /* upsample chroma */
      prev = chain_upsample (convert, prev, i);
      /* convert to gamma decoded RGB */
      prev = chain_convert_to_RGB (convert, prev, i);
      /* do all downscaling */
      prev = chain_scale (convert, prev, FALSE, i);
      /* do conversion between color spaces */
      prev = chain_convert (convert, prev, i);
      /* do alpha channels */
      prev = chain_alpha (convert, prev, i);
      /* do all remaining (up)scaling */
      prev = chain_scale (convert, prev, TRUE, i);
      /* convert to gamma encoded Y'Cb'Cr' */
      prev = chain_convert_to_YUV (convert, prev, i);
      /* downsample chroma */
      prev = chain_downsample (convert, prev, i);
      /* dither */
      prev = chain_dither (convert, prev, i);
      /* pack into final format */
      convert->pack_lines[i] = chain_pack (convert, prev, i);
    }
  }

  setup_borderline (convert);
  /* now figure out allocators */
  setup_allocators (convert);

done:
  return convert;

  /* ERRORS */
no_unpack_func:
  {
    GST_ERROR ("no unpack_func for format %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (in_info)));
    gst_video_converter_free (convert);
    return NULL;
  }
no_pack_func:
  {
    GST_ERROR ("no pack_func for format %s",
        gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (out_info)));
    gst_video_converter_free (convert);
    return NULL;
  }
}

/**
 * gst_video_converter_new: (skip)
 * @in_info: a #GstVideoInfo
 * @out_info: a #GstVideoInfo
 * @config: (transfer full): a #GstStructure with configuration options
 *
 * Create a new converter object to convert between @in_info and @out_info
 * with @config.
 *
 * Returns (nullable): a #GstVideoConverter or %NULL if conversion is not possible.
 *
 * Since: 1.6
 */
GstVideoConverter *
gst_video_converter_new (const GstVideoInfo * in_info,
    const GstVideoInfo * out_info, GstStructure * config)
{
  return gst_video_converter_new_with_pool (in_info, out_info, config, NULL);
}

static void
clear_matrix_data (MatrixData * data)
{
  g_free (data->t_r);
  g_free (data->t_g);
  g_free (data->t_b);
}

/**
 * gst_video_converter_free:
 * @convert: a #GstVideoConverter
 *
 * Free @convert
 *
 * Since: 1.6
 */
void
gst_video_converter_free (GstVideoConverter * convert)
{
  guint i, j;

  g_return_if_fail (convert != NULL);

  for (i = 0; i < convert->conversion_runner->n_threads; i++) {
    if (convert->upsample_p && convert->upsample_p[i])
      gst_video_chroma_resample_free (convert->upsample_p[i]);
    if (convert->upsample_i && convert->upsample_i[i])
      gst_video_chroma_resample_free (convert->upsample_i[i]);
    if (convert->downsample_p && convert->downsample_p[i])
      gst_video_chroma_resample_free (convert->downsample_p[i]);
    if (convert->downsample_i && convert->downsample_i[i])
      gst_video_chroma_resample_free (convert->downsample_i[i]);
    if (convert->v_scaler_p && convert->v_scaler_p[i])
      gst_video_scaler_free (convert->v_scaler_p[i]);
    if (convert->v_scaler_i && convert->v_scaler_i[i])
      gst_video_scaler_free (convert->v_scaler_i[i]);
    if (convert->h_scaler && convert->h_scaler[i])
      gst_video_scaler_free (convert->h_scaler[i]);
    if (convert->unpack_lines && convert->unpack_lines[i])
      gst_line_cache_free (convert->unpack_lines[i]);
    if (convert->upsample_lines && convert->upsample_lines[i])
      gst_line_cache_free (convert->upsample_lines[i]);
    if (convert->to_RGB_lines && convert->to_RGB_lines[i])
      gst_line_cache_free (convert->to_RGB_lines[i]);
    if (convert->hscale_lines && convert->hscale_lines[i])
      gst_line_cache_free (convert->hscale_lines[i]);
    if (convert->vscale_lines && convert->vscale_lines[i])
      gst_line_cache_free (convert->vscale_lines[i]);
    if (convert->convert_lines && convert->convert_lines[i])
      gst_line_cache_free (convert->convert_lines[i]);
    if (convert->alpha_lines && convert->alpha_lines[i])
      gst_line_cache_free (convert->alpha_lines[i]);
    if (convert->to_YUV_lines && convert->to_YUV_lines[i])
      gst_line_cache_free (convert->to_YUV_lines[i]);
    if (convert->downsample_lines && convert->downsample_lines[i])
      gst_line_cache_free (convert->downsample_lines[i]);
    if (convert->dither_lines && convert->dither_lines[i])
      gst_line_cache_free (convert->dither_lines[i]);
    if (convert->dither && convert->dither[i])
      gst_video_dither_free (convert->dither[i]);
  }
  g_free (convert->upsample_p);
  g_free (convert->upsample_i);
  g_free (convert->downsample_p);
  g_free (convert->downsample_i);
  g_free (convert->v_scaler_p);
  g_free (convert->v_scaler_i);
  g_free (convert->h_scaler);
  g_free (convert->unpack_lines);
  g_free (convert->pack_lines);
  g_free (convert->upsample_lines);
  g_free (convert->to_RGB_lines);
  g_free (convert->hscale_lines);
  g_free (convert->vscale_lines);
  g_free (convert->convert_lines);
  g_free (convert->alpha_lines);
  g_free (convert->to_YUV_lines);
  g_free (convert->downsample_lines);
  g_free (convert->dither_lines);
  g_free (convert->dither);

  g_free (convert->gamma_dec.gamma_table);
  g_free (convert->gamma_enc.gamma_table);

  if (convert->tmpline) {
    for (i = 0; i < convert->conversion_runner->n_threads; i++)
      g_free (convert->tmpline[i]);
    g_free (convert->tmpline);
  }

  g_free (convert->borderline);

  if (convert->config)
    gst_structure_free (convert->config);

  for (i = 0; i < 4; i++) {
    for (j = 0; j < convert->conversion_runner->n_threads; j++) {
      if (convert->fv_scaler[i].scaler)
        gst_video_scaler_free (convert->fv_scaler[i].scaler[j]);
      if (convert->fh_scaler[i].scaler)
        gst_video_scaler_free (convert->fh_scaler[i].scaler[j]);
    }
    g_free (convert->fv_scaler[i].scaler);
    g_free (convert->fh_scaler[i].scaler);
  }

  if (convert->conversion_runner)
    gst_parallelized_task_runner_free (convert->conversion_runner);

  clear_matrix_data (&convert->to_RGB_matrix);
  clear_matrix_data (&convert->convert_matrix);
  clear_matrix_data (&convert->to_YUV_matrix);

  for (i = 0; i < 4; i++) {
    g_free (convert->tasks[i]);
    g_free (convert->tasks_p[i]);
  }

  g_free (convert);
}

static gboolean
copy_config (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstVideoConverter *convert = user_data;

  gst_structure_id_set_value (convert->config, field_id, value);

  return TRUE;
}

/**
 * gst_video_converter_set_config:
 * @convert: a #GstVideoConverter
 * @config: (transfer full): a #GstStructure
 *
 * Set @config as extra configuration for @convert.
 *
 * If the parameters in @config can not be set exactly, this function returns
 * %FALSE and will try to update as much state as possible. The new state can
 * then be retrieved and refined with gst_video_converter_get_config().
 *
 * Look at the `GST_VIDEO_CONVERTER_OPT_*` fields to check valid configuration
 * option and values.
 *
 * Returns: %TRUE when @config could be set.
 *
 * Since: 1.6
 */
gboolean
gst_video_converter_set_config (GstVideoConverter * convert,
    GstStructure * config)
{
  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (config != NULL, FALSE);

  gst_structure_foreach (config, copy_config, convert);
  gst_structure_free (config);

  return TRUE;
}

/**
 * gst_video_converter_get_config:
 * @convert: a #GstVideoConverter
 *
 * Get the current configuration of @convert.
 *
 * Returns: a #GstStructure that remains valid for as long as @convert is valid
 *   or until gst_video_converter_set_config() is called.
 */
const GstStructure *
gst_video_converter_get_config (GstVideoConverter * convert)
{
  g_return_val_if_fail (convert != NULL, NULL);

  return convert->config;
}

/**
 * gst_video_converter_frame:
 * @convert: a #GstVideoConverter
 * @dest: a #GstVideoFrame
 * @src: a #GstVideoFrame
 *
 * Convert the pixels of @src into @dest using @convert.
 *
 * If #GST_VIDEO_CONVERTER_OPT_ASYNC_TASKS is %TRUE then this function will
 * return immediately and needs to be followed by a call to
 * gst_video_converter_frame_finish().
 *
 * Since: 1.6
 */
void
gst_video_converter_frame (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest)
{
  g_return_if_fail (convert != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  /* Check the frames we've been passed match the layout
   * we were configured for or we might go out of bounds */
  if (G_UNLIKELY (GST_VIDEO_INFO_FORMAT (&convert->in_info) !=
          GST_VIDEO_FRAME_FORMAT (src)
          || GST_VIDEO_INFO_WIDTH (&convert->in_info) >
          GST_VIDEO_FRAME_WIDTH (src)
          || GST_VIDEO_INFO_FIELD_HEIGHT (&convert->in_info) >
          GST_VIDEO_FRAME_HEIGHT (src))) {
    g_critical ("Input video frame does not match configuration");
    return;
  }
  if (G_UNLIKELY (GST_VIDEO_INFO_FORMAT (&convert->out_info) !=
          GST_VIDEO_FRAME_FORMAT (dest)
          || GST_VIDEO_INFO_WIDTH (&convert->out_info) >
          GST_VIDEO_FRAME_WIDTH (dest)
          || GST_VIDEO_INFO_FIELD_HEIGHT (&convert->out_info) >
          GST_VIDEO_FRAME_HEIGHT (dest))) {
    g_critical ("Output video frame does not match configuration");
    return;
  }

  if (G_UNLIKELY (convert->in_width == 0 || convert->in_height == 0 ||
          convert->out_width == 0 || convert->out_height == 0))
    return;

  convert->convert (convert, src, dest);
}

/**
 * gst_video_converter_frame_finish:
 * @convert: a #GstVideoConverter
 *
 * Wait for a previous async conversion performed using
 * gst_video_converter_frame() to complete.
 *
 * Since: 1.20
 */
void
gst_video_converter_frame_finish (GstVideoConverter * convert)
{
  g_return_if_fail (convert);
  g_return_if_fail (convert->conversion_runner);
  g_return_if_fail (convert->conversion_runner->async_tasks);

  gst_parallelized_task_runner_finish (convert->conversion_runner);
}

static void
video_converter_compute_matrix (GstVideoConverter * convert)
{
  MatrixData *dst = &convert->convert_matrix;

  color_matrix_set_identity (dst);
  compute_matrix_to_RGB (convert, dst);
  compute_matrix_to_YUV (convert, dst, FALSE);

  convert->current_bits = 8;
  prepare_matrix (convert, dst);
}

static void
video_converter_compute_resample (GstVideoConverter * convert, gint idx)
{
  GstVideoInfo *in_info, *out_info;
  const GstVideoFormatInfo *sfinfo, *dfinfo;

  if (CHECK_CHROMA_NONE (convert))
    return;

  in_info = &convert->in_info;
  out_info = &convert->out_info;

  sfinfo = in_info->finfo;
  dfinfo = out_info->finfo;

  GST_LOG ("site: %d->%d, w_sub: %d->%d, h_sub: %d->%d", in_info->chroma_site,
      out_info->chroma_site, sfinfo->w_sub[2], dfinfo->w_sub[2],
      sfinfo->h_sub[2], dfinfo->h_sub[2]);

  if (sfinfo->w_sub[2] != dfinfo->w_sub[2] ||
      sfinfo->h_sub[2] != dfinfo->h_sub[2] ||
      in_info->chroma_site != out_info->chroma_site ||
      in_info->width != out_info->width ||
      in_info->height != out_info->height) {
    if (GST_VIDEO_INFO_IS_INTERLACED (in_info)
        && GST_VIDEO_INFO_INTERLACE_MODE (in_info) !=
        GST_VIDEO_INTERLACE_MODE_ALTERNATE) {
      if (!CHECK_CHROMA_DOWNSAMPLE (convert))
        convert->upsample_i[idx] = gst_video_chroma_resample_new (0,
            in_info->chroma_site, GST_VIDEO_CHROMA_FLAG_INTERLACED,
            sfinfo->unpack_format, sfinfo->w_sub[2], sfinfo->h_sub[2]);
      if (!CHECK_CHROMA_UPSAMPLE (convert))
        convert->downsample_i[idx] =
            gst_video_chroma_resample_new (0, out_info->chroma_site,
            GST_VIDEO_CHROMA_FLAG_INTERLACED, dfinfo->unpack_format,
            -dfinfo->w_sub[2], -dfinfo->h_sub[2]);
    }
    if (!CHECK_CHROMA_DOWNSAMPLE (convert))
      convert->upsample_p[idx] = gst_video_chroma_resample_new (0,
          in_info->chroma_site, 0, sfinfo->unpack_format, sfinfo->w_sub[2],
          sfinfo->h_sub[2]);
    if (!CHECK_CHROMA_UPSAMPLE (convert))
      convert->downsample_p[idx] = gst_video_chroma_resample_new (0,
          out_info->chroma_site, 0, dfinfo->unpack_format, -dfinfo->w_sub[2],
          -dfinfo->h_sub[2]);
  }
}

#define FRAME_GET_PLANE_STRIDE(frame, plane) \
  GST_VIDEO_FRAME_PLANE_STRIDE (frame, plane)
#define FRAME_GET_PLANE_LINE(frame, plane, line) \
  (gpointer)(((guint8*)(GST_VIDEO_FRAME_PLANE_DATA (frame, plane))) + \
      FRAME_GET_PLANE_STRIDE (frame, plane) * (line))

#define FRAME_GET_COMP_STRIDE(frame, comp) \
  GST_VIDEO_FRAME_COMP_STRIDE (frame, comp)
#define FRAME_GET_COMP_LINE(frame, comp, line) \
  (gpointer)(((guint8*)(GST_VIDEO_FRAME_COMP_DATA (frame, comp))) + \
      FRAME_GET_COMP_STRIDE (frame, comp) * (line))

#define FRAME_GET_STRIDE(frame)      FRAME_GET_PLANE_STRIDE (frame, 0)
#define FRAME_GET_LINE(frame,line)   FRAME_GET_PLANE_LINE (frame, 0, line)

#define FRAME_GET_Y_LINE(frame,line) FRAME_GET_COMP_LINE(frame, GST_VIDEO_COMP_Y, line)
#define FRAME_GET_U_LINE(frame,line) FRAME_GET_COMP_LINE(frame, GST_VIDEO_COMP_U, line)
#define FRAME_GET_V_LINE(frame,line) FRAME_GET_COMP_LINE(frame, GST_VIDEO_COMP_V, line)
#define FRAME_GET_A_LINE(frame,line) FRAME_GET_COMP_LINE(frame, GST_VIDEO_COMP_A, line)

#define FRAME_GET_Y_STRIDE(frame)    FRAME_GET_COMP_STRIDE(frame, GST_VIDEO_COMP_Y)
#define FRAME_GET_U_STRIDE(frame)    FRAME_GET_COMP_STRIDE(frame, GST_VIDEO_COMP_U)
#define FRAME_GET_V_STRIDE(frame)    FRAME_GET_COMP_STRIDE(frame, GST_VIDEO_COMP_V)
#define FRAME_GET_A_STRIDE(frame)    FRAME_GET_COMP_STRIDE(frame, GST_VIDEO_COMP_A)


#define UNPACK_FRAME(frame,dest,line,x,width)        \
  frame->info.finfo->unpack_func (frame->info.finfo, \
      (GST_VIDEO_FRAME_IS_INTERLACED (frame) ?       \
        GST_VIDEO_PACK_FLAG_INTERLACED :             \
        GST_VIDEO_PACK_FLAG_NONE),                   \
      dest, frame->data, frame->info.stride, x,      \
      line, width)
#define PACK_FRAME(frame,src,line,width)             \
  frame->info.finfo->pack_func (frame->info.finfo,   \
      (GST_VIDEO_FRAME_IS_INTERLACED (frame) ?       \
        GST_VIDEO_PACK_FLAG_INTERLACED :             \
        GST_VIDEO_PACK_FLAG_NONE),                   \
      src, 0, frame->data, frame->info.stride,       \
      frame->info.chroma_site, line, width);

static gpointer
get_dest_line (GstLineCache * cache, gint idx, gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  guint8 *line;
  gint pstride = convert->pack_pstride;
  gint out_x = convert->out_x;
  guint cline;

  cline = CLAMP (idx, 0, convert->out_maxheight - 1);

  line = FRAME_GET_LINE (convert->dest, cline);
  GST_LOG ("get dest line %d %p", cline, line);

  if (convert->borderline) {
    gint r_border = (out_x + convert->out_width) * pstride;
    gint rb_width = convert->out_maxwidth * pstride - r_border;
    gint lb_width = out_x * pstride;

    memcpy (line, convert->borderline, lb_width);
    memcpy (line + r_border, convert->borderline, rb_width);
  }
  line += out_x * pstride;

  return line;
}

static gboolean
do_unpack_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer tmpline;
  guint cline;

  cline = CLAMP (in_line + convert->in_y, 0, convert->in_maxheight - 1);

  if (cache->alloc_writable || !convert->identity_unpack) {
    tmpline = gst_line_cache_alloc_line (cache, out_line);
    GST_LOG ("unpack line %d (%u) %p", in_line, cline, tmpline);
    UNPACK_FRAME (convert->src, tmpline, cline, convert->in_x,
        convert->in_width);
  } else {
    tmpline = ((guint8 *) FRAME_GET_LINE (convert->src, cline)) +
        convert->in_x * convert->unpack_pstride;
    GST_LOG ("get src line %d (%u) %p", in_line, cline, tmpline);
  }
  gst_line_cache_add_line (cache, in_line, tmpline);

  return TRUE;
}

static gboolean
do_upsample_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer *lines;
  gint i, start_line, n_lines;

  n_lines = convert->up_n_lines;
  start_line = in_line;
  if (start_line < n_lines + convert->up_offset) {
    start_line += convert->up_offset;
    out_line += convert->up_offset;
  }

  /* get the lines needed for chroma upsample */
  lines =
      gst_line_cache_get_lines (cache->prev, idx, out_line, start_line,
      n_lines);

  if (convert->upsample[idx]) {
    GST_LOG ("doing upsample %d-%d %p", start_line, start_line + n_lines - 1,
        lines[0]);
    gst_video_chroma_resample (convert->upsample[idx], lines,
        convert->in_width);
  }

  for (i = 0; i < n_lines; i++)
    gst_line_cache_add_line (cache, start_line + i, lines[i]);

  return TRUE;
}

static gboolean
do_convert_to_RGB_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  MatrixData *data = &convert->to_RGB_matrix;
  gpointer *lines, destline;

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);
  destline = lines[0];

  if (data->matrix_func) {
    GST_LOG ("to RGB line %d %p", in_line, destline);
    data->matrix_func (data, destline);
  }
  if (convert->gamma_dec.gamma_func) {
    destline = gst_line_cache_alloc_line (cache, out_line);

    GST_LOG ("gamma decode line %d %p->%p", in_line, lines[0], destline);
    convert->gamma_dec.gamma_func (&convert->gamma_dec, destline, lines[0]);
  }
  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_hscale_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer *lines, destline;

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);

  destline = gst_line_cache_alloc_line (cache, out_line);

  GST_LOG ("hresample line %d %p->%p", in_line, lines[0], destline);
  gst_video_scaler_horizontal (convert->h_scaler[idx], convert->h_scale_format,
      lines[0], destline, 0, convert->out_width);

  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_vscale_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer *lines, destline;
  guint sline, n_lines;
  guint cline;

  cline = CLAMP (in_line, 0, convert->out_height - 1);

  gst_video_scaler_get_coeff (convert->v_scaler[idx], cline, &sline, &n_lines);
  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, sline, n_lines);

  destline = gst_line_cache_alloc_line (cache, out_line);

  GST_LOG ("vresample line %d %d-%d %p->%p", in_line, sline,
      sline + n_lines - 1, lines[0], destline);
  gst_video_scaler_vertical (convert->v_scaler[idx], convert->v_scale_format,
      lines, destline, cline, convert->v_scale_width);

  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_convert_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  MatrixData *data = &convert->convert_matrix;
  gpointer *lines, destline;
  guint in_bits, out_bits;
  gint width;

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);

  destline = lines[0];

  in_bits = convert->in_bits;
  out_bits = convert->out_bits;

  width = MIN (convert->in_width, convert->out_width);

  if (out_bits == 16 || in_bits == 16) {
    gpointer srcline = lines[0];

    if (out_bits != in_bits)
      destline = gst_line_cache_alloc_line (cache, out_line);

    /* FIXME, we can scale in the conversion matrix */
    if (in_bits == 8) {
      GST_LOG ("8->16 line %d %p->%p", in_line, srcline, destline);
      video_orc_convert_u8_to_u16 (destline, srcline, width * 4);
      srcline = destline;
    }

    if (data->matrix_func) {
      GST_LOG ("matrix line %d %p", in_line, srcline);
      data->matrix_func (data, srcline);
    }

    /* FIXME, dither here */
    if (out_bits == 8) {
      GST_LOG ("16->8 line %d %p->%p", in_line, srcline, destline);
      video_orc_convert_u16_to_u8 (destline, srcline, width * 4);
    }
  } else {
    if (data->matrix_func) {
      GST_LOG ("matrix line %d %p", in_line, destline);
      data->matrix_func (data, destline);
    }
  }
  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_alpha_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  gpointer *lines, destline;
  GstVideoConverter *convert = user_data;
  gint width = MIN (convert->in_width, convert->out_width);

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);
  destline = lines[0];

  GST_LOG ("alpha line %d %p", in_line, destline);
  convert->alpha_func (convert, destline, width);

  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_convert_to_YUV_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  MatrixData *data = &convert->to_YUV_matrix;
  gpointer *lines, destline;

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);
  destline = lines[0];

  if (convert->gamma_enc.gamma_func) {
    destline = gst_line_cache_alloc_line (cache, out_line);

    GST_LOG ("gamma encode line %d %p->%p", in_line, lines[0], destline);
    convert->gamma_enc.gamma_func (&convert->gamma_enc, destline, lines[0]);
  }
  if (data->matrix_func) {
    GST_LOG ("to YUV line %d %p", in_line, destline);
    data->matrix_func (data, destline);
  }
  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

static gboolean
do_downsample_lines (GstLineCache * cache, gint idx, gint out_line,
    gint in_line, gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer *lines;
  gint i, start_line, n_lines;

  n_lines = convert->down_n_lines;
  start_line = in_line;
  if (start_line < n_lines + convert->down_offset)
    start_line += convert->down_offset;

  /* get the lines needed for chroma downsample */
  lines =
      gst_line_cache_get_lines (cache->prev, idx, out_line, start_line,
      n_lines);

  if (convert->downsample[idx]) {
    GST_LOG ("downsample line %d %d-%d %p", in_line, start_line,
        start_line + n_lines - 1, lines[0]);
    gst_video_chroma_resample (convert->downsample[idx], lines,
        convert->out_width);
  }

  for (i = 0; i < n_lines; i++)
    gst_line_cache_add_line (cache, start_line + i, lines[i]);

  return TRUE;
}

static gboolean
do_dither_lines (GstLineCache * cache, gint idx, gint out_line, gint in_line,
    gpointer user_data)
{
  GstVideoConverter *convert = user_data;
  gpointer *lines, destline;

  lines = gst_line_cache_get_lines (cache->prev, idx, out_line, in_line, 1);
  destline = lines[0];

  if (convert->dither[idx]) {
    GST_LOG ("Dither line %d %p", in_line, destline);
    gst_video_dither_line (convert->dither[idx], destline, 0, out_line,
        convert->out_width);
  }
  gst_line_cache_add_line (cache, in_line, destline);

  return TRUE;
}

typedef struct
{
  GstLineCache *pack_lines;
  gint idx;
  gint h_0, h_1;
  gint pack_lines_count;
  gint out_y;
  gboolean identity_pack;
  gint lb_width, out_maxwidth;
  GstVideoFrame *dest;
} ConvertTask;

static void
convert_generic_task (ConvertTask * task)
{
  gint i;

  for (i = task->h_0; i < task->h_1; i += task->pack_lines_count) {
    gpointer *lines;

    /* load the lines needed to pack */
    lines =
        gst_line_cache_get_lines (task->pack_lines, task->idx, i + task->out_y,
        i, task->pack_lines_count);

    if (!task->identity_pack) {
      /* take away the border */
      guint8 *l = ((guint8 *) lines[0]) - task->lb_width;
      /* and pack into destination */
      GST_LOG ("pack line %d %p (%p)", i + task->out_y, lines[0], l);
      PACK_FRAME (task->dest, l, i + task->out_y, task->out_maxwidth);
    }
  }
}

static void
video_converter_generic (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint i;
  gint out_maxwidth, out_maxheight;
  gint out_x, out_y, out_height;
  gint pack_lines, pstride;
  gint lb_width;
  ConvertTask *tasks;
  ConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  out_height = convert->out_height;
  out_maxwidth = convert->out_maxwidth;
  out_maxheight = convert->out_maxheight;

  out_x = convert->out_x;
  out_y = convert->out_y;

  convert->src = src;
  convert->dest = dest;

  if (GST_VIDEO_FRAME_IS_INTERLACED (src)) {
    GST_LOG ("setup interlaced frame");
    convert->upsample = convert->upsample_i;
    convert->downsample = convert->downsample_i;
    convert->v_scaler = convert->v_scaler_i;
  } else {
    GST_LOG ("setup progressive frame");
    convert->upsample = convert->upsample_p;
    convert->downsample = convert->downsample_p;
    convert->v_scaler = convert->v_scaler_p;
  }
  if (convert->upsample[0]) {
    gst_video_chroma_resample_get_info (convert->upsample[0],
        &convert->up_n_lines, &convert->up_offset);
  } else {
    convert->up_n_lines = 1;
    convert->up_offset = 0;
  }
  if (convert->downsample[0]) {
    gst_video_chroma_resample_get_info (convert->downsample[0],
        &convert->down_n_lines, &convert->down_offset);
  } else {
    convert->down_n_lines = 1;
    convert->down_offset = 0;
  }

  pack_lines = convert->pack_nlines;    /* only 1 for now */
  pstride = convert->pack_pstride;

  lb_width = out_x * pstride;

  if (convert->borderline) {
    /* FIXME we should try to avoid PACK_FRAME */
    for (i = 0; i < out_y; i++)
      PACK_FRAME (dest, convert->borderline, i, out_maxwidth);
  }

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (ConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (ConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread =
      GST_ROUND_UP_N ((out_height + n_threads - 1) / n_threads, pack_lines);

  for (i = 0; i < n_threads; i++) {
    tasks[i].dest = dest;
    tasks[i].pack_lines = convert->pack_lines[i];
    tasks[i].idx = i;
    tasks[i].pack_lines_count = pack_lines;
    tasks[i].out_y = out_y;
    tasks[i].identity_pack = convert->identity_pack;
    tasks[i].lb_width = lb_width;
    tasks[i].out_maxwidth = out_maxwidth;

    tasks[i].h_0 = i * lines_per_thread;
    tasks[i].h_1 = MIN ((i + 1) * lines_per_thread, out_height);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_generic_task, (gpointer) tasks_p);

  if (convert->borderline) {
    for (i = out_y + out_height; i < out_maxheight; i++)
      PACK_FRAME (dest, convert->borderline, i, out_maxwidth);
  }
  if (convert->pack_pal) {
    memcpy (GST_VIDEO_FRAME_PLANE_DATA (dest, 1), convert->pack_pal,
        convert->pack_palsize);
  }
}

static void convert_fill_border (GstVideoConverter * convert,
    GstVideoFrame * dest);

/* Fast paths */

#define GET_LINE_OFFSETS(interlaced,line,l1,l2) \
    if (interlaced) {                           \
      l1 = (line & 2 ? line - 1 : line);        \
      l2 = l1 + 2;                              \
    } else {                                    \
      l1 = line;                                \
      l2 = l1 + 1;                              \
    }

typedef struct
{
  const GstVideoFrame *src;
  GstVideoFrame *dest;
  gint height_0, height_1;

  /* parameters */
  gboolean interlaced;
  gint width;
  gint alpha;
  MatrixData *data;
  gint in_x, in_y;
  gint out_x, out_y;
  gpointer tmpline;
} FConvertTask;

static void
convert_I420_YUY2_task (FConvertTask * task)
{
  gint i;
  gint l1, l2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    video_orc_convert_I420_YUY2 (FRAME_GET_LINE (task->dest, l1),
        FRAME_GET_LINE (task->dest, l2),
        FRAME_GET_Y_LINE (task->src, l1),
        FRAME_GET_Y_LINE (task->src, l2),
        FRAME_GET_U_LINE (task->src, i >> 1),
        FRAME_GET_V_LINE (task->src, i >> 1), (task->width + 1) / 2);
  }
}

static void
convert_I420_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_YUY2_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);
      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_I420_UYVY_task (FConvertTask * task)
{
  gint i;
  gint l1, l2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    video_orc_convert_I420_UYVY (FRAME_GET_LINE (task->dest, l1),
        FRAME_GET_LINE (task->dest, l2),
        FRAME_GET_Y_LINE (task->src, l1),
        FRAME_GET_Y_LINE (task->src, l2),
        FRAME_GET_U_LINE (task->src, i >> 1),
        FRAME_GET_V_LINE (task->src, i >> 1), (task->width + 1) / 2);
  }
}

static void
convert_I420_UYVY (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_UYVY_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);
      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_I420_AYUV_task (FConvertTask * task)
{
  gint i;
  gint l1, l2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    video_orc_convert_I420_AYUV (FRAME_GET_LINE (task->dest, l1),
        FRAME_GET_LINE (task->dest, l2),
        FRAME_GET_Y_LINE (task->src, l1),
        FRAME_GET_Y_LINE (task->src, l2),
        FRAME_GET_U_LINE (task->src, i >> 1), FRAME_GET_V_LINE (task->src,
            i >> 1), task->alpha, task->width);
  }
}

static void
convert_I420_AYUV (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  guint8 alpha = MIN (convert->alpha_value, 255);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);


  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;
    tasks[i].alpha = alpha;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_AYUV_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);
      if (alpha != 0xff)
        convert_set_alpha_u8 (convert, convert->tmpline[0], width);
      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_I420_v210_task (FConvertTask * task)
{
  gint i, j;
  gint l1, l2;
  const guint8 *s_y1, *s_y2, *s_u, *s_v;
  guint8 *d1, *d2;
  guint32 a0, a1, a2, a3;
  guint8 y0_1, y1_1, y2_1, y3_1, y4_1, y5_1;
  guint8 u0_1, u2_1, u4_1;
  guint8 v0_1, v2_1, v4_1;
  guint8 y0_2, y1_2, y2_2, y3_2, y4_2, y5_2;
  guint8 u0_2, u2_2, u4_2;
  guint8 v0_2, v2_2, v4_2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    s_y1 = FRAME_GET_Y_LINE (task->src, l1);
    s_y2 = FRAME_GET_Y_LINE (task->src, l2);
    s_u = FRAME_GET_U_LINE (task->src, i >> 1);
    s_v = FRAME_GET_V_LINE (task->src, i >> 1);

    d1 = FRAME_GET_LINE (task->dest, l1);
    d2 = FRAME_GET_LINE (task->dest, l2);

    for (j = 0; j < task->width; j += 6) {
      y1_1 = y2_1 = y3_1 = y4_1 = y5_1 = 0;
      u2_1 = u4_1 = v2_1 = v4_1 = 0;
      y1_2 = y2_2 = y3_2 = y4_2 = y5_2 = 0;
      u2_2 = u4_2 = v2_2 = v4_2 = 0;

      y0_1 = s_y1[j];
      y0_2 = s_y2[j];

      u0_1 = u0_2 = s_u[j / 2];
      v0_1 = v0_2 = s_v[j / 2];

      if (j < task->width - 1) {
        y1_1 = s_y1[j + 1];
        y1_2 = s_y2[j + 1];
      }

      if (j < task->width - 2) {
        y2_1 = s_y1[j + 2];
        y2_2 = s_y2[j + 2];

        u2_1 = u2_2 = s_u[j / 2 + 1];
        v2_1 = v2_2 = s_v[j / 2 + 1];
      }

      if (j < task->width - 3) {
        y3_1 = s_y1[j + 3];
        y3_2 = s_y2[j + 3];
      }

      if (j < task->width - 4) {
        y4_1 = s_y1[j + 4];
        y4_2 = s_y2[j + 4];

        u4_1 = u4_2 = s_u[j / 2 + 2];
        v4_1 = v4_2 = s_v[j / 2 + 2];
      }

      if (j < task->width - 5) {
        y5_1 = s_y1[j + 5];
        y5_2 = s_y2[j + 5];
      }

      a0 = u0_1 << 2 | (y0_1 << 12) | (v0_1 << 22);
      a1 = y1_1 << 2 | (u2_1 << 12) | (y2_1 << 22);
      a2 = v2_1 << 2 | (y3_1 << 12) | (u4_1 << 22);
      a3 = y4_1 << 2 | (v4_1 << 12) | (y5_1 << 22);

      GST_WRITE_UINT32_LE (d1 + (j / 6) * 16 + 0, a0);
      GST_WRITE_UINT32_LE (d1 + (j / 6) * 16 + 4, a1);
      GST_WRITE_UINT32_LE (d1 + (j / 6) * 16 + 8, a2);
      GST_WRITE_UINT32_LE (d1 + (j / 6) * 16 + 12, a3);

      a0 = u0_2 << 2 | (y0_2 << 12) | (v0_2 << 22);
      a1 = y1_2 << 2 | (u2_2 << 12) | (y2_2 << 22);
      a2 = v2_2 << 2 | (y3_2 << 12) | (u4_2 << 22);
      a3 = y4_2 << 2 | (v4_2 << 12) | (y5_2 << 22);

      GST_WRITE_UINT32_LE (d2 + (j / 6) * 16 + 0, a0);
      GST_WRITE_UINT32_LE (d2 + (j / 6) * 16 + 4, a1);
      GST_WRITE_UINT32_LE (d2 + (j / 6) * 16 + 8, a2);
      GST_WRITE_UINT32_LE (d2 + (j / 6) * 16 + 12, a3);
    }
  }
}

static void
convert_I420_v210 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  guint8 *tmpline_8;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_v210_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);

      tmpline_8 = (guint8 *) convert->tmpline[0];
      for (int j = width * 4 - 1; j >= 0; j--) {
        convert->tmpline[0][j] = tmpline_8[j] << 8;
      }

      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_YUY2_I420_task (FConvertTask * task)
{
  gint i;
  gint l1, l2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    video_orc_convert_YUY2_I420 (FRAME_GET_Y_LINE (task->dest, l1),
        FRAME_GET_Y_LINE (task->dest, l2),
        FRAME_GET_U_LINE (task->dest, i >> 1),
        FRAME_GET_V_LINE (task->dest, i >> 1),
        FRAME_GET_LINE (task->src, l1), FRAME_GET_LINE (task->src, l2),
        (task->width + 1) / 2);
  }
}

static void
convert_YUY2_I420 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_YUY2_I420_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);
      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_v210_I420_task (FConvertTask * task)
{
  gint i, j;
  gint l1, l2;
  guint8 *d_y1, *d_y2, *d_u, *d_v;
  const guint8 *s1, *s2;
  guint32 a0, a1, a2, a3;
  guint16 y0_1, y1_1, y2_1, y3_1, y4_1, y5_1;
  guint16 u0_1, u2_1, u4_1;
  guint16 v0_1, v2_1, v4_1;
  guint16 y0_2, y1_2, y2_2, y3_2, y4_2, y5_2;
  guint16 u0_2, u2_2, u4_2;
  guint16 v0_2, v2_2, v4_2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    d_y1 = FRAME_GET_Y_LINE (task->dest, l1);
    d_y2 = FRAME_GET_Y_LINE (task->dest, l2);
    d_u = FRAME_GET_U_LINE (task->dest, i >> 1);
    d_v = FRAME_GET_V_LINE (task->dest, i >> 1);

    s1 = FRAME_GET_LINE (task->src, l1);
    s2 = FRAME_GET_LINE (task->src, l2);

    for (j = 0; j < task->width; j += 6) {
      a0 = GST_READ_UINT32_LE (s1 + (j / 6) * 16 + 0);
      a1 = GST_READ_UINT32_LE (s1 + (j / 6) * 16 + 4);
      a2 = GST_READ_UINT32_LE (s1 + (j / 6) * 16 + 8);
      a3 = GST_READ_UINT32_LE (s1 + (j / 6) * 16 + 12);

      u0_1 = ((a0 >> 0) & 0x3ff) >> 2;
      y0_1 = ((a0 >> 10) & 0x3ff) >> 2;
      v0_1 = ((a0 >> 20) & 0x3ff) >> 2;
      y1_1 = ((a1 >> 0) & 0x3ff) >> 2;

      u2_1 = ((a1 >> 10) & 0x3ff) >> 2;
      y2_1 = ((a1 >> 20) & 0x3ff) >> 2;
      v2_1 = ((a2 >> 0) & 0x3ff) >> 2;
      y3_1 = ((a2 >> 10) & 0x3ff) >> 2;

      u4_1 = ((a2 >> 20) & 0x3ff) >> 2;
      y4_1 = ((a3 >> 0) & 0x3ff) >> 2;
      v4_1 = ((a3 >> 10) & 0x3ff) >> 2;
      y5_1 = ((a3 >> 20) & 0x3ff) >> 2;

      a0 = GST_READ_UINT32_LE (s2 + (j / 6) * 16 + 0);
      a1 = GST_READ_UINT32_LE (s2 + (j / 6) * 16 + 4);
      a2 = GST_READ_UINT32_LE (s2 + (j / 6) * 16 + 8);
      a3 = GST_READ_UINT32_LE (s2 + (j / 6) * 16 + 12);

      u0_2 = ((a0 >> 0) & 0x3ff) >> 2;
      y0_2 = ((a0 >> 10) & 0x3ff) >> 2;
      v0_2 = ((a0 >> 20) & 0x3ff) >> 2;
      y1_2 = ((a1 >> 0) & 0x3ff) >> 2;

      u2_2 = ((a1 >> 10) & 0x3ff) >> 2;
      y2_2 = ((a1 >> 20) & 0x3ff) >> 2;
      v2_2 = ((a2 >> 0) & 0x3ff) >> 2;
      y3_2 = ((a2 >> 10) & 0x3ff) >> 2;

      u4_2 = ((a2 >> 20) & 0x3ff) >> 2;
      y4_2 = ((a3 >> 0) & 0x3ff) >> 2;
      v4_2 = ((a3 >> 10) & 0x3ff) >> 2;
      y5_2 = ((a3 >> 20) & 0x3ff) >> 2;

      d_y1[j] = y0_1;
      d_y2[j] = y0_2;
      d_u[j / 2] = (u0_1 + u0_2) / 2;
      d_v[j / 2] = (v0_1 + v0_2) / 2;

      if (j < task->width - 1) {
        d_y1[j + 1] = y1_1;
        d_y2[j + 1] = y1_2;
      }

      if (j < task->width - 2) {
        d_y1[j + 2] = y2_1;
        d_y2[j + 2] = y2_2;
        d_u[j / 2 + 1] = (u2_1 + u2_2) / 2;
        d_v[j / 2 + 1] = (v2_1 + v2_2) / 2;
      }

      if (j < task->width - 3) {
        d_y1[j + 3] = y3_1;
        d_y2[j + 3] = y3_2;
      }

      if (j < task->width - 4) {
        d_y1[j + 4] = y4_1;
        d_y2[j + 4] = y4_2;
        d_u[j / 2 + 2] = (u4_1 + u4_2) / 2;
        d_v[j / 2 + 2] = (v4_1 + v4_2) / 2;
      }

      if (j < task->width - 5) {
        d_y1[j + 5] = y5_1;
        d_y2[j + 5] = y5_2;
      }
    }
  }
}

static void
convert_v210_I420 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  guint8 *tmpline_8;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_v210_I420_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);

      tmpline_8 = (guint8 *) convert->tmpline[0];
      for (int j = 0; j < width * 4; j++) {
        tmpline_8[j] = convert->tmpline[0][j] >> 8;
      }

      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

typedef struct
{
  const guint8 *s, *s2, *su, *sv;
  guint8 *d, *d2, *du, *dv;
  gint sstride, sustride, svstride;
  gint dstride, dustride, dvstride;
  gint width, height;
  gint alpha;
  MatrixData *data;
} FConvertPlaneTask;

static void
convert_YUY2_AYUV_task (FConvertPlaneTask * task)
{
  video_orc_convert_YUY2_AYUV (task->d, task->dstride, task->s,
      task->sstride, task->alpha, (task->width + 1) / 2, task->height);
}

static void
convert_YUY2_AYUV (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  guint8 alpha = MIN (convert->alpha_value, 255);
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].alpha = alpha;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_YUY2_AYUV_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_YUY2_v210_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d;
  const guint8 *s;
  guint32 a0, a1, a2, a3;
  guint8 y0, y1, y2, y3, y4, y5;
  guint8 u0, u2, u4;
  guint8 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d = task->d + i * task->dstride;
    s = task->s + i * task->sstride;

    for (j = 0; j < task->width; j += 6) {
      y1 = y2 = y3 = y4 = y5 = 0;
      u2 = u4 = v2 = v4 = 0;

      y0 = s[2 * j];
      u0 = s[2 * j + 1];
      v0 = s[2 * j + 3];

      if (j < task->width - 1) {
        y1 = s[2 * j + 2];
      }

      if (j < task->width - 2) {
        y2 = s[2 * j + 4];
        u2 = s[2 * j + 5];
        v2 = s[2 * j + 7];
      }

      if (j < task->width - 3) {
        y3 = s[2 * j + 6];
      }

      if (j < task->width - 4) {
        y4 = s[2 * j + 8];
        u4 = s[2 * j + 9];
        v4 = s[2 * j + 11];
      }

      if (j < task->width - 5) {
        y5 = s[2 * j + 10];
      }

      a0 = u0 << 2 | (y0 << 12) | (v0 << 22);
      a1 = y1 << 2 | (u2 << 12) | (y2 << 22);
      a2 = v2 << 2 | (y3 << 12) | (u4 << 22);
      a3 = y4 << 2 | (v4 << 12) | (y5 << 22);

      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 0, a0);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 4, a1);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 8, a2);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 12, a3);
    }
  }
}

static void
convert_YUY2_v210 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_YUY2_v210_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_YUY2_Y42B_task (FConvertPlaneTask * task)
{
  video_orc_convert_YUY2_Y42B (task->d, task->dstride, task->du,
      task->dustride, task->dv, task->dvstride,
      task->s, task->sstride, (task->width + 1) / 2, task->height);
}

static void
convert_YUY2_Y42B (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x >> 1;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x >> 1;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_YUY2_Y42B_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_YUY2_Y444_task (FConvertPlaneTask * task)
{
  video_orc_convert_YUY2_Y444 (task->d,
      task->dstride, task->du,
      task->dustride, task->dv,
      task->dvstride, task->s,
      task->sstride, (task->width + 1) / 2, task->height);
}

static void
convert_YUY2_Y444 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_YUY2_Y444_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_v210_Y42B_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d_y, *d_u, *d_v;
  const guint8 *s;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d_y = task->d + i * task->dstride;
    d_u = task->du + i * task->dustride;
    d_v = task->dv + i * task->dvstride;
    s = task->s + i * task->sstride;

    for (j = 0; j < task->width; j += 6) {
      a0 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 0);
      a1 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 4);
      a2 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 8);
      a3 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 12);

      u0 = ((a0 >> 0) & 0x3ff) >> 2;
      y0 = ((a0 >> 10) & 0x3ff) >> 2;
      v0 = ((a0 >> 20) & 0x3ff) >> 2;
      y1 = ((a1 >> 0) & 0x3ff) >> 2;

      u2 = ((a1 >> 10) & 0x3ff) >> 2;
      y2 = ((a1 >> 20) & 0x3ff) >> 2;
      v2 = ((a2 >> 0) & 0x3ff) >> 2;
      y3 = ((a2 >> 10) & 0x3ff) >> 2;

      u4 = ((a2 >> 20) & 0x3ff) >> 2;
      y4 = ((a3 >> 0) & 0x3ff) >> 2;
      v4 = ((a3 >> 10) & 0x3ff) >> 2;
      y5 = ((a3 >> 20) & 0x3ff) >> 2;

      d_y[j] = y0;
      d_u[j / 2] = u0;
      d_v[j / 2] = v0;

      if (j < task->width - 1) {
        d_y[j + 1] = y1;
      }

      if (j < task->width - 2) {
        d_y[j + 2] = y2;
        d_u[j / 2 + 1] = u2;
        d_v[j / 2 + 1] = v2;
      }

      if (j < task->width - 3) {
        d_y[j + 3] = y3;
      }

      if (j < task->width - 4) {
        d_y[j + 4] = y4;
        d_u[j / 2 + 2] = u4;
        d_v[j / 2 + 2] = v4;
      }

      if (j < task->width - 5) {
        d_y[j + 5] = y5;
      }
    }
  }
}

static void
convert_v210_Y42B (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x >> 1;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x >> 1;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_v210_Y42B_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_I420_task (FConvertTask * task)
{
  gint i;
  gint l1, l2;

  for (i = task->height_0; i < task->height_1; i += 2) {
    GET_LINE_OFFSETS (task->interlaced, i, l1, l2);

    video_orc_convert_UYVY_I420 (FRAME_GET_COMP_LINE (task->dest, 0, l1),
        FRAME_GET_COMP_LINE (task->dest, 0, l2),
        FRAME_GET_COMP_LINE (task->dest, 1, i >> 1),
        FRAME_GET_COMP_LINE (task->dest, 2, i >> 1),
        FRAME_GET_LINE (task->src, l1), FRAME_GET_LINE (task->src, l2),
        (task->width + 1) / 2);
  }
}

static void
convert_UYVY_I420 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  gboolean interlaced = GST_VIDEO_FRAME_IS_INTERLACED (src)
      && (GST_VIDEO_INFO_INTERLACE_MODE (&src->info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE);
  gint h2;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  /* I420 has half as many chroma lines, as such we have to
   * always merge two into one. For non-interlaced these are
   * the two next to each other, for interlaced one is skipped
   * in between. */
  if (interlaced)
    h2 = GST_ROUND_DOWN_4 (height);
  else
    h2 = GST_ROUND_DOWN_2 (height);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((h2 + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].interlaced = interlaced;
    tasks[i].width = width;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (h2, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_I420_task, (gpointer) tasks_p);

  /* now handle last lines. For interlaced these are up to 3 */
  if (h2 != height) {
    for (i = h2; i < height; i++) {
      UNPACK_FRAME (src, convert->tmpline[0], i, convert->in_x, width);
      PACK_FRAME (dest, convert->tmpline[0], i, width);
    }
  }
}

static void
convert_UYVY_AYUV_task (FConvertPlaneTask * task)
{
  video_orc_convert_UYVY_AYUV (task->d, task->dstride, task->s,
      task->sstride, task->alpha, (task->width + 1) / 2, task->height);
}

static void
convert_UYVY_AYUV (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  guint8 alpha = MIN (convert->alpha_value, 255);
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].alpha = alpha;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_AYUV_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_v210_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d;
  const guint8 *s;
  guint32 a0, a1, a2, a3;
  guint8 y0, y1, y2, y3, y4, y5;
  guint8 u0, u2, u4;
  guint8 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d = task->d + i * task->dstride;
    s = task->s + i * task->sstride;

    for (j = 0; j < task->width; j += 6) {
      y1 = y2 = y3 = y4 = y5 = 0;
      u2 = u4 = v2 = v4 = 0;

      y0 = s[2 * j + 1];
      u0 = s[2 * j];
      v0 = s[2 * j + 2];

      if (j < task->width - 1) {
        y1 = s[2 * j + 3];
      }

      if (j < task->width - 2) {
        y2 = s[2 * j + 5];
        u2 = s[2 * j + 4];
        v2 = s[2 * j + 6];
      }

      if (j < task->width - 3) {
        y3 = s[2 * j + 7];
      }

      if (j < task->width - 4) {
        y4 = s[2 * j + 9];
        u4 = s[2 * j + 8];
        v4 = s[2 * j + 10];
      }

      if (j < task->width - 5) {
        y5 = s[2 * j + 11];
      }

      a0 = u0 << 2 | (y0 << 12) | (v0 << 22);
      a1 = y1 << 2 | (u2 << 12) | (y2 << 22);
      a2 = v2 << 2 | (y3 << 12) | (u4 << 22);
      a3 = y4 << 2 | (v4 << 12) | (y5 << 22);

      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 0, a0);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 4, a1);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 8, a2);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 12, a3);
    }
  }
}

static void
convert_UYVY_v210 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_v210_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_YUY2_task (FConvertPlaneTask * task)
{
  video_orc_convert_UYVY_YUY2 (task->d, task->dstride, task->s,
      task->sstride, (task->width + 1) / 2, task->height);
}

static void
convert_UYVY_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_YUY2_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_v210_UYVY_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d;
  const guint8 *s;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d = task->d + i * task->dstride;
    s = task->s + i * task->sstride;

    for (j = 0; j < task->width; j += 6) {
      a0 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 0);
      a1 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 4);
      a2 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 8);
      a3 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 12);

      u0 = ((a0 >> 0) & 0x3ff) >> 2;
      y0 = ((a0 >> 10) & 0x3ff) >> 2;
      v0 = ((a0 >> 20) & 0x3ff) >> 2;
      y1 = ((a1 >> 0) & 0x3ff) >> 2;

      u2 = ((a1 >> 10) & 0x3ff) >> 2;
      y2 = ((a1 >> 20) & 0x3ff) >> 2;
      v2 = ((a2 >> 0) & 0x3ff) >> 2;
      y3 = ((a2 >> 10) & 0x3ff) >> 2;

      u4 = ((a2 >> 20) & 0x3ff) >> 2;
      y4 = ((a3 >> 0) & 0x3ff) >> 2;
      v4 = ((a3 >> 10) & 0x3ff) >> 2;
      y5 = ((a3 >> 20) & 0x3ff) >> 2;

      d[2 * j + 1] = y0;
      d[2 * j] = u0;
      d[2 * j + 2] = v0;

      if (j < task->width - 1) {
        d[2 * j + 3] = y1;
      }

      if (j < task->width - 2) {
        d[2 * j + 5] = y2;
        d[2 * j + 4] = u2;
        d[2 * j + 6] = v2;
      }

      if (j < task->width - 3) {
        d[2 * j + 7] = y3;
      }

      if (j < task->width - 4) {
        d[2 * j + 9] = y4;
        d[2 * j + 8] = u4;
        d[2 * j + 10] = v4;
      }

      if (j < task->width - 5) {
        d[2 * j + 11] = y5;
      }
    }
  }
}

static void
convert_v210_UYVY (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_v210_UYVY_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_v210_YUY2_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d;
  const guint8 *s;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d = task->d + i * task->dstride;
    s = task->s + i * task->sstride;

    for (j = 0; j < task->width; j += 6) {
      a0 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 0);
      a1 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 4);
      a2 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 8);
      a3 = GST_READ_UINT32_LE (s + (j / 6) * 16 + 12);

      u0 = ((a0 >> 0) & 0x3ff) >> 2;
      y0 = ((a0 >> 10) & 0x3ff) >> 2;
      v0 = ((a0 >> 20) & 0x3ff) >> 2;
      y1 = ((a1 >> 0) & 0x3ff) >> 2;

      u2 = ((a1 >> 10) & 0x3ff) >> 2;
      y2 = ((a1 >> 20) & 0x3ff) >> 2;
      v2 = ((a2 >> 0) & 0x3ff) >> 2;
      y3 = ((a2 >> 10) & 0x3ff) >> 2;

      u4 = ((a2 >> 20) & 0x3ff) >> 2;
      y4 = ((a3 >> 0) & 0x3ff) >> 2;
      v4 = ((a3 >> 10) & 0x3ff) >> 2;
      y5 = ((a3 >> 20) & 0x3ff) >> 2;

      d[2 * j] = y0;
      d[2 * j + 1] = u0;
      d[2 * j + 3] = v0;

      if (j < task->width - 1) {
        d[2 * j + 2] = y1;
      }

      if (j < task->width - 2) {
        d[2 * j + 4] = y2;
        d[2 * j + 5] = u2;
        d[2 * j + 7] = v2;
      }

      if (j < task->width - 3) {
        d[2 * j + 6] = y3;
      }

      if (j < task->width - 4) {
        d[2 * j + 8] = y4;
        d[2 * j + 9] = u4;
        d[2 * j + 11] = v4;
      }

      if (j < task->width - 5) {
        d[2 * j + 10] = y5;
      }
    }
  }
}

static void
convert_v210_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_v210_YUY2_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_Y42B_task (FConvertPlaneTask * task)
{
  video_orc_convert_UYVY_Y42B (task->d, task->dstride, task->du,
      task->dustride, task->dv, task->dvstride,
      task->s, task->sstride, (task->width + 1) / 2, task->height);
}

static void
convert_UYVY_Y42B (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x >> 1;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x >> 1;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_Y42B_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_Y444_task (FConvertPlaneTask * task)
{
  video_orc_convert_UYVY_Y444 (task->d,
      task->dstride, task->du,
      task->dustride, task->dv,
      task->dvstride, task->s,
      task->sstride, (task->width + 1) / 2, task->height);
}

static void
convert_UYVY_Y444 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (GST_ROUND_UP_2 (convert->in_x) * 2);

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_Y444_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_UYVY_GRAY8_task (FConvertPlaneTask * task)
{
  video_orc_convert_UYVY_GRAY8 (task->d, task->dstride, (guint16 *) task->s,
      task->sstride, task->width, task->height);
}

static void
convert_UYVY_GRAY8 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s;
  guint8 *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = GST_VIDEO_FRAME_PLANE_DATA (src, 0);
  d = GST_VIDEO_FRAME_PLANE_DATA (dest, 0);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_UYVY_GRAY8_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_I420_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_I420 (task->d,
      2 * task->dstride, task->d2,
      2 * task->dstride, task->du,
      task->dustride, task->dv,
      task->dvstride, task->s,
      2 * task->sstride, task->s2,
      2 * task->sstride, task->width / 2, task->height / 2);
}

static void
convert_AYUV_I420 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s1, *s2, *dy1, *dy2, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s1 = FRAME_GET_LINE (src, convert->in_y + 0);
  s1 += convert->in_x * 4;
  s2 = FRAME_GET_LINE (src, convert->in_y + 1);
  s2 += convert->in_x * 4;

  dy1 = FRAME_GET_Y_LINE (dest, convert->out_y + 0);
  dy1 += convert->out_x;
  dy2 = FRAME_GET_Y_LINE (dest, convert->out_y + 1);
  dy2 += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y >> 1);
  du += convert->out_x >> 1;
  dv = FRAME_GET_V_LINE (dest, convert->out_y >> 1);
  dv += convert->out_x >> 1;

  /* only for even width/height */

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = GST_ROUND_UP_2 ((height + n_threads - 1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy1 + i * lines_per_thread * tasks[i].dstride;
    tasks[i].d2 = dy2 + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride / 2;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride / 2;
    tasks[i].s = s1 + i * lines_per_thread * tasks[i].sstride;
    tasks[i].s2 = s2 + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_I420_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_YUY2_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_YUY2 (task->d, task->dstride, task->s,
      task->sstride, task->width / 2, task->height);
}

static void
convert_AYUV_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += convert->in_x * 4;
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  /* only for even width */
  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_YUY2_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_UYVY_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_UYVY (task->d, task->dstride, task->s,
      task->sstride, task->width / 2, task->height);
}

static void
convert_AYUV_UYVY (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += convert->in_x * 4;
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  /* only for even width */
  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_UYVY_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_Y42B_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_Y42B (task->d, task->dstride, task->du,
      task->dustride, task->dv, task->dvstride,
      task->s, task->sstride, task->width / 2, task->height);
}

static void
convert_AYUV_Y42B (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += convert->in_x * 4;

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x >> 1;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x >> 1;

  /* only works for even width */
  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_Y42B_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_Y444_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_Y444 (task->d, task->dstride, task->du,
      task->dustride, task->dv, task->dvstride,
      task->s, task->sstride, task->width, task->height);
}

static void
convert_AYUV_Y444 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *s, *dy, *du, *dv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += convert->in_x * 4;

  dy = FRAME_GET_Y_LINE (dest, convert->out_y);
  dy += convert->out_x;
  du = FRAME_GET_U_LINE (dest, convert->out_y);
  du += convert->out_x;
  dv = FRAME_GET_V_LINE (dest, convert->out_y);
  dv += convert->out_x;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_Y_STRIDE (dest);
    tasks[i].dustride = FRAME_GET_U_STRIDE (dest);
    tasks[i].dvstride = FRAME_GET_V_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = dy + i * lines_per_thread * tasks[i].dstride;
    tasks[i].du = du + i * lines_per_thread * tasks[i].dustride;
    tasks[i].dv = dv + i * lines_per_thread * tasks[i].dvstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_Y444_task, (gpointer) tasks_p);
  convert_fill_border (convert, dest);
}

static void
convert_Y42B_YUY2_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y42B_YUY2 (task->d, task->dstride,
      task->s, task->sstride,
      task->su, task->sustride,
      task->sv, task->svstride, (task->width + 1) / 2, task->height);
}

static void
convert_Y42B_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x >> 1;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x >> 1;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y42B_YUY2_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y42B_UYVY_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y42B_UYVY (task->d, task->dstride,
      task->s, task->sstride,
      task->su, task->sustride,
      task->sv, task->svstride, (task->width + 1) / 2, task->height);
}

static void
convert_Y42B_UYVY (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x >> 1;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x >> 1;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y42B_UYVY_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y42B_AYUV_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y42B_AYUV (task->d, task->dstride, task->s,
      task->sstride,
      task->su,
      task->sustride,
      task->sv, task->svstride, task->alpha, task->width / 2, task->height);
}

static void
convert_Y42B_AYUV (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  guint8 alpha = MIN (convert->alpha_value, 255);
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x >> 1;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x >> 1;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += convert->out_x * 4;

  /* only for even width */
  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].alpha = alpha;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y42B_AYUV_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y42B_v210_task (FConvertPlaneTask * task)
{
  gint i, j;
  guint8 *d;
  const guint8 *s_y, *s_u, *s_v;
  guint32 a0, a1, a2, a3;
  guint8 y0, y1, y2, y3, y4, y5;
  guint8 u0, u2, u4;
  guint8 v0, v2, v4;

  for (i = 0; i < task->height; i++) {
    d = task->d + i * task->dstride;
    s_y = task->s + i * task->sstride;
    s_u = task->su + i * task->sustride;
    s_v = task->sv + i * task->svstride;

    for (j = 0; j < task->width; j += 6) {
      y1 = y2 = y3 = y4 = y5 = 0;
      u2 = u4 = v2 = v4 = 0;

      y0 = s_y[j];
      u0 = s_u[j / 2];
      v0 = s_v[j / 2];

      if (j < task->width - 1) {
        y1 = s_y[j + 1];
      }

      if (j < task->width - 2) {
        y2 = s_y[j + 2];
        u2 = s_u[j / 2 + 1];
        v2 = s_v[j / 2 + 1];
      }

      if (j < task->width - 3) {
        y3 = s_y[j + 3];
      }

      if (j < task->width - 4) {
        y4 = s_y[j + 4];
        u4 = s_u[j / 2 + 2];
        v4 = s_v[j / 2 + 2];
      }

      if (j < task->width - 5) {
        y5 = s_y[j + 5];
      }

      a0 = u0 << 2 | (y0 << 12) | (v0 << 22);
      a1 = y1 << 2 | (u2 << 12) | (y2 << 22);
      a2 = v2 << 2 | (y3 << 12) | (u4 << 22);
      a3 = y4 << 2 | (v4 << 12) | (y5 << 22);

      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 0, a0);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 4, a1);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 8, a2);
      GST_WRITE_UINT32_LE (d + (j / 6) * 16 + 12, a3);
    }
  }
}

static void
convert_Y42B_v210 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *d, *sy, *su, *sv;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x >> 1;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x >> 1;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y42B_v210_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y444_YUY2_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y444_YUY2 (task->d, task->dstride, task->s,
      task->sstride,
      task->su,
      task->sustride, task->sv, task->svstride, task->width / 2, task->height);
}

static void
convert_Y444_YUY2 (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y444_YUY2_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y444_UYVY_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y444_UYVY (task->d, task->dstride, task->s,
      task->sstride,
      task->su,
      task->sustride, task->sv, task->svstride, task->width / 2, task->height);
}

static void
convert_Y444_UYVY (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (GST_ROUND_UP_2 (convert->out_x) * 2);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y444_UYVY_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_Y444_AYUV_task (FConvertPlaneTask * task)
{
  video_orc_convert_Y444_AYUV (task->d, task->dstride, task->s,
      task->sstride,
      task->su,
      task->sustride,
      task->sv, task->svstride, task->alpha, task->width, task->height);
}

static void
convert_Y444_AYUV (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  guint8 *sy, *su, *sv, *d;
  guint8 alpha = MIN (convert->alpha_value, 255);
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  sy = FRAME_GET_Y_LINE (src, convert->in_y);
  sy += convert->in_x;
  su = FRAME_GET_U_LINE (src, convert->in_y);
  su += convert->in_x;
  sv = FRAME_GET_V_LINE (src, convert->in_y);
  sv += convert->in_x;

  d = FRAME_GET_LINE (dest, convert->out_y);
  d += convert->out_x * 4;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_Y_STRIDE (src);
    tasks[i].sustride = FRAME_GET_U_STRIDE (src);
    tasks[i].svstride = FRAME_GET_V_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = sy + i * lines_per_thread * tasks[i].sstride;
    tasks[i].su = su + i * lines_per_thread * tasks[i].sustride;
    tasks[i].sv = sv + i * lines_per_thread * tasks[i].svstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].alpha = alpha;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_Y444_AYUV_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
static void
convert_AYUV_ARGB_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_ARGB (task->d, task->dstride, task->s,
      task->sstride, task->data->im[0][0], task->data->im[0][2],
      task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
      task->width, task->height);
}

static void
convert_AYUV_ARGB (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (convert->in_x * 4);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].data = data;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_ARGB_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_BGRA_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_BGRA (task->d, task->dstride, task->s,
      task->sstride, task->data->im[0][0], task->data->im[0][2],
      task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
      task->width, task->height);
}

static void
convert_AYUV_BGRA (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (convert->in_x * 4);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].data = data;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_BGRA_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_ABGR_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_ABGR (task->d, task->dstride, task->s,
      task->sstride, task->data->im[0][0], task->data->im[0][2],
      task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
      task->width, task->height);
}

static void
convert_AYUV_ABGR (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (convert->in_x * 4);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].data = data;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_ABGR_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_AYUV_RGBA_task (FConvertPlaneTask * task)
{
  video_orc_convert_AYUV_RGBA (task->d, task->dstride, task->s,
      task->sstride, task->data->im[0][0], task->data->im[0][2],
      task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
      task->width, task->height);
}

static void
convert_AYUV_RGBA (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  guint8 *s, *d;
  FConvertPlaneTask *tasks;
  FConvertPlaneTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_LINE (src, convert->in_y);
  s += (convert->in_x * 4);
  d = FRAME_GET_LINE (dest, convert->out_y);
  d += (convert->out_x * 4);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertPlaneTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertPlaneTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_STRIDE (dest);
    tasks[i].sstride = FRAME_GET_STRIDE (src);
    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = width;
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, height);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].data = data;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_AYUV_RGBA_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}
#endif

static void
convert_I420_BGRA_task (FConvertTask * task)
{
  gint i;

  for (i = task->height_0; i < task->height_1; i++) {
    guint8 *sy, *su, *sv, *d;

    d = FRAME_GET_LINE (task->dest, i + task->out_y);
    d += (task->out_x * 4);
    sy = FRAME_GET_Y_LINE (task->src, i + task->in_y);
    sy += task->in_x;
    su = FRAME_GET_U_LINE (task->src, (i + task->in_y) >> 1);
    su += (task->in_x >> 1);
    sv = FRAME_GET_V_LINE (task->src, (i + task->in_y) >> 1);
    sv += (task->in_x >> 1);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    video_orc_convert_I420_BGRA (d, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#else
    video_orc_convert_I420_ARGB (d, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#endif
  }
}

static void
convert_I420_BGRA (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].width = width;
    tasks[i].data = data;
    tasks[i].in_x = convert->in_x;
    tasks[i].in_y = convert->in_y;
    tasks[i].out_x = convert->out_x;
    tasks[i].out_y = convert->out_y;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (height, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_BGRA_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_I420_ARGB_task (FConvertTask * task)
{
  gint i;

  for (i = task->height_0; i < task->height_1; i++) {
    guint8 *sy, *su, *sv, *d;

    d = FRAME_GET_LINE (task->dest, i + task->out_y);
    d += (task->out_x * 4);
    sy = FRAME_GET_Y_LINE (task->src, i + task->in_y);
    sy += task->in_x;
    su = FRAME_GET_U_LINE (task->src, (i + task->in_y) >> 1);
    su += (task->in_x >> 1);
    sv = FRAME_GET_V_LINE (task->src, (i + task->in_y) >> 1);
    sv += (task->in_x >> 1);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    video_orc_convert_I420_ARGB (d, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#else
    video_orc_convert_I420_BGRA (d, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#endif
  }
}

static void
convert_I420_ARGB (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].width = width;
    tasks[i].data = data;
    tasks[i].in_x = convert->in_x;
    tasks[i].in_y = convert->in_y;
    tasks[i].out_x = convert->out_x;
    tasks[i].out_y = convert->out_y;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (height, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_ARGB_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_I420_pack_ARGB_task (FConvertTask * task)
{
  gint i;
  gpointer d[GST_VIDEO_MAX_PLANES];

  d[0] = FRAME_GET_LINE (task->dest, 0);
  d[0] =
      (guint8 *) d[0] +
      task->out_x * GST_VIDEO_FORMAT_INFO_PSTRIDE (task->dest->info.finfo, 0);

  for (i = task->height_0; i < task->height_1; i++) {
    guint8 *sy, *su, *sv;

    sy = FRAME_GET_Y_LINE (task->src, i + task->in_y);
    sy += task->in_x;
    su = FRAME_GET_U_LINE (task->src, (i + task->in_y) >> 1);
    su += (task->in_x >> 1);
    sv = FRAME_GET_V_LINE (task->src, (i + task->in_y) >> 1);
    sv += (task->in_x >> 1);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    video_orc_convert_I420_ARGB (task->tmpline, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#else
    video_orc_convert_I420_BGRA (task->tmpline, sy, su, sv,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#endif
    task->dest->info.finfo->pack_func (task->dest->info.finfo,
        (GST_VIDEO_FRAME_IS_INTERLACED (task->dest) ?
            GST_VIDEO_PACK_FLAG_INTERLACED :
            GST_VIDEO_PACK_FLAG_NONE),
        task->tmpline, 0, d, task->dest->info.stride,
        task->dest->info.chroma_site, i + task->out_y, task->width);
  }
}

static void
convert_I420_pack_ARGB (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].width = width;
    tasks[i].data = data;
    tasks[i].in_x = convert->in_x;
    tasks[i].in_y = convert->in_y;
    tasks[i].out_x = convert->out_x;
    tasks[i].out_y = convert->out_y;
    tasks[i].tmpline = convert->tmpline[i];

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (height, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_I420_pack_ARGB_task,
      (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_A420_pack_ARGB_task (FConvertTask * task)
{
  gint i;
  gpointer d[GST_VIDEO_MAX_PLANES];

  d[0] = FRAME_GET_LINE (task->dest, 0);
  d[0] =
      (guint8 *) d[0] +
      task->out_x * GST_VIDEO_FORMAT_INFO_PSTRIDE (task->dest->info.finfo, 0);

  for (i = task->height_0; i < task->height_1; i++) {
    guint8 *sy, *su, *sv, *sa;

    sy = FRAME_GET_Y_LINE (task->src, i + task->in_y);
    sy += task->in_x;
    su = FRAME_GET_U_LINE (task->src, (i + task->in_y) >> 1);
    su += (task->in_x >> 1);
    sv = FRAME_GET_V_LINE (task->src, (i + task->in_y) >> 1);
    sv += (task->in_x >> 1);
    sa = FRAME_GET_A_LINE (task->src, i + task->in_y);
    sa += task->in_x;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    video_orc_convert_A420_ARGB (task->tmpline, sy, su, sv, sa,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#else
    video_orc_convert_A420_BGRA (task->tmpline, sy, su, sv, sa,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#endif

    task->dest->info.finfo->pack_func (task->dest->info.finfo,
        (GST_VIDEO_FRAME_IS_INTERLACED (task->dest) ?
            GST_VIDEO_PACK_FLAG_INTERLACED :
            GST_VIDEO_PACK_FLAG_NONE),
        task->tmpline, 0, d, task->dest->info.stride,
        task->dest->info.chroma_site, i + task->out_y, task->width);
  }
}

static void
convert_A420_pack_ARGB (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].width = width;
    tasks[i].data = data;
    tasks[i].in_x = convert->in_x;
    tasks[i].in_y = convert->in_y;
    tasks[i].out_x = convert->out_x;
    tasks[i].out_y = convert->out_y;
    tasks[i].tmpline = convert->tmpline[i];

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (height, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_A420_pack_ARGB_task,
      (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
convert_A420_BGRA_task (FConvertTask * task)
{
  gint i;

  for (i = task->height_0; i < task->height_1; i++) {
    guint8 *sy, *su, *sv, *sa, *d;

    d = FRAME_GET_LINE (task->dest, i + task->out_y);
    d += (task->out_x * 4);
    sy = FRAME_GET_Y_LINE (task->src, i + task->in_y);
    sy += task->in_x;
    su = FRAME_GET_U_LINE (task->src, (i + task->in_y) >> 1);
    su += (task->in_x >> 1);
    sv = FRAME_GET_V_LINE (task->src, (i + task->in_y) >> 1);
    sv += (task->in_x >> 1);
    sa = FRAME_GET_A_LINE (task->src, i + task->in_y);
    sa += task->in_x;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    video_orc_convert_A420_BGRA (d, sy, su, sv, sa,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#else
    video_orc_convert_A420_ARGB (d, sy, su, sv, sa,
        task->data->im[0][0], task->data->im[0][2],
        task->data->im[2][1], task->data->im[1][1], task->data->im[1][2],
        task->width);
#endif
  }
}

static void
convert_A420_BGRA (GstVideoConverter * convert, const GstVideoFrame * src,
    GstVideoFrame * dest)
{
  int i;
  gint width = convert->in_width;
  gint height = convert->in_height;
  MatrixData *data = &convert->convert_matrix;
  FConvertTask *tasks;
  FConvertTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[0] =
      g_renew (FConvertTask, convert->tasks[0], n_threads);
  tasks_p = convert->tasks_p[0] =
      g_renew (FConvertTask *, convert->tasks_p[0], n_threads);

  lines_per_thread = (height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].src = src;
    tasks[i].dest = dest;

    tasks[i].width = width;
    tasks[i].data = data;
    tasks[i].in_x = convert->in_x;
    tasks[i].in_y = convert->in_y;
    tasks[i].out_x = convert->out_x;
    tasks[i].out_y = convert->out_y;

    tasks[i].height_0 = i * lines_per_thread;
    tasks[i].height_1 = tasks[i].height_0 + lines_per_thread;
    tasks[i].height_1 = MIN (height, tasks[i].height_1);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_A420_BGRA_task, (gpointer) tasks_p);

  convert_fill_border (convert, dest);
}

static void
memset_u24 (guint8 * data, guint8 col[3], unsigned int n)
{
  unsigned int i;

  for (i = 0; i < n; i++) {
    data[0] = col[0];
    data[1] = col[1];
    data[2] = col[2];
    data += 3;
  }
}

static void
memset_u32_16 (guint8 * data, guint8 col[4], unsigned int n)
{
  unsigned int i;

  for (i = 0; i < n; i += 2) {
    data[0] = col[0];
    data[1] = col[1];
    if (i + 1 < n) {
      data[2] = col[2];
      data[3] = col[3];
    }
    data += 4;
  }
}

#define MAKE_BORDER_FUNC(func)                                                  \
        for (i = 0; i < out_y; i++)                                             \
          func (FRAME_GET_PLANE_LINE (dest, k, i), col, out_maxwidth);          \
        if (rb_width || lb_width) {                                             \
          for (i = 0; i < out_height; i++) {                                    \
            guint8 *d = FRAME_GET_PLANE_LINE (dest, k, i + out_y);              \
            if (lb_width)                                                       \
              func (d, col, lb_width);                                          \
            if (rb_width)                                                       \
              func (d + (pstride * r_border), col, rb_width);                   \
          }                                                                     \
        }                                                                       \
        for (i = out_y + out_height; i < out_maxheight; i++)                    \
          func (FRAME_GET_PLANE_LINE (dest, k, i), col, out_maxwidth);          \

static void
convert_fill_border (GstVideoConverter * convert, GstVideoFrame * dest)
{
  int k, n_planes;
  const GstVideoFormatInfo *out_finfo;

  if (!convert->fill_border || !convert->borderline)
    return;

  out_finfo = convert->out_info.finfo;

  n_planes = GST_VIDEO_FRAME_N_PLANES (dest);

  for (k = 0; k < n_planes; k++) {
    gint comp[GST_VIDEO_MAX_COMPONENTS];
    gint i, out_x, out_y, out_width, out_height, pstride, pgroup;
    gint r_border, lb_width, rb_width;
    gint out_maxwidth, out_maxheight;
    gpointer borders;

    gst_video_format_info_component (out_finfo, k, comp);
    out_x = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo, comp[0],
        convert->out_x);
    out_y = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (out_finfo, comp[0],
        convert->out_y);
    out_width = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo, comp[0],
        convert->out_width);
    out_height = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (out_finfo, comp[0],
        convert->out_height);
    out_maxwidth = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo, comp[0],
        convert->out_maxwidth);
    out_maxheight = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (out_finfo, comp[0],
        convert->out_maxheight);

    pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (out_finfo, comp[0]);

    switch (GST_VIDEO_FORMAT_INFO_FORMAT (out_finfo)) {
      case GST_VIDEO_FORMAT_YUY2:
      case GST_VIDEO_FORMAT_YVYU:
      case GST_VIDEO_FORMAT_UYVY:
        pgroup = 42;
        out_maxwidth = GST_ROUND_UP_2 (out_maxwidth);
        break;
      default:
        pgroup = pstride;
        break;
    }

    r_border = out_x + out_width;
    rb_width = out_maxwidth - r_border;
    lb_width = out_x;

    borders = &convert->borders[k];

    switch (pgroup) {
      case 1:
      {
        guint8 col = ((guint8 *) borders)[0];
        MAKE_BORDER_FUNC (memset);
        break;
      }
      case 2:
      {
        guint16 col = ((guint16 *) borders)[0];
        MAKE_BORDER_FUNC (video_orc_splat_u16);
        break;
      }
      case 3:
      {
        guint8 col[3];
        col[0] = ((guint8 *) borders)[0];
        col[1] = ((guint8 *) borders)[1];
        col[2] = ((guint8 *) borders)[2];
        MAKE_BORDER_FUNC (memset_u24);
        break;
      }
      case 4:
      {
        guint32 col = ((guint32 *) borders)[0];
        MAKE_BORDER_FUNC (video_orc_splat_u32);
        break;
      }
      case 8:
      {
        guint64 col = ((guint64 *) borders)[0];
        MAKE_BORDER_FUNC (video_orc_splat_u64);
        break;
      }
      case 42:
      {
        guint8 col[4];
        col[0] = ((guint8 *) borders)[0];
        col[2] = ((guint8 *) borders)[2];
        col[1] = ((guint8 *) borders)[r_border & 1 ? 3 : 1];
        col[3] = ((guint8 *) borders)[r_border & 1 ? 1 : 3];
        MAKE_BORDER_FUNC (memset_u32_16);
        break;
      }
      default:
        break;
    }
  }
}

typedef struct
{
  const guint8 *s, *s2;
  guint8 *d, *d2;
  gint sstride, dstride;
  gint width, height;
  gint fill;
} FSimpleScaleTask;

static void
convert_plane_fill_task (FSimpleScaleTask * task)
{
  video_orc_memset_2d (task->d, task->dstride,
      task->fill, task->width, task->height);
}

static void
convert_plane_fill (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *d;
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  d = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d += convert->fout_x[plane];

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread = (convert->fout_height[plane] + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].d = d + i * lines_per_thread * convert->fout_width[plane];

    tasks[i].fill = convert->ffill[plane];
    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;
    tasks[i].dstride = FRAME_GET_PLANE_STRIDE (dest, plane);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_fill_task, (gpointer) tasks_p);
}

static void
convert_plane_h_double_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_422_444 (task->d,
      task->dstride, task->s, task->sstride, task->width / 2, task->height);
}

static void
convert_plane_h_double (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s, *d;
  gint splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s += convert->fin_x[splane];
  d = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d += convert->fout_x[plane];

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread = (convert->fout_height[plane] + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_PLANE_STRIDE (dest, plane);
    tasks[i].sstride = FRAME_GET_PLANE_STRIDE (src, splane);

    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_h_double_task,
      (gpointer) tasks_p);
}

static void
convert_plane_h_halve_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_444_422 (task->d,
      task->dstride, task->s, task->sstride, task->width, task->height);
}

static void
convert_plane_h_halve (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s, *d;
  gint splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s += convert->fin_x[splane];
  d = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d += convert->fout_x[plane];

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread = (convert->fout_height[plane] + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].dstride = FRAME_GET_PLANE_STRIDE (dest, plane);
    tasks[i].sstride = FRAME_GET_PLANE_STRIDE (src, splane);

    tasks[i].d = d + i * lines_per_thread * tasks[i].dstride;
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_h_halve_task, (gpointer) tasks_p);
}

static void
convert_plane_v_double_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_420_422 (task->d, 2 * task->dstride, task->d2,
      2 * task->dstride, task->s, task->sstride, task->width, task->height / 2);
}

static void
convert_plane_v_double (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s, *d1, *d2;
  gint ds, splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s += convert->fin_x[splane];
  d1 = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d1 += convert->fout_x[plane];
  d2 = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane] + 1);
  d2 += convert->fout_x[plane];
  ds = FRAME_GET_PLANE_STRIDE (dest, plane);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread =
      GST_ROUND_UP_2 ((convert->fout_height[plane] + n_threads -
          1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].d = d1 + i * lines_per_thread * ds;
    tasks[i].d2 = d2 + i * lines_per_thread * ds;
    tasks[i].dstride = ds;
    tasks[i].sstride = FRAME_GET_PLANE_STRIDE (src, splane);
    tasks[i].s = s + i * lines_per_thread * tasks[i].sstride / 2;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_v_double_task,
      (gpointer) tasks_p);
}

static void
convert_plane_v_halve_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_422_420 (task->d, task->dstride, task->s,
      2 * task->sstride, task->s2, 2 * task->sstride, task->width,
      task->height);
}

static void
convert_plane_v_halve (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s1, *s2, *d;
  gint ss, ds, splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s1 = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s1 += convert->fin_x[splane];
  s2 = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane] + 1);
  s2 += convert->fin_x[splane];
  d = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d += convert->fout_x[plane];

  ss = FRAME_GET_PLANE_STRIDE (src, splane);
  ds = FRAME_GET_PLANE_STRIDE (dest, plane);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread = (convert->fout_height[plane] + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].d = d + i * lines_per_thread * ds;
    tasks[i].dstride = ds;
    tasks[i].s = s1 + i * lines_per_thread * ss * 2;
    tasks[i].s2 = s2 + i * lines_per_thread * ss * 2;
    tasks[i].sstride = ss;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_v_halve_task, (gpointer) tasks_p);
}

static void
convert_plane_hv_double_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_420_444 (task->d, 2 * task->dstride, task->d2,
      2 * task->dstride, task->s, task->sstride, (task->width + 1) / 2,
      task->height / 2);
}

static void
convert_plane_hv_double (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s, *d1, *d2;
  gint ss, ds, splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s += convert->fin_x[splane];
  d1 = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d1 += convert->fout_x[plane];
  d2 = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane] + 1);
  d2 += convert->fout_x[plane];
  ss = FRAME_GET_PLANE_STRIDE (src, splane);
  ds = FRAME_GET_PLANE_STRIDE (dest, plane);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread =
      GST_ROUND_UP_2 ((convert->fout_height[plane] + n_threads -
          1) / n_threads);

  for (i = 0; i < n_threads; i++) {
    tasks[i].d = d1 + i * lines_per_thread * ds;
    tasks[i].d2 = d2 + i * lines_per_thread * ds;
    tasks[i].dstride = ds;
    tasks[i].sstride = ss;
    tasks[i].s = s + i * lines_per_thread * ss / 2;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_hv_double_task,
      (gpointer) tasks_p);
}

static void
convert_plane_hv_halve_task (FSimpleScaleTask * task)
{
  video_orc_planar_chroma_444_420 (task->d, task->dstride, task->s,
      2 * task->sstride, task->s2, 2 * task->sstride, task->width,
      task->height);
}

static void
convert_plane_hv_halve (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  guint8 *s1, *s2, *d;
  gint ss, ds, splane = convert->fsplane[plane];
  FSimpleScaleTask *tasks;
  FSimpleScaleTask **tasks_p;
  gint n_threads;
  gint lines_per_thread;
  gint i;

  s1 = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane]);
  s1 += convert->fin_x[splane];
  s2 = FRAME_GET_PLANE_LINE (src, splane, convert->fin_y[splane] + 1);
  s2 += convert->fin_x[splane];
  d = FRAME_GET_PLANE_LINE (dest, plane, convert->fout_y[plane]);
  d += convert->fout_x[plane];
  ss = FRAME_GET_PLANE_STRIDE (src, splane);
  ds = FRAME_GET_PLANE_STRIDE (dest, plane);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FSimpleScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FSimpleScaleTask *, convert->tasks_p[plane], n_threads);
  lines_per_thread = (convert->fout_height[plane] + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].d = d + i * lines_per_thread * ds;
    tasks[i].dstride = ds;
    tasks[i].s = s1 + i * lines_per_thread * ss * 2;
    tasks[i].s2 = s2 + i * lines_per_thread * ss * 2;
    tasks[i].sstride = ss;

    tasks[i].width = convert->fout_width[plane];
    tasks[i].height = (i + 1) * lines_per_thread;
    tasks[i].height = MIN (tasks[i].height, convert->fout_height[plane]);
    tasks[i].height -= i * lines_per_thread;

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_hv_halve_task,
      (gpointer) tasks_p);
}

typedef struct
{
  GstVideoScaler *h_scaler, *v_scaler;
  GstVideoFormat format;
  const guint8 *s;
  guint8 *d;
  gint sstride, dstride;
  guint x, y, w, h;
} FScaleTask;

static void
convert_plane_hv_task (FScaleTask * task)
{
  gst_video_scaler_2d (task->h_scaler, task->v_scaler, task->format,
      (guint8 *) task->s, task->sstride,
      task->d, task->dstride, task->x, task->y, task->w, task->h);
}

static void
convert_plane_hv (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest, gint plane)
{
  gint in_x, in_y, out_x, out_y, out_width, out_height;
  GstVideoFormat format;
  gint splane = convert->fsplane[plane];
  guint8 *s, *d;
  gint sstride, dstride;
  FScaleTask *tasks;
  FScaleTask **tasks_p;
  gint i, n_threads, lines_per_thread;

  in_x = convert->fin_x[splane];
  in_y = convert->fin_y[splane];
  out_x = convert->fout_x[plane];
  out_y = convert->fout_y[plane];
  out_width = convert->fout_width[plane];
  out_height = convert->fout_height[plane];
  format = convert->fformat[plane];

  s = FRAME_GET_PLANE_LINE (src, splane, in_y);
  s += in_x;
  d = FRAME_GET_PLANE_LINE (dest, plane, out_y);
  d += out_x;

  sstride = FRAME_GET_PLANE_STRIDE (src, splane);
  dstride = FRAME_GET_PLANE_STRIDE (dest, plane);

  n_threads = convert->conversion_runner->n_threads;
  tasks = convert->tasks[plane] =
      g_renew (FScaleTask, convert->tasks[plane], n_threads);
  tasks_p = convert->tasks_p[plane] =
      g_renew (FScaleTask *, convert->tasks_p[plane], n_threads);

  lines_per_thread = (out_height + n_threads - 1) / n_threads;

  for (i = 0; i < n_threads; i++) {
    tasks[i].h_scaler =
        convert->fh_scaler[plane].scaler ? convert->
        fh_scaler[plane].scaler[i] : NULL;
    tasks[i].v_scaler =
        convert->fv_scaler[plane].scaler ? convert->
        fv_scaler[plane].scaler[i] : NULL;
    tasks[i].format = format;
    tasks[i].s = s;
    tasks[i].d = d;
    tasks[i].sstride = sstride;
    tasks[i].dstride = dstride;

    tasks[i].x = 0;
    tasks[i].w = out_width;

    tasks[i].y = i * lines_per_thread;
    tasks[i].h = tasks[i].y + lines_per_thread;
    tasks[i].h = MIN (out_height, tasks[i].h);

    tasks_p[i] = &tasks[i];
  }

  gst_parallelized_task_runner_run (convert->conversion_runner,
      (GstParallelizedTaskFunc) convert_plane_hv_task, (gpointer) tasks_p);
}

static void
convert_scale_planes (GstVideoConverter * convert,
    const GstVideoFrame * src, GstVideoFrame * dest)
{
  int i, n_planes;

  n_planes = GST_VIDEO_FRAME_N_PLANES (dest);
  for (i = 0; i < n_planes; i++) {
    if (convert->fconvert[i])
      convert->fconvert[i] (convert, src, dest, i);
  }
  convert_fill_border (convert, dest);
}

static GstVideoFormat
get_scale_format (GstVideoFormat format, gint plane)
{
  GstVideoFormat res = GST_VIDEO_FORMAT_UNKNOWN;

  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_Y41B:
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_GRAY8:
    case GST_VIDEO_FORMAT_A420:
    case GST_VIDEO_FORMAT_A422:
    case GST_VIDEO_FORMAT_A444:
    case GST_VIDEO_FORMAT_YUV9:
    case GST_VIDEO_FORMAT_YVU9:
    case GST_VIDEO_FORMAT_GBR:
    case GST_VIDEO_FORMAT_GBRA:
    case GST_VIDEO_FORMAT_RGBP:
    case GST_VIDEO_FORMAT_BGRP:
      res = GST_VIDEO_FORMAT_GRAY8;
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      res = GST_VIDEO_FORMAT_GRAY16_BE;
      break;
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_RGB:
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_v308:
    case GST_VIDEO_FORMAT_IYU2:
    case GST_VIDEO_FORMAT_ARGB64:
    case GST_VIDEO_FORMAT_ARGB64_LE:
    case GST_VIDEO_FORMAT_ARGB64_BE:
    case GST_VIDEO_FORMAT_RGBA64_BE:
    case GST_VIDEO_FORMAT_RGBA64_LE:
    case GST_VIDEO_FORMAT_BGRA64_BE:
    case GST_VIDEO_FORMAT_BGRA64_LE:
    case GST_VIDEO_FORMAT_ABGR64_BE:
    case GST_VIDEO_FORMAT_ABGR64_LE:
    case GST_VIDEO_FORMAT_AYUV64:
    case GST_VIDEO_FORMAT_RBGA:
      res = format;
      break;
    case GST_VIDEO_FORMAT_RGB15:
    case GST_VIDEO_FORMAT_BGR15:
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR16:
      res = GST_VIDEO_FORMAT_NV12;
      break;
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_NV61:
    case GST_VIDEO_FORMAT_NV24:
      res = plane == 0 ? GST_VIDEO_FORMAT_GRAY8 : GST_VIDEO_FORMAT_NV12;
      break;
    case GST_VIDEO_FORMAT_AV12:
      res = (plane == 0
          || plane == 2) ? GST_VIDEO_FORMAT_GRAY8 : GST_VIDEO_FORMAT_NV12;
      break;
    case GST_VIDEO_FORMAT_UNKNOWN:
    case GST_VIDEO_FORMAT_ENCODED:
    case GST_VIDEO_FORMAT_v210:
    case GST_VIDEO_FORMAT_v216:
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_UYVP:
    case GST_VIDEO_FORMAT_RGB8P:
    case GST_VIDEO_FORMAT_IYU1:
    case GST_VIDEO_FORMAT_r210:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_I420_12BE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I422_12BE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_Y444_12BE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_GBR_10BE:
    case GST_VIDEO_FORMAT_GBR_10LE:
    case GST_VIDEO_FORMAT_GBRA_10BE:
    case GST_VIDEO_FORMAT_GBRA_10LE:
    case GST_VIDEO_FORMAT_GBR_12BE:
    case GST_VIDEO_FORMAT_GBR_12LE:
    case GST_VIDEO_FORMAT_GBRA_12BE:
    case GST_VIDEO_FORMAT_GBRA_12LE:
    case GST_VIDEO_FORMAT_GBR_16BE:
    case GST_VIDEO_FORMAT_GBR_16LE:
    case GST_VIDEO_FORMAT_NV12_64Z32:
    case GST_VIDEO_FORMAT_NV12_4L4:
    case GST_VIDEO_FORMAT_NV12_32L32:
    case GST_VIDEO_FORMAT_NV12_16L32S:
    case GST_VIDEO_FORMAT_A420_10BE:
    case GST_VIDEO_FORMAT_A420_10LE:
    case GST_VIDEO_FORMAT_A422_10BE:
    case GST_VIDEO_FORMAT_A422_10LE:
    case GST_VIDEO_FORMAT_A444_10BE:
    case GST_VIDEO_FORMAT_A444_10LE:
    case GST_VIDEO_FORMAT_A444_12BE:
    case GST_VIDEO_FORMAT_A444_12LE:
    case GST_VIDEO_FORMAT_A422_12BE:
    case GST_VIDEO_FORMAT_A422_12LE:
    case GST_VIDEO_FORMAT_A420_12BE:
    case GST_VIDEO_FORMAT_A420_12LE:
    case GST_VIDEO_FORMAT_A444_16BE:
    case GST_VIDEO_FORMAT_A444_16LE:
    case GST_VIDEO_FORMAT_A422_16BE:
    case GST_VIDEO_FORMAT_A422_16LE:
    case GST_VIDEO_FORMAT_A420_16BE:
    case GST_VIDEO_FORMAT_A420_16LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_GRAY10_LE32:
    case GST_VIDEO_FORMAT_NV12_10LE32:
    case GST_VIDEO_FORMAT_NV16_10LE32:
    case GST_VIDEO_FORMAT_NV12_10LE40:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_Y444_16BE:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_P016_BE:
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P012_BE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_Y212_BE:
    case GST_VIDEO_FORMAT_Y212_LE:
    case GST_VIDEO_FORMAT_Y412_BE:
    case GST_VIDEO_FORMAT_Y412_LE:
    case GST_VIDEO_FORMAT_NV12_8L128:
    case GST_VIDEO_FORMAT_NV12_10BE_8L128:
    case GST_VIDEO_FORMAT_NV12_10LE40_4L4:
    case GST_VIDEO_FORMAT_DMA_DRM:
    case GST_VIDEO_FORMAT_MT2110T:
    case GST_VIDEO_FORMAT_MT2110R:
      res = format;
      g_assert_not_reached ();
      break;
  }
  return res;
}

static gboolean
is_merge_yuv (GstVideoInfo * info)
{
  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
    case GST_VIDEO_FORMAT_VYUY:
      return TRUE;
    default:
      return FALSE;
  }
}

static gboolean
setup_scale (GstVideoConverter * convert)
{
  int i, n_planes;
  gint method, cr_method, in_width, in_height, out_width, out_height;
  guint taps;
  GstVideoInfo *in_info, *out_info;
  const GstVideoFormatInfo *in_finfo, *out_finfo;
  GstVideoFormat in_format, out_format;
  gboolean interlaced;
  guint n_threads = convert->conversion_runner->n_threads;

  in_info = &convert->in_info;
  out_info = &convert->out_info;

  in_finfo = in_info->finfo;
  out_finfo = out_info->finfo;

  n_planes = GST_VIDEO_INFO_N_PLANES (out_info);

  interlaced = GST_VIDEO_INFO_IS_INTERLACED (&convert->in_info)
      && GST_VIDEO_INFO_INTERLACE_MODE (&convert->in_info) !=
      GST_VIDEO_INTERLACE_MODE_ALTERNATE;

  method = GET_OPT_RESAMPLER_METHOD (convert);
  if (method == GST_VIDEO_RESAMPLER_METHOD_NEAREST)
    cr_method = method;
  else
    cr_method = GET_OPT_CHROMA_RESAMPLER_METHOD (convert);
  taps = GET_OPT_RESAMPLER_TAPS (convert);

  in_format = GST_VIDEO_INFO_FORMAT (in_info);
  out_format = GST_VIDEO_INFO_FORMAT (out_info);

  switch (in_format) {
    case GST_VIDEO_FORMAT_RGB15:
    case GST_VIDEO_FORMAT_RGB16:
    case GST_VIDEO_FORMAT_BGR15:
    case GST_VIDEO_FORMAT_BGR16:
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    case GST_VIDEO_FORMAT_GRAY16_BE:
#else
    case GST_VIDEO_FORMAT_GRAY16_LE:
#endif
      if (method != GST_VIDEO_RESAMPLER_METHOD_NEAREST) {
        GST_LOG ("%s only with nearest resampling",
            gst_video_format_to_string (in_format));
        return FALSE;
      }
      break;
    default:
      break;
  }

  in_width = convert->in_width;
  in_height = convert->in_height;
  out_width = convert->out_width;
  out_height = convert->out_height;

  if (n_planes == 1 && !GST_VIDEO_FORMAT_INFO_IS_GRAY (out_finfo)) {
    gint pstride;
    guint j;

    if (is_merge_yuv (in_info)) {
      GstVideoScaler *y_scaler, *uv_scaler;

      if (in_width != out_width) {
        convert->fh_scaler[0].scaler = g_new (GstVideoScaler *, n_threads);
        for (j = 0; j < n_threads; j++) {
          y_scaler =
              gst_video_scaler_new (method, GST_VIDEO_SCALER_FLAG_NONE, taps,
              GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (in_finfo, GST_VIDEO_COMP_Y,
                  in_width), GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo,
                  GST_VIDEO_COMP_Y, out_width), convert->config);
          uv_scaler =
              gst_video_scaler_new (method, GST_VIDEO_SCALER_FLAG_NONE,
              gst_video_scaler_get_max_taps (y_scaler),
              GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (in_finfo, GST_VIDEO_COMP_U,
                  in_width), GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo,
                  GST_VIDEO_COMP_U, out_width), convert->config);

          convert->fh_scaler[0].scaler[j] =
              gst_video_scaler_combine_packed_YUV (y_scaler, uv_scaler,
              in_format, out_format);

          gst_video_scaler_free (y_scaler);
          gst_video_scaler_free (uv_scaler);
        }
      } else {
        convert->fh_scaler[0].scaler = NULL;
      }

      pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (out_finfo, GST_VIDEO_COMP_Y);
      convert->fin_x[0] = GST_ROUND_UP_2 (convert->in_x) * pstride;
      convert->fout_x[0] = GST_ROUND_UP_2 (convert->out_x) * pstride;

    } else {
      if (in_width != out_width && in_width != 0 && out_width != 0) {
        convert->fh_scaler[0].scaler = g_new (GstVideoScaler *, n_threads);
        for (j = 0; j < n_threads; j++) {
          convert->fh_scaler[0].scaler[j] =
              gst_video_scaler_new (method, GST_VIDEO_SCALER_FLAG_NONE, taps,
              in_width, out_width, convert->config);
        }
      } else {
        convert->fh_scaler[0].scaler = NULL;
      }

      pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (out_finfo, GST_VIDEO_COMP_R);
      convert->fin_x[0] = convert->in_x * pstride;
      convert->fout_x[0] = convert->out_x * pstride;
    }

    if (in_height != out_height && in_height != 0 && out_height != 0) {
      convert->fv_scaler[0].scaler = g_new (GstVideoScaler *, n_threads);

      for (j = 0; j < n_threads; j++) {
        convert->fv_scaler[0].scaler[j] =
            gst_video_scaler_new (method,
            interlaced ?
            GST_VIDEO_SCALER_FLAG_INTERLACED : GST_VIDEO_SCALER_FLAG_NONE, taps,
            in_height, out_height, convert->config);
      }
    } else {
      convert->fv_scaler[0].scaler = NULL;
    }

    convert->fin_y[0] = convert->in_y;
    convert->fout_y[0] = convert->out_y;
    convert->fout_width[0] = out_width;
    convert->fout_height[0] = out_height;
    convert->fconvert[0] = convert_plane_hv;
    convert->fformat[0] = get_scale_format (in_format, 0);
    convert->fsplane[0] = 0;
  } else {
    for (i = 0; i < n_planes; i++) {
      gint out_comp[GST_VIDEO_MAX_COMPONENTS];
      gint comp, j, iw, ih, ow, oh, pstride;
      gboolean need_v_scaler, need_h_scaler;
      GstStructure *config;
      gint resample_method;

      gst_video_format_info_component (out_finfo, i, out_comp);
      ow = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo, out_comp[0],
          out_width);
      oh = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (out_finfo, out_comp[0],
          out_height);
      pstride = GST_VIDEO_FORMAT_INFO_PSTRIDE (out_finfo, out_comp[0]);

      /* find the component in this plane and map it to the plane of
       * the source */
      if (out_comp[0] < GST_VIDEO_FORMAT_INFO_N_COMPONENTS (in_finfo)) {
        comp = out_comp[0];
        iw = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (in_finfo, comp, in_width);
        ih = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (in_finfo, comp, in_height);
        convert->fin_x[i] = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (in_finfo, comp,
            convert->in_x);
        convert->fin_x[i] *= pstride;
        convert->fin_y[i] = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (in_finfo, comp,
            convert->in_y);
      } else {
        /* we will use a fill instead, setting the parameters to an invalid
         * size to reduce confusion */
        comp = -1;
        iw = ih = -1;
        convert->fin_x[i] = -1;
        convert->fin_y[i] = -1;
      }

      convert->fout_width[i] = ow;
      convert->fout_height[i] = oh;

      convert->fout_x[i] = GST_VIDEO_FORMAT_INFO_SCALE_WIDTH (out_finfo,
          out_comp[0], convert->out_x);
      convert->fout_x[i] *= pstride;
      convert->fout_y[i] = GST_VIDEO_FORMAT_INFO_SCALE_HEIGHT (out_finfo,
          out_comp[0], convert->out_y);

      GST_LOG ("plane %d: %dx%d -> %dx%d", i, iw, ih, ow, oh);
      GST_LOG ("plane %d: pstride %d", i, pstride);
      GST_LOG ("plane %d: in_x %d, in_y %d", i, convert->fin_x[i],
          convert->fin_y[i]);
      GST_LOG ("plane %d: out_x %d, out_y %d", i, convert->fout_x[i],
          convert->fout_y[i]);

      if (comp == -1) {
        convert->fconvert[i] = convert_plane_fill;
        if (GST_VIDEO_INFO_IS_YUV (out_info)) {
          if (i == 3)
            convert->ffill[i] = convert->alpha_value;
          if (i == 0)
            convert->ffill[i] = 0x00;
          else
            convert->ffill[i] = 0x80;
        } else {
          if (i == 3)
            convert->ffill[i] = convert->alpha_value;
          else
            convert->ffill[i] = 0x00;
        }
        GST_LOG ("plane %d fill %02x", i, convert->ffill[i]);
        continue;
      } else {
        convert->fsplane[i] = GST_VIDEO_FORMAT_INFO_PLANE (in_finfo, comp);
        GST_LOG ("plane %d -> %d (comp %d)", i, convert->fsplane[i], comp);
      }

      config = gst_structure_copy (convert->config);

      resample_method = (i == 0 ? method : cr_method);

      need_v_scaler = FALSE;
      need_h_scaler = FALSE;
      if (iw == ow) {
        if (!interlaced && ih == oh) {
          convert->fconvert[i] = convert_plane_hv;
          GST_LOG ("plane %d: copy", i);
        } else if (!interlaced && ih == 2 * oh && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_LINEAR) {
          convert->fconvert[i] = convert_plane_v_halve;
          GST_LOG ("plane %d: vertical halve", i);
        } else if (!interlaced && 2 * ih == oh && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_NEAREST) {
          convert->fconvert[i] = convert_plane_v_double;
          GST_LOG ("plane %d: vertical double", i);
        } else {
          convert->fconvert[i] = convert_plane_hv;
          GST_LOG ("plane %d: vertical scale", i);
          need_v_scaler = TRUE;
        }
      } else if (ih == oh) {
        if (!interlaced && iw == 2 * ow && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_LINEAR) {
          convert->fconvert[i] = convert_plane_h_halve;
          GST_LOG ("plane %d: horizontal halve", i);
        } else if (!interlaced && 2 * iw == ow && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_NEAREST) {
          convert->fconvert[i] = convert_plane_h_double;
          GST_LOG ("plane %d: horizontal double", i);
        } else {
          convert->fconvert[i] = convert_plane_hv;
          GST_LOG ("plane %d: horizontal scale", i);
          need_h_scaler = TRUE;
        }
      } else {
        if (!interlaced && iw == 2 * ow && ih == 2 * oh && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_LINEAR) {
          convert->fconvert[i] = convert_plane_hv_halve;
          GST_LOG ("plane %d: horizontal/vertical halve", i);
        } else if (!interlaced && 2 * iw == ow && 2 * ih == oh && pstride == 1
            && resample_method == GST_VIDEO_RESAMPLER_METHOD_NEAREST) {
          convert->fconvert[i] = convert_plane_hv_double;
          GST_LOG ("plane %d: horizontal/vertical double", i);
        } else {
          convert->fconvert[i] = convert_plane_hv;
          GST_LOG ("plane %d: horizontal/vertical scale", i);
          need_v_scaler = TRUE;
          need_h_scaler = TRUE;
        }
      }

      if (need_h_scaler && iw != 0 && ow != 0) {
        convert->fh_scaler[i].scaler = g_new (GstVideoScaler *, n_threads);

        for (j = 0; j < n_threads; j++) {
          convert->fh_scaler[i].scaler[j] =
              gst_video_scaler_new (resample_method, GST_VIDEO_SCALER_FLAG_NONE,
              taps, iw, ow, config);
        }
      } else {
        convert->fh_scaler[i].scaler = NULL;
      }

      if (need_v_scaler && ih != 0 && oh != 0) {
        convert->fv_scaler[i].scaler = g_new (GstVideoScaler *, n_threads);

        for (j = 0; j < n_threads; j++) {
          convert->fv_scaler[i].scaler[j] =
              gst_video_scaler_new (resample_method,
              interlaced ?
              GST_VIDEO_SCALER_FLAG_INTERLACED : GST_VIDEO_SCALER_FLAG_NONE,
              taps, ih, oh, config);
        }
      } else {
        convert->fv_scaler[i].scaler = NULL;
      }

      gst_structure_free (config);
      convert->fformat[i] = get_scale_format (in_format, i);
    }
  }

  return TRUE;
}

/* Fast paths */

typedef struct
{
  GstVideoFormat in_format;
  GstVideoFormat out_format;
  gboolean keeps_interlaced;
  gboolean needs_color_matrix;
  gboolean keeps_size;
  gboolean do_crop;
  gboolean do_border;
  gboolean alpha_copy;
  gboolean alpha_set;
  gboolean alpha_mult;
  gint width_align, height_align;
  void (*convert) (GstVideoConverter * convert, const GstVideoFrame * src,
      GstVideoFrame * dest);
} VideoTransform;

static const VideoTransform transforms[] = {
  /* planar -> packed */
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_YUY2},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_UYVY},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, TRUE, FALSE, 0, 0, convert_I420_AYUV},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_v210, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_v210},

  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_YUY2},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_UYVY},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, TRUE, FALSE, 0, 0, convert_I420_AYUV},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_v210, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_I420_v210},

  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_Y42B_YUY2},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_Y42B_UYVY},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 1, 0, convert_Y42B_AYUV},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_v210, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_Y42B_v210},

  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 0, convert_Y444_YUY2},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 0, convert_Y444_UYVY},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_Y444_AYUV},

  /* packed -> packed */
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_YUY2},      /* alias */
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 1, 0, convert_YUY2_AYUV},
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_v210, TRUE, FALSE, TRUE, FALSE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_YUY2_v210},

  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_YUY2},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_UYVY_AYUV},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_v210, TRUE, FALSE, TRUE, FALSE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_v210},

  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_AYUV, TRUE, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 0, convert_AYUV_YUY2},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 0, convert_AYUV_UYVY},

  {GST_VIDEO_FORMAT_v210, GST_VIDEO_FORMAT_UYVY, TRUE, FALSE, TRUE, FALSE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_v210_UYVY},
  {GST_VIDEO_FORMAT_v210, GST_VIDEO_FORMAT_YUY2, TRUE, FALSE, TRUE, FALSE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_v210_YUY2},

  /* packed -> planar */
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_I420, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_YUY2_I420},
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_YV12, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_YUY2_I420},
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_Y42B, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_YUY2_Y42B},
  {GST_VIDEO_FORMAT_YUY2, GST_VIDEO_FORMAT_Y444, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_YUY2_Y444},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_GRAY8, TRUE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_GRAY8},

  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_I420, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_I420},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_YV12, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_I420},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_Y42B, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_Y42B},
  {GST_VIDEO_FORMAT_UYVY, GST_VIDEO_FORMAT_Y444, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_UYVY_Y444},

  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_I420, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 1, convert_AYUV_I420},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 1, convert_AYUV_I420},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_Y42B, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 1, 0, convert_AYUV_Y42B},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_Y444, TRUE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_AYUV_Y444},

  {GST_VIDEO_FORMAT_v210, GST_VIDEO_FORMAT_I420, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_v210_I420},
  {GST_VIDEO_FORMAT_v210, GST_VIDEO_FORMAT_YV12, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_v210_I420},
  {GST_VIDEO_FORMAT_v210, GST_VIDEO_FORMAT_Y42B, TRUE, FALSE, TRUE, FALSE,
      FALSE, FALSE, FALSE, FALSE, 0, 0, convert_v210_Y42B},

  /* planar -> planar */
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_I420, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_YV12, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_I420, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_YV12, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_Y41B, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y41B, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_Y42B, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_Y444, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_Y444, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_GRAY8, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY8, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_A420, TRUE, FALSE, FALSE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_YUV9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_YVU9, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_YUV9, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YUV9, GST_VIDEO_FORMAT_YVU9, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_I420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_YV12, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_Y41B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_Y42B, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_Y444, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_GRAY8, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_A420, FALSE, FALSE, FALSE, TRUE,
      TRUE, FALSE, TRUE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_YUV9, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_YVU9, GST_VIDEO_FORMAT_YVU9, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  /* sempiplanar -> semiplanar */
  {GST_VIDEO_FORMAT_NV12, GST_VIDEO_FORMAT_NV12, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV12, GST_VIDEO_FORMAT_NV16, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV12, GST_VIDEO_FORMAT_NV24, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_NV21, GST_VIDEO_FORMAT_NV21, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_NV16, GST_VIDEO_FORMAT_NV12, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV16, GST_VIDEO_FORMAT_NV16, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV16, GST_VIDEO_FORMAT_NV24, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_NV61, GST_VIDEO_FORMAT_NV61, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_NV24, GST_VIDEO_FORMAT_NV12, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV24, GST_VIDEO_FORMAT_NV16, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_NV24, GST_VIDEO_FORMAT_NV24, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_ARGB, TRUE, TRUE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_AYUV_ARGB},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_BGRA, TRUE, TRUE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_AYUV_BGRA},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_xRGB, TRUE, TRUE, TRUE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_AYUV_ARGB},    /* alias */
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_BGRx, TRUE, TRUE, TRUE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_AYUV_BGRA},    /* alias */
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_ABGR, TRUE, TRUE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_AYUV_ABGR},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_RGBA, TRUE, TRUE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_AYUV_RGBA},
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_xBGR, TRUE, TRUE, TRUE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_AYUV_ABGR},    /* alias */
  {GST_VIDEO_FORMAT_AYUV, GST_VIDEO_FORMAT_RGBx, TRUE, TRUE, TRUE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_AYUV_RGBA},    /* alias */
#endif

  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_BGRA, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_BGRA},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_BGRx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_BGRA},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_BGRA, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_BGRA},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_BGRx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_BGRA},

  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_ARGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_xRGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_ARGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_xRGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_ARGB},

  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_ABGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_xBGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_RGBA, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_RGBx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_RGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_BGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_RGB15, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_BGR15, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_RGB16, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_BGR16, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},

  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_ABGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_xBGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_RGBA, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_RGBx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_RGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_BGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_RGB15, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_BGR15, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_RGB16, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_YV12, GST_VIDEO_FORMAT_BGR16, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},

  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_ABGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_A420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_RGBA, FALSE, TRUE, TRUE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_A420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_BGRA, FALSE, TRUE, TRUE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_A420_BGRA},
  /* A420 to non-alpha RGB formats, reuse I420_* method */
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_BGRx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_BGRA},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_xBGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_RGBx, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_RGB, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_BGR, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_RGB15, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},
  {GST_VIDEO_FORMAT_A420, GST_VIDEO_FORMAT_BGR16, FALSE, TRUE, TRUE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_I420_pack_ARGB},

  /* scalers */
  {GST_VIDEO_FORMAT_GBR, GST_VIDEO_FORMAT_GBR, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GBRA, GST_VIDEO_FORMAT_GBRA, TRUE, FALSE, FALSE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_RGBP, GST_VIDEO_FORMAT_RGBP, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGRP, GST_VIDEO_FORMAT_BGRP, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_YVYU, GST_VIDEO_FORMAT_YVYU, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_RGB15, GST_VIDEO_FORMAT_RGB15, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_RGB16, GST_VIDEO_FORMAT_RGB16, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGR15, GST_VIDEO_FORMAT_BGR15, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGR16, GST_VIDEO_FORMAT_BGR16, TRUE, FALSE, FALSE, TRUE,
      TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_RGB, GST_VIDEO_FORMAT_RGB, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGR, GST_VIDEO_FORMAT_BGR, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_v308, GST_VIDEO_FORMAT_v308, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_IYU2, GST_VIDEO_FORMAT_IYU2, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_ARGB, GST_VIDEO_FORMAT_ARGB, TRUE, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_xRGB, GST_VIDEO_FORMAT_xRGB, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_ABGR, GST_VIDEO_FORMAT_ABGR, TRUE, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_xBGR, GST_VIDEO_FORMAT_xBGR, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_RGBA, GST_VIDEO_FORMAT_RGBA, TRUE, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_RGBx, GST_VIDEO_FORMAT_RGBx, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGRA, GST_VIDEO_FORMAT_BGRA, TRUE, FALSE, FALSE, TRUE, TRUE,
      TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_BGRx, GST_VIDEO_FORMAT_BGRx, TRUE, FALSE, FALSE, TRUE, TRUE,
      FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_ARGB64, GST_VIDEO_FORMAT_ARGB64, TRUE, FALSE, FALSE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_AYUV64, GST_VIDEO_FORMAT_AYUV64, TRUE, FALSE, FALSE, TRUE,
      TRUE, TRUE, FALSE, FALSE, 0, 0, convert_scale_planes},

  {GST_VIDEO_FORMAT_GRAY16_LE, GST_VIDEO_FORMAT_GRAY16_LE, TRUE, FALSE, FALSE,
      TRUE, TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
  {GST_VIDEO_FORMAT_GRAY16_BE, GST_VIDEO_FORMAT_GRAY16_BE, TRUE, FALSE, FALSE,
      TRUE, TRUE, FALSE, FALSE, FALSE, 0, 0, convert_scale_planes},
};

static gboolean
video_converter_lookup_fastpath (GstVideoConverter * convert)
{
  int i;
  GstVideoFormat in_format, out_format;
  GstVideoTransferFunction in_transf, out_transf;
  gboolean interlaced, same_matrix, same_primaries, same_size, crop, border;
  gboolean need_copy, need_set, need_mult;
  gint width, height;
  guint in_bpp, out_bpp;

  width = GST_VIDEO_INFO_WIDTH (&convert->in_info);
  height = GST_VIDEO_INFO_FIELD_HEIGHT (&convert->in_info);

  if (GET_OPT_DITHER_QUANTIZATION (convert) != 1)
    return FALSE;

  in_bpp = convert->in_info.finfo->bits;
  out_bpp = convert->out_info.finfo->bits;

  /* we don't do gamma conversion in fastpath */
  in_transf = convert->in_info.colorimetry.transfer;
  out_transf = convert->out_info.colorimetry.transfer;

  same_size = (width == convert->out_width && height == convert->out_height);

  /* fastpaths don't do gamma */
  if (CHECK_GAMMA_REMAP (convert) && (!same_size
          || !gst_video_transfer_function_is_equivalent (in_transf, in_bpp,
              out_transf, out_bpp)))
    return FALSE;

  need_copy = (convert->alpha_mode & ALPHA_MODE_COPY) == ALPHA_MODE_COPY;
  need_set = (convert->alpha_mode & ALPHA_MODE_SET) == ALPHA_MODE_SET;
  need_mult = (convert->alpha_mode & ALPHA_MODE_MULT) == ALPHA_MODE_MULT;
  GST_LOG ("alpha copy %d, set %d, mult %d", need_copy, need_set, need_mult);

  in_format = GST_VIDEO_INFO_FORMAT (&convert->in_info);
  out_format = GST_VIDEO_INFO_FORMAT (&convert->out_info);

  if (CHECK_MATRIX_NONE (convert)) {
    same_matrix = TRUE;
  } else {
    GstVideoColorMatrix in_matrix, out_matrix;

    in_matrix = convert->in_info.colorimetry.matrix;
    out_matrix = convert->out_info.colorimetry.matrix;
    same_matrix = in_matrix == out_matrix;
  }

  if (CHECK_PRIMARIES_NONE (convert)) {
    same_primaries = TRUE;
  } else {
    GstVideoColorPrimaries in_primaries, out_primaries;

    in_primaries = convert->in_info.colorimetry.primaries;
    out_primaries = convert->out_info.colorimetry.primaries;
    same_primaries = gst_video_color_primaries_is_equivalent (in_primaries,
        out_primaries);
  }

  interlaced = GST_VIDEO_INFO_IS_INTERLACED (&convert->in_info);
  interlaced |= GST_VIDEO_INFO_IS_INTERLACED (&convert->out_info);

  crop = convert->in_x || convert->in_y
      || convert->in_width < convert->in_maxwidth
      || convert->in_height < convert->in_maxheight;
  border = convert->out_x || convert->out_y
      || convert->out_width < convert->out_maxwidth
      || convert->out_height < convert->out_maxheight;

  for (i = 0; i < G_N_ELEMENTS (transforms); i++) {
    if (transforms[i].in_format == in_format &&
        transforms[i].out_format == out_format &&
        (transforms[i].keeps_interlaced || !interlaced) &&
        (transforms[i].needs_color_matrix || (same_matrix && same_primaries))
        && (!transforms[i].keeps_size || same_size)
        && (transforms[i].width_align & width) == 0
        && (transforms[i].height_align & height) == 0
        && (transforms[i].do_crop || !crop)
        && (transforms[i].do_border || !border)
        && (transforms[i].alpha_copy || !need_copy)
        && (transforms[i].alpha_set || !need_set)
        && (transforms[i].alpha_mult || !need_mult)) {
      guint j;

      GST_LOG ("using fastpath");
      if (transforms[i].needs_color_matrix)
        video_converter_compute_matrix (convert);
      convert->convert = transforms[i].convert;

      convert->tmpline =
          g_new (guint16 *, convert->conversion_runner->n_threads);
      for (j = 0; j < convert->conversion_runner->n_threads; j++)
        convert->tmpline[j] = g_malloc0 (sizeof (guint16) * (width + 8) * 4);

      if (!transforms[i].keeps_size)
        if (!setup_scale (convert))
          return FALSE;
      if (border)
        setup_borderline (convert);
      return TRUE;
    }
  }
  GST_LOG ("no fastpath found");
  return FALSE;
}

/**
 * gst_video_converter_get_in_info:
 * @convert: a #GstVideoConverter
 *
 * Retrieve the input format of @convert.
 *
 * Returns: (transfer none): a #GstVideoInfo
 *
 * Since: 1.22
 */
const GstVideoInfo *
gst_video_converter_get_in_info (GstVideoConverter * convert)
{
  return &convert->in_info;
}

/**
 * gst_video_converter_get_out_info:
 * @convert: a #GstVideoConverter
 *
 * Retrieve the output format of @convert.
 *
 * Returns: (transfer none): a #GstVideoInfo
 *
 * Since: 1.22
 */
const GstVideoInfo *
gst_video_converter_get_out_info (GstVideoConverter * convert)
{
  return &convert->out_info;
}
