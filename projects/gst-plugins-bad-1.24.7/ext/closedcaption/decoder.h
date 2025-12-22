/*
 *  libzvbi -- Old raw VBI decoder
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: decoder.h,v 1.11 2008-02-19 00:35:15 mschimek Exp $ */

#ifndef DECODER_H
#define DECODER_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "bcd.h"
#include "sliced.h"

/* Public */

#include <glib.h>

/* Bit slicer */

/**
 * @ingroup Rawdec
 * @brief Image format used as source to vbi_bit_slice() and vbi_raw_decode().
 *
 * @htmlonly
<table border=1>
<tr><th>Symbol</th><th>Byte&nbsp;0</th><th>Byte&nbsp;1</th><th>Byte&nbsp;2</th><th>Byte&nbsp;3</th></tr>
<tr><td colspan=5>Planar YUV 4:2:0 data.</td></tr>
<tr><td>VBI_PIXFMT_YUV420</td><td colspan=4>
 <table>
  <tr><th>Y plane</th><th>U plane</th><th>V plane</th></tr>
  <tr><td><table border=1>
   <tr><td>Y00</td><td>Y01</td><td>Y02</td><td>Y03</td></tr>
   <tr><td>Y10</td><td>Y11</td><td>Y12</td><td>Y13</td></tr>
   <tr><td>Y20</td><td>Y21</td><td>Y22</td><td>Y23</td></tr>
   <tr><td>Y30</td><td>Y31</td><td>Y32</td><td>Y33</td></tr>
  </table></td>
  <td><table border=1>
   <tr><td>Cb00</td><td>Cb01</td></tr>
   <tr><td>Cb10</td><td>Cb11</td></tr>
  </table></td>
  <td><table border=1>
   <tr><td>Cr00</td><td>Cr01</td></tr>
   <tr><td>Cr10</td><td>Cr11</td></tr>
  </table></td>
 </tr></table></td>
</tr>
<tr><td colspan=5>Packed YUV 4:2:2 data.</td></tr>
<tr><td>VBI_PIXFMT_YUYV</td><td>Y0</td><td>Cb</td><td>Y1</td><td>Cr</td></tr>
<tr><td>VBI_PIXFMT_YVYU</td><td>Y0</td><td>Cr</td><td>Y1</td><td>Cb</td></tr>
<tr><td>VBI_PIXFMT_UYVY</td><td>Cb</td><td>Y0</td><td>Cr</td><td>Y1</td></tr>
<tr><td>VBI_PIXFMT_VYUY</td><td>Cr</td><td>Y0</td><td>Cb</td><td>Y1</td></tr>
<tr><td colspan=5>Packed 32 bit RGB data.</td></tr>
<tr><td>VBI_PIXFMT_RGBA32_LE VBI_PIXFMT_ARGB32_BE</td>
<td>r7&nbsp;...&nbsp;r0</td><td>g7&nbsp;...&nbsp;g0</td>
<td>b7&nbsp;...&nbsp;b0</td><td>a7&nbsp;...&nbsp;a0</td></tr>
<tr><td>VBI_PIXFMT_BGRA32_LE VBI_PIXFMT_ARGB32_BE</td>
<td>b7&nbsp;...&nbsp;b0</td><td>g7&nbsp;...&nbsp;g0</td>
<td>r7&nbsp;...&nbsp;r0</td><td>a7&nbsp;...&nbsp;a0</td></tr>
<tr><td>VBI_PIXFMT_ARGB32_LE VBI_PIXFMT_BGRA32_BE</td>
<td>a7&nbsp;...&nbsp;a0</td><td>r7&nbsp;...&nbsp;r0</td>
<td>g7&nbsp;...&nbsp;g0</td><td>b7&nbsp;...&nbsp;b0</td></tr>
<tr><td>VBI_PIXFMT_ABGR32_LE VBI_PIXFMT_RGBA32_BE</td>
<td>a7&nbsp;...&nbsp;a0</td><td>b7&nbsp;...&nbsp;b0</td>
<td>g7&nbsp;...&nbsp;g0</td><td>r7&nbsp;...&nbsp;r0</td></tr>
<tr><td colspan=5>Packed 24 bit RGB data.</td></tr>
<tr><td>VBI_PIXFMT_RGBA24</td>
<td>r7&nbsp;...&nbsp;r0</td><td>g7&nbsp;...&nbsp;g0</td>
<td>b7&nbsp;...&nbsp;b0</td><td>&nbsp;</td></tr>
<tr><td>VBI_PIXFMT_BGRA24</td>
<td>b7&nbsp;...&nbsp;b0</td><td>g7&nbsp;...&nbsp;g0</td>
<td>r7&nbsp;...&nbsp;r0</td><td>&nbsp;</td></tr>
<tr><td colspan=5>Packed 16 bit RGB data.</td></tr>
<tr><td>VBI_PIXFMT_RGB16_LE</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
<td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_BGR16_LE</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
<td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_RGB16_BE</td>
<td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_BGR16_BE</td>
<td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr>
<tr><td colspan=5>Packed 15 bit RGB data.</td></tr>
<tr><td>VBI_PIXFMT_RGBA15_LE</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
<td>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_BGRA15_LE</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
<td>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_ARGB15_LE</td>
<td>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</td>
<td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_ABGR15_LE</td>
<td>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</td>
<td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_RGBA15_BE</td>
<td>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_BGRA15_BE</td>
<td>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</td>
<td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_ARGB15_BE</td>
<td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</td>
<td>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>VBI_PIXFMT_ABGR15_BE</td>
<td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</td>
<td>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</td>
<td>&nbsp;</td><td>&nbsp;</td></tr>
</table>
@endhtmlonly */
/* Attn: keep this in sync with rte, don't change order */
typedef enum {
	VBI_PIXFMT_YUV420 = 1,
	VBI_PIXFMT_YUYV,
	VBI_PIXFMT_YVYU,
	VBI_PIXFMT_UYVY,
	VBI_PIXFMT_VYUY,
        VBI_PIXFMT_PAL8,
	VBI_PIXFMT_RGBA32_LE = 32,
	VBI_PIXFMT_RGBA32_BE,
	VBI_PIXFMT_BGRA32_LE,
	VBI_PIXFMT_BGRA32_BE,
	VBI_PIXFMT_ABGR32_BE = 32, /* synonyms */
	VBI_PIXFMT_ABGR32_LE,
	VBI_PIXFMT_ARGB32_BE,
	VBI_PIXFMT_ARGB32_LE,
	VBI_PIXFMT_RGB24,
	VBI_PIXFMT_BGR24,
	VBI_PIXFMT_RGB16_LE,
	VBI_PIXFMT_RGB16_BE,
	VBI_PIXFMT_BGR16_LE,
	VBI_PIXFMT_BGR16_BE,
	VBI_PIXFMT_RGBA15_LE,
	VBI_PIXFMT_RGBA15_BE,
	VBI_PIXFMT_BGRA15_LE,
	VBI_PIXFMT_BGRA15_BE,
	VBI_PIXFMT_ARGB15_LE,
	VBI_PIXFMT_ARGB15_BE,
	VBI_PIXFMT_ABGR15_LE,
	VBI_PIXFMT_ABGR15_BE
} vbi_pixfmt;

