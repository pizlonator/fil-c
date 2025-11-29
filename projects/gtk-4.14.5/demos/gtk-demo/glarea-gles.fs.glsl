precision highp float;

void main() {
  float lerpVal = gl_FragCoord.y / 500.0;

  gl_FragColor = mix(vec4(1.0, 0.85, 0.35, 1.0), vec4(0.2, 0.2, 0.2, 1.0), lerpVal);
}
