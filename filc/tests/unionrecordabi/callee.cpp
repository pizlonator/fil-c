#include "value.h"

S make(int *p) { S s; s.tag = 1; s.u.p = p; return s; }
int take(S value) { return *value.u.p; }
std::variant<long, int *> mv(int *p) { return p; }
Derived inherited(Derived value) { return value; }
Nonzero nonzero(Nonzero value) { return value; }
Nested nested(Nested value) { return value; }
Outer outer(Outer value) { return value; }
Method method(Method value) { return value; }
Nontrivial nontrivial(Nontrivial value) { return value; }