/* Private */

typedef uint64_t vbi_pixfmt_set;

#define VBI_MAX_PIXFMTS 64
#define VBI_PIXFMT_SET(pixfmt) (((vbi_pixfmt_set) 1) << (pixfmt))
#define VBI_PIXFMT_SET_YUV (VBI_PIXFMT_SET (VBI_PIXFMT_YUV420) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_YUYV) |		\
			    VBI_PIXFMT_SET (VBI_PIXFMT_YVYU) |		\
			    VBI_PIXFMT_SET (VBI_PIXFMT_UYVY) |		\
			    VBI_PIXFMT_SET (VBI_PIXFMT_VYUY))
#define VBI_PIXFMT_SET_RGB (VBI_PIXFMT_SET (VBI_PIXFMT_RGBA32_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGBA32_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGRA32_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGRA32_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGB24) |		\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGR24) |		\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGB16_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGB16_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGR16_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGR16_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGBA15_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_RGBA15_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGRA15_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_BGRA15_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_ARGB15_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_ARGB15_BE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_ABGR15_LE) |	\
			    VBI_PIXFMT_SET (VBI_PIXFMT_ABGR15_BE))
#define VBI_PIXFMT_SET_ALL (VBI_PIXFMT_SET_YUV |			\
			    VBI_PIXFMT_SET_RGB)

