/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 * Copyright © 2019, 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>

#include <glib.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>

#include "cairo-glue.hh"
#include "pango-glue.hh"
#include "refptr.hh"
#include "vteunistr.h"

/* Overview:
 *
 *
 * This file implements vte rendering using pangocairo.  Note that this does
 * NOT implement any kind of complex text rendering.  That's not currently a
 * goal.
 *
 * The aim is to be super-fast and avoid unneeded work as much as possible.
 * Here is an overview of how that is accomplished:
 *
 *   - We attach a font_info to the draw.  A font_info has all the information
 *     to quickly draw text.
 *
 *   - A font_info keeps uses unistr_font_info structs that represent all
 *     information needed to quickly draw a single vteunistr.  The font_info
 *     creates those unistr_font_info structs on demand and caches them
 *     indefinitely.  It uses a direct array for the ASCII range and a hash
 *     table for the rest.
 *
 *
 * Fast rendering of unistrs:
 *
 * A unistr_font_info (uinfo) calls Pango to set text for the unistr upon
 * initialization and then caches information needed to draw the results
 * later.  It uses three different internal representations and respectively
 * three drawing paths:
 *
 *   - Coverage::USE_CAIRO_GLYPH:
 *     Keeping a single glyph index and a cairo scaled-font.  This is the
 *     fastest way to draw text as it bypasses Pango completely and allows
 *     for stuffing multiple glyphs into a single cairo_show_glyphs() request
 *     (if scaled-fonts match).  This method is used if the glyphs used for
 *     the vteunistr as determined by Pango consists of a single regular glyph
 *     positioned at 0,0 using a regular font.  This method is used for more
 *     than 99% of the cases.  Only exceptional cases fall through to the
 *     other two methods.
 *
 *   - Coverage::USE_PANGO_GLYPH_STRING:
 *     Keeping a pango glyphstring and a pango font.  This is slightly slower
 *     than the previous case as drawing each glyph goes through pango
 *     separately and causes a separate cairo_show_glyphs() call.  This method
 *     is used when the previous method cannot be used but the glyphs for the
 *     character all use a single font.  This is the method used for hexboxes
 *     and "empty" characters like U+200C ZERO WIDTH NON-JOINER for example.
 *
 *   - Coverage::USE_PANGO_LAYOUT_LINE:
 *     Keeping a pango layout line.  This method is used only in the very
 *     weird and exceptional case that a single vteunistr uses more than one
 *     font to be drawn.  This happens for example if some diacretics is not
 *     available in the font chosen for the base character.
 *
 *
 * Caching of font infos:
 *
 * To avoid recreating font info structs for the same font again and again we
 * do the following:
 *
 *   - Use a global cache to share font info structs across different widgets.
 *     We use pango language, cairo font options, resolution, and font description
 *     as the key for our hash table.
 *
 *   - When a font info struct is no longer used by any widget, we delay
 *     destroying it for a while (FONT_CACHE_TIMEOUT seconds).  This is
 *     supposed to serve two purposes:
 *
 *       * Destroying a terminal widget and creating it again right after will
 *         reuse the font info struct from the previous widget.
 *
 *       * Zooming in and out a terminal reuses the font info structs.
 *
 *
 * Pre-caching ASCII letters:
 *
 * When initializing a font info struct we measure a string consisting of all
 * ASCII letters and some other ASCII characters.  Since we have a shaped pango
 * layout at hand, we walk over it and cache unistr font info for the ASCII
 * letters if we can do that easily using Coverage::USE_CAIRO_GLYPH.  This
 * means that we precache all ASCII letters without any extra pango shaping
 * involved.
 */

namespace vte {
namespace view {

class DrawingContext;

class FontInfo {
        friend class DrawingContext;

        int const font_cache_timeout = 30; // seconds

public:
        FontInfo(vte::glib::RefPtr<PangoContext> context);
        ~FontInfo();

        FontInfo* ref()
        {
                // refcount is 0 when unused but still in cache
                assert(m_ref_count >= 0);

                ++m_ref_count;

                if (m_destroy_timeout != 0) {
                        g_source_remove (m_destroy_timeout);
                        m_destroy_timeout = 0;
                }

                return this;
        }

        void unref()
        {
                assert(m_ref_count > 0);
                if (--m_ref_count > 0)
                        return;

                /* Delay destruction by a few seconds, in case we need it again */
                m_destroy_timeout = g_timeout_add_seconds(font_cache_timeout,
                                                          (GSourceFunc)destroy_delayed_cb,
                                                          this);
        }

        struct UnistrInfo {
                enum class Coverage : uint8_t {
                        /* in increasing order of speed */
                        UNKNOWN = 0u,           /* we don't know about the character yet    */
#if VTE_GTK == 3
                        USE_PANGO_LAYOUT_LINE,  /* use a PangoLayoutLine for the character  */
                        USE_PANGO_GLYPH_STRING, /* use a PangoGlyphString for the character */
                        USE_CAIRO_GLYPH,        /* use a cairo_glyph_t for the character    */
#elif VTE_GTK == 4
                        USE_PANGO_GLYPH_STRING,
#endif
                };

