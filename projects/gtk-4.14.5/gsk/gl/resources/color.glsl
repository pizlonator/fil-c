// VERTEX_SHADER:
// color.glsl

_OUT_ vec4 final_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  final_color = gsk_scaled_premultiply(aColor, u_alpha);
}

// FRAGMENT_SHADER:
// color.glsl

_IN_ vec4 final_color;

void main() {
  gskSetOutputColor(final_color);
}

