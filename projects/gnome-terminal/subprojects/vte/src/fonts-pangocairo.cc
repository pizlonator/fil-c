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

#include "config.h"

#include "fonts-pangocairo.hh"

#include "cairo-glue.hh"
#include "debug.h"
#include "vtedefines.hh"

/* Have a space between letters to make sure ligatures aren't used when caching the glyphs: bug 793391. */
#define VTE_DRAW_SINGLE_WIDE_CHARACTERS	\
					"  ! \" # $ % & ' ( ) * + , - . / " \
					"0 1 2 3 4 5 6 7 8 9 " \
					": ; < = > ? @ " \
					"A B C D E F G H I J K L M N O P Q R S T U V W X Y Z " \
					"[ \\ ] ^ _ ` " \
					"a b c d e f g h i j k l m n o p q r s t u v w x y z " \
					"{ | } ~ " \
					""

static inline bool
_vte_double_equal(double a,
                  double b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        return a == b;
#pragma GCC diagnostic pop
}

#define FONT_CACHE_TIMEOUT (30) /* seconds */

namespace vte {
namespace view {

static GHashTable* s_font_info_for_context{nullptr};

FontInfo::UnistrInfo*
FontInfo::find_unistr_info(vteunistr c)
{
	if (G_LIKELY (c < G_N_ELEMENTS(m_ascii_unistr_info)))
		return &m_ascii_unistr_info[c];

	if (G_UNLIKELY (m_other_unistr_info == nullptr))
		m_other_unistr_info = g_hash_table_new_full(nullptr, nullptr, nullptr, (GDestroyNotify)unistr_info_destroy);

	auto uinfo = reinterpret_cast<UnistrInfo*>(g_hash_table_lookup(m_other_unistr_info, GINT_TO_POINTER(c)));
	if (G_LIKELY (uinfo))
		return uinfo;

	uinfo = new UnistrInfo{};
	g_hash_table_insert(m_other_unistr_info, GINT_TO_POINTER (c), uinfo);
	return uinfo;
}

void
FontInfo::cache_ascii()
{
	PangoLayoutLine *line;
	PangoGlyphItemIter iter;
	PangoGlyphItem *glyph_item;
	PangoGlyphString *glyph_string;
	PangoFont *pango_font;
	cairo_scaled_font_t *scaled_font;
	const char *text;
	gboolean more;
	PangoLanguage *language;
	gboolean latin_uses_default_language;

	/* We have m_layout holding most ASCII characters.  We want to
	 * cache as much info as we can about the ASCII letters so we don't
	 * have to look them up again later */

	/* Don't cache if unknown glyphs found in layout */
	if (pango_layout_get_unknown_glyphs_count(m_layout.get()) != 0)
		return;

	language = pango_context_get_language(pango_layout_get_context(m_layout.get()));
	if (language == nullptr)
		language = pango_language_get_default ();
	latin_uses_default_language = pango_language_includes_script (language, PANGO_SCRIPT_LATIN);

	text = pango_layout_get_text(m_layout.get());

	line = pango_layout_get_line_readonly(m_layout.get(), 0);

	/* Don't cache if more than one font used for the line */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
		return;

	glyph_item = (PangoGlyphItem *)line->runs->data;
	glyph_string = glyph_item->glyphs;
	pango_font = glyph_item->item->analysis.font;
	if (!pango_font)
		return;
	scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);
	if (!scaled_font)
		return;

	for (more = pango_glyph_item_iter_init_start (&iter, glyph_item, text);
	     more;
	     more = pango_glyph_item_iter_next_cluster (&iter))
	{
		PangoGlyphGeometry *geometry;
		PangoGlyph glyph;
		vteunistr c;

		/* Only cache simple clusters */
		if (iter.start_char +1 != iter.end_char  ||
		    iter.start_index+1 != iter.end_index ||
		    iter.start_glyph+1 != iter.end_glyph)
			continue;

		c = text[iter.start_index];
		glyph = glyph_string->glyphs[iter.start_glyph].glyph;
		geometry = &glyph_string->glyphs[iter.start_glyph].geometry;

		/* If not using the default locale language, only cache non-common
		 * characters as common characters get their font from their neighbors
		 * and we don't want to force Latin on them. */
		if (!latin_uses_default_language &&
                    g_unichar_get_script (c) <= G_UNICODE_SCRIPT_INHERITED)
			continue;

		/* Only cache simple glyphs */
		if (!(glyph <= 0xFFFF) || (geometry->x_offset | geometry->y_offset) != 0)
			continue;

		auto uinfo = find_unistr_info(c);
		if (G_UNLIKELY (uinfo->coverage() != UnistrInfo::Coverage::UNKNOWN))
			continue;

		auto ufi = &uinfo->m_ufi;

		uinfo->width = PANGO_PIXELS_CEIL (geometry->width);
		uinfo->has_unknown_chars = false;

#if VTE_GTK == 3
		uinfo->set_coverage(UnistrInfo::Coverage::USE_CAIRO_GLYPH);

		ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
		ufi->using_cairo_glyph.glyph_index = glyph;
#elif VTE_GTK == 4
		uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING);

