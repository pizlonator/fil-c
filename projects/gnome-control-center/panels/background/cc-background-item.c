/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <gnome-bg/gnome-bg.h>
#include <gdesktop-enums.h>

#include "cc-background-item.h"
#include "gdesktop-enums-types.h"
#include "cc-background-enum-types.h"

typedef struct {
        int        width;
        int        height;
        int        frame;
        int        scale_factor;
        GdkPixbuf *thumbnail;
} CachedThumbnail;

struct _CcBackgroundItem
{
        GObject          parent_instance;

        /* properties */
        char            *name;
        char            *uri;
        char            *uri_dark;
        char            *size;
        GDesktopBackgroundStyle placement;
        GDesktopBackgroundShading shading;
        char            *primary_color;
        char            *secondary_color;
        char            *source_url; /* Used by the Flickr source */
        char            *source_xml; /* Used by the Wallpapers source */
        gboolean         is_deleted;
        gboolean         needs_download;
        CcBackgroundItemFlags flags;
        guint64          modified;

        /* internal */
        GnomeBG         *bg;
        char            *mime_type;
        int              width;
        int              height;

        GnomeBG         *bg_dark;

        CachedThumbnail cached_thumbnail;
        CachedThumbnail cached_thumbnail_dark;
};

enum {
        PROP_0,
        PROP_NAME,
        PROP_URI,
        PROP_URI_DARK,
        PROP_PLACEMENT,
        PROP_SHADING,
        PROP_PRIMARY_COLOR,
        PROP_SECONDARY_COLOR,
        PROP_IS_DELETED,
        PROP_SOURCE_URL,
        PROP_SOURCE_XML,
        PROP_FLAGS,
        PROP_SIZE,
        PROP_NEEDS_DOWNLOAD,
        PROP_MODIFIED,
        N_PROPS
};

static GParamSpec *props [N_PROPS];

static void     cc_background_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcBackgroundItem, cc_background_item, G_TYPE_OBJECT)

static void
set_bg_properties (CcBackgroundItem *item)
{
        GdkRGBA pcolor = { 0, 0, 0, 0 };
        GdkRGBA scolor = { 0, 0, 0, 0 };

        if (item->uri) {
		g_autoptr(GFile) file = NULL;
		g_autofree gchar *filename = NULL;

		file = g_file_new_for_commandline_arg (item->uri);
		filename = g_file_get_path (file);
		gnome_bg_set_filename (item->bg, filename);
	}

        if (item->uri_dark) {
		g_autoptr(GFile) file = NULL;
		g_autofree gchar *filename = NULL;

		file = g_file_new_for_commandline_arg (item->uri_dark);
		filename = g_file_get_path (file);
		gnome_bg_set_filename (item->bg_dark, filename);
	}

        if (item->primary_color != NULL) {
                gdk_rgba_parse (&pcolor, item->primary_color);
        }
        if (item->secondary_color != NULL) {
                gdk_rgba_parse (&scolor, item->secondary_color);
        }

        gnome_bg_set_rgba (item->bg, item->shading, &pcolor, &scolor);
        gnome_bg_set_rgba (item->bg_dark, item->shading, &pcolor, &scolor);
        gnome_bg_set_placement (item->bg, item->placement);
        gnome_bg_set_placement (item->bg_dark, item->placement);
}


gboolean
cc_background_item_changes_with_time (CcBackgroundItem *item)
{
        gboolean changes;

	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), FALSE);

        changes = FALSE;
        if (item->bg != NULL) {
                changes = gnome_bg_changes_with_time (item->bg);
        }
        if (item->bg_dark != NULL) {
                changes |= gnome_bg_changes_with_time (item->bg_dark);
        }
        return changes;
}

gboolean
cc_background_item_has_dark_version (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), FALSE);

        return item->uri && item->uri_dark;
}

