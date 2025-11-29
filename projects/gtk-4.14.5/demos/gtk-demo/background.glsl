uniform float u_time;

void
mainImage(out vec4 fragColor, in vec2 fragCoord, in vec2 resolution, in vec2 uv)
{
  vec2 pos = (fragCoord.xy * 2.0 - resolution.xy)/ min (resolution.x, resolution.y) ;

  float t0 = sin ((u_time + 0.00)*1.0);
  float t1 = sin ((u_time + 0.30)*0.4);
  float t2 = cos ((u_time + 0.23)*0.9);
  float t3 = cos ((u_time + 0.41)*0.6);
  float t4 = cos ((u_time + 0.11)*0.3);

  vec2 p0 = vec2 (t1, t0) ;
  vec2 p1 = vec2 (t2, t3) ;
  vec2 p2 = vec2 (t4, t3) ;

  float r = 1.0/distance (pos, p0);
  float g = 1.0/distance (pos, p1);
  float b = 1.0/distance (pos, p2);
  float sum = r + g + b;

  float alpha = 1.0 - pow (1.0/(sum), 40.0)*pow (10.0, 40.0*0.7);

  fragColor = vec4 (r*0.5, g*0.5, b*0.5, 1.0) * alpha;
}
