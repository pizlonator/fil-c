// VERTEX_SHADER
// radial_gradient.glsl

uniform vec4 u_geometry;

_OUT_ vec2 coord;

void main() {
  gl_Position = u_projection * (u_modelview * vec4(aPosition, 0.0, 1.0));

  vec2 mv0 = u_modelview[0].xy;
  vec2 mv1 = u_modelview[1].xy;
  vec2 offset = aPosition - u_geometry.xy;
  vec2 dir = vec2(dot(mv0, offset),
                  dot(mv1, offset));

  coord = dir * u_geometry.zw;
}

// FRAGMENT_SHADER:
// radial_gradient.glsl

#define MAX_COLOR_STOPS 6

#ifdef GSK_LEGACY
uniform int u_num_color_stops;
#else
uniform highp int u_num_color_stops;
#endif

uniform bool u_repeat;
uniform vec2 u_range;
uniform float u_color_stops[MAX_COLOR_STOPS * 5];

_IN_ vec2 coord;

float get_offset(int index) {
  // u_color_stops[5 * index] makes Intel Windows driver crash.
  // See https://gitlab.gnome.org/GNOME/gtk/-/issues/3783
  int base = 5 * index;
  return u_color_stops[base];
}

vec4 get_color(int index) {
  int base = 5 * index + 1;

  return vec4(u_color_stops[base],
              u_color_stops[base + 1],
              u_color_stops[base + 2],
              u_color_stops[base + 3]);
}

void main() {
  // Reverse scale
  float offset = length(coord) * u_range.x + u_range.y;
  float curr_offset;
  float next_offset;

  if (u_repeat) {
    offset = fract(offset);
  }

  next_offset = get_offset(0);
  if (offset < next_offset) {
    gskSetOutputColor(gsk_scaled_premultiply(get_color(0), u_alpha));
    return;
  }

  if (offset >= get_offset(u_num_color_stops - 1)) {
    gskSetOutputColor(gsk_scaled_premultiply(get_color(u_num_color_stops - 1), u_alpha));
    return;
  }

  for (int i = 0; i < MAX_COLOR_STOPS; i++) {
    curr_offset = next_offset;
    next_offset = get_offset(i + 1);

    if (offset < next_offset) {
      float f = (offset - curr_offset) / (next_offset - curr_offset);
      vec4 curr_color = get_color(i);
      vec4 next_color = get_color(i + 1);
      vec4 color = mix(curr_color, next_color, f);
      gskSetScaledOutputColor(gsk_premultiply(color), u_alpha);
      return;
    }
  }
}
