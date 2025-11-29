// VERTEX_SHADER:
// coloring.glsl

_OUT_ vec4 final_color;
_OUT_ float use_color;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  // We use this shader for both plain glyphs (used as mask)
  // and color glyphs (used as source). The renderer sets
  // aColor to vec4(-1) for color glyphs.
  if (distance(aColor,vec4(-1)) < 0.1)
    use_color = 0.0;
  else
    use_color = 1.0;

  final_color = gsk_scaled_premultiply(aColor, u_alpha);
}

// FRAGMENT_SHADER:
// coloring.glsl

_IN_ vec4 final_color;
_IN_ float use_color;

void main() {
  vec4 diffuse = GskTexture(u_source, vUv);

  gskSetOutputColor(mix(diffuse * u_alpha, final_color * diffuse.a, use_color));
}
