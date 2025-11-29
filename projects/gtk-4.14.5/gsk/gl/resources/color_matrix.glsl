// VERTEX_SHADER:
// color_matrix.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// color_matrix.glsl

uniform mat4 u_color_matrix;
uniform vec4 u_color_offset;

void main() {
  vec4 color = GskTexture(u_source, vUv);

  // Un-premultilpy
  if (color.a != 0.0)
    color.rgb /= color.a;

  color = u_color_matrix * color + u_color_offset;
  color = clamp(color, 0.0, 1.0);

  gskSetOutputColor(gsk_scaled_premultiply(color, u_alpha));
}
