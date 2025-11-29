#ifndef _ELLIPSE_
#define _ELLIPSE_

struct Ellipse
{
  vec2 center;
  vec2 radius;
};

float
ellipse_distance (Ellipse r, vec2 p)
{
  vec2 e = r.radius;
  p = p - r.center;

  if (e.x == e.y)
    return length (p) - e.x;

  /* from https://www.shadertoy.com/view/tt3yz7 */
  vec2 pAbs = abs(p);
  vec2 ei = 1.0 / e;
  vec2 e2 = e*e;
  vec2 ve = ei * vec2(e2.x - e2.y, e2.y - e2.x);
  
  vec2 t = vec2(0.70710678118654752, 0.70710678118654752);
  for (int i = 0; i < 3; i++) {
      vec2 v = ve*t*t*t;
      vec2 u = normalize(pAbs - v) * length(t * e - v);
      vec2 w = ei * (v + u);
      t = normalize(clamp(w, 0.0, 1.0));
  }
  
  vec2 nearestAbs = t * e;
  float dist = length(pAbs - nearestAbs);
  return dot(pAbs, pAbs) < dot(nearestAbs, nearestAbs) ? -dist : dist;
}

#endif
