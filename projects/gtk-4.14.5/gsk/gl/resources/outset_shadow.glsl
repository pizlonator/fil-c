// VERTEX_SHADER:
// outset_shadow.glsl

uniform vec4[3] u_outline_rect;

_OUT_ vec4 final_color;
_OUT_ _GSK_ROUNDED_RECT_UNIFORM_ transformed_outline;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  final_color = gsk_scaled_premultiply(aColor, u_alpha);

  GskRoundedRect outline = gsk_create_rect(u_outline_rect);
  gsk_rounded_rect_transform(outline, u_modelview);
  gsk_rounded_rect_normalize(outline);
  gsk_rounded_rect_encode(outline, transformed_outline);
}

// FRAGMENT_SHADER:
// outset_shadow.glsl

_IN_ vec4 final_color;
_IN_ _GSK_ROUNDED_RECT_UNIFORM_ transformed_outline;

void main() {
  vec2 frag = gsk_get_frag_coord();
  float alpha = GskTexture(u_source, vUv).a;

  alpha *= (1.0 -  clamp(gsk_rounded_rect_coverage(gsk_decode_rect(transformed_outline), frag), 0.0, 1.0));

  gskSetScaledOutputColor(final_color, alpha);
}
