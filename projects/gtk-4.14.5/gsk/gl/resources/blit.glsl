// VERTEX_SHADER:
// blit.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// blit.glsl

void main() {
  vec4 diffuse = GskTexture(u_source, vUv);

  gskSetScaledOutputColor(diffuse, u_alpha);
}
