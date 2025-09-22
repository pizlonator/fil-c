signed char fun_char (void) { return 5; }
short fun_short (void) { return 6; }
int fun_int (void) { return 7; }
void *fun_ptr (void) { return &fun_ptr; }
int fun_iptr (void) { return 8; }
long fun_long (void) { return 9; }
__int128 fun_int128 (void) { return 10; }

typedef struct { int i[10]; } large_struct1_t;
large_struct1_t fun_large_struct1 (void) {
  large_struct1_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } };
  return ret;
}

typedef struct { int i1; int i2; int i3; int i4; int i5;
  int i6; int i7; int i8; int i9; int i10; } large_struct2_t;
large_struct2_t fun_large_struct2 (void) {
  large_struct2_t ret = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  return ret;
}

float fun_float (void) { return 1.5; }
float _Complex fun_float_complex (void) { return 1.5 + 2.5i; }

double fun_double (void) { return 2.5; }
double _Complex fun_double_complex (void) { return 2.5 + 3.5i; }

long double fun_long_double (void) { return 3.5; }
long double _Complex fun_long_double_complex (void) { return 4.5 + 5.5i; }

#ifdef FLOAT128
__float128 fun_float128 (void) { return 3.5; }
#endif

// 8 byte vectors.

typedef signed char __attribute__ ((vector_size (8))) vec_char_8_t;
vec_char_8_t fun_vec_char_8 (void) {
  vec_char_8_t ret = { 1, 2, 3, 4, 5, 6, 7, 8 };
  return ret;
}

typedef short __attribute__ ((vector_size (8))) vec_short_8_t;
vec_short_8_t fun_vec_short_8 (void) {
  vec_short_8_t ret = { 2, 3, 4, 5 };
  return ret;
}

typedef int __attribute__ ((vector_size (8))) vec_int_8_t;
vec_int_8_t fun_vec_int_8 (void) {
  vec_int_8_t ret = { 3, 4 };
  return ret;
}

typedef long __attribute__ ((vector_size (8))) vec_long_8_t;
vec_long_8_t fun_vec_long_8 (void) {
  vec_long_8_t ret = { 5 };
  return ret;
}

typedef float __attribute__ ((vector_size (8))) vec_float_8_t;
vec_float_8_t fun_vec_float_8 (void) {
  vec_float_8_t ret = { 1.5, 2.5 };
  return ret;
}

typedef double __attribute__ ((vector_size (8))) vec_double_8_t;
#ifndef AARCH64_BUG_1032854
// https://bugzilla.redhat.com/show_bug.cgi?id=1032854
vec_double_8_t fun_vec_double_8 (void) {
  vec_double_8_t ret = { 3.5 };
  return ret;
}
#endif

// 16 byte vectors.

typedef signed char __attribute__ ((vector_size (16))) vec_char_16_t;
vec_char_16_t fun_vec_char_16 (void) {
  vec_char_16_t ret = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
  return ret;
}

typedef short __attribute__ ((vector_size (16))) vec_short_16_t;
vec_short_16_t fun_vec_short_16 (void) {
  vec_short_16_t ret = { 2, 3, 4, 5, 6, 7, 8 };
  return ret;
}

typedef int __attribute__ ((vector_size (16))) vec_int_16_t;
vec_int_16_t fun_vec_int_16 (void) {
  vec_int_16_t ret = { 2, 3, 4 };
  return ret;
}

typedef long __attribute__ ((vector_size (16))) vec_long_16_t;
vec_long_16_t fun_vec_long_16 (void) {
  vec_long_16_t ret = { 3, 4 };
  return ret;
}

typedef __int128 __attribute__ ((vector_size (16))) vec_int128_16_t;
vec_int128_16_t fun_vec_int128_16 (void) {
  vec_int128_16_t ret = { 999 };
  return ret;
}

typedef float __attribute__ ((vector_size (16))) vec_float_16_t;
vec_float_16_t fun_vec_float_16 (void) {
  vec_float_16_t ret = { 1.5, 2.5, 3.5, 4.5 };
  return ret;
}

typedef double __attribute__ ((vector_size (16))) vec_double_16_t;
vec_double_16_t fun_vec_double_16 (void) {
  vec_double_16_t ret = { 2.5, 5 };
  return ret;
}

#ifdef FLOAT128
typedef __float128 __attribute__ ((vector_size (16))) vec_float128_16_t;
vec_float128_16_t fun_vec_float128_16 (void) {
  vec_float128_16_t ret = { 7.5 };
  return ret;
}
#endif

