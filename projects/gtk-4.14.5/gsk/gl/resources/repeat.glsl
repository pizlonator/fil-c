// VERTEX_SHADER:
// repeat.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// repeat.glsl

uniform vec4 u_child_bounds;
uniform vec4 u_texture_rect;

float wrap(float f, float wrap_for) {
  return mod(f, wrap_for);
}

/* We get the texture coordinates via vUv,
 * but that might be on a texture atlas, so we need to do the
 * wrapping ourselves.
 */
void main() {

  /* We map the texture coordinate to [1;0], then wrap it and scale the result again */

  float tw = u_texture_rect.z - u_texture_rect.x;
  float th = u_texture_rect.w - u_texture_rect.y;

  float mapped_x = (vUv.x - u_texture_rect.x) / tw;
  float mapped_y = (vUv.y - u_texture_rect.y) / th;

  float wrapped_x = wrap(u_child_bounds.x + mapped_x * u_child_bounds.z, 1.0);
  float wrapped_y = wrap(u_child_bounds.y + mapped_y * u_child_bounds.w, 1.0);

  vec2 tp;
  tp.x = u_texture_rect.x + (wrapped_x * tw);
  tp.y = u_texture_rect.y + (wrapped_y * th);

  vec4 diffuse = GskTexture(u_source, tp);

  gskSetScaledOutputColor(diffuse, u_alpha);
}
