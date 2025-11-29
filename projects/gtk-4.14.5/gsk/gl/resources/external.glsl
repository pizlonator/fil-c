// VERTEX_SHADER:
// external.glsl

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);
}

// FRAGMENT_SHADER:
// external.glsl

#if defined(GSK_GLES) || defined(GSK_GLES3)
uniform samplerExternalOES u_external_source;
#else
/* Just to make this compile, we won't use it without GLES */
uniform sampler2D u_external_source;
#endif

uniform int u_premultiply;

void main() {
/* Open-code this here, since GskTexture() expects a sampler2D */
#if defined(GSK_GLES) || defined(GSK_LEGACY)
  vec4 color = texture2D(u_external_source, vUv);
#else
  vec4 color = texture(u_external_source, vUv);
#endif

  if (u_premultiply == 1)
    color.rgb *= color.a;

  gskSetOutputColor(color);
}