		ufi->using_pango_glyph_string.font = (PangoFont *)g_object_ref (pango_font);
		ufi->using_pango_glyph_string.glyph_string = pango_glyph_string_new ();
		pango_glyph_string_set_size (ufi->using_pango_glyph_string.glyph_string, 1);
		ufi->using_pango_glyph_string.glyph_string->num_glyphs = 1;
		ufi->using_pango_glyph_string.glyph_string->glyphs[0] = glyph_string->glyphs[iter.start_glyph];
		ufi->using_pango_glyph_string.glyph_string->log_clusters[0] = 0;
#endif

#if VTE_DEBUG
		m_coverage_count[0]++;
		m_coverage_count[(unsigned)uinfo->coverage()]++;
#endif
	}

#if VTE_DEBUG
	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p cached %d ASCII letters\n",
			  (void*)this, m_coverage_count[0]);
#endif
}

void
FontInfo::measure_font()
{
	PangoRectangle logical;

        /* Measure U+0021..U+007E individually instead of all together and then
         * averaging. For monospace fonts, the results should be the same, but
         * if the user (by design, or trough mis-configuration) uses a proportional
         * font, the latter method will greatly underestimate the required width,
         * leading to unreadable, overlapping characters.
         * https://gitlab.gnome.org/GNOME/vte/issues/138
         */
        auto max_width = 1;
        auto max_height = 1;
        for (char c = 0x21; c < 0x7f; ++c) {
                pango_layout_set_text(m_layout.get(), &c, 1);
                pango_layout_get_extents(m_layout.get(), nullptr, &logical);
                max_width = std::max(max_width, PANGO_PIXELS_CEIL(logical.width));
                max_height = std::max(max_height, PANGO_PIXELS_CEIL(logical.height));
        }

        /* Use the sample text to get the baseline */
	pango_layout_set_text(m_layout.get(), VTE_DRAW_SINGLE_WIDE_CHARACTERS, -1);
	pango_layout_get_extents(m_layout.get(), nullptr, &logical);
	/* We don't do CEIL for width since we are averaging;
	 * rounding is more accurate */
	m_ascent = PANGO_PIXELS_CEIL(pango_layout_get_baseline(m_layout.get()));

        m_height = max_height;
        m_width = max_width;

	/* Now that we shaped the entire ASCII character string, cache glyph
	 * info for them */
	cache_ascii();
}

FontInfo::FontInfo(vte::glib::RefPtr<PangoContext> context)
{
	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p allocating FontInfo\n",
			  (void*)this);

	m_layout = vte::glib::take_ref(pango_layout_new(context.get()));

	auto tabs = pango_tab_array_new_with_positions(1, FALSE, PANGO_TAB_LEFT, 1);
	pango_layout_set_tabs(m_layout.get(), tabs);
	pango_tab_array_free(tabs);

        // FIXME!!!
	m_string = g_string_sized_new(VTE_UTF8_BPC+1);

        measure_font();

