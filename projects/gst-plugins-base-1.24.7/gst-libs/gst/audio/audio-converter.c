/* GStreamer
 * Copyright (C) 2005 Wim Taymans <wim at fluendo dot com>
 *           (C) 2015 Wim Taymans <wim.taymans@gmail.com>
 *
 * audioconverter.c: Convert audio to different audio formats automatically
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

#include <math.h>
#include <string.h>

#include "audio-converter.h"
#include "gstaudiopack.h"

/**
 * SECTION:gstaudioconverter
 * @title: GstAudioConverter
 * @short_description: Generic audio conversion
 *
 * This object is used to convert audio samples from one format to another.
 * The object can perform conversion of:
 *
 *  * audio format with optional dithering and noise shaping
 *
 *  * audio samplerate
 *
 *  * audio channels and channel layout
 *
 */

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("audio-converter", 0,
        "audio-converter object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

typedef struct _AudioChain AudioChain;

typedef void (*AudioConvertFunc) (gpointer dst, const gpointer src, gint count);
typedef gboolean (*AudioConvertSamplesFunc) (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames);
typedef void (*AudioConvertEndianFunc) (gpointer dst, const gpointer src,
    gint count);

/*                           int/int    int/float  float/int float/float
 *
 *  unpack                     S32          S32         F64       F64
 *  convert                               S32->F64
 *  channel mix                S32          F64         F64       F64
 *  convert                                           F64->S32
 *  quantize                   S32                      S32
 *  pack                       S32          F64         S32       F64
 *
 *
 *  interleave
 *  deinterleave
 *  resample
 */
struct _GstAudioConverter
{
  GstAudioInfo in;
  GstAudioInfo out;

  GstStructure *config;

  GstAudioConverterFlags flags;
  GstAudioFormat current_format;
  GstAudioLayout current_layout;
  gint current_channels;

  gboolean in_writable;
  gpointer *in_data;
  gsize in_frames;
  gpointer *out_data;
  gsize out_frames;

  gboolean in_place;            /* the conversion can be done in place; returned by gst_audio_converter_supports_inplace() */

  gboolean passthrough;

  /* unpack */
  gboolean in_default;
  gboolean unpack_ip;

  /* convert in */
  AudioConvertFunc convert_in;

  /* channel mix */
  gboolean mix_passthrough;
  GstAudioChannelMixer *mix;

  /* resample */
  GstAudioResampler *resampler;

  /* convert out */
  AudioConvertFunc convert_out;

  /* quant */
  GstAudioQuantize *quant;

  /* change layout */
  GstAudioFormat chlayout_format;
  GstAudioLayout chlayout_target;
  gint chlayout_channels;

  /* pack */
  gboolean out_default;
  AudioChain *chain_end;        /* NULL for empty chain or points to the last element in the chain */

  /* endian swap */
  AudioConvertEndianFunc swap_endian;

  AudioConvertSamplesFunc convert;
};

static GstAudioConverter *
gst_audio_converter_copy (GstAudioConverter * convert)
{
  GstAudioConverter *res =
      gst_audio_converter_new (convert->flags, &convert->in, &convert->out,
      convert->config);

  return res;
}

G_DEFINE_BOXED_TYPE (GstAudioConverter, gst_audio_converter,
    (GBoxedCopyFunc) gst_audio_converter_copy,
    (GBoxedFreeFunc) gst_audio_converter_free);

typedef gboolean (*AudioChainFunc) (AudioChain * chain, gpointer user_data);
typedef gpointer *(*AudioChainAllocFunc) (AudioChain * chain, gsize num_samples,
    gpointer user_data);

struct _AudioChain
{
  AudioChain *prev;

  AudioChainFunc make_func;
  gpointer make_func_data;
  GDestroyNotify make_func_notify;

  const GstAudioFormatInfo *finfo;
  gint stride;
  gint inc;
  gint blocks;

  gboolean pass_alloc;
  gboolean allow_ip;

  AudioChainAllocFunc alloc_func;
  gpointer alloc_data;

  gpointer *tmp;
  gsize allocated_samples;

  gpointer *samples;
  gsize num_samples;
};

static AudioChain *
audio_chain_new (AudioChain * prev, GstAudioConverter * convert)
{
  AudioChain *chain;

  chain = g_new0 (AudioChain, 1);
  chain->prev = prev;

  if (convert->current_layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    chain->inc = 1;
    chain->blocks = convert->current_channels;
  } else {
    chain->inc = convert->current_channels;
    chain->blocks = 1;
  }
  chain->finfo = gst_audio_format_get_info (convert->current_format);
  chain->stride = (chain->finfo->width * chain->inc) / 8;

  return chain;
}

static void
audio_chain_set_make_func (AudioChain * chain,
    AudioChainFunc make_func, gpointer user_data, GDestroyNotify notify)
{
  chain->make_func = make_func;
  chain->make_func_data = user_data;
  chain->make_func_notify = notify;
}

static void
audio_chain_free (AudioChain * chain)
{
  GST_LOG ("free chain %p", chain);
  if (chain->make_func_notify)
    chain->make_func_notify (chain->make_func_data);
  g_free (chain->tmp);
  g_free (chain);
}

static gpointer *
audio_chain_alloc_samples (AudioChain * chain, gsize num_samples)
{
  return chain->alloc_func (chain, num_samples, chain->alloc_data);
}

static void
audio_chain_set_samples (AudioChain * chain, gpointer * samples,
    gsize num_samples)
{
  GST_LOG ("set samples %p %" G_GSIZE_FORMAT, samples, num_samples);

  chain->samples = samples;
  chain->num_samples = num_samples;
}

static gpointer *
audio_chain_get_samples (AudioChain * chain, gsize * avail)
{
  gpointer *res;

  if (!chain->samples)
    chain->make_func (chain, chain->make_func_data);

  res = chain->samples;
  *avail = chain->num_samples;
  chain->samples = NULL;

  return res;
}

static guint
get_opt_uint (GstAudioConverter * convert, const gchar * opt, guint def)
{
  guint res;
  if (!gst_structure_get_uint (convert->config, opt, &res))
    res = def;
  return res;
}

static gint
get_opt_enum (GstAudioConverter * convert, const gchar * opt, GType type,
    gint def)
{
  gint res;
  if (!gst_structure_get_enum (convert->config, opt, type, &res))
    res = def;
  return res;
}

static const GValue *
get_opt_value (GstAudioConverter * convert, const gchar * opt)
{
  return gst_structure_get_value (convert->config, opt);
}

#define DEFAULT_OPT_RESAMPLER_METHOD GST_AUDIO_RESAMPLER_METHOD_BLACKMAN_NUTTALL
#define DEFAULT_OPT_DITHER_METHOD GST_AUDIO_DITHER_NONE
#define DEFAULT_OPT_DITHER_THRESHOLD 20
#define DEFAULT_OPT_NOISE_SHAPING_METHOD GST_AUDIO_NOISE_SHAPING_NONE
#define DEFAULT_OPT_QUANTIZATION 1

#define GET_OPT_RESAMPLER_METHOD(c) get_opt_enum(c, \
    GST_AUDIO_CONVERTER_OPT_RESAMPLER_METHOD, GST_TYPE_AUDIO_RESAMPLER_METHOD, \
    DEFAULT_OPT_RESAMPLER_METHOD)