static void
update_size (CcBackgroundItem *item)
{
	g_clear_pointer (&item->size, g_free);

	if (item->uri == NULL) {
		item->size = g_strdup ("");
	} else {
		if (gnome_bg_has_multiple_sizes (item->bg) || gnome_bg_changes_with_time (item->bg)) {
			item->size = g_strdup (_("multiple sizes"));
		} else {
			gdk_pixbuf_get_file_info (gnome_bg_get_filename (item->bg),
						  &item->width,
						  &item->height);
			/* translators: 100 × 100px
			 * Note that this is not an "x", but U+00D7 MULTIPLICATION SIGN */
			item->size = g_strdup_printf (_("%d × %d"),
						      item->width,
						      item->height);
		}
	}
}

GdkPixbuf *
cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                        GnomeDesktopThumbnailFactory *thumbs,
                                        int                           width,
                                        int                           height,
                                        int                           scale_factor,
                                        int                           frame,
                                        gboolean                      dark)
{
        GdkPixbuf *pixbuf;
        CachedThumbnail *thumbnail;
        GnomeBG *bg;
        g_autoptr(GdkMonitor) monitor = NULL;
        GdkDisplay *display;
        GListModel *monitors;
        GdkRectangle monitor_layout;

	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);
	g_return_val_if_fail (width > 0 && height > 0, NULL);

        thumbnail = dark ? &item->cached_thumbnail_dark : &item->cached_thumbnail;
        bg = dark ? item->bg_dark : item->bg;

        /* Use the cached thumbnail if the sizes match */
        if (thumbnail->thumbnail &&
            thumbnail->width == width &&
            thumbnail->height == height &&
            thumbnail->scale_factor == scale_factor &&
            thumbnail->frame == frame)
                    return g_object_ref (thumbnail->thumbnail);

        set_bg_properties (item);

        display = gdk_display_get_default ();
        monitors = gdk_display_get_monitors (display);
        monitor = g_list_model_get_item (monitors, 0);
        gdk_monitor_get_geometry (monitor, &monitor_layout);

        if (frame >= 0) {
                pixbuf = gnome_bg_create_frame_thumbnail (bg,
                                                          thumbs,
                                                          &monitor_layout,
                                                          scale_factor * width,
                                                          scale_factor * height,
                                                          frame);
        } else {
                pixbuf = gnome_bg_create_thumbnail (bg,
                                                    thumbs,
                                                    &monitor_layout,
                                                    scale_factor * width,
                                                    scale_factor * height);
        }

        /* Cache the new thumbnail */
        g_set_object (&thumbnail->thumbnail, pixbuf);
        thumbnail->width = width;
        thumbnail->height = height;
        thumbnail->scale_factor = scale_factor;
        thumbnail->frame = frame;

        return pixbuf;
}


GdkPixbuf *
cc_background_item_get_thumbnail (CcBackgroundItem             *item,
                                  GnomeDesktopThumbnailFactory *thumbs,
                                  int                           width,
                                  int                           height,
                                  int                           scale_factor,
                                  gboolean                      dark)
{
        return cc_background_item_get_frame_thumbnail (item, thumbs, width, height, scale_factor, -1, dark);
}

static void
update_info (CcBackgroundItem *item,
	     GFileInfo        *_info)
{
        g_autoptr(GFileInfo) info = NULL;

	if (_info == NULL) {
		g_autoptr(GFile) file = NULL;

		file = g_file_new_for_uri (item->uri);

		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_NAME ","
					  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
					  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
					  G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL);
	} else {
		info = g_object_ref (_info);
	}

	g_clear_pointer (&item->mime_type, g_free);

        if (info == NULL
            || g_file_info_get_content_type (info) == NULL) {
                if (item->uri == NULL) {
                        item->mime_type = g_strdup ("image/x-no-data");
                        g_free (item->name);
                        item->name = g_strdup (_("No Desktop Background"));
                }
        } else {
                if (item->name == NULL)
                        item->name = g_strdup (g_file_info_get_display_name (info));

                item->mime_type = g_strdup (g_file_info_get_content_type (info));
                if (item->modified == 0)
                        item->modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        }
}