// Homogeneous floating-point aggregates.

typedef struct { float f; } hfa1_float_t;
hfa1_float_t fun_hfa1_float (void) {
  hfa1_float_t ret = { 1.5 };
  return ret;
}

typedef struct { double f; } hfa1_double_t;
hfa1_double_t fun_hfa1_double (void) {
  hfa1_double_t ret = { 3.0 };
  return ret;
}

typedef struct { long double f; } hfa1_long_double_t;
hfa1_long_double_t fun_hfa1_long_double (void) {
  hfa1_long_double_t ret = { 3.0 };
  return ret;
}

typedef struct { float f[1]; } hfa1_float_a_t;
hfa1_float_a_t fun_hfa1_float_a (void) {
  hfa1_float_a_t ret = { { 1.5 } };
  return ret;
}

typedef struct { double f[1]; } hfa1_double_a_t;
hfa1_double_a_t fun_hfa1_double_a (void) {
  hfa1_double_a_t ret = { { 3.0 } };
  return ret;
}

typedef struct { long double f[1]; } hfa1_long_double_a_t;
hfa1_long_double_a_t fun_hfa1_long_double_a (void) {
  hfa1_long_double_a_t ret = { { 3.0 } };
  return ret;
}

typedef struct { float f; float g; } hfa2_float_t;
hfa2_float_t fun_hfa2_float (void) {
  hfa2_float_t ret = { 1.5, 3.0 };
  return ret;
}

typedef struct { double f; double g; } hfa2_double_t;
hfa2_double_t fun_hfa2_double (void) {
  hfa2_double_t ret = { 3.0, 4.5 };
  return ret;
}

typedef struct { long double f; long double g; } hfa2_long_double_t;
hfa2_long_double_t fun_hfa2_long_double (void) {
  hfa2_long_double_t ret = { 3.0, 4.5 };
  return ret;
}

typedef struct { float f[2]; } hfa2_float_a_t;
hfa2_float_a_t fun_hfa2_float_a (void) {
  hfa2_float_a_t ret = { { 2.5, 3.5 } };
  return ret;
}

typedef struct { double f[2]; } hfa2_double_a_t;
hfa2_double_a_t fun_hfa2_double_a (void) {
  hfa2_double_a_t ret = { { 3.0, 3.5 } };
  return ret;
}

typedef struct { long double f[2]; } hfa2_long_double_a_t;
hfa2_long_double_a_t fun_hfa2_long_double_a (void) {
  hfa2_long_double_a_t ret = { { 3.0, 4.0 } };
  return ret;
}

typedef struct { float f; float g; float h; } hfa3_float_t;
hfa3_float_t fun_hfa3_float (void) {
  hfa3_float_t ret = { 1.5, 3.0, 4.5 };
  return ret;
}

typedef struct { double f; double g; double h; } hfa3_double_t;
hfa3_double_t fun_hfa3_double (void) {
  hfa3_double_t ret = { 3.0, 4.5, 9.5 };
  return ret;
}

typedef struct { long double f; long double g; long double h; } hfa3_long_double_t;
hfa3_long_double_t fun_hfa3_long_double (void) {
  hfa3_long_double_t ret = { 3.0, 4.5, 9.5 };
  return ret;
}

typedef struct { float f[3]; } hfa3_float_a_t;
hfa3_float_a_t fun_hfa3_float_a (void) {
  hfa3_float_a_t ret = { { 3.5, 4.5, 5.5 } };
  return ret;
}

typedef struct { double f[3]; } hfa3_double_a_t;
hfa3_double_a_t fun_hfa3_double_a (void) {
  hfa3_double_a_t ret = { { 3.0, 3.5, 4.0 } };
  return ret;
}

typedef struct { long double f[3]; } hfa3_long_double_a_t;
hfa3_long_double_a_t fun_hfa3_long_double_a (void) {
  hfa3_long_double_a_t ret = { { 3.0, 4.0, 5.0 } };
  return ret;
}

typedef struct { float f; float g; float h; float i; } hfa4_float_t;
hfa4_float_t fun_hfa4_float (void) {
  hfa4_float_t ret = { 1.5, 3.5, 4.5, 9.5 };
  return ret;
}

typedef struct { double f; double g; double h; double i; } hfa4_double_t;
hfa4_double_t fun_hfa4_double (void) {
  hfa4_double_t ret = { 3.5, 4.5, 9.5, 1.5 };
  return ret;
}