#define GET_OPT_DITHER_METHOD(c) get_opt_enum(c, \
    GST_AUDIO_CONVERTER_OPT_DITHER_METHOD, GST_TYPE_AUDIO_DITHER_METHOD, \
    DEFAULT_OPT_DITHER_METHOD)
#define GET_OPT_DITHER_THRESHOLD(c) get_opt_uint(c, \
    GST_AUDIO_CONVERTER_OPT_DITHER_THRESHOLD, DEFAULT_OPT_DITHER_THRESHOLD)
#define GET_OPT_NOISE_SHAPING_METHOD(c) get_opt_enum(c, \
    GST_AUDIO_CONVERTER_OPT_NOISE_SHAPING_METHOD, GST_TYPE_AUDIO_NOISE_SHAPING_METHOD, \
    DEFAULT_OPT_NOISE_SHAPING_METHOD)
#define GET_OPT_QUANTIZATION(c) get_opt_uint(c, \
    GST_AUDIO_CONVERTER_OPT_QUANTIZATION, DEFAULT_OPT_QUANTIZATION)
#define GET_OPT_MIX_MATRIX(c) get_opt_value(c, \
    GST_AUDIO_CONVERTER_OPT_MIX_MATRIX)

static gboolean
copy_config (GQuark field_id, const GValue * value, gpointer user_data)
{
  GstAudioConverter *convert = user_data;

  gst_structure_id_set_value (convert->config, field_id, value);

  return TRUE;
}

/**
 * gst_audio_converter_update_config:
 * @convert: a #GstAudioConverter
 * @in_rate: input rate
 * @out_rate: output rate
 * @config: (transfer full) (allow-none): a #GstStructure or %NULL
 *
 * Set @in_rate, @out_rate and @config as extra configuration for @convert.
 *
 * @in_rate and @out_rate specify the new sample rates of input and output
 * formats. A value of 0 leaves the sample rate unchanged.
 *
 * @config can be %NULL, in which case, the current configuration is not
 * changed.
 *
 * If the parameters in @config can not be set exactly, this function returns
 * %FALSE and will try to update as much state as possible. The new state can
 * then be retrieved and refined with gst_audio_converter_get_config().
 *
 * Look at the `GST_AUDIO_CONVERTER_OPT_*` fields to check valid configuration
 * option and values.
 *
 * Returns: %TRUE when the new parameters could be set
 */
gboolean
gst_audio_converter_update_config (GstAudioConverter * convert,
    gint in_rate, gint out_rate, GstStructure * config)
{
  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail ((in_rate == 0 && out_rate == 0) ||
      convert->flags & GST_AUDIO_CONVERTER_FLAG_VARIABLE_RATE, FALSE);

  GST_LOG ("new rate %d -> %d", in_rate, out_rate);

  if (in_rate <= 0)
    in_rate = convert->in.rate;
  if (out_rate <= 0)
    out_rate = convert->out.rate;

  convert->in.rate = in_rate;
  convert->out.rate = out_rate;

  if (convert->resampler)
    gst_audio_resampler_update (convert->resampler, in_rate, out_rate, config);

  if (config) {
    gst_structure_foreach (config, copy_config, convert);
    gst_structure_free (config);
  }

  return TRUE;
}

/**
 * gst_audio_converter_get_config:
 * @convert: a #GstAudioConverter
 * @in_rate: (out) (optional): result input rate
 * @out_rate: (out) (optional): result output rate
 *
 * Get the current configuration of @convert.
 *
 * Returns: (transfer none):
 *   a #GstStructure that remains valid for as long as @convert is valid
 *   or until gst_audio_converter_update_config() is called.
 */
const GstStructure *
gst_audio_converter_get_config (GstAudioConverter * convert,
    gint * in_rate, gint * out_rate)
{
  g_return_val_if_fail (convert != NULL, NULL);

  if (in_rate)
    *in_rate = convert->in.rate;
  if (out_rate)
    *out_rate = convert->out.rate;

  return convert->config;
}

static gpointer *
get_output_samples (AudioChain * chain, gsize num_samples, gpointer user_data)
{
  GstAudioConverter *convert = user_data;

  GST_LOG ("output samples %p %" G_GSIZE_FORMAT, convert->out_data,
      num_samples);

  return convert->out_data;
}

#define MEM_ALIGN(m,a) ((gint8 *)((guintptr)((gint8 *)(m) + ((a)-1)) & ~((a)-1)))
#define ALIGN 16

static gpointer *
get_temp_samples (AudioChain * chain, gsize num_samples, gpointer user_data)
{
  if (num_samples > chain->allocated_samples) {
    gint i;
    gint8 *s;
    gsize stride = GST_ROUND_UP_N (num_samples * chain->stride, ALIGN);
    /* first part contains the pointers, second part the data, add some extra bytes
     * for alignment */
    gsize needed = (stride + sizeof (gpointer)) * chain->blocks + ALIGN - 1;

    GST_DEBUG ("alloc samples %d %" G_GSIZE_FORMAT " %" G_GSIZE_FORMAT,
        chain->stride, num_samples, needed);
    chain->tmp = g_realloc (chain->tmp, needed);
    chain->allocated_samples = num_samples;

    /* pointer to the data, make sure it's 16 bytes aligned */
    s = MEM_ALIGN (&chain->tmp[chain->blocks], ALIGN);

    /* set up the pointers */
    for (i = 0; i < chain->blocks; i++)
      chain->tmp[i] = s + i * stride;
  }
  GST_LOG ("temp samples %p %" G_GSIZE_FORMAT, chain->tmp, num_samples);

  return chain->tmp;
}

