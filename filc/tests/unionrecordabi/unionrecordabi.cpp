#include "value.h"
#include <cassert>
#include <cstdio>

int main() {
  int x = 42;
  S s = make(&x);
  Derived d;
  d.u.p = &x;
  d.tag = 73;
  d = inherited(d);
  assert(d.tag == 73 && *d.u.p == 42);
  // Nonzero-null initialization must keep its original representation;
  // declaring another pointer alternative cannot change null.
  Nonzero null = { nullptr };
  null = nonzero(null);
  assert(null.member == nullptr);
  Nonzero n;
  n.pointer = &x;
  n = nonzero(n);
  assert(*n.pointer == 42);
  Nested inside = { 97, n };
  inside = nested(inside);
  assert(inside.tag == 97 && *inside.value.pointer == 42);
  // Normalization also finds pointers in nested union alternatives, even when
  // the outer union's first member itself is zero-initializable.
  Outer outside;
  outside.value.pointer = &x;
  outside = outer(outside);
  assert(*outside.value.pointer == 42);
  Both object;
  object.value = 29;
  Method m;
  m.function = &Both::add;
  m = method(m);
  assert((object.*m.function)(13) == 42);
  Method empty = { nullptr };
  empty = method(empty);
  assert(empty.function == nullptr);
  Nontrivial nt(&x);
  nt = nontrivial(nt);
  assert(*nt.pointer == 42);
  std::printf("%d %d %d\n", *s.u.p, take(s), *std::get<int *>(mv(&x)));
}
