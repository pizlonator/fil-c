fadd za.h[w7, 0], {z0.h-z1.h}
fadd za.h[w12, 0], {z0.h-z1.h}
fadd za.h[w8, 8], {z0.h-z1.h}
fadd za.h[w8, 0], {z1.h-z2.h}

fadd za.h[w7, 0], {z0.h-z3.h}
fadd za.h[w12, 0], {z0.h-z3.h}
fadd za.h[w8, 8], {z0.h-z3.h}
fadd za.h[w8, 0], {z2.h-z5.h}

fadd za.h[w0, 0], z0.h

fsub za.h[w7, 0], {z0.h-z1.h}
fsub za.h[w12, 0], {z0.h-z1.h}
fsub za.h[w8, 8], {z0.h-z1.h}
fsub za.h[w8, 0], {z1.h-z2.h}

fsub za.h[w7, 0], {z0.h-z3.h}
fsub za.h[w12, 0], {z0.h-z3.h}
fsub za.h[w8, 8], {z0.h-z3.h}
fsub za.h[w8, 0], {z2.h-z5.h}

fsub za.h[w0, 0], z0.h

