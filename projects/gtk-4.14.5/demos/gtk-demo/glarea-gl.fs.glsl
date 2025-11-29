#version 330

out vec4 outputColor;

void main() {
  float lerpVal = gl_FragCoord.y / 500.0f;

  outputColor = mix(vec4(1.0f, 0.85f, 0.35f, 1.0f), vec4(0.2f, 0.2f, 0.2f, 1.0f), lerpVal);
}