typedef struct { long double f; long double g; long double h; long double i; } hfa4_long_double_t;
hfa4_long_double_t fun_hfa4_long_double (void) {
  hfa4_long_double_t ret = { 3.5, 4.5, 9.5, 1.5 };
  return ret;
}

typedef struct { float f[4]; } hfa4_float_a_t;
hfa4_float_a_t fun_hfa4_float_a (void) {
  hfa4_float_a_t ret = { { 4.5, 5.5, 6.5, 7.5 } };
  return ret;
}

typedef struct { double f[4]; } hfa4_double_a_t;
hfa4_double_a_t fun_hfa4_double_a (void) {
  hfa4_double_a_t ret = { { 3.0, 4.5, 5.0, 5.5 } };
  return ret;
}

typedef struct { long double f[4]; } hfa4_long_double_a_t;
hfa4_long_double_a_t fun_hfa4_long_double_a (void) {
  hfa4_long_double_a_t ret = { { 3.0, 4.0, 5.0, 6.0 } };
  return ret;
}

typedef struct { float f; float g; float h; float i; float j; } nfa5_float_t;
nfa5_float_t fun_nfa5_float (void) {
  nfa5_float_t ret = { 1.5, 3.5, 4.5, 9.5, 10.5 };
  return ret;
}

typedef struct { double f; double g; double h; double i; double j; } nfa5_double_t;
nfa5_double_t fun_nfa5_double (void) {
  nfa5_double_t ret = { 3.5, 4.5, 9.5, 1.5, 2.5 };
  return ret;
}

typedef struct { long double f; long double g; long double h; long double i; long double j; } nfa5_long_double_t;
nfa5_long_double_t fun_nfa5_long_double (void) {
  nfa5_long_double_t ret = { 3.5, 4.5, 9.5, 1.5, 2.5 };
  return ret;
}

typedef struct { float f[5]; } nfa5_float_a_t;
nfa5_float_a_t fun_nfa5_float_a (void) {
  nfa5_float_a_t ret = { { 4.5, 5.5, 6.5, 7.5, 9.5 } };
  return ret;
}

typedef struct { double f[5]; } nfa5_double_a_t;
nfa5_double_a_t fun_nfa5_double_a (void) {
  nfa5_double_a_t ret = { { 3.0, 4.5, 5.0, 5.5, 6.5 } };
  return ret;
}

typedef struct { long double f[5]; } nfa5_long_double_a_t;
nfa5_long_double_a_t fun_nfa5_long_double_a (void) {
  nfa5_long_double_a_t ret = { { 3.0, 4.0, 5.0, 6.0, 7.0 } };
  return ret;
}

#ifdef FLOAT128
typedef struct { __float128 f; } hfa1_float128_t;
hfa1_float128_t fun_hfa1_float128 (void) {
  hfa1_float128_t ret = { 4.5 };
  return ret;
}

typedef struct { __float128 f; __float128 g; } hfa2_float128_t;
hfa2_float128_t fun_hfa2_float128 (void) {
  hfa2_float128_t ret = { 4.5, 9.5 };
  return ret;
}

typedef struct { __float128 f; __float128 g; __float128 h; } hfa3_float128_t;
hfa3_float128_t fun_hfa3_float128 (void) {
  hfa3_float128_t ret = { 4.5, 9.5, 12.5 };
  return ret;
}

typedef struct { __float128 f; __float128 g; __float128 h; __float128 i; } hfa4_float128_t;
hfa4_float128_t fun_hfa4_float128 (void) {
  hfa4_float128_t ret = { 4.5, 9.5, 3.5, 1.5 };
  return ret;
}
#endif

// Homogeneous vector aggregates of 1 element.

typedef struct { vec_char_8_t a; } hva1_vec_char_8_t;
hva1_vec_char_8_t fun_hva1_vec_char_8 (void) {
  hva1_vec_char_8_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8 } };
  return ret;
}

typedef struct { vec_short_8_t a; } hva1_vec_short_8_t;
hva1_vec_short_8_t fun_hva1_vec_short_8 (void) {
  hva1_vec_short_8_t ret = { { 2, 3, 4, 5 } };
  return ret;
}

typedef struct { vec_int_8_t a; } hva1_vec_int_8_t;
hva1_vec_int_8_t fun_hva1_vec_int_8 (void) {
  hva1_vec_int_8_t ret = { { 3, 4 } };
  return ret;
}