static gboolean
do_unpack (AudioChain * chain, gpointer user_data)
{
  GstAudioConverter *convert = user_data;
  gsize num_samples;
  gpointer *tmp;
  gboolean in_writable;

  in_writable = convert->in_writable;
  num_samples = convert->in_frames;

  if (!chain->allow_ip || !in_writable || !convert->in_default) {
    gint i;

    if (in_writable && chain->allow_ip) {
      tmp = convert->in_data;
      GST_LOG ("unpack in-place %p, %" G_GSIZE_FORMAT, tmp, num_samples);
    } else {
      tmp = audio_chain_alloc_samples (chain, num_samples);
      GST_LOG ("unpack to tmp %p, %" G_GSIZE_FORMAT, tmp, num_samples);
    }

    if (convert->in_data) {
      for (i = 0; i < chain->blocks; i++) {
        if (convert->in_default) {
          GST_LOG ("copy %p, %p, %" G_GSIZE_FORMAT, tmp[i], convert->in_data[i],
              num_samples);
          memcpy (tmp[i], convert->in_data[i], num_samples * chain->stride);
        } else {
          GST_LOG ("unpack %p, %p, %" G_GSIZE_FORMAT, tmp[i],
              convert->in_data[i], num_samples);
          convert->in.finfo->unpack_func (convert->in.finfo,
              GST_AUDIO_PACK_FLAG_TRUNCATE_RANGE, tmp[i], convert->in_data[i],
              num_samples * chain->inc);
        }
      }
    } else {
      for (i = 0; i < chain->blocks; i++) {
        gst_audio_format_info_fill_silence (chain->finfo, tmp[i],
            num_samples * chain->inc);
      }
    }
  } else {
    tmp = convert->in_data;
    GST_LOG ("get in samples %p", tmp);
  }
  audio_chain_set_samples (chain, tmp, num_samples);

  return TRUE;
}

static gboolean
do_convert_in (AudioChain * chain, gpointer user_data)
{
  gsize num_samples;
  GstAudioConverter *convert = user_data;
  gpointer *in, *out;
  gint i;

  in = audio_chain_get_samples (chain->prev, &num_samples);
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, num_samples));
  GST_LOG ("convert in %p, %p, %" G_GSIZE_FORMAT, in, out, num_samples);

  for (i = 0; i < chain->blocks; i++)
    convert->convert_in (out[i], in[i], num_samples * chain->inc);

  audio_chain_set_samples (chain, out, num_samples);

  return TRUE;
}

static gboolean
do_mix (AudioChain * chain, gpointer user_data)
{
  gsize num_samples;
  GstAudioConverter *convert = user_data;
  gpointer *in, *out;

  in = audio_chain_get_samples (chain->prev, &num_samples);
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, num_samples));
  GST_LOG ("mix %p, %p, %" G_GSIZE_FORMAT, in, out, num_samples);

  gst_audio_channel_mixer_samples (convert->mix, in, out, num_samples);

  audio_chain_set_samples (chain, out, num_samples);

  return TRUE;
}

static gboolean
do_resample (AudioChain * chain, gpointer user_data)
{
  GstAudioConverter *convert = user_data;
  gpointer *in, *out;
  gsize in_frames, out_frames;

  in = audio_chain_get_samples (chain->prev, &in_frames);
  out_frames = convert->out_frames;
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, out_frames));

  GST_LOG ("resample %p %p,%" G_GSIZE_FORMAT " %" G_GSIZE_FORMAT, in,
      out, in_frames, out_frames);

  gst_audio_resampler_resample (convert->resampler, in, in_frames, out,
      out_frames);

  audio_chain_set_samples (chain, out, out_frames);

  return TRUE;
}

static gboolean
do_convert_out (AudioChain * chain, gpointer user_data)
{
  GstAudioConverter *convert = user_data;
  gsize num_samples;
  gpointer *in, *out;
  gint i;

  in = audio_chain_get_samples (chain->prev, &num_samples);
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, num_samples));
  GST_LOG ("convert out %p, %p %" G_GSIZE_FORMAT, in, out, num_samples);

  for (i = 0; i < chain->blocks; i++)
    convert->convert_out (out[i], in[i], num_samples * chain->inc);

  audio_chain_set_samples (chain, out, num_samples);

  return TRUE;
}

static gboolean
do_quantize (AudioChain * chain, gpointer user_data)
{
  GstAudioConverter *convert = user_data;
  gsize num_samples;
  gpointer *in, *out;

  in = audio_chain_get_samples (chain->prev, &num_samples);
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, num_samples));
  GST_LOG ("quantize %p, %p %" G_GSIZE_FORMAT, in, out, num_samples);

  if (in && out)
    gst_audio_quantize_samples (convert->quant, in, out, num_samples);

  audio_chain_set_samples (chain, out, num_samples);

  return TRUE;
}

#define MAKE_INTERLEAVE_FUNC(type) \
static inline void \
interleave_##type (const type * in[], type * out[], \
    gsize num_samples, gint channels) \
{ \
  gsize s; \
  gint c; \
  for (s = 0; s < num_samples; s++) { \
    for (c = 0; c < channels; c++) { \
      out[0][s * channels + c] = in[c][s]; \
    } \
  } \
}

#define MAKE_DEINTERLEAVE_FUNC(type) \
static inline void \
deinterleave_##type (const type * in[], type * out[], \
    gsize num_samples, gint channels) \
{ \
  gsize s; \
  gint c; \
  for (s = 0; s < num_samples; s++) { \
    for (c = 0; c < channels; c++) { \
      out[c][s] = in[0][s * channels + c]; \
    } \
  } \
}

MAKE_INTERLEAVE_FUNC (gint16);
MAKE_INTERLEAVE_FUNC (gint32);
MAKE_INTERLEAVE_FUNC (gfloat);
MAKE_INTERLEAVE_FUNC (gdouble);
MAKE_DEINTERLEAVE_FUNC (gint16);
MAKE_DEINTERLEAVE_FUNC (gint32);
MAKE_DEINTERLEAVE_FUNC (gfloat);
MAKE_DEINTERLEAVE_FUNC (gdouble);

