uniform float u_time;
uniform vec2 u_mouse;
uniform sampler2D u_texture1;

#define PI      3.141592654

float decay(float v, float t)
{
  return v * (1.0 / (1.0 + t*t));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  // Temporary to loop time every 1 sec
  float time = u_time;
  // we normalize all the effects according to the min height/width
  float size = min(resolution.x, resolution.y);

  // Animate one wave over size in 0.3 sec
  float wave_speed = size / 0.3;
  float wave_length = size / 1.0;
  float wave_height = size  * 0.1;

  vec2 center = u_mouse;
  vec2 direction_from_center = fragCoord - center;
  float distance_from_center = length(direction_from_center);

  /* Normalize direction */
  direction_from_center = direction_from_center / distance_from_center;

  float propagation_length = time * wave_speed;

  float t = (propagation_length - distance_from_center) / wave_length;
  float offset_magnitude = 0.0;
  if (t > 0.0)
    offset_magnitude = decay(wave_height * sin(t * 2.0 * PI), t);

  vec2 offset = direction_from_center * min(offset_magnitude, distance_from_center);
  vec2 source = fragCoord - offset;

  vec2 uv2 = source / resolution;
  fragColor = GskTexture(u_texture1, vec2(uv2.x, 1.0-uv2.y));
}
