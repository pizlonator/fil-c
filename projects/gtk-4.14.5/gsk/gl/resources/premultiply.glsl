// VERTEX_SHADER:
// premultiply.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// premultiply.glsl

void main() {
  vec4 color = GskTexture(u_source, vUv);

  color.rgb *= color.a;

  gskSetOutputColor(color);
}