typedef struct { vec_long_8_t a; } hva1_vec_long_8_t;
hva1_vec_long_8_t fun_hva1_vec_long_8 (void) {
  hva1_vec_long_8_t ret = { { 5 } };
  return ret;
}

typedef struct { vec_float_8_t a; } hva1_vec_float_8_t;
hva1_vec_float_8_t fun_hva1_vec_float_8 (void) {
  hva1_vec_float_8_t ret = { { 1.5, 2.5 } };
  return ret;
}

typedef struct { vec_double_8_t a; } hva1_vec_double_8_t;
hva1_vec_double_8_t fun_hva1_vec_double_8 (void) {
  hva1_vec_double_8_t ret = { { 3.5 } };
  return ret;
}

typedef struct { vec_char_16_t a; } hva1_vec_char_16_t;
hva1_vec_char_16_t fun_hva1_vec_char_16_t (void) {
  hva1_vec_char_16_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8,
			       9, 10, 11, 12, 13, 14, 15, 16 } };
  return ret;
}

typedef struct { vec_short_16_t a; } hva1_vec_short_16_t;
hva1_vec_short_16_t fun_hva1_vec_short_16_t (void) {
  hva1_vec_short_16_t ret = { { 2, 3, 4, 5, 6, 7, 8, 9 } };
  return ret;
}

typedef struct { vec_int_16_t a; } hva1_vec_int_16_t;
hva1_vec_int_16_t fun_hva1_vec_int_16_t (void) {
  hva1_vec_int_16_t ret = { { 3, 4, 5, 6 } };
  return ret;
}

typedef struct { vec_long_16_t a; } hva1_vec_long_16_t;
hva1_vec_long_16_t fun_hva1_vec_long_16_t (void) {
  hva1_vec_long_16_t ret = { { 4, 5 } };
  return ret;
}

typedef struct { vec_int128_16_t a; } hva1_vec_int128_16_t;
hva1_vec_int128_16_t fun_hva1_vec_int128_16_t (void) {
  hva1_vec_int128_16_t ret = { { 6 } };
  return ret;
}

typedef struct { vec_float_16_t a; } hva1_vec_float_16_t;
hva1_vec_float_16_t fun_hva1_vec_float_16_t (void) {
  hva1_vec_float_16_t ret = { { 1.5, 2.5, 3.5, 4.5 } };
  return ret;
}

typedef struct { vec_double_16_t a; } hva1_vec_double_16_t;
hva1_vec_double_16_t fun_hva1_vec_double_16_t (void) {
  hva1_vec_double_16_t ret = { { 2.5, 3.5 } };
  return ret;
}

#ifdef FLOAT128
typedef struct { vec_float128_16_t a; } hva1_vec_float128_16_t;
hva1_vec_float128_16_t fun_hva1_vec_float128_16_t (void) {
  hva1_vec_float128_16_t ret = { { 4.5 } };
  return ret;
}
#endif

// Homogeneous vector aggregates of 2 elements.

typedef struct { vec_char_8_t a; vec_char_8_t b; } hva2_vec_char_8_t;
hva2_vec_char_8_t fun_hva2_vec_char_8 (void) {
  hva2_vec_char_8_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8 },
			    { 2, 3, 4, 5, 6, 7, 8, 9 } };
  return ret;
}

typedef struct { vec_short_8_t a; vec_short_8_t b; } hva2_vec_short_8_t;
hva2_vec_short_8_t fun_hva2_vec_short_8 (void) {
  hva2_vec_short_8_t ret = { { 2, 3, 4, 5 },
			     { 3, 4, 5, 6 } };
  return ret;
}

typedef struct { vec_int_8_t a; vec_int_8_t b; } hva2_vec_int_8_t;
hva2_vec_int_8_t fun_hva2_vec_int_8 (void) {
  hva2_vec_int_8_t ret = { { 3, 4 },
			   { 4, 5 } };
  return ret;
}

typedef struct { vec_long_8_t a; vec_long_8_t b; } hva2_vec_long_8_t;
hva2_vec_long_8_t fun_hva2_vec_long_8 (void) {
  hva2_vec_long_8_t ret = { { 5 },
			    { 6 } };
  return ret;
}

typedef struct { vec_float_8_t a; vec_float_8_t b; } hva2_vec_float_8_t;
hva2_vec_float_8_t fun_hva2_vec_float_8 (void) {
  hva2_vec_float_8_t ret = { { 1.5, 2.5 },
			     { 2.5, 3.5 } };
  return ret;
}

