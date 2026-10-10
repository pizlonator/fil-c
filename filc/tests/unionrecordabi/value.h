#ifndef UNION_RECORD_ABI_VALUE_H
#define UNION_RECORD_ABI_VALUE_H

#include <variant>

struct S { long tag; union { long i; int *p; } u; };
struct Base { union { long i; int *p; } u; };
struct Derived : Base { long tag; };
struct C { int value; int add(int x) { return value + x; } };
union Nonzero { int C::*member; int *pointer; };
struct Nested { long tag; Nonzero value; };
union Outer { unsigned long bits; Nonzero value; };
struct Padding { long prefix; };
struct Both : Padding, C { };
union Method { int (Both::*function)(int); unsigned long bits[2]; };
union Nontrivial {
  int *pointer;
  long integer;
  Nontrivial(int *p) : pointer(p) { }
  Nontrivial(const Nontrivial &other) : pointer(other.pointer) { }
  ~Nontrivial() { }
};

S make(int *p);
int take(S value);
std::variant<long, int *> mv(int *p);
Derived inherited(Derived value);
Nonzero nonzero(Nonzero value);
Nested nested(Nested value);
Outer outer(Outer value);
Method method(Method value);
Nontrivial nontrivial(Nontrivial value);

#endif