#if PANGO_VERSION_CHECK(1, 44, 0)
        /* Try using the font's metrics; see issue#163. */
        if (auto metrics = vte::take_freeable
            (pango_context_get_metrics(context.get(),
                                       nullptr /* use font from context */,
                                       nullptr /* use language from context */))) {
		/* Use provided metrics if possible */
		auto const ascent = PANGO_PIXELS_CEIL(pango_font_metrics_get_ascent(metrics.get()));
		auto const height = PANGO_PIXELS_CEIL(pango_font_metrics_get_height(metrics.get()));
#if 0
                /* Note that we cannot use the font's width, since doing so
                 * regresses issue#138 (non-monospaced font).
                 * FIXME: Make sure the font is monospace before we get
                 * here, and then use the font's width too.
                 */
		auto const width = PANGO_PIXELS_CEIL(pango_font_metrics_get_approximate_char_width(metrics.get()));
#endif /* 0 */

                /* Sometimes, the metrics return a lower height than the one we measured
                 * in measure_font(), causing cut-off at the bottom of the last line, see
                 * https://gitlab.gnome.org/GNOME/gnome-terminal/-/issues/340 . Therefore
                 * we only use the metrics when its height is at least that which we measured.
                 */
                if (ascent > 0 && height >= m_height) {
                        _vte_debug_print(VTE_DEBUG_PANGOCAIRO, "Using pango metrics\n");

                        m_ascent = ascent;
                        m_height = height;
#if 0
                        m_width = width;
#endif
                } else if (ascent >= 0 && height > 0) {
                        _vte_debug_print(VTE_DEBUG_PANGOCAIRO, "Disregarding pango metrics due to incorrect height (%d < %d)\n",
                                         height, m_height);
                } else {
                        _vte_debug_print(VTE_DEBUG_PANGOCAIRO, "Not using pango metrics due to not providing height or ascent\n");
                }
	}
#endif /* pango >= 1.44 */

	_vte_debug_print (VTE_DEBUG_PANGOCAIRO | VTE_DEBUG_MISC,
			  "vtepangocairo: %p font metrics = %dx%d (%d)\n",
			  (void*)this, m_width, m_height, m_ascent);

	g_hash_table_insert(s_font_info_for_context,
                            pango_layout_get_context(m_layout.get()),
                            this);

}

FontInfo::~FontInfo()
{
	g_hash_table_remove(s_font_info_for_context,
                            pango_layout_get_context(m_layout.get()));

#if VTE_DEBUG
	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p freeing font_info.  coverages %d = %d + %d + %d\n",
			  (void*)this,
			  m_coverage_count[0],
			  m_coverage_count[1],
			  m_coverage_count[2],
			  m_coverage_count[3]);
#endif

	g_string_free(m_string, true);

	if (m_other_unistr_info) {
		g_hash_table_destroy(m_other_unistr_info);
	}
}

static GQuark
fontconfig_timestamp_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("vte-fontconfig-timestamp");

	return quark;
}

static void
vte_pango_context_set_fontconfig_timestamp (PangoContext *context,
					    guint         fontconfig_timestamp)
{
	g_object_set_qdata ((GObject *) context,
			    fontconfig_timestamp_quark (),
			    GUINT_TO_POINTER (fontconfig_timestamp));
}

static guint
vte_pango_context_get_fontconfig_timestamp (PangoContext *context)
{
	return GPOINTER_TO_UINT (g_object_get_qdata ((GObject *) context,
						     fontconfig_timestamp_quark ()));
}

static guint
context_hash (PangoContext *context)
{
	return pango_units_from_double (pango_cairo_context_get_resolution (context))
	     ^ pango_font_description_hash (pango_context_get_font_description (context))
	     ^ cairo_font_options_hash (pango_cairo_context_get_font_options (context))
	     ^ GPOINTER_TO_UINT (pango_context_get_language (context))
	     ^ vte_pango_context_get_fontconfig_timestamp (context);
}

static gboolean
context_equal (PangoContext *a,
	       PangoContext *b)
{
	return _vte_double_equal(pango_cairo_context_get_resolution(a), pango_cairo_context_get_resolution (b))
	    && pango_font_description_equal (pango_context_get_font_description (a), pango_context_get_font_description (b))
	    && cairo_font_options_equal (pango_cairo_context_get_font_options (a), pango_cairo_context_get_font_options (b))
	    && pango_context_get_language (a) == pango_context_get_language (b)
	    && vte_pango_context_get_fontconfig_timestamp (a) == vte_pango_context_get_fontconfig_timestamp (b);
}