typedef struct { vec_double_8_t a; vec_double_8_t b; } hva2_vec_double_8_t;
hva2_vec_double_8_t fun_hva2_vec_double_8 (void) {
  hva2_vec_double_8_t ret = { { 3.5 },
			      { 4.5 } };
  return ret;
}

typedef struct { vec_char_16_t a; vec_char_16_t b; } hva2_vec_char_16_t;
hva2_vec_char_16_t fun_hva2_vec_char_16_t (void) {
  hva2_vec_char_16_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8,
			       9, 10, 11, 12, 13, 14, 15, 16 },
			     { 2, 3, 4, 5, 6, 7, 8, 9,
			       10, 11, 12, 13, 14, 15, 16, 17 } };
  return ret;
}

typedef struct { vec_short_16_t a; vec_short_16_t b; } hva2_vec_short_16_t;
hva2_vec_short_16_t fun_hva2_vec_short_16_t (void) {
  hva2_vec_short_16_t ret = { { 2, 3, 4, 5, 6, 7, 8, 9 },
			      { 3, 4, 5, 6, 7, 8, 9, 10 } };
  return ret;
}

typedef struct { vec_int_16_t a; vec_int_16_t b; } hva2_vec_int_16_t;
hva2_vec_int_16_t fun_hva2_vec_int_16_t (void) {
  hva2_vec_int_16_t ret = { { 3, 4, 5, 6 },
			    { 4, 5, 6, 7 } };
  return ret;
}

typedef struct { vec_long_16_t a; vec_long_16_t b; } hva2_vec_long_16_t;
hva2_vec_long_16_t fun_hva2_vec_long_16_t (void) {
  hva2_vec_long_16_t ret = { { 4, 5 },
			     { 5, 6 } };
  return ret;
}

typedef struct { vec_int128_16_t a; vec_int128_16_t b; } hva2_vec_int128_16_t;
hva2_vec_int128_16_t fun_hva2_vec_int128_16_t (void) {
  hva2_vec_int128_16_t ret = { { 6 },
			       { 7 } };
  return ret;
}

typedef struct { vec_float_16_t a; vec_float_16_t b; } hva2_vec_float_16_t;
hva2_vec_float_16_t fun_hva2_vec_float_16_t (void) {
  hva2_vec_float_16_t ret = { { 1.5, 2.5, 3.5, 4.5 },
			      { 2.5, 3.5, 4.5, 5.5 } };
  return ret;
}

typedef struct { vec_double_16_t a; vec_double_16_t b; } hva2_vec_double_16_t;
hva2_vec_double_16_t fun_hva2_vec_double_16_t (void) {
  hva2_vec_double_16_t ret = { { 2.5, 3.5 },
			       { 3.5, 4.5 } };
  return ret;
}

#ifdef FLOAT128
typedef struct { vec_float128_16_t a; vec_float128_16_t b; } hva2_vec_float128_16_t;
hva2_vec_float128_16_t fun_hva2_vec_float128_16_t (void) {
  hva2_vec_float128_16_t ret = { { 4.5 },
				 { 5.5 } };
  return ret;
}
#endif

// Homogeneous vector aggregates of 3 elements.

typedef struct { vec_char_8_t a; vec_char_8_t b; vec_char_8_t c; } hva3_vec_char_8_t;
hva3_vec_char_8_t fun_hva3_vec_char_8 (void) {
  hva3_vec_char_8_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8 },
			    { 2, 3, 4, 5, 6, 7, 8, 9 },
			    { 3, 4, 5, 6, 7, 8, 9, 10 } };
  return ret;
}

typedef struct { vec_short_8_t a; vec_short_8_t b; vec_short_8_t c; } hva3_vec_short_8_t;
hva3_vec_short_8_t fun_hva3_vec_short_8 (void) {
  hva3_vec_short_8_t ret = { { 2, 3, 4, 5 },
			     { 3, 4, 5, 6 },
			     { 4, 5, 6, 7 } };
  return ret;
}

typedef struct { vec_int_8_t a; vec_int_8_t b; vec_int_8_t c; } hva3_vec_int_8_t;
hva3_vec_int_8_t fun_hva3_vec_int_8 (void) {
  hva3_vec_int_8_t ret = { { 3, 4 },
			   { 4, 5 },
			   { 5, 6 } };
  return ret;
}