                uint8_t m_coverage{uint8_t(Coverage::UNKNOWN)};
                uint8_t has_unknown_chars;
                uint16_t width;

                inline constexpr Coverage coverage() const noexcept { return Coverage{m_coverage}; }
                inline constexpr void set_coverage(Coverage coverage) { m_coverage = uint8_t(coverage); }

                // FIXME: use std::variant<std::monostate, RefPtr<PangoLayoutLine>, ...> ?
                union unistr_font_info {
#if VTE_GTK == 3
                        /* Coverage::USE_PANGO_LAYOUT_LINE */
                        struct {
                                PangoLayoutLine *line;
                        } using_pango_layout_line;
                        /* Coverage::USE_CAIRO_GLYPH */
                        struct {
                                cairo_scaled_font_t *scaled_font;
                                unsigned int glyph_index;
                        } using_cairo_glyph;
#endif
                        /* Coverage::USE_PANGO_GLYPH_STRING */
                        struct {
                                PangoFont *font;
                                PangoGlyphString *glyph_string;
                        } using_pango_glyph_string;
                } m_ufi;

                UnistrInfo() noexcept = default;

                ~UnistrInfo() noexcept
                {
                        switch (coverage()) {
                        default:
                        case Coverage::UNKNOWN:
                                break;
#if VTE_GTK == 3
                        case Coverage::USE_PANGO_LAYOUT_LINE:
                                /* we hold a manual reference on layout */
                                g_object_unref (m_ufi.using_pango_layout_line.line->layout);
                                m_ufi.using_pango_layout_line.line->layout = NULL;
                                pango_layout_line_unref (m_ufi.using_pango_layout_line.line);
                                m_ufi.using_pango_layout_line.line = NULL;
                                break;
                        case Coverage::USE_CAIRO_GLYPH:
                                cairo_scaled_font_destroy (m_ufi.using_cairo_glyph.scaled_font);
                                m_ufi.using_cairo_glyph.scaled_font = NULL;
                                break;
#endif
                        case Coverage::USE_PANGO_GLYPH_STRING:
                                if (m_ufi.using_pango_glyph_string.font)
                                        g_object_unref (m_ufi.using_pango_glyph_string.font);
                                m_ufi.using_pango_glyph_string.font = NULL;
                                pango_glyph_string_free (m_ufi.using_pango_glyph_string.glyph_string);
                                m_ufi.using_pango_glyph_string.glyph_string = NULL;
                                break;
                        }
                }

        }; // struct UnistrInfo

        UnistrInfo *get_unistr_info(vteunistr c);
        inline constexpr int width() const { return m_width; }
        inline constexpr int height() const { return m_height; }
        inline constexpr int ascent() const { return m_ascent; }

private:

        static void unistr_info_destroy(UnistrInfo* uinfo)
        {
                delete uinfo;
        }

        static gboolean destroy_delayed_cb(void* that)
        {
                auto info = reinterpret_cast<FontInfo*>(that);
                info->m_destroy_timeout = 0;
                delete info;
                return false;
        }

        mutable int m_ref_count{1};

        UnistrInfo* find_unistr_info(vteunistr c);
        void cache_ascii();
        void measure_font();
        guint m_destroy_timeout{0}; /* only used when ref_count == 0 */

	/* reusable layout set with font and everything set */
        vte::glib::RefPtr<PangoLayout> m_layout{};

	/* cache of character info */
        // FIXME: use std::array<UnistrInfo, 128>
	UnistrInfo m_ascii_unistr_info[128];
        // FIXME: use std::unordered_map<vteunistr, UnistrInfo>
	GHashTable* m_other_unistr_info{nullptr};

        /* cell metrics as taken from the font, not yet scaled by cell_{width,height}_scale */
	int m_width{1};
        int m_height{1};
        int m_ascent{0};

	/* reusable string for UTF-8 conversion */
        // FIXME: use std::string
	GString* m_string{nullptr};

#if VTE_DEBUG
	/* profiling info */
	int m_coverage_count[4]{0, 0, 0, 0};
#endif

        static FontInfo* create_for_context(vte::glib::RefPtr<PangoContext> context,
                                            PangoFontDescription const* desc,
                                            PangoLanguage* language,
                                            cairo_font_options_t const* font_options,
                                            guint fontconfig_timestamp);
#if VTE_GTK == 3
        static FontInfo *create_for_screen(GdkScreen* screen,
                                           PangoFontDescription const* desc,
                                           PangoLanguage* language,
                                           cairo_font_options_t const* font_options);
#endif

public:

        static FontInfo *create_for_widget(GtkWidget* widget,
                                           PangoFontDescription const* desc,
                                           cairo_font_options_t const* font_options);

}; // class FontInfo

} // namespace view
} // namespace vte
