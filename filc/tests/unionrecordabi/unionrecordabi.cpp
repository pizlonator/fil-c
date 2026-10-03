#include <cstdio>
#include <variant>
struct S { long tag; union { long i; int *p; } u; };
__attribute__((noinline)) S make(int *p) { S s; s.tag = 1; s.u.p = p; return s; }
__attribute__((noinline)) int take(S s) { return *s.u.p; }
__attribute__((noinline)) std::variant<long, int *> mv(int *p) { return p; }
int main() {
  int x = 42;
  S s = make(&x);
  std::printf("%d %d %d\n", *s.u.p, take(s), *std::get<int *>(mv(&x)));
}
