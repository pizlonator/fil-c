// VERTEX_SHADER:
// blur.glsl

uniform float u_blur_radius;
uniform vec2 u_blur_size;
uniform vec2 u_blur_dir;

_OUT_ vec2 pixel_step;
_OUT_ float pixels_per_side;
_OUT_ vec3 initial_gaussian;

const float PI = 3.14159265;
const float RADIUS_MULTIPLIER = 2.0;

void main() {
  gl_Position = u_projection * u_modelview * vec4(aPosition, 0.0, 1.0);

  vUv = vec2(aUv.x, aUv.y);

  pixel_step = (vec2(1.0) / u_blur_size) * u_blur_dir;
  pixels_per_side = floor(u_blur_radius * RADIUS_MULTIPLIER / 2.0);

  float sigma = u_blur_radius / 2.0; // *shrug*
  initial_gaussian.x = 1.0 / (sqrt(2.0 * PI) * sigma);
  initial_gaussian.y = exp(-0.5 / (sigma * sigma));
  initial_gaussian.z = initial_gaussian.y * initial_gaussian.y;
}

// FRAGMENT_SHADER:
// blur.glsl

_IN_ vec2 pixel_step;
_IN_ float pixels_per_side;
_IN_ vec3 initial_gaussian;

// blur_radius 0 is NOT supported and MUST be caught before.

// Partially from http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
void main() {
  vec3 incrementalGaussian = initial_gaussian;

  float coefficientSum = 0.0;
  vec4 sum = GskTexture(u_source, vUv) * incrementalGaussian.x;
  coefficientSum += incrementalGaussian.x;
  incrementalGaussian.xy *= incrementalGaussian.yz;

  vec2 p = pixel_step;
  for (int i = 1; i <= int(pixels_per_side); i++) {
    sum += GskTexture(u_source, vUv - p) * incrementalGaussian.x;
    sum += GskTexture(u_source, vUv + p) * incrementalGaussian.x;

    coefficientSum += 2.0 * incrementalGaussian.x;
    incrementalGaussian.xy *= incrementalGaussian.yz;

    p += pixel_step;
  }

  gskSetOutputColor(sum / coefficientSum);
}
