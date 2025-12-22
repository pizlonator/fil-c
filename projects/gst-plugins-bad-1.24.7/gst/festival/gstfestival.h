/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                        Copyright (c) 1999                             */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Alan W Black (awb@cstr.ed.ac.uk)                */
/*             Date   :  March 1999                                      */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Client end of Festival server API (in C) designed specifically for    */
/* Galaxy Communicator use, though might be of use for other things      */
/*                                                                       */
/*=======================================================================*/

#ifndef __GST_FESTIVAL_H__
#define __GST_FESTIVAL_H__


#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define FESTIVAL_DEFAULT_SERVER_HOST "localhost"
#define FESTIVAL_DEFAULT_SERVER_PORT 1314
#define FESTIVAL_DEFAULT_TEXT_MODE "fundamental"

typedef struct FT_Info
{
    int encoding;
    const char *server_host;
    int server_port;
    const char *text_mode;
    
    int server_fd;
} FT_Info;

#define GST_TYPE_FESTIVAL \
  (gst_festival_get_type())
#define GST_FESTIVAL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FESTIVAL,GstFestival))
#define GST_FESTIVAL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FESTIVAL,GstFestivalClass))
#define GST_IS_FESTIVAL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FESTIVAL))
#define GST_IS_FESTIVAL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FESTIVAL))

typedef enum {
  GST_FESTIVAL_OPEN             = (GST_ELEMENT_FLAG_LAST << 0),

  GST_FESTIVAL_FLAG_LAST        = (GST_ELEMENT_FLAG_LAST << 2),
} GstFestivalFlags;

typedef struct _GstFestival GstFestival;
typedef struct _GstFestivalClass GstFestivalClass;

struct _GstFestival {
  GstElement element;

  /* pads */
  GstPad *sinkpad,*srcpad;

  FT_Info *info;
};

struct _GstFestivalClass {
  GstElementClass parent_class;
};

GType gst_festival_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (festival);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GST_FESTIVAL_H__ */
