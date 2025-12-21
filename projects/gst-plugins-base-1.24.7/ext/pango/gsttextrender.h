#ifndef __GST_TEXT_RENDER_H__
#define __GST_TEXT_RENDER_H__

#include <gst/gst.h>
#include <pango/pangocairo.h>

G_BEGIN_DECLS

#define GST_TYPE_TEXT_RENDER            (gst_text_render_get_type())
#define GST_TEXT_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                        GST_TYPE_TEXT_RENDER, GstTextRender))
#define GST_TEXT_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                        GST_TYPE_TEXT_RENDER, GstTextRenderClass))
#define GST_TEXT_RENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                        GST_TYPE_TEXT_RENDER, GstTextRenderClass))
#define GST_IS_TEXT_RENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                        GST_TYPE_TEXT_RENDER))
#define GST_IS_TEXT_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                        GST_TYPE_TEXT_RENDER))

typedef struct _GstTextRender      GstTextRender;
typedef struct _GstTextRenderClass GstTextRenderClass;

/**
 * GstTextRenderVAlign:
 * @GST_TEXT_RENDER_VALIGN_BASELINE: draw text on the baseline
 * @GST_TEXT_RENDER_VALIGN_BOTTOM: draw text on the bottom
 * @GST_TEXT_RENDER_VALIGN_TOP: draw test on top
 *
 * Vertical alignment of the text.
 */
typedef enum {
    GST_TEXT_RENDER_VALIGN_BASELINE,
    GST_TEXT_RENDER_VALIGN_BOTTOM,
    GST_TEXT_RENDER_VALIGN_TOP
} GstTextRenderVAlign;

/**
 * GstTextRenderHAlign:
 * @GST_TEXT_RENDER_HALIGN_LEFT: align text left
 * @GST_TEXT_RENDER_HALIGN_CENTER: align text center
 * @GST_TEXT_RENDER_HALIGN_RIGHT: align text right
 *
 * Horizontal alignment of the text.
 */
typedef enum {
    GST_TEXT_RENDER_HALIGN_LEFT,
    GST_TEXT_RENDER_HALIGN_CENTER,
    GST_TEXT_RENDER_HALIGN_RIGHT
} GstTextRenderHAlign;

/**
 * GstTextRenderLineAlign:
 * @GST_TEXT_RENDER_LINE_ALIGN_LEFT: lines are left-aligned
 * @GST_TEXT_RENDER_LINE_ALIGN_CENTER: lines are center-aligned
 * @GST_TEXT_RENDER_LINE_ALIGN_RIGHT: lines are right-aligned
 *
 * Alignment of text lines relative to each other
 */
typedef enum {
    GST_TEXT_RENDER_LINE_ALIGN_LEFT = PANGO_ALIGN_LEFT,
    GST_TEXT_RENDER_LINE_ALIGN_CENTER = PANGO_ALIGN_CENTER,
    GST_TEXT_RENDER_LINE_ALIGN_RIGHT = PANGO_ALIGN_RIGHT
} GstTextRenderLineAlign;

/**
 * GstTextRender:
 *
 * Opaque textrender data structure.
 */
struct _GstTextRender {
    GstElement            element;

    GstPad               *sinkpad, *srcpad;
    gint                  width;
    gint                  height;
    PangoLayout          *layout;
    gdouble               shadow_offset;
    gdouble               outline_offset;
    guchar               *text_image;
    gint                  image_width;
    gint                  image_height;
    gint                  baseline_y;
    gboolean              use_ARGB;

    PangoContext         *pango_context;

    GstTextRenderVAlign     valign;
    GstTextRenderHAlign     halign;
    GstTextRenderLineAlign  line_align;

    gint xpad;
    gint ypad;

    GstEvent             *segment_event;
    gboolean              have_pango_markup;
};

struct _GstTextRenderClass {
    GstElementClass parent_class;
};

GType gst_text_render_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_TEXT_RENDER_H */