#define VBI_PIXFMT_BPP(fmt)						\
	(((fmt) == VBI_PIXFMT_YUV420) ? 1 :				\
	 (((fmt) >= VBI_PIXFMT_RGBA32_LE				\
	   && (fmt) <= VBI_PIXFMT_BGRA32_BE) ? 4 :			\
	  (((fmt) == VBI_PIXFMT_RGB24					\
	    || (fmt) == VBI_PIXFMT_BGR24) ? 3 : 2)))

/* Public */

/**
 * @ingroup Rawdec
 * @brief Modulation used for VBI data transmission.
 */
typedef enum {
	/**
	 * The data is 'non-return to zero' coded, logical '1' bits
	 * are described by high sample values, logical '0' bits by
	 * low values. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_NRZ_LSB,
	/**
	 * 'Non-return to zero' coded, most significant bit first
	 * transmitted.
	 */
	VBI_MODULATION_NRZ_MSB,
	/**
	 * The data is 'bi-phase' coded. Each data bit is described
	 * by two complementary signalling elements, a logical '1'
	 * by a sequence of '10' elements, a logical '0' by a '01'
	 * sequence. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_BIPHASE_LSB,
	/**
	 * 'Bi-phase' coded, most significant bit first transmitted.
	 */
	VBI_MODULATION_BIPHASE_MSB
} vbi_modulation;

#if 0
/**
 * @ingroup Rawdec
 * @brief Bit slicer context.
 *
 * The contents of this structure are private,
 * use vbi_bit_slicer_init() to initialize.
 */
typedef struct vbi_bit_slicer {
	vbi_bool	(* func)(struct vbi_bit_slicer *slicer,
				 uint8_t *raw, uint8_t *buf);
	unsigned int	cri;
	unsigned int	cri_mask;
	int		thresh;
	int		cri_bytes;
	int		cri_rate;
	int		oversampling_rate;
	int		phase_shift;
	int		step;
	unsigned int	frc;
	int		frc_bits;
	int		payload;
	int		endian;
	int		skip;
} vbi_bit_slicer;

/**
 * @addtogroup Rawdec
 * @{
 */
extern void		vbi_bit_slicer_init(vbi_bit_slicer *slicer,
					    int raw_samples, int sampling_rate,
					    int cri_rate, int bit_rate,
					    unsigned int cri_frc, unsigned int cri_mask,
					    int cri_bits, int frc_bits, int payload,
					    vbi_modulation modulation, vbi_pixfmt fmt);
/**
 * @param slicer Pointer to initialized vbi_bit_slicer object.
 * @param raw Input data. At least the number of pixels or samples
 *  given as @a raw_samples to vbi_bit_slicer_init().
 * @param buf Output data. The buffer must be large enough to store
 *   the number of bits given as @a payload to vbi_bit_slicer_init().
 * 
 * Decode one scan line of raw vbi data. Note the bit slicer tries
 * to adapt to the average signal amplitude, you should avoid
 * using the same vbi_bit_slicer object for data from different
 * devices.
 *
 * @note As a matter of speed this function does not lock the
 * @a slicer. When you want to share a vbi_bit_slicer object between
 * multiple threads you must implement your own locking mechanism.
 * 
 * @return
 * @c FALSE if the raw data does not contain the expected
 * information, i. e. the CRI/FRC has not been found. This may also
 * result from a too weak or noisy signal. Error correction must be
 * implemented at a higher layer.
 */
