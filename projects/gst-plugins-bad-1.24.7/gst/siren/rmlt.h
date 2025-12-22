/*
 * Siren Encoder/Decoder library
 *
 *   @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
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


#ifndef _SIREN7_RMLT_H_
#define _SIREN7_RMLT_H_

extern void siren_rmlt_init(void);
extern int siren_rmlt_encode_samples(float *samples, float *old_samples, int dct_length, float *rmlt_coefs);
extern int siren_rmlt_decode_samples(float *coefs, float *old_coefs, int dct_length, float *samples);

#endif /* _SIREN7_RMLT_H_ */