gboolean
cc_background_item_load (CcBackgroundItem *item,
			 GFileInfo        *info)
{
        g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), FALSE);

        if (item->uri == NULL)
		return TRUE;

        update_info (item, info);

        if (item->mime_type != NULL
            && (g_str_has_prefix (item->mime_type, "image/")
                || strcmp (item->mime_type, "application/xml") == 0)) {
                set_bg_properties (item);
        } else {
		return FALSE;
        }

	/* FIXME we should handle XML files as well */
        if (item->mime_type != NULL &&
            g_str_has_prefix (item->mime_type, "image/")) {
		g_autofree gchar *filename = NULL;

		filename = g_filename_from_uri (item->uri, NULL, NULL);
		gdk_pixbuf_get_file_info (filename,
					  &item->width,
					  &item->height);
	}

        return TRUE;
}

static void
_set_name (CcBackgroundItem *item,
           const char       *value)
{
        g_free (item->name);
        item->name = g_strdup (value);
}

const char *
cc_background_item_get_name (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->name;
}

static void
_add_flag (CcBackgroundItem      *item,
           CcBackgroundItemFlags  flag)
{
        item->flags |= flag;
        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_FLAGS]);
}

static void
_set_uri (CcBackgroundItem *item,
	  const char       *value)
{
        g_free (item->uri);
        if (value && *value == '\0') {
		item->uri = NULL;
	} else {
		if (value && strstr (value, "://") == NULL)
			g_warning ("URI '%s' is invalid", value);
		item->uri = g_strdup (value);
	}
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_URI);
}


static void
_set_uri_dark (CcBackgroundItem *item,
               const char       *value)
{
        g_free (item->uri_dark);
        if (value && *value == '\0') {
		item->uri_dark = NULL;
	} else {
		if (value && strstr (value, "://") == NULL)
			g_warning ("URI '%s' is invalid", value);
		item->uri_dark = g_strdup (value);
	}
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_URI_DARK);
}

const char *
cc_background_item_get_uri (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->uri;
}

const char *
cc_background_item_get_uri_dark (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->uri_dark;
}

static void
_set_placement (CcBackgroundItem        *item,
                GDesktopBackgroundStyle  value)
{
        item->placement = value;
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_PLACEMENT);
}

static void
_set_shading (CcBackgroundItem          *item,
              GDesktopBackgroundShading  value)
{
        item->shading = value;
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_SHADING);
}

static void
_set_primary_color (CcBackgroundItem *item,
                    const char       *value)
{
        g_free (item->primary_color);
        item->primary_color = g_strdup (value);
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_PCOLOR);
}

const char *
cc_background_item_get_pcolor (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->primary_color;
}

static void
_set_secondary_color (CcBackgroundItem *item,
                      const char       *value)
{
        g_free (item->secondary_color);
        item->secondary_color = g_strdup (value);
        _add_flag (item, CC_BACKGROUND_ITEM_HAS_SCOLOR);
}

const char *
cc_background_item_get_scolor (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->secondary_color;
}

GDesktopBackgroundStyle
cc_background_item_get_placement (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), G_DESKTOP_BACKGROUND_STYLE_SCALED);

	return item->placement;
}

GDesktopBackgroundShading
cc_background_item_get_shading (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), G_DESKTOP_BACKGROUND_SHADING_SOLID);

	return item->shading;
}

static void
_set_is_deleted (CcBackgroundItem *item,
                 gboolean          value)
{
        item->is_deleted = value;
}

static void
_set_source_url (CcBackgroundItem *item,
                 const char       *value)
{
        g_free (item->source_url);
        item->source_url = g_strdup (value);
}

const char *
cc_background_item_get_source_url (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->source_url;
}

static void
_set_source_xml (CcBackgroundItem *item,
                 const char       *value)
{
        g_free (item->source_xml);
        item->source_xml = g_strdup (value);
}

const char *
cc_background_item_get_source_xml (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->source_xml;
}

CcBackgroundItemFlags
cc_background_item_get_flags (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->flags;
}

const char *
cc_background_item_get_size (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	update_size (item);

	return item->size;
}

static void
_set_needs_download (CcBackgroundItem *item,
		     gboolean          value)
{
	item->needs_download = value;
}

gboolean
cc_background_item_get_needs_download (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->needs_download;
}

static void
_set_modified (CcBackgroundItem *item,
               guint64           value)
{
        item->modified = value;
}

guint64
cc_background_item_get_modified (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->modified;
}