// FIXMEchpe return vte::base::RefPtr<FontInfo>
FontInfo*
FontInfo::create_for_context(vte::glib::RefPtr<PangoContext> context,
                             PangoFontDescription const* desc,
                             PangoLanguage* language,
                             cairo_font_options_t const* font_options,
                             guint fontconfig_timestamp)
{
	if (!PANGO_IS_CAIRO_FONT_MAP(pango_context_get_font_map(context.get()))) {
		/* Ouch, Gtk+ switched over to some drawing system?
		 * Lets just create one from the default font map.
		 */
		context = vte::glib::take_ref(pango_font_map_create_context(pango_cairo_font_map_get_default()));
	}

	vte_pango_context_set_fontconfig_timestamp(context.get(), fontconfig_timestamp);

	pango_context_set_base_dir(context.get(), PANGO_DIRECTION_LTR);

	if (desc)
		pango_context_set_font_description(context.get(), desc);

        if (language != nullptr &&
            language != pango_context_get_language(context.get()))
                pango_context_set_language(context.get(), language);

        {
                // Make sure our contexts have a font_options set.  We use
                // this invariant in our context hash and equal functions.
                auto builtin_font_options = vte::take_freeable(cairo_font_options_create());

#if VTE_GTK == 4
                // On gtk4, we need to ensure Pango and cairo are configured to quantize
                // and hint font metrics.  Terminal cells in vte have integer pixel sizes.
                // If Pango is configured to do sub-pixel glyph advances, a small fractional
                // part might get rounded up to a whole pixel; so the character spacing will
                // appear too wide. Setting the cairo hint metrics option ensures that there
                // are integer numbers of pixels both above and below the baseline.
                // See issue#2573.
                cairo_font_options_set_hint_metrics(builtin_font_options.get(),
                                                    CAIRO_HINT_METRICS_ON);
#endif /* VTE_GTK == 4 */

                // Allow using the API to override the built-in hint metrics setting.
                if (!font_options)
                        font_options = builtin_font_options.get();

                if (auto const ctx_font_options = pango_cairo_context_get_font_options(context.get())) {
                        auto const merged_font_options =
                                vte::take_freeable(cairo_font_options_copy(ctx_font_options));
                        cairo_font_options_merge(merged_font_options.get(),
                                                 font_options);
                        pango_cairo_context_set_font_options(context.get(),
                                                             merged_font_options.get());
                } else {
                        pango_cairo_context_set_font_options(context.get(), font_options);
                }

#if VTE_GTK == 4
                // If hinting font metrics, also make sure to round glyph positions
                // to integers.  See issue#2573.
                if (auto const ctx_font_options = pango_cairo_context_get_font_options(context.get());
                    ctx_font_options &&
                    cairo_version() >= CAIRO_VERSION_ENCODE(1, 17, 4)) {
                        auto const hint_metrics = cairo_font_options_get_hint_metrics(ctx_font_options);
                        pango_context_set_round_glyph_positions(context.get(),
                                                                hint_metrics == CAIRO_HINT_METRICS_ON);
                } else {
                        pango_context_set_round_glyph_positions(context.get(), false);
                }
#endif /* VTE_GTK == 4 */
        }

	if (G_UNLIKELY(s_font_info_for_context == nullptr))
		s_font_info_for_context = g_hash_table_new((GHashFunc) context_hash, (GEqualFunc) context_equal);

	auto info = reinterpret_cast<FontInfo*>(g_hash_table_lookup(s_font_info_for_context, context.get()));
	if (G_LIKELY(info)) {
		_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
				  "vtepangocairo: %p found FontInfo in cache\n",
				  info);
		info = info->ref();
	} else {
                info = new FontInfo{std::move(context)};
	}

	return info;
}

#if VTE_GTK == 3
FontInfo*
FontInfo::create_for_screen(GdkScreen* screen,
                            PangoFontDescription const* desc,
                            PangoLanguage* language,
                            cairo_font_options_t const* font_options)
{
	auto settings = gtk_settings_get_for_screen(screen);
	auto fontconfig_timestamp = guint{};
	g_object_get (settings, "gtk-fontconfig-timestamp", &fontconfig_timestamp, nullptr);
	return create_for_context(vte::glib::take_ref(gdk_pango_context_get_for_screen(screen)),
                                  desc, language, font_options, fontconfig_timestamp);
}
#endif /* VTE_GTK */

FontInfo*
FontInfo::create_for_widget(GtkWidget* widget,
                            PangoFontDescription const* desc,
                            cairo_font_options_t const* font_options)
{
#if VTE_GTK == 3
	auto screen = gtk_widget_get_screen(widget);
	return create_for_screen(screen, desc, nullptr, font_options);
#elif VTE_GTK == 4
        auto display = gtk_widget_get_display(widget);
        auto settings = gtk_settings_get_for_display(display);
        auto fontconfig_timestamp = guint{};
        g_object_get (settings, "gtk-fontconfig-timestamp", &fontconfig_timestamp, nullptr);
        return create_for_context(vte::glib::take_ref(gtk_widget_create_pango_context(widget)),
                                  desc, nullptr, font_options, fontconfig_timestamp);
        // FIXMEgtk4: this uses a per-widget context, while the gtk3 code uses a per-screen
        // one. That means there may be a lot less sharing and a lot more FontInfo's around?
#endif
}

