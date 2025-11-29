#pragma once

typedef struct _GdkHSLA GdkHSLA;

struct _GdkHSLA {
  float hue;
  float saturation;
  float lightness;
  float alpha;
};

void            _gdk_hsla_init_from_rgba    (GdkHSLA          *hsla,
                                             const GdkRGBA    *rgba);
void            _gdk_rgba_init_from_hsla    (GdkRGBA          *rgba,
                                             const GdkHSLA    *hsla);