static void
cc_background_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        CcBackgroundItem *self;

        self = CC_BACKGROUND_ITEM (object);

        switch (prop_id) {
        case PROP_NAME:
                _set_name (self, g_value_get_string (value));
                break;
        case PROP_URI:
                _set_uri (self, g_value_get_string (value));
                break;
        case PROP_URI_DARK:
                _set_uri_dark (self, g_value_get_string (value));
                break;
        case PROP_PLACEMENT:
                _set_placement (self, g_value_get_enum (value));
                break;
        case PROP_SHADING:
                _set_shading (self, g_value_get_enum (value));
                break;
        case PROP_PRIMARY_COLOR:
                _set_primary_color (self, g_value_get_string (value));
                break;
        case PROP_SECONDARY_COLOR:
                _set_secondary_color (self, g_value_get_string (value));
                break;
        case PROP_IS_DELETED:
                _set_is_deleted (self, g_value_get_boolean (value));
                break;
	case PROP_SOURCE_URL:
		_set_source_url (self, g_value_get_string (value));
		break;
	case PROP_SOURCE_XML:
		_set_source_xml (self, g_value_get_string (value));
		break;
	case PROP_NEEDS_DOWNLOAD:
		_set_needs_download (self, g_value_get_boolean (value));
		break;
	case PROP_MODIFIED:
		_set_modified (self, g_value_get_uint64 (value));
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_background_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        CcBackgroundItem *self;

        self = CC_BACKGROUND_ITEM (object);

        switch (prop_id) {
        case PROP_NAME:
                g_value_set_string (value, self->name);
                break;
        case PROP_URI:
                g_value_set_string (value, self->uri);
                break;
        case PROP_URI_DARK:
                g_value_set_string (value, self->uri_dark);
                break;
        case PROP_PLACEMENT:
                g_value_set_enum (value, self->placement);
                break;
        case PROP_SHADING:
                g_value_set_enum (value, self->shading);
                break;
        case PROP_PRIMARY_COLOR:
                g_value_set_string (value, self->primary_color);
                break;
        case PROP_SECONDARY_COLOR:
                g_value_set_string (value, self->secondary_color);
                break;
        case PROP_IS_DELETED:
                g_value_set_boolean (value, self->is_deleted);
                break;
	case PROP_SOURCE_URL:
		g_value_set_string (value, self->source_url);
		break;
	case PROP_SOURCE_XML:
		g_value_set_string (value, self->source_xml);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, self->flags);
		break;
	case PROP_SIZE:
		update_size (self);
		g_value_set_string (value, self->size);
		break;
	case PROP_NEEDS_DOWNLOAD:
		g_value_set_boolean (value, self->needs_download);
		break;
	case PROP_MODIFIED:
		g_value_set_uint64 (value, self->modified);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
cc_background_item_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
        CcBackgroundItem      *background_item;

        background_item = CC_BACKGROUND_ITEM (G_OBJECT_CLASS (cc_background_item_parent_class)->constructor (type,
                                                                                                             n_construct_properties,
                                                                                                             construct_properties));

        return G_OBJECT (background_item);
}

