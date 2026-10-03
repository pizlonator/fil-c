/* The defining module: filcc emits the pizlonated_g getter (and the DO).

   The first `#! global ptr` access from the asm side runs the getter's
   slow path (lazy initialization/registration), and the C initializers
   must be visible. */
int g[8] __attribute__((aligned(16))) = { 1, 2, 3, 4, 5, 6, 7, 8 };