typedef struct { vec_long_8_t a; vec_long_8_t b; vec_long_8_t c; } hva3_vec_long_8_t;
hva3_vec_long_8_t fun_hva3_vec_long_8 (void) {
  hva3_vec_long_8_t ret = { { 5 },
			    { 6 },
			    { 7 } };
  return ret;
}

typedef struct { vec_float_8_t a; vec_float_8_t b; vec_float_8_t c; } hva3_vec_float_8_t;
hva3_vec_float_8_t fun_hva3_vec_float_8 (void) {
  hva3_vec_float_8_t ret = { { 1.5, 2.5 },
			     { 2.5, 3.5 },
			     { 3.5, 4.5 } };
  return ret;
}

typedef struct { vec_double_8_t a; vec_double_8_t b; vec_double_8_t c; } hva3_vec_double_8_t;
hva3_vec_double_8_t fun_hva3_vec_double_8 (void) {
  hva3_vec_double_8_t ret = { { 3.5 },
			      { 4.5 },
			      { 5.5 } };
  return ret;
}

typedef struct { vec_char_16_t a; vec_char_16_t b; vec_char_16_t c; } hva3_vec_char_16_t;
hva3_vec_char_16_t fun_hva3_vec_char_16_t (void) {
  hva3_vec_char_16_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8,
			       9, 10, 11, 12, 13, 14, 15, 16 },
			     { 2, 3, 4, 5, 6, 7, 8, 9,
			       10, 11, 12, 13, 14, 15, 16, 17 },
			     { 3, 4, 5, 6, 7, 8, 9, 10,
			       11, 12, 13, 14, 15, 16, 17, 18 } };
  return ret;
}

typedef struct { vec_short_16_t a; vec_short_16_t b; vec_short_16_t c; } hva3_vec_short_16_t;
hva3_vec_short_16_t fun_hva3_vec_short_16_t (void) {
  hva3_vec_short_16_t ret = { { 2, 3, 4, 5, 6, 7, 8, 9 },
			      { 3, 4, 5, 6, 7, 8, 9, 10 },
			      { 4, 5, 6, 7, 8, 9, 10, 11 } };
  return ret;
}

typedef struct { vec_int_16_t a; vec_int_16_t b; vec_int_16_t c; } hva3_vec_int_16_t;
hva3_vec_int_16_t fun_hva3_vec_int_16_t (void) {
  hva3_vec_int_16_t ret = { { 3, 4, 5, 6 },
			    { 4, 5, 6, 7 },
			    { 5, 6, 7, 8 } };
  return ret;
}

typedef struct { vec_long_16_t a; vec_long_16_t b; vec_long_16_t c; } hva3_vec_long_16_t;
hva3_vec_long_16_t fun_hva3_vec_long_16_t (void) {
  hva3_vec_long_16_t ret = { { 3, 4 },
			     { 4, 5 },
			     { 5, 6 } };
  return ret;
}

typedef struct { vec_int128_16_t a; vec_int128_16_t b; vec_int128_16_t c; } hva3_vec_int128_16_t;
hva3_vec_int128_16_t fun_hva3_vec_int128_16_t (void) {
  hva3_vec_int128_16_t ret = { { 6 },
			       { 7 },
			       { 8 } };
  return ret;
}

typedef struct { vec_float_16_t a; vec_float_16_t b; vec_float_16_t c; } hva3_vec_float_16_t;
hva3_vec_float_16_t fun_hva3_vec_float_16_t (void) {
  hva3_vec_float_16_t ret = { { 1.5, 2.5, 3.5, 4.5 },
			      { 2.5, 3.5, 4.5, 5.5 },
			      { 3.5, 4.5, 5.5, 6.5 } };
  return ret;
}

typedef struct { vec_double_16_t a; vec_double_16_t b; vec_double_16_t c; } hva3_vec_double_16_t;
hva3_vec_double_16_t fun_hva3_vec_double_16_t (void) {
  hva3_vec_double_16_t ret = { { 2.5, 3.5 },
			       { 3.5, 4.5 },
			       { 4.5, 5.5 } };
  return ret;
}

#ifdef FLOAT128
typedef struct { vec_float128_16_t a; vec_float128_16_t b; vec_float128_16_t c; } hva3_vec_float128_16_t;
hva3_vec_float128_16_t fun_hva3_vec_float128_16_t (void) {
  hva3_vec_float128_16_t ret = { { 4.5 },
				 { 5.5 },
				 { 6.5 } };
  return ret;
}
#endif

// Homogeneous vector aggregates of 3 elements.