static void
cc_background_item_class_init (CcBackgroundItemClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_background_item_get_property;
        object_class->set_property = cc_background_item_set_property;
        object_class->constructor = cc_background_item_constructor;
        object_class->finalize = cc_background_item_finalize;

        props[PROP_NAME] = g_param_spec_string ("name",
                                                "name",
                                                "name",
                                                NULL,
                                                G_PARAM_READWRITE);

        props[PROP_URI] = g_param_spec_string ("uri",
                                               "uri",
                                               "uri",
                                               NULL,
                                               G_PARAM_READWRITE);

        props[PROP_URI_DARK] = g_param_spec_string ("uri-dark",
                                                    "uri-dark",
                                                    "uri-dark",
                                                    NULL,
                                                    G_PARAM_READWRITE);

        props[PROP_PLACEMENT] = g_param_spec_enum ("placement",
                                                   "placement",
                                                   "placement",
                                                   G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE,
                                                   G_DESKTOP_BACKGROUND_STYLE_SCALED,
                                                   G_PARAM_READWRITE);

        props[PROP_SHADING] = g_param_spec_enum ("shading",
                                                 "shading",
                                                 "shading",
                                                 G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING,
                                                 G_DESKTOP_BACKGROUND_SHADING_SOLID,
                                                 G_PARAM_READWRITE);

        props[PROP_PRIMARY_COLOR] = g_param_spec_string ("primary-color",
                                                         "primary-color",
                                                         "primary-color",
                                                         "#000000000000",
                                                         G_PARAM_READWRITE);

        props[PROP_SECONDARY_COLOR] = g_param_spec_string ("secondary-color",
                                                           "secondary-color",
                                                           "secondary-color",
                                                           "#000000000000",
                                                           G_PARAM_READWRITE);

        props[PROP_IS_DELETED] = g_param_spec_boolean ("is-deleted",
                                                       NULL,
                                                       NULL,
                                                       FALSE,
                                                       G_PARAM_READWRITE);

        props[PROP_SOURCE_URL] = g_param_spec_string ("source-url",
                                                      "source-url",
                                                      "source-url",
                                                      NULL,
                                                      G_PARAM_READWRITE);

        props[PROP_SOURCE_XML] = g_param_spec_string ("source-xml",
                                                      "source-xml",
                                                      "source-xml",
                                                      NULL,
                                                      G_PARAM_READWRITE);

        props[PROP_FLAGS] = g_param_spec_flags ("flags",
                                                "flags",
                                                "flags",
                                                CC_TYPE_BACKGROUND_ITEM_FLAGS,
                                                0,
                                                G_PARAM_READABLE);

        props[PROP_SIZE] = g_param_spec_string ("size",
                                                "size",
                                                "size",
                                                NULL,
                                                G_PARAM_READABLE);

        props[PROP_NEEDS_DOWNLOAD] = g_param_spec_boolean ("needs-download",
                                                           NULL,
                                                           NULL,
                                                           TRUE,
                                                           G_PARAM_READWRITE);

        props[PROP_MODIFIED] = g_param_spec_uint64 ("modified",
                                                    "modified",
                                                    NULL,
                                                    0,
                                                    G_MAXUINT64,
                                                    0,
                                                    G_PARAM_READWRITE);

        g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_background_item_init (CcBackgroundItem *item)
{
        item->bg = gnome_bg_new ();
        item->bg_dark = gnome_bg_new ();

        item->shading = G_DESKTOP_BACKGROUND_SHADING_SOLID;
        item->placement = G_DESKTOP_BACKGROUND_STYLE_SCALED;
        item->primary_color = g_strdup ("#000000000000");
        item->secondary_color = g_strdup ("#000000000000");
        item->needs_download = TRUE;
        item->flags = 0;
        item->modified = 0;
}

static void
cc_background_item_finalize (GObject *object)
{
        CcBackgroundItem *item;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUND_ITEM (object));

        item = CC_BACKGROUND_ITEM (object);

        g_return_if_fail (item != NULL);

        g_clear_object (&item->cached_thumbnail.thumbnail);
        g_clear_object (&item->cached_thumbnail_dark.thumbnail);
        g_free (item->name);
        g_free (item->uri);
        g_free (item->primary_color);
        g_free (item->secondary_color);
        g_free (item->mime_type);
        g_free (item->size);
        g_free (item->source_url);
        g_free (item->source_xml);

        g_clear_object (&item->bg);
        g_clear_object (&item->bg_dark);

        G_OBJECT_CLASS (cc_background_item_parent_class)->finalize (object);
}

CcBackgroundItem *
cc_background_item_new (const char *uri)
{
        GObject *object;

        object = g_object_new (CC_TYPE_BACKGROUND_ITEM,
                               "uri", uri,
                               NULL);

        return CC_BACKGROUND_ITEM (object);
}

CcBackgroundItem *
cc_background_item_copy (CcBackgroundItem *item)
{
	CcBackgroundItem *ret;

	ret = cc_background_item_new (item->uri);
	ret->name = g_strdup (item->name);
	ret->size = g_strdup (item->size);
	ret->placement = item->placement;
	ret->shading = item->shading;
	ret->primary_color = g_strdup (item->primary_color);
	ret->secondary_color = g_strdup (item->secondary_color);
	ret->source_url = g_strdup (item->source_url);
	ret->source_xml = g_strdup (item->source_xml);
	ret->is_deleted = item->is_deleted;
	ret->needs_download = item->needs_download;
	ret->flags = item->flags;

	return ret;
}

