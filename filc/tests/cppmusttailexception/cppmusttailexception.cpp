#include <cstdio>
#include <stdexcept>
int thrower(int n) { if (n == 0) throw std::runtime_error("boom"); return n; }
int middle(int n) { [[clang::musttail]] return thrower(n); }
int main() {
  try { middle(0); } catch (std::exception &e) { printf("caught %s\n", e.what()); }
  printf("%d\n", middle(5));
}