static gboolean
do_change_layout (AudioChain * chain, gpointer user_data)
{
  GstAudioConverter *convert = user_data;
  GstAudioFormat format = convert->chlayout_format;
  GstAudioLayout out_layout = convert->chlayout_target;
  gint channels = convert->chlayout_channels;
  gsize num_samples;
  gpointer *in, *out;

  in = audio_chain_get_samples (chain->prev, &num_samples);
  out = (chain->allow_ip ? in : audio_chain_alloc_samples (chain, num_samples));

  if (out_layout == GST_AUDIO_LAYOUT_INTERLEAVED) {
    /* interleave */
    GST_LOG ("interleaving %p, %p %" G_GSIZE_FORMAT, in, out, num_samples);
    switch (format) {
      case GST_AUDIO_FORMAT_S16:
        interleave_gint16 ((const gint16 **) in, (gint16 **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_S32:
        interleave_gint32 ((const gint32 **) in, (gint32 **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_F32:
        interleave_gfloat ((const gfloat **) in, (gfloat **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_F64:
        interleave_gdouble ((const gdouble **) in, (gdouble **) out,
            num_samples, channels);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  } else {
    /* deinterleave */
    GST_LOG ("deinterleaving %p, %p %" G_GSIZE_FORMAT, in, out, num_samples);
    switch (format) {
      case GST_AUDIO_FORMAT_S16:
        deinterleave_gint16 ((const gint16 **) in, (gint16 **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_S32:
        deinterleave_gint32 ((const gint32 **) in, (gint32 **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_F32:
        deinterleave_gfloat ((const gfloat **) in, (gfloat **) out,
            num_samples, channels);
        break;
      case GST_AUDIO_FORMAT_F64:
        deinterleave_gdouble ((const gdouble **) in, (gdouble **) out,
            num_samples, channels);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }

  audio_chain_set_samples (chain, out, num_samples);
  return TRUE;
}

static gboolean
is_intermediate_format (GstAudioFormat format)
{
  return (format == GST_AUDIO_FORMAT_S16 ||
      format == GST_AUDIO_FORMAT_S32 ||
      format == GST_AUDIO_FORMAT_F32 || format == GST_AUDIO_FORMAT_F64);
}

static AudioChain *
chain_unpack (GstAudioConverter * convert)
{
  AudioChain *prev;
  GstAudioInfo *in = &convert->in;
  GstAudioInfo *out = &convert->out;
  gboolean same_format;

  same_format = in->finfo->format == out->finfo->format;

  /* do not unpack if we have the same input format as the output format
   * and it is a possible intermediate format */
  if (same_format && is_intermediate_format (in->finfo->format)) {
    convert->current_format = in->finfo->format;
  } else {
    convert->current_format = in->finfo->unpack_format;
  }
  convert->current_layout = in->layout;
  convert->current_channels = in->channels;

  convert->in_default = convert->current_format == in->finfo->format;

  GST_INFO ("unpack format %s to %s",
      gst_audio_format_to_string (in->finfo->format),
      gst_audio_format_to_string (convert->current_format));

  prev = audio_chain_new (NULL, convert);
  prev->allow_ip = prev->finfo->width <= in->finfo->width;
  prev->pass_alloc = FALSE;
  audio_chain_set_make_func (prev, do_unpack, convert, NULL);

  return prev;
}

static AudioChain *
chain_convert_in (GstAudioConverter * convert, AudioChain * prev)
{
  gboolean in_int, out_int;
  GstAudioInfo *in = &convert->in;
  GstAudioInfo *out = &convert->out;

  in_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (in->finfo);
  out_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (out->finfo);

  if (in_int && !out_int) {
    GST_INFO ("convert S32 to F64");
    convert->convert_in = (AudioConvertFunc) audio_orc_s32_to_double;
    convert->current_format = GST_AUDIO_FORMAT_F64;

    prev = audio_chain_new (prev, convert);
    prev->allow_ip = FALSE;
    prev->pass_alloc = FALSE;
    audio_chain_set_make_func (prev, do_convert_in, convert, NULL);
  }
  return prev;
}

static gboolean
check_mix_matrix (guint in_channels, guint out_channels, const GValue * value)
{
  guint i, j;

  /* audio-channel-mixer will generate an identity matrix */
  if (gst_value_array_get_size (value) == 0)
    return TRUE;

  if (gst_value_array_get_size (value) != out_channels) {
    GST_ERROR ("Invalid mix matrix size, should be %d", out_channels);
    goto fail;
  }

  for (j = 0; j < out_channels; j++) {
    const GValue *row = gst_value_array_get_value (value, j);

    if (gst_value_array_get_size (row) != in_channels) {
      GST_ERROR ("Invalid mix matrix row size, should be %d", in_channels);
      goto fail;
    }

    for (i = 0; i < in_channels; i++) {
      const GValue *itm;

      itm = gst_value_array_get_value (row, i);
      if (!G_VALUE_HOLDS_FLOAT (itm)) {
        GST_ERROR ("Invalid mix matrix element type, should be float");
        goto fail;
      }
    }
  }

  return TRUE;

fail:
  return FALSE;
}

static gfloat **
mix_matrix_from_g_value (guint in_channels, guint out_channels,
    const GValue * value)
{
  guint i, j;
  gfloat **matrix = g_new (gfloat *, in_channels);

  for (i = 0; i < in_channels; i++)
    matrix[i] = g_new (gfloat, out_channels);

  for (j = 0; j < out_channels; j++) {
    const GValue *row = gst_value_array_get_value (value, j);

    for (i = 0; i < in_channels; i++) {
      const GValue *itm;
      gfloat coefficient;

      itm = gst_value_array_get_value (row, i);
      coefficient = g_value_get_float (itm);
      matrix[i][j] = coefficient;
    }
  }

  return matrix;
}

static AudioChain *
chain_mix (GstAudioConverter * convert, AudioChain * prev)
{
  GstAudioInfo *in = &convert->in;
  GstAudioInfo *out = &convert->out;
  GstAudioFormat format = convert->current_format;
  const GValue *opt_matrix = GET_OPT_MIX_MATRIX (convert);
  GstAudioChannelMixerFlags flags = 0;

  convert->current_channels = out->channels;

  /* keep the input layout */
  if (convert->current_layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    flags |= GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_IN;
    flags |= GST_AUDIO_CHANNEL_MIXER_FLAGS_NON_INTERLEAVED_OUT;
  }

  if (opt_matrix) {
    gfloat **matrix = NULL;

    if (gst_value_array_get_size (opt_matrix))
      matrix =
          mix_matrix_from_g_value (in->channels, out->channels, opt_matrix);

    convert->mix =
        gst_audio_channel_mixer_new_with_matrix (flags, format, in->channels,
        out->channels, matrix);
  } else {
    flags |=
        GST_AUDIO_INFO_IS_UNPOSITIONED (in) ?
        GST_AUDIO_CHANNEL_MIXER_FLAGS_UNPOSITIONED_IN : 0;
    flags |=
        GST_AUDIO_INFO_IS_UNPOSITIONED (out) ?
        GST_AUDIO_CHANNEL_MIXER_FLAGS_UNPOSITIONED_OUT : 0;

    convert->mix =
        gst_audio_channel_mixer_new (flags, format, in->channels, in->position,
        out->channels, out->position);
  }

  convert->mix_passthrough =
      gst_audio_channel_mixer_is_passthrough (convert->mix);
  GST_INFO ("mix format %s, passthrough %d, in_channels %d, out_channels %d",
      gst_audio_format_to_string (format), convert->mix_passthrough,
      in->channels, out->channels);

  if (!convert->mix_passthrough) {
    prev = audio_chain_new (prev, convert);
    prev->allow_ip = FALSE;
    prev->pass_alloc = FALSE;
    audio_chain_set_make_func (prev, do_mix, convert, NULL);
  }
  return prev;
}

static AudioChain *
chain_resample (GstAudioConverter * convert, AudioChain * prev)
{
  GstAudioInfo *in = &convert->in;
  GstAudioInfo *out = &convert->out;
  GstAudioResamplerMethod method;
  GstAudioResamplerFlags flags;
  GstAudioFormat format = convert->current_format;
  gint channels = convert->current_channels;
  gboolean variable_rate;

  variable_rate = convert->flags & GST_AUDIO_CONVERTER_FLAG_VARIABLE_RATE;

  if (in->rate != out->rate || variable_rate) {
    method = GET_OPT_RESAMPLER_METHOD (convert);

    flags = 0;
    if (convert->current_layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
      flags |= GST_AUDIO_RESAMPLER_FLAG_NON_INTERLEAVED_IN;
    }
    /* if the resampler is activated, it is optimal to change layout here */
    if (out->layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
      flags |= GST_AUDIO_RESAMPLER_FLAG_NON_INTERLEAVED_OUT;
    }
    convert->current_layout = out->layout;

    if (variable_rate)
      flags |= GST_AUDIO_RESAMPLER_FLAG_VARIABLE_RATE;

    convert->resampler =
        gst_audio_resampler_new (method, flags, format, channels, in->rate,
        out->rate, convert->config);

    prev = audio_chain_new (prev, convert);
    prev->allow_ip = FALSE;
    prev->pass_alloc = FALSE;
    audio_chain_set_make_func (prev, do_resample, convert, NULL);
  }
  return prev;
}

static AudioChain *
chain_convert_out (GstAudioConverter * convert, AudioChain * prev)
{
  gboolean in_int, out_int;
  GstAudioInfo *in = &convert->in;
  GstAudioInfo *out = &convert->out;

  in_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (in->finfo);
  out_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (out->finfo);

  if (!in_int && out_int) {
    convert->convert_out = (AudioConvertFunc) audio_orc_double_to_s32;
    convert->current_format = GST_AUDIO_FORMAT_S32;

    GST_INFO ("convert F64 to S32");
    prev = audio_chain_new (prev, convert);
    prev->allow_ip = TRUE;
    prev->pass_alloc = FALSE;
    audio_chain_set_make_func (prev, do_convert_out, convert, NULL);
  }
  return prev;
}

static AudioChain *
chain_quantize (GstAudioConverter * convert, AudioChain * prev)
{
  const GstAudioFormatInfo *cur_finfo;
  GstAudioInfo *out = &convert->out;
  gint in_depth, out_depth;
  gboolean in_int, out_int;
  GstAudioDitherMethod dither;
  guint dither_threshold;
  GstAudioNoiseShapingMethod ns;

  dither = GET_OPT_DITHER_METHOD (convert);
  dither_threshold = GET_OPT_DITHER_THRESHOLD (convert);
  ns = GET_OPT_NOISE_SHAPING_METHOD (convert);

  cur_finfo = gst_audio_format_get_info (convert->current_format);

  in_depth = GST_AUDIO_FORMAT_INFO_DEPTH (cur_finfo);
  out_depth = GST_AUDIO_FORMAT_INFO_DEPTH (out->finfo);
  GST_INFO ("depth in %d, out %d", in_depth, out_depth);

  in_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (cur_finfo);
  out_int = GST_AUDIO_FORMAT_INFO_IS_INTEGER (out->finfo);

  /* Don't dither or apply noise shaping if target depth is bigger than 20 bits
   * as DA converters only can do a SNR up to 20 bits in reality.
   * Also don't dither or apply noise shaping if target depth is larger than
   * source depth. */
  if (out_depth > dither_threshold || (in_int && out_depth >= in_depth)) {
    dither = GST_AUDIO_DITHER_NONE;
    ns = GST_AUDIO_NOISE_SHAPING_NONE;
    GST_INFO ("using no dither and noise shaping");
  } else {
    GST_INFO ("using dither %d and noise shaping %d", dither, ns);
    /* Use simple error feedback when output sample rate is smaller than
     * 32000 as the other methods might move the noise to audible ranges */
    if (ns > GST_AUDIO_NOISE_SHAPING_ERROR_FEEDBACK && out->rate < 32000)
      ns = GST_AUDIO_NOISE_SHAPING_ERROR_FEEDBACK;
  }
  /* we still want to run the quantization step when reducing bits to get
   * the rounding correct */
  if (out_int && out_depth < 32
      && convert->current_format == GST_AUDIO_FORMAT_S32) {
    GST_INFO ("quantize to %d bits, dither %d, ns %d", out_depth, dither, ns);
    convert->quant =
        gst_audio_quantize_new (dither, ns, 0, convert->current_format,
        out->channels, 1U << (32 - out_depth));

    prev = audio_chain_new (prev, convert);
    prev->allow_ip = TRUE;
    prev->pass_alloc = TRUE;
    audio_chain_set_make_func (prev, do_quantize, convert, NULL);
  }
  return prev;
}

static AudioChain *
chain_change_layout (GstAudioConverter * convert, AudioChain * prev)
{
  GstAudioInfo *out = &convert->out;

  if (convert->current_layout != out->layout) {
    convert->current_layout = out->layout;

    /* if there is only 1 channel, layouts are identical */
    if (convert->current_channels > 1) {
      convert->chlayout_target = convert->current_layout;
      convert->chlayout_format = convert->current_format;
      convert->chlayout_channels = convert->current_channels;

      prev = audio_chain_new (prev, convert);
      prev->allow_ip = FALSE;
      prev->pass_alloc = FALSE;
      audio_chain_set_make_func (prev, do_change_layout, convert, NULL);
    }
  }
  return prev;
}

static AudioChain *
chain_pack (GstAudioConverter * convert, AudioChain * prev)
{
  GstAudioInfo *out = &convert->out;
  GstAudioFormat format = convert->current_format;

  convert->current_format = out->finfo->format;

  convert->out_default = format == out->finfo->format;
  GST_INFO ("pack format %s to %s", gst_audio_format_to_string (format),
      gst_audio_format_to_string (out->finfo->format));

  return prev;
}

static void
setup_allocators (GstAudioConverter * convert)
{
  AudioChain *chain;
  AudioChainAllocFunc alloc_func;
  gboolean allow_ip;

  /* start with using dest if we can directly write into it */
  if (convert->out_default) {
    alloc_func = get_output_samples;
    allow_ip = FALSE;
  } else {
    alloc_func = get_temp_samples;
    allow_ip = TRUE;
  }
  /* now walk backwards, we try to write into the dest samples directly
   * and keep track if the source needs to be writable */
  for (chain = convert->chain_end; chain; chain = chain->prev) {
    chain->alloc_func = alloc_func;
    chain->alloc_data = convert;
    chain->allow_ip = allow_ip && chain->allow_ip;
    GST_LOG ("chain %p: %d %d", chain, allow_ip, chain->allow_ip);

    if (!chain->pass_alloc) {
      /* can't pass allocator, make new temp line allocator */
      alloc_func = get_temp_samples;
      allow_ip = TRUE;
    }
  }
}

static gboolean
converter_passthrough (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  gint i;
  AudioChain *chain;
  gsize samples;

  /* in-place passthrough -> do nothing */
  if (in == out) {
    g_assert (convert->in_place);
    return TRUE;
  }

  chain = convert->chain_end;

  samples = in_frames * chain->inc;

  GST_LOG ("passthrough: %" G_GSIZE_FORMAT " / %" G_GSIZE_FORMAT " samples",
      in_frames, samples);

  if (in) {
    gsize bytes;

    bytes = samples * (convert->in.bpf / convert->in.channels);

    for (i = 0; i < chain->blocks; i++) {
      if (out[i] == in[i]) {
        g_assert (convert->in_place);
        continue;
      }

      memcpy (out[i], in[i], bytes);
    }
  } else {
    for (i = 0; i < chain->blocks; i++)
      gst_audio_format_info_fill_silence (convert->in.finfo, out[i], samples);
  }
  return TRUE;
}

/* perform LE<->BE conversion on a block of @count 16-bit samples
 * dst may equal src for in-place conversion
 */
static void
converter_swap_endian_16 (gpointer dst, const gpointer src, gint count)
{
  guint16 *out = dst;
  const guint16 *in = src;
  gint i;

  for (i = 0; i < count; i++)
    out[i] = GUINT16_SWAP_LE_BE (in[i]);
}

/* perform LE<->BE conversion on a block of @count 24-bit samples
 * dst may equal src for in-place conversion
 *
 * naive algorithm, which performs better with -O3 and worse with -O2
 * than the commented out optimized algorithm below
 */
static void
converter_swap_endian_24 (gpointer dst, const gpointer src, gint count)
{
  guint8 *out = dst;
  const guint8 *in = src;
  gint i;

  count *= 3;

  for (i = 0; i < count; i += 3) {
    guint8 x = in[i + 0];
    out[i + 0] = in[i + 2];
    out[i + 1] = in[i + 1];
    out[i + 2] = x;
  }
}

/* the below code performs better with -O2 but worse with -O3 */
#if 0
/* perform LE<->BE conversion on a block of @count 24-bit samples
 * dst may equal src for in-place conversion
 *
 * assumes that dst and src are 32-bit aligned
 */
static void
converter_swap_endian_24 (gpointer dst, const gpointer src, gint count)
{
  guint32 *out = dst;
  const guint32 *in = src;
  guint8 *out8;
  const guint8 *in8;
  gint i;

  /* first convert 24-bit samples in multiples of 4 reading 3x 32-bits in one cycle
   *
   * input:               A1 B1 C1 A2 , B2 C2 A3 B3 , C3 A4 B4 C4
   * 32-bit endian swap:  A2 C1 B1 A1 , B3 A3 C2 B2 , C4 B4 A4 C3
   *                      <--  x  -->   <--  y  --> , <--  z  -->
   *
   * desired output:      C1 B1 A1 C2 , B2 A2 C3 B3 , A3 C4 B4 A4
   */
  for (i = 0; i < count / 4; i++, in += 3, out += 3) {
    guint32 x, y, z;

    x = GUINT32_SWAP_LE_BE (in[0]);
    y = GUINT32_SWAP_LE_BE (in[1]);
    z = GUINT32_SWAP_LE_BE (in[2]);

#if G_BYTE_ORDER == G_BIG_ENDIAN
    out[0] = (x << 8) + ((y >> 8) & 0xff);
    out[1] = (in[1] & 0xff0000ff) + ((x >> 8) & 0xff0000) + ((z << 8) & 0xff00);
    out[2] = (z >> 8) + ((y << 8) & 0xff000000);
#else
    out[0] = (x >> 8) + ((y << 8) & 0xff000000);
    out[1] = (in[1] & 0xff0000ff) + ((x << 8) & 0xff00) + ((z >> 8) & 0xff0000);
    out[2] = (z << 8) + ((y >> 8) & 0xff);
#endif
  }

  /* convert the remainder less efficiently */
  for (out8 = (guint8 *) out, in8 = (const guint8 *) in, i = 0; i < (count & 3);
      i++) {
    guint8 x = in8[i + 0];
    out8[i + 0] = in8[i + 2];
    out8[i + 1] = in8[i + 1];
    out8[i + 2] = x;
  }
}
#endif

/* perform LE<->BE conversion on a block of @count 32-bit samples
 * dst may equal src for in-place conversion
 */
static void
converter_swap_endian_32 (gpointer dst, const gpointer src, gint count)
{
  guint32 *out = dst;
  const guint32 *in = src;
  gint i;

  for (i = 0; i < count; i++)
    out[i] = GUINT32_SWAP_LE_BE (in[i]);
}

/* perform LE<->BE conversion on a block of @count 64-bit samples
 * dst may equal src for in-place conversion
 */
static void
converter_swap_endian_64 (gpointer dst, const gpointer src, gint count)
{
  guint64 *out = dst;
  const guint64 *in = src;
  gint i;

  for (i = 0; i < count; i++)
    out[i] = GUINT64_SWAP_LE_BE (in[i]);
}

/* the worker function to perform endian-conversion only
 * assuming finfo and foutinfo have the same depth
 */
static gboolean
converter_endian (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  gint i;
  AudioChain *chain;
  gsize samples;

  chain = convert->chain_end;
  samples = in_frames * chain->inc;

  GST_LOG ("convert endian: %" G_GSIZE_FORMAT " / %" G_GSIZE_FORMAT " samples",
      in_frames, samples);

  if (in) {
    for (i = 0; i < chain->blocks; i++)
      convert->swap_endian (out[i], in[i], samples);
  } else {
    for (i = 0; i < chain->blocks; i++)
      gst_audio_format_info_fill_silence (convert->in.finfo, out[i], samples);
  }
  return TRUE;
}

static gboolean
converter_generic (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  AudioChain *chain;
  gpointer *tmp;
  gint i;
  gsize produced;

  chain = convert->chain_end;

  convert->in_writable = flags & GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE;
  convert->in_data = in;
  convert->in_frames = in_frames;
  convert->out_data = out;
  convert->out_frames = out_frames;

  /* get frames to pack */
  tmp = audio_chain_get_samples (chain, &produced);

  if (!convert->out_default && tmp && out) {
    GST_LOG ("pack %p, %p %" G_GSIZE_FORMAT, tmp, out, produced);
    /* and pack if needed */
    for (i = 0; i < chain->blocks; i++)
      convert->out.finfo->pack_func (convert->out.finfo, 0, tmp[i], out[i],
          produced * chain->inc);
  }
  return TRUE;
}

static gboolean
converter_resample (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  gst_audio_resampler_resample (convert->resampler, in, in_frames, out,
      out_frames);

  return TRUE;
}

#define GST_AUDIO_FORMAT_IS_ENDIAN_CONVERSION(info1, info2) \
		( \
			!(((info1)->flags ^ (info2)->flags) & (~GST_AUDIO_FORMAT_FLAG_UNPACK)) && \
			(info1)->endianness != (info2)->endianness && \
			(info1)->width == (info2)->width && \
			(info1)->depth == (info2)->depth \
		)

/**
 * gst_audio_converter_new:
 * @flags: extra #GstAudioConverterFlags
 * @in_info: a source #GstAudioInfo
 * @out_info: a destination #GstAudioInfo
 * @config: (transfer full) (nullable): a #GstStructure with configuration options
 *
 * Create a new #GstAudioConverter that is able to convert between @in and @out
 * audio formats.
 *
 * @config contains extra configuration options, see `GST_AUDIO_CONVERTER_OPT_*`
 * parameters for details about the options and values.
 *
 * Returns: (nullable): a #GstAudioConverter or %NULL if conversion is not possible.
 */
GstAudioConverter *
gst_audio_converter_new (GstAudioConverterFlags flags, GstAudioInfo * in_info,
    GstAudioInfo * out_info, GstStructure * config)
{
  GstAudioConverter *convert;
  AudioChain *prev;
  const GValue *opt_matrix = NULL;

  g_return_val_if_fail (in_info != NULL, FALSE);
  g_return_val_if_fail (out_info != NULL, FALSE);

  if (config)
    opt_matrix =
        gst_structure_get_value (config, GST_AUDIO_CONVERTER_OPT_MIX_MATRIX);

  if (opt_matrix
      && !check_mix_matrix (in_info->channels, out_info->channels, opt_matrix))
    goto invalid_mix_matrix;

  if ((GST_AUDIO_INFO_CHANNELS (in_info) != GST_AUDIO_INFO_CHANNELS (out_info))
      && (GST_AUDIO_INFO_IS_UNPOSITIONED (in_info)
          || GST_AUDIO_INFO_IS_UNPOSITIONED (out_info))
      && !opt_matrix)
    goto unpositioned;

  convert = g_new0 (GstAudioConverter, 1);

  convert->flags = flags;
  convert->in = *in_info;
  convert->out = *out_info;

  /* default config */
  convert->config = gst_structure_new_empty ("GstAudioConverter");
  if (config)
    gst_audio_converter_update_config (convert, 0, 0, config);

  GST_INFO ("unitsizes: %d -> %d", in_info->bpf, out_info->bpf);

  /* step 1, unpack */
  prev = chain_unpack (convert);
  /* step 2, optional convert from S32 to F64 for channel mix */
  prev = chain_convert_in (convert, prev);
  /* step 3, channel mix */
  prev = chain_mix (convert, prev);
  /* step 4, resample */
  prev = chain_resample (convert, prev);
  /* step 5, optional convert for quantize */
  prev = chain_convert_out (convert, prev);
  /* step 6, optional quantize */
  prev = chain_quantize (convert, prev);
  /* step 7, change layout */
  prev = chain_change_layout (convert, prev);
  /* step 8, pack */
  convert->chain_end = chain_pack (convert, prev);

  convert->convert = converter_generic;
  convert->in_place = FALSE;
  convert->passthrough = FALSE;

  /* optimize */
  if (convert->mix_passthrough) {
    if (out_info->finfo->format == in_info->finfo->format) {
      if (convert->resampler == NULL) {
        if (out_info->layout == in_info->layout) {
          GST_INFO ("same formats, same layout, no resampler and "
              "passthrough mixing -> passthrough");
          convert->convert = converter_passthrough;
          convert->in_place = TRUE;
          convert->passthrough = TRUE;
        }
      } else {
        if (is_intermediate_format (in_info->finfo->format)) {
          GST_INFO ("same formats, and passthrough mixing -> only resampling");
          convert->convert = converter_resample;
        }
      }
    } else if (GST_AUDIO_FORMAT_IS_ENDIAN_CONVERSION (out_info->finfo,
            in_info->finfo)) {
      if (convert->resampler == NULL && out_info->layout == in_info->layout) {
        GST_INFO ("no resampler, passthrough mixing -> only endian conversion");
        convert->convert = converter_endian;
        convert->in_place = TRUE;

        switch (GST_AUDIO_INFO_WIDTH (in_info)) {
          case 16:
            GST_DEBUG ("initializing 16-bit endian conversion");
            convert->swap_endian = converter_swap_endian_16;
            break;
          case 24:
            GST_DEBUG ("initializing 24-bit endian conversion");
            convert->swap_endian = converter_swap_endian_24;
            break;
          case 32:
            GST_DEBUG ("initializing 32-bit endian conversion");
            convert->swap_endian = converter_swap_endian_32;
            break;
          case 64:
            GST_DEBUG ("initializing 64-bit endian conversion");
            convert->swap_endian = converter_swap_endian_64;
            break;
          default:
            GST_ERROR ("unsupported sample width for endian conversion");
            g_assert_not_reached ();
        }
      }
    }
  }

  setup_allocators (convert);

  return convert;

  /* ERRORS */
unpositioned:
  {
    GST_WARNING ("unpositioned channels");
    g_clear_pointer (&config, gst_structure_free);
    return NULL;
  }

invalid_mix_matrix:
  {
    GST_WARNING ("Invalid mix matrix");
    g_clear_pointer (&config, gst_structure_free);
    return NULL;
  }
}

/**
 * gst_audio_converter_free:
 * @convert: a #GstAudioConverter
 *
 * Free a previously allocated @convert instance.
 */
void
gst_audio_converter_free (GstAudioConverter * convert)
{
  AudioChain *chain;

  g_return_if_fail (convert != NULL);

  /* walk the chain backwards and free all elements */
  for (chain = convert->chain_end; chain;) {
    AudioChain *prev = chain->prev;
    audio_chain_free (chain);
    chain = prev;
  }

  if (convert->quant)
    gst_audio_quantize_free (convert->quant);
  if (convert->mix)
    gst_audio_channel_mixer_free (convert->mix);
  if (convert->resampler)
    gst_audio_resampler_free (convert->resampler);
  gst_audio_info_init (&convert->in);
  gst_audio_info_init (&convert->out);

  gst_structure_free (convert->config);

  g_free (convert);
}

/**
 * gst_audio_converter_get_out_frames:
 * @convert: a #GstAudioConverter
 * @in_frames: number of input frames
 *
 * Calculate how many output frames can be produced when @in_frames input
 * frames are given to @convert.
 *
 * Returns: the number of output frames
 */
gsize
gst_audio_converter_get_out_frames (GstAudioConverter * convert,
    gsize in_frames)
{
  if (convert->resampler)
    return gst_audio_resampler_get_out_frames (convert->resampler, in_frames);
  else
    return in_frames;
}

/**
 * gst_audio_converter_get_in_frames:
 * @convert: a #GstAudioConverter
 * @out_frames: number of output frames
 *
 * Calculate how many input frames are currently needed by @convert to produce
 * @out_frames of output frames.
 *
 * Returns: the number of input frames
 */
gsize
gst_audio_converter_get_in_frames (GstAudioConverter * convert,
    gsize out_frames)
{
  if (convert->resampler)
    return gst_audio_resampler_get_in_frames (convert->resampler, out_frames);
  else
    return out_frames;
}

/**
 * gst_audio_converter_get_max_latency:
 * @convert: a #GstAudioConverter
 *
 * Get the maximum number of input frames that the converter would
 * need before producing output.
 *
 * Returns: the latency of @convert as expressed in the number of
 * frames.
 */
gsize
gst_audio_converter_get_max_latency (GstAudioConverter * convert)
{
  if (convert->resampler)
    return gst_audio_resampler_get_max_latency (convert->resampler);
  else
    return 0;
}

/**
 * gst_audio_converter_reset:
 * @convert: a #GstAudioConverter
 *
 * Reset @convert to the state it was when it was first created, clearing
 * any history it might currently have.
 */
void
gst_audio_converter_reset (GstAudioConverter * convert)
{
  if (convert->resampler)
    gst_audio_resampler_reset (convert->resampler);
  if (convert->quant)
    gst_audio_quantize_reset (convert->quant);
}

/**
 * gst_audio_converter_samples:
 * @convert: a #GstAudioConverter
 * @flags: extra #GstAudioConverterFlags
 * @in: input frames
 * @in_frames: number of input frames
 * @out: output frames
 * @out_frames: number of output frames
 *
 * Perform the conversion with @in_frames in @in to @out_frames in @out
 * using @convert.
 *
 * In case the samples are interleaved, @in and @out must point to an
 * array with a single element pointing to a block of interleaved samples.
 *
 * If non-interleaved samples are used, @in and @out must point to an
 * array with pointers to memory blocks, one for each channel.
 *
 * @in may be %NULL, in which case @in_frames of silence samples are processed
 * by the converter.
 *
 * This function always produces @out_frames of output and consumes @in_frames of
 * input. Use gst_audio_converter_get_out_frames() and
 * gst_audio_converter_get_in_frames() to make sure @in_frames and @out_frames
 * are matching and @in and @out point to enough memory.
 *
 * Returns: %TRUE is the conversion could be performed.
 */
gboolean
gst_audio_converter_samples (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in[], gsize in_frames,
    gpointer out[], gsize out_frames)
{
  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  if (in_frames == 0) {
    GST_LOG ("skipping empty buffer");
    return TRUE;
  }
  return convert->convert (convert, flags, in, in_frames, out, out_frames);
}

/**
 * gst_audio_converter_convert:
 * @convert: a #GstAudioConverter
 * @flags: extra #GstAudioConverterFlags
 * @in: (array length=in_size) (element-type guint8): input data
 * @in_size: size of @in
 * @out: (out) (array length=out_size) (element-type guint8): a pointer where
 *  the output data will be written
 * @out_size: (out): a pointer where the size of @out will be written
 *
 * Convenience wrapper around gst_audio_converter_samples(), which will
 * perform allocation of the output buffer based on the result from
 * gst_audio_converter_get_out_frames().
 *
 * Returns: %TRUE is the conversion could be performed.
 *
 * Since: 1.14
 */
gboolean
gst_audio_converter_convert (GstAudioConverter * convert,
    GstAudioConverterFlags flags, gpointer in, gsize in_size,
    gpointer * out, gsize * out_size)
{
  gsize in_frames;
  gsize out_frames;

  g_return_val_if_fail (convert != NULL, FALSE);
  g_return_val_if_fail (flags ^ GST_AUDIO_CONVERTER_FLAG_IN_WRITABLE, FALSE);

  in_frames = in_size / convert->in.bpf;
  out_frames = gst_audio_converter_get_out_frames (convert, in_frames);

  *out_size = out_frames * convert->out.bpf;
  *out = g_malloc0 (*out_size);

  return gst_audio_converter_samples (convert, flags, &in, in_frames, out,
      out_frames);
}

/**
 * gst_audio_converter_supports_inplace:
 * @convert: a #GstAudioConverter
 *
 * Returns whether the audio converter can perform the conversion in-place.
 * The return value would be typically input to gst_base_transform_set_in_place()
 *
 * Returns: %TRUE when the conversion can be done in place.
 *
 * Since: 1.12
 */
gboolean
gst_audio_converter_supports_inplace (GstAudioConverter * convert)
{
  return convert->in_place;
}

/**
 * gst_audio_converter_is_passthrough:
 *
 * Returns whether the audio converter will operate in passthrough mode.
 * The return value would be typically input to gst_base_transform_set_passthrough()
 *
 * Returns: %TRUE when no conversion will actually occur.
 *
 * Since: 1.16
 */
gboolean
gst_audio_converter_is_passthrough (GstAudioConverter * convert)
{
  return convert->passthrough;
}