static const char *
flags_to_str (CcBackgroundItemFlags flag)
{
	GFlagsClass *fclass;
	GFlagsValue *value;

	fclass = G_FLAGS_CLASS (g_type_class_peek (CC_TYPE_BACKGROUND_ITEM_FLAGS));
	value = g_flags_get_first_value (fclass, flag);

	g_assert (value);

	return value->value_nick;
}

static const char *
enum_to_str (GType type,
	     int   v)
{
	GEnumClass *eclass;
	GEnumValue *value;

	eclass = G_ENUM_CLASS (g_type_class_peek (type));
	value = g_enum_get_value (eclass, v);

	g_assert (value);

	return value->value_nick;
}

void
cc_background_item_dump (CcBackgroundItem *item)
{
	g_autoptr(GString) flags = NULL;
	int i;

	g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

	update_size (item);

	g_debug ("name:\t\t\t%s", item->name);
	g_debug ("URI:\t\t\t%s", item->uri ? item->uri : "NULL");
	if (item->size)
		g_debug ("size:\t\t\t'%s'", item->size);
	flags = g_string_new (NULL);
	for (i = 0; i < 5; i++) {
		if (item->flags & (1 << i)) {
			g_string_append (flags, flags_to_str (1 << i));
			g_string_append_c (flags, ' ');
		}
	}
	if (flags->len == 0)
		g_string_append (flags, "-none-");
	g_debug ("flags:\t\t\t%s", flags->str);
	if (item->primary_color)
		g_debug ("pcolor:\t\t\t%s", item->primary_color);
	if (item->secondary_color)
		g_debug ("scolor:\t\t\t%s", item->secondary_color);
	g_debug ("placement:\t\t%s", enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE, item->placement));
	g_debug ("shading:\t\t%s", enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING, item->shading));
	if (item->source_url)
		g_debug ("source URL:\t\t%s", item->source_url);
	if (item->source_xml)
		g_debug ("source XML:\t\t%s", item->source_xml);
	g_debug ("deleted:\t\t%s", item->is_deleted ? "yes" : "no");
	if (item->mime_type)
		g_debug ("mime-type:\t\t%s", item->mime_type);
	g_debug ("dimensions:\t\t%d x %d", item->width, item->height);
        g_debug ("modified: %"G_GUINT64_FORMAT, item->modified);
	g_debug (" ");
}

static gboolean
files_equal (const char *a,
	     const char *b)
{
	g_autoptr(GFile) file1 = NULL;
	g_autoptr(GFile) file2 = NULL;

	if (a == NULL && b == NULL)
		return TRUE;

	if (a == NULL || b == NULL)
		return FALSE;

	file1 = g_file_new_for_commandline_arg (a);
	file2 = g_file_new_for_commandline_arg (b);

	return g_file_equal (file1, file2);
}

static gboolean
colors_equal (const char *a,
	      const char *b)
{
	GdkRGBA color1, color2;

	gdk_rgba_parse (&color1, a);
	gdk_rgba_parse (&color2, b);

	return gdk_rgba_equal (&color1, &color2);
}

gboolean
cc_background_item_compare (CcBackgroundItem *saved,
			    CcBackgroundItem *configured)
{
	CcBackgroundItemFlags flags;

	flags = saved->flags;
	if (flags == 0)
		return FALSE;

	if (flags & CC_BACKGROUND_ITEM_HAS_URI) {
		if (files_equal (saved->uri, configured->uri) == FALSE)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_URI_DARK) {
		if (files_equal (saved->uri_dark, configured->uri_dark) == FALSE)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_SHADING) {
		if (saved->shading != configured->shading)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT) {
		if (saved->placement != configured->placement)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_PCOLOR) {
		if (colors_equal (saved->primary_color,
				  configured->primary_color) == FALSE) {
			return FALSE;
		}
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_SCOLOR) {
		if (colors_equal (saved->secondary_color,
				  configured->secondary_color) == FALSE) {
			return FALSE;
		}
	}

	return TRUE;
}
