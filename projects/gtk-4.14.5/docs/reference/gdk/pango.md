Title: Pango Interaction

Pango is the text layout system used by GDK and GTK. The functions
and types in this section are used to obtain clip regions for
`PangoLayout`s, and to get `PangoContext`s that can be used with
GDK.

## Using Pango in GDK

Creating a `PangoLayout` object is the first step in rendering text,
and requires getting a handle to a `PangoContext`. For GTK programs,
you’ll usually want to use [method@Gtk.Widget.get_pango_context], or
[method@Gtk.Widget.create_pango_layout]. Once you have a `PangoLayout`,
you can set the text and attributes of it with Pango functions like
[method@Pango.Layout.set_text] and get its size with
[method@Pango.Layout.get_size].

*Note*: Pango uses a fixed point system internally, so converting
between Pango units and pixels using `PANGO_SCALE` or the `PANGO_PIXELS()`
macro.

Rendering a Pango layout is done most simply with [func@PangoCairo.show_layout];
you can also draw pieces of the layout with [func@PangoCairo.show_layout_line].

### Draw transformed text with Pango and cairo

```c
#define RADIUS 100
#define N_WORDS 10
#define FONT "Sans Bold 18"

PangoContext *context;
PangoLayout *layout;
PangoFontDescription *desc;

double radius;
int width, height;
int i;

// Set up a transformation matrix so that the user space coordinates for
// where we are drawing are [-RADIUS, RADIUS], [-RADIUS, RADIUS]
// We first center, then change the scale

width = gdk_surface_get_width (surface);
height = gdk_surface_get_height (surface);
radius = MIN (width, height) / 2.;

cairo_translate (cr,
                 radius + (width - 2 * radius) / 2,
                 radius + (height - 2 * radius) / 2);
                 cairo_scale (cr, radius / RADIUS, radius / RADIUS);

// Create a PangoLayout, set the font and text
context = gdk_pango_context_get_for_display (display);
layout = pango_layout_new (context);
pango_layout_set_text (layout, "Text", -1);
desc = pango_font_description_from_string (FONT);
pango_layout_set_font_description (layout, desc);
pango_font_description_free (desc);

// Draw the layout N_WORDS times in a circle
for (i = 0; i < N_WORDS; i++)
  {
    double red, green, blue;
    double angle = 2 * G_PI * i / n_words;

    cairo_save (cr);

    // Gradient from red at angle == 60 to blue at angle == 300
    red = (1 + cos (angle - 60)) / 2;
    green = 0;
    blue = 1 - red;

    cairo_set_source_rgb (cr, red, green, blue);
    cairo_rotate (cr, angle);

    // Inform Pango to re-layout the text with the new transformation matrix
    pango_cairo_update_layout (cr, layout);

    pango_layout_get_size (layout, &width, &height);

    cairo_move_to (cr, - width / 2 / PANGO_SCALE, - DEFAULT_TEXT_RADIUS);
    pango_cairo_show_layout (cr, layout);

    cairo_restore (cr);
  }

g_object_unref (layout);
g_object_unref (context);
```

The example code above will yield the following result:

![](rotated-text.png)