typedef struct { vec_char_8_t a; vec_char_8_t b; vec_char_8_t c; vec_char_8_t d; } hva4_vec_char_8_t;
hva4_vec_char_8_t fun_hva4_vec_char_8 (void) {
  hva4_vec_char_8_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8 },
			    { 2, 3, 4, 5, 6, 7, 8, 9 },
			    { 3, 4, 5, 6, 7, 8, 9, 10 },
			    { 4, 5, 6, 7, 8, 9, 10, 11 } };
  return ret;
}

typedef struct { vec_short_8_t a; vec_short_8_t b; vec_short_8_t c; vec_short_8_t d; } hva4_vec_short_8_t;
hva4_vec_short_8_t fun_hva4_vec_short_8 (void) {
  hva4_vec_short_8_t ret = { { 2, 3, 4, 5 },
			     { 3, 4, 5, 6 },
			     { 4, 5, 6, 7 },
			     { 5, 6, 7, 8 } };
  return ret;
}

typedef struct { vec_int_8_t a; vec_int_8_t b; vec_int_8_t c; vec_int_8_t d; } hva4_vec_int_8_t;
hva4_vec_int_8_t fun_hva4_vec_int_8 (void) {
  hva4_vec_int_8_t ret = { { 3, 4 },
			   { 4, 5 },
			   { 5, 6 },
			   { 6, 7 } };
  return ret;
}

typedef struct { vec_long_8_t a; vec_long_8_t b; vec_long_8_t c; vec_long_8_t d; } hva4_vec_long_8_t;
hva4_vec_long_8_t fun_hva4_vec_long_8 (void) {
  hva4_vec_long_8_t ret = { { 5 },
			    { 6 },
			    { 7 },
			    { 8 } };
  return ret;
}

typedef struct { vec_float_8_t a; vec_float_8_t b; vec_float_8_t c; vec_float_8_t d; } hva4_vec_float_8_t;
hva4_vec_float_8_t fun_hva4_vec_float_8 (void) {
  hva4_vec_float_8_t ret = { { 1.5, 2.5 },
			     { 2.5, 3.5 },
			     { 3.5, 4.5 },
			     { 4.5, 5.5 } };
  return ret;
}

typedef struct { vec_double_8_t a; vec_double_8_t b; vec_double_8_t c; vec_double_8_t d; } hva4_vec_double_8_t;
hva4_vec_double_8_t fun_hva4_vec_double_8 (void) {
  hva4_vec_double_8_t ret = { { 3.5 },
			      { 4.5 },
			      { 5.5 },
			      { 6.5 } };
  return ret;
}

typedef struct { vec_char_16_t a; vec_char_16_t b; vec_char_16_t c; vec_char_16_t d; } hva4_vec_char_16_t;
hva4_vec_char_16_t fun_hva4_vec_char_16_t (void) {
  hva4_vec_char_16_t ret = { { 1, 2, 3, 4, 5, 6, 7, 8,
			       9, 10, 11, 12, 13, 14, 15, 16 },
			     { 2, 3, 4, 5, 6, 7, 8, 9,
			       10, 11, 12, 13, 14, 15, 16, 17 },
			     { 3, 4, 5, 6, 7, 8, 9, 10,
			       11, 12, 13, 14, 15, 16, 17, 18 },
			     { 4, 5, 6, 7, 8, 9, 10, 11,
			       12, 13, 14, 15, 16, 17, 18, 19 } };
  return ret;
}

typedef struct { vec_short_16_t a; vec_short_16_t b; vec_short_16_t c; vec_short_16_t d; } hva4_vec_short_16_t;
hva4_vec_short_16_t fun_hva4_vec_short_16_t (void) {
  hva4_vec_short_16_t ret = { { 2, 3, 4, 5, 6, 7, 8, 9 },
			      { 3, 4, 5, 6, 7, 8, 9, 10 },
			      { 4, 5, 6, 7, 8, 9, 10, 11 },
			      { 5, 6, 7, 8, 9, 10, 11, 12 } };
  return ret;
}

typedef struct { vec_int_16_t a; vec_int_16_t b; vec_int_16_t c; vec_int_16_t d; } hva4_vec_int_16_t;
hva4_vec_int_16_t fun_hva4_vec_int_16_t (void) {
  hva4_vec_int_16_t ret = { { 3, 4, 5, 6 },
			    { 4, 5, 6, 7 },
			    { 5, 6, 7, 8 },
			    { 6, 7, 8, 9 } };
  return ret;
}

