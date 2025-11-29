#pragma once

#include <gtk/gtk.h>

#define GTK_TYPE_PAGE_THUMBNAIL (gtk_page_thumbnail_get_type ())
G_DECLARE_FINAL_TYPE (GtkPageThumbnail, gtk_page_thumbnail, GTK, PAGE_THUMBNAIL, GtkWidget)

GtkPageThumbnail * gtk_page_thumbnail_new          (void);
void               gtk_page_thumbnail_set_page_num (GtkPageThumbnail *self,
                                                    int               page_num);
int                gtk_page_thumbnail_get_page_num (GtkPageThumbnail *self);

