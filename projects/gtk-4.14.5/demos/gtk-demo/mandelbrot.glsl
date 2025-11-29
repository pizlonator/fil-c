// Originally from: https://www.shadertoy.com/view/wdBfDK
// License: CC0

#define MANDELBROT_ZOOM_START 0.0
#define MANDELBROT_ITER       240

void pR(inout vec2 p, in float a) {
  p = cos(a)*p + sin(a)*vec2(p.y, -p.x);
}

vec2 pMod2(inout vec2 p, in vec2 size) {
  vec2 c = floor((p + size*0.5)/size);
  p = mod(p + size*0.5,size) - size*0.5;
  return c;
}


vec3 mandelbrot(float time, vec2 p, out float ii) {
  vec3 col = vec3(0.0);

  float ztime = (time - MANDELBROT_ZOOM_START)*step(MANDELBROT_ZOOM_START, time);

  float zoo = 0.64 + 0.36*cos(.07*ztime);
  float coa = cos(0.15*(1.0-zoo)*ztime);
  float sia = sin(0.15*(1.0-zoo)*ztime);
  zoo = pow(zoo,8.0);
  vec2 xy = vec2( p.x*coa-p.y*sia, p.x*sia+p.y*coa);
  vec2 c = vec2(-.745,.186) + xy*zoo;

  const float B = 10.0;
  float l = 0.0;
  vec2 z  = vec2(0.0);

  vec2 zc = vec2(1.0);

  pR(zc, ztime);

  float d = 1e20;

  int i = 0;

  for(int j = 0; j < MANDELBROT_ITER; ++j) {
    float re2 = z.x*z.x;
    float im2 = z.y*z.y;
    float reim= z.x*z.y;

    if(re2 + im2 > (B*B)) break;

    z = vec2(re2 - im2, 2.0*reim) + c;

    vec2 zm = z;
    vec2 n = pMod2(zm, vec2(4));
    vec2 pp = zm - zc;
    float dd = dot(pp, pp);

    d = min(d, dd);

    l += 1.0;

    i = j;
  }

  ii = float(i)/float(MANDELBROT_ITER);

  float sl = l - log2(log2(dot(z,z))) + 4.0;

  vec3 dc = vec3(pow(max(1.0 - d, 0.0), 20.0));
  vec3 gc = 0.5 + 0.5*cos(3.0 + sl*0.15 + vec3(0.1,0.5,0.9));
  return gc + dc*smoothstep(28.8, 29.0, ztime);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  float s = 2.0/iResolution.y;

  vec2 o1 = vec2(1.0/8.0, 3.0/8.0)*s;
  vec2 o2 = vec2(-3.0/8.0, 1.0/8.0)*s;

  vec2 p = (-iResolution.xy + 2.0*fragCoord.xy)/iResolution.y;
  float ii = 0.0;
  vec3 col = mandelbrot(iTime, p+o1, ii);

  // "smart" AA? Is that a good idea?
  vec2 dii2 = vec2(dFdx(ii), dFdy(ii));
  float dii = length(dii2);

  if(abs(dii) > 0.01) {
    col += mandelbrot(iTime, p-o1, ii);
    col += mandelbrot(iTime, p+o2, ii);
    col += mandelbrot(iTime, p-o2, ii);
    col *=0.25;
//    col = vec3(1.0, 0.0, 0.0);
  }

  fragColor = vec4(col, 1.0);
}
