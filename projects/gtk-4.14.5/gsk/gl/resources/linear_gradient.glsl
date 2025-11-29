// VERTEX_SHADER
// linear_gradient.glsl
uniform vec4 u_points;

_OUT_ vec4 info;

void main() {
  gl_Position = u_projection * (u_modelview * vec4(aPosition, 0.0, 1.0));

  vec2 mv0 = u_modelview[0].xy;
  vec2 mv1 = u_modelview[1].xy;
  vec2 offset = aPosition - u_points.xy;
  vec2 coord = vec2(dot(mv0, offset),
                    dot(mv1, offset));

  // Original equation:
  // VS | maxDist = length(end - start);
  // VS | gradient = end - start;
  // VS | gradientLength = length(gradient);
  // FS | pos = frag_coord - start
  // FS | proj = (dot(gradient, pos) / (gradientLength * gradientLength)) * gradient
  // FS | offset = length(proj) / maxDist

  // Simplified formula derivation:
  // 1. Notice that maxDist = gradientLength:
  // offset = length(proj) / gradientLength
  // 2. Let gnorm = gradient / gradientLength, then:
  // proj = (dot(gnorm * gradientLength, pos) / (gradientLength * gradientLength)) * (gnorm * gradientLength) =
  //      = dot(gnorm, pos) * gnorm
  // 3. Since gnorm is unit length then:
  // length(proj) = length(dot(gnorm, pos) * gnorm) = dot(gnorm, pos)
  // 4. We can avoid the FS division by passing a scaled pos from the VS:
  // offset = dot(gnorm, pos) / gradientLength = dot(gnorm, pos / gradientLength)
  // 5. 1.0 / length(gradient) is inversesqrt(dot(gradient, gradient)) in GLSL
  vec2 gradient = vec2(dot(mv0, u_points.zw),
                       dot(mv1, u_points.zw));
  float rcp_gradient_length = inversesqrt(dot(gradient, gradient));

  info = rcp_gradient_length * vec4(coord, gradient);
}

// FRAGMENT_SHADER:
// linear_gradient.glsl

#define MAX_COLOR_STOPS 6

#ifdef GSK_LEGACY
uniform int u_num_color_stops;
#else
uniform highp int u_num_color_stops; // Why? Because it works like this.
#endif

uniform float u_color_stops[MAX_COLOR_STOPS * 5];
uniform bool u_repeat;

_IN_ vec4 info;

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
  float offset = dot(info.xy, info.zw);
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
      gskSetScaledOutputColor(gsk_premultiply (color), u_alpha);
      return;
    }
  }
}
