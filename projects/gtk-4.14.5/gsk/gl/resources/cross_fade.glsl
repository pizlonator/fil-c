// VERTEX_SHADER:
// cross_fade.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// cross_fade.glsl

uniform float u_progress;
uniform sampler2D u_source2;

void main() {
  vec4 source1 = GskTexture(u_source, vUv);  // start child
  vec4 source2 = GskTexture(u_source2, vUv); // end child

  float p_start = (1.0 - u_progress) * u_alpha;
  float p_end = u_progress * u_alpha;
  vec4 color = (p_start * source1) + (p_end * source2);
  gskSetOutputColor(color);
}