FontInfo::UnistrInfo*
FontInfo::get_unistr_info(vteunistr c)
{
	PangoRectangle logical;
	PangoLayoutLine *line;

	auto uinfo = find_unistr_info(c);
	if (G_LIKELY (uinfo->coverage() != UnistrInfo::Coverage::UNKNOWN))
		return uinfo;

	auto ufi = &uinfo->m_ufi;

	g_string_truncate(m_string, 0);
	_vte_unistr_append_to_string(c, m_string);
	pango_layout_set_text(m_layout.get(), m_string->str, m_string->len);
	pango_layout_get_extents(m_layout.get(), NULL, &logical);

	uinfo->width = PANGO_PIXELS_CEIL (logical.width);

	line = pango_layout_get_line_readonly(m_layout.get(), 0);

	uinfo->has_unknown_chars = pango_layout_get_unknown_glyphs_count(m_layout.get()) != 0;

#if VTE_GTK == 3
	/* we use PangoLayoutRun rendering unless there is exactly one run in the line. */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
	{
		uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_LAYOUT_LINE);

                // When using a cairo surface which uses show_text_glyphs,
                // pango_cairo_show_layout_line() will use the text from
                // @line->layout and it must be the text that was used when
                // the PangoLayoutLine was created.  Also, since @line was
                // obtained from m_layout, when setting m_layout to a new
                // text later this will change @line->layout to %NULL.
                // To make this work, we instead adopt the @m_layout instance
                // into @line->layout, and create a new @m_layout object.

                line->layout = m_layout.release(); // adopted
		ufi->using_pango_layout_line.line = pango_layout_line_ref (line);

                auto const context = pango_layout_get_context(line->layout);
                m_layout = vte::glib::take_ref(pango_layout_new(context));

	} else {
		PangoGlyphItem *glyph_item = (PangoGlyphItem *)line->runs->data;
		PangoFont *pango_font = glyph_item->item->analysis.font;
		PangoGlyphString *glyph_string = glyph_item->glyphs;

		/* we use fast cairo path if glyph string has only one real
		 * glyph and at origin */
		if (!uinfo->has_unknown_chars &&
		    glyph_string->num_glyphs == 1 && glyph_string->glyphs[0].glyph <= 0xFFFF &&
		    (glyph_string->glyphs[0].geometry.x_offset |
		     glyph_string->glyphs[0].geometry.y_offset) == 0)
		{
			cairo_scaled_font_t *scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);

			if (scaled_font) {
				uinfo->set_coverage(UnistrInfo::Coverage::USE_CAIRO_GLYPH);

				ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
				ufi->using_cairo_glyph.glyph_index = glyph_string->glyphs[0].glyph;
			}
		}

		/* use pango fast path otherwise */
		if (G_UNLIKELY (uinfo->coverage() == UnistrInfo::Coverage::UNKNOWN)) {
			uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING);

			ufi->using_pango_glyph_string.font = pango_font ? (PangoFont *)g_object_ref (pango_font) : NULL;
			ufi->using_pango_glyph_string.glyph_string = pango_glyph_string_copy (glyph_string);
		}
	}
#elif VTE_GTK == 4
        if (line != nullptr && line->runs != nullptr)
        {
                PangoGlyphItem *glyph_item = (PangoGlyphItem *)line->runs->data;
                PangoGlyphString *glyph_string = glyph_item->glyphs;

                uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING);

		ufi->using_pango_glyph_string.font = glyph_item->item->analysis.font ? g_object_ref (glyph_item->item->analysis.font) : nullptr;
                ufi->using_pango_glyph_string.glyph_string = pango_glyph_string_copy (glyph_string);
        }
#endif

        // Don't reset m_layout here; it'll get reset anyway when we next use it.

#if VTE_DEBUG
	m_coverage_count[0]++;
	m_coverage_count[uinfo->m_coverage]++;
#endif

	return uinfo;
}

} // namespace view
} // namespace vte