typedef struct { vec_long_16_t a; vec_long_16_t b; vec_long_16_t c; vec_long_16_t d; } hva4_vec_long_16_t;
hva4_vec_long_16_t fun_hva4_vec_long_16_t (void) {
  hva4_vec_long_16_t ret = { { 3, 4 },
			     { 4, 5 },
			     { 5, 6 },
			     { 6, 7 } };
  return ret;
}

typedef struct { vec_int128_16_t a; vec_int128_16_t b; vec_int128_16_t c; vec_int128_16_t d; } hva4_vec_int128_16_t;
hva4_vec_int128_16_t fun_hva4_vec_int128_16_t (void) {
  hva4_vec_int128_16_t ret = { { 6 },
			       { 7 },
			       { 8 },
			       { 9 } };
  return ret;
}

typedef struct { vec_float_16_t a; vec_float_16_t b; vec_float_16_t c; vec_float_16_t d; } hva4_vec_float_16_t;
hva4_vec_float_16_t fun_hva4_vec_float_16_t (void) {
  hva4_vec_float_16_t ret = { { 1.5, 2.5, 3.5, 4.5 },
			      { 2.5, 3.5, 4.5, 5.5 },
			      { 3.5, 4.5, 5.5, 6.5 },
			      { 4.5, 5.5, 6.5, 7.5 } };
  return ret;
}

typedef struct { vec_double_16_t a; vec_double_16_t b; vec_double_16_t c; vec_double_16_t d; } hva4_vec_double_16_t;
hva4_vec_double_16_t fun_hva4_vec_double_16_t (void) {
  hva4_vec_double_16_t ret = { { 2.5, 3.5 },
			       { 3.5, 4.5 },
			       { 4.5, 5.5 },
			       { 5.5, 6.5 } };
  return ret;
}

#ifdef FLOAT128
typedef struct { vec_float128_16_t a; vec_float128_16_t b; vec_float128_16_t c; vec_float128_16_t d; } hva4_vec_float128_16_t;
hva4_vec_float128_16_t fun_hva4_vec_float128_16_t (void) {
  hva4_vec_float128_16_t ret = { { 4.5 },
				 { 5.5 },
				 { 6.5 },
				 { 7.5 } };
  return ret;
}
#endif

// Mixed HFA.
typedef struct { float _Complex a; float b; } mixed_hfa3_cff_t;
mixed_hfa3_cff_t fun_mixed_hfa3_cff (void) {
  mixed_hfa3_cff_t ret = { 1.5 + 2.5i, 3.5 };
  return ret;
}

typedef struct { double _Complex a; double b; } mixed_hfa3_cdd_t;
mixed_hfa3_cdd_t fun_mixed_hfa3_cdd (void) {
  mixed_hfa3_cdd_t ret = { 1.5 + 2.5i, 3.5 };
  return ret;
}

typedef struct { long double _Complex a; long double b; } mixed_hfa3_cldld_t;
mixed_hfa3_cldld_t fun_mixed_hfa3_cldld (void) {
  mixed_hfa3_cldld_t ret = { 1.5 + 2.5i, 3.5 };
  return ret;
}

typedef struct { float b; float _Complex a; } mixed_hfa3_fcf_t;
mixed_hfa3_fcf_t fun_mixed_hfa3_fcf (void) {
  mixed_hfa3_fcf_t ret = { 3.5, 1.5 + 2.5i };
  return ret;
}

typedef struct { double b; double _Complex a; } mixed_hfa3_dcd_t;
mixed_hfa3_dcd_t fun_mixed_hfa3_dcd (void) {
  mixed_hfa3_dcd_t ret = { 3.5, 1.5 + 2.5i };
  return ret;
}

typedef struct { long double b; long double _Complex a; } mixed_hfa3_ldcld_t;
mixed_hfa3_ldcld_t fun_mixed_hfa3_ldcld (void) {
  mixed_hfa3_ldcld_t ret = { 3.5, 1.5 + 2.5i };
  return ret;
}

typedef struct { vec_float_8_t a; vec_short_8_t b; } mixed_hfa2_fltsht_t;
mixed_hfa2_fltsht_t fun_mixed_hfa2_fltsht_t (void) {
  mixed_hfa2_fltsht_t ret = { { 3.5, 4.5 }, { 1, 2, 3, 4 } };
  return ret;
}

int main(int argc, char *argv[])
{
  return 0;
}
