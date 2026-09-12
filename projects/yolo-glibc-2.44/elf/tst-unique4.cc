// BZ 12511
#include "tst-unique4.h"

#include <cstdio>

extern template struct S<1>;
extern template struct S<2>;
extern template struct S<3>;
extern template struct S<4>;
extern template struct S<5>;
extern template struct S<6>;
extern template struct S<7>;
extern template struct S<8>;
extern template struct S<9>;
extern template struct S<10>;
extern template struct S<11>;
extern template struct S<12>;
extern template struct S<13>;
extern template struct S<14>;
extern template struct S<15>;
extern template struct S<16>;
extern template struct S<17>;
extern template struct S<18>;
extern template struct S<19>;
extern template struct S<20>;
extern template struct S<21>;
extern template struct S<22>;
extern template struct S<23>;
extern template struct S<24>;

static int a[24] =
  {
    S<1>::i, S<2>::i, S<3>::i, S<4>::i, S<5>::i, S<6>::i, S<7>::i, S<8>::i,
    S<9>::i, S<10>::i, S<11>::i, S<12>::i, S<13>::i, S<14>::i, S<15>::i,
    S<16>::i, S<17>::i, S<18>::i, S<19>::i, S<20>::i, S<21>::i, S<22>::i,
    S<23>::i, S<24>::i
  };

int
main (void)
{
  int result = 0;
  for (int i = 0; i < 24; ++i)
    {
      printf("%d ", a[i]);
      result |= a[i] != i + 1;
    }

  printf("\n%d\n", S<1>::j);
  result |= S<1>::j != -1;

  return result;
}
