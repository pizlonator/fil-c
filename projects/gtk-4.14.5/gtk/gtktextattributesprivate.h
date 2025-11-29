/* gtktextattributes.h - text attributes
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */

#pragma once


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>


G_BEGIN_DECLS

typedef struct _GtkTextAttributes GtkTextAttributes;
typedef struct _GtkTextAppearance GtkTextAppearance;

struct _GtkTextAppearance
{
  GdkRGBA *bg_rgba;
  GdkRGBA *fg_rgba;
  GdkRGBA *underline_rgba;
  GdkRGBA *overline_rgba;
  GdkRGBA *strikethrough_rgba;

  /* super/subscript rise, can be negative */
  int rise;

  guint underline : 4;          /* PangoUnderline */
  guint overline  : 2;          /* PangoOverline */
  guint strikethrough : 1;

  /* Whether to use background-related values; this is irrelevant for
   * the values struct when in a tag, but is used for the composite
   * values struct; it's true if any of the tags being composited
   * had background stuff set.
   */
  guint draw_bg : 1;

  /* These are only used when we are actually laying out and rendering
   * a paragraph; not when a GtkTextAppearance is part of a
   * GtkTextAttributes.
   */
  guint inside_selection : 1;
  guint is_text : 1;
};

/**
 * GtkTextAttributes:
 * @appearance: GtkTextAppearance for text.
 * @justification: GtkJustification for text.
 * @direction: GtkTextDirection for text.
 * @font: `PangoFontDescription` for text.
 * @font_scale: Font scale factor.
 * @left_margin: Width of the left margin in pixels.
 * @right_margin: Width of the right margin in pixels.
 * @indent: Amount to indent the paragraph, in pixels.
 * @pixels_above_lines: Pixels of blank space above paragraphs.
 * @pixels_below_lines: Pixels of blank space below paragraphs.
 * @pixels_inside_wrap: Pixels of blank space between wrapped lines in
 *   a paragraph.
 * @tabs: Custom `PangoTabArray` for this text.
 * @wrap_mode: `GtkWrapMode` for text.
 * @language: `PangoLanguage` for text.
 * @invisible: Hide the text.
 * @bg_full_height: Background is fit to full line height rather than
 *    baseline +/- ascent/descent (font height).
 * @editable: Can edit this text.
 * @no_fallback: Whether to disable font fallback.
 * @letter_spacing: Extra space to insert between graphemes, in Pango units
 *
 * Using GtkTextAttributes directly should rarely be necessary.
 * It’s primarily useful with gtk_text_iter_get_attributes().
 * As with most GTK structs, the fields in this struct should only
 * be read, never modified directly.
 */
struct _GtkTextAttributes
{
  guint refcount;

  GtkJustification justification;
  GtkTextDirection direction;
  GtkWrapMode wrap_mode;

  GtkTextAppearance appearance;

  PangoFontDescription *font;
  char *font_features;

  GdkRGBA *pg_bg_rgba;

  PangoTabArray *tabs;
  PangoLanguage *language;

  double font_scale;

  int left_margin;
  int right_margin;
  int indent;

  int pixels_above_lines;
  int pixels_below_lines;
  int pixels_inside_wrap;

  float line_height;

  int letter_spacing;

  guint invisible : 1;
  guint bg_full_height : 1;
  guint editable : 1;
  guint no_fallback: 1;
  guint no_breaks : 1;
  guint show_spaces : 3; /* PangoShowFlags */
  guint no_hyphens : 1;
  guint line_height_is_absolute : 1;
  guint text_transform : 3; /* PangoTextTransform */
  guint word : 1;
  guint sentence : 1;
};

GtkTextAttributes* gtk_text_attributes_new         (void);
GtkTextAttributes* gtk_text_attributes_copy        (GtkTextAttributes *src);
void               gtk_text_attributes_copy_values (GtkTextAttributes *src,
                                                    GtkTextAttributes *dest);
void               gtk_text_attributes_unref       (GtkTextAttributes *values);
GtkTextAttributes *gtk_text_attributes_ref         (GtkTextAttributes *values);

G_END_DECLS