_vbi_inline vbi_bool
vbi_bit_slice(vbi_bit_slicer *slicer, uint8_t *raw, uint8_t *buf)
{
	return slicer->func(slicer, raw, buf);
}
/** @} */
#endif
/**
 * @ingroup Rawdec
 * @brief Raw vbi decoder context.
 *
 * Only the sampling parameters are public. See
 * vbi_raw_decoder_parameters() and vbi_raw_decoder_add_services()
 * for usage.
 */
typedef struct vbi_raw_decoder {
	/* Sampling parameters */

	/**
	 * Either 525 (M/NTSC, M/PAL) or 625 (PAL, SECAM), describing the
	 * scan line system all line numbers refer to.
	 */
	int			scanning;
	/**
	 * Format of the raw vbi data.
	 */
	vbi_pixfmt		sampling_format;
	/**
	 * Sampling rate in Hz, the number of samples or pixels
	 * captured per second.
	 */
	int			sampling_rate;		/* Hz */
	/**
	 * Number of samples or pixels captured per scan line,
	 * in bytes. This determines the raw vbi image width and you
	 * want it large enough to cover all data transmitted in the line (with
	 * headroom).
	 */
	int			bytes_per_line;
	/**
	 * The distance from 0H (leading edge hsync, half amplitude point)
	 * to the first sample (pixel) captured, in samples (pixels). You want
	 * an offset small enough not to miss the start of the data
	 * transmitted.
	 */
	int			offset;			/* 0H, samples */
	/**
	 * First scan line to be captured, first and second field
	 * respectively, according to the ITU-R line numbering scheme
	 * (see vbi_sliced). Set to zero if the exact line number isn't
	 * known.
	 */
	int			start[2];		/* ITU-R numbering */
	/**
	 * Number of scan lines captured, first and second
	 * field respectively. This can be zero if only data from one
	 * field is required. The sum @a count[0] + @a count[1] determines the
	 * raw vbi image height.
	 */
	int			count[2];		/* field lines */
	/**
	 * In the raw vbi image, normally all lines of the second
	 * field are supposed to follow all lines of the first field. When
	 * this flag is set, the scan lines of first and second field
	 * will be interleaved in memory. This implies @a count[0] and @a count[1]
	 * are equal.
	 */
	vbi_bool		interlaced;
	/**
	 * Fields must be stored in temporal order, i. e. as the
	 * lines have been captured. It is assumed that the first field is
	 * also stored first in memory, however if the hardware cannot reliable
	 * distinguish fields this flag shall be cleared, which disables
	 * decoding of data services depending on the field number.
	 */
	vbi_bool		synchronous;

	/*< private >*/

	GMutex			mutex;

	unsigned int		services;
#if 0				/* DISABLED LEGACY DECODER */
	int			num_jobs;
#endif

	int8_t *		pattern; /* The real vbi3_raw_decoder */
#if 0		/* DISABLED LEGACY DECODER */
	struct _vbi_raw_decoder_job {
		unsigned int		id;
		int			offset;
		vbi_bit_slicer		slicer;
	}			jobs[8];
#endif
} vbi_raw_decoder;

/**
 * @addtogroup Rawdec
 * @{
 */
extern void		vbi_raw_decoder_init(vbi_raw_decoder *rd);
extern void		vbi_raw_decoder_reset(vbi_raw_decoder *rd);
extern void		vbi_raw_decoder_destroy(vbi_raw_decoder *rd);
extern unsigned int	vbi_raw_decoder_add_services(vbi_raw_decoder *rd,
						     unsigned int services,
						     int strict);
extern unsigned int     vbi_raw_decoder_check_services(vbi_raw_decoder *rd,
						     unsigned int services, int strict);
extern unsigned int	vbi_raw_decoder_remove_services(vbi_raw_decoder *rd,
							unsigned int services);
extern void             vbi_raw_decoder_resize( vbi_raw_decoder *rd,
						int * start, unsigned int * count );
extern unsigned int	vbi_raw_decoder_parameters(vbi_raw_decoder *rd, unsigned int services,
						   int scanning, int *max_rate);
extern int		vbi_raw_decode(vbi_raw_decoder *rd, uint8_t *raw, vbi_sliced *out);

void vbi_initialize_gst_debug (void);
/** @} */

/* Private */

#endif /* DECODER_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
