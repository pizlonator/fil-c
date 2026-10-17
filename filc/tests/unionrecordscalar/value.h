#ifndef UNION_RECORD_SCALAR_VALUE_H
#define UNION_RECORD_SCALAR_VALUE_H

union Scalar {
    long integer;
    double number;
    void* pointer;
    int (*function)(int);
};

struct First { union Scalar value; long tag; };
struct Last { long tag; union Scalar value; };
struct Array { union Scalar values[1]; long tag; };
struct Hidden { union { long integer; int* pointers[1]; } value; long tag; };
union WideValue { void* pointer; int* pointers[2]; unsigned long bits[2]; };
struct Wide { union WideValue value; };
struct Large { union { void* pointer; __int128 integer; } value; };
union Numeric { unsigned long bits; long integer; double number; };
union NumericPair { unsigned long bits[2]; double numbers[2]; };
union __attribute__((aligned(16))) Aligned { int* pointer; long integer; };
union MixedIS { struct { int* pointer; double number; } value; double numbers[2]; };
union MixedSI { struct { double number; int* pointer; } value; double numbers[2]; };
union HiddenLast { unsigned long bits[2]; struct { long tag; int* pointer; } value; };

union Scalar scalar(union Scalar value);
union WideValue wide_value(union WideValue value);
union Numeric numeric(union Numeric value);
union NumericPair numeric_pair(union NumericPair value);
union Aligned aligned(union Aligned value);
union MixedIS mixed_is(union MixedIS value);
union MixedSI mixed_si(union MixedSI value);
union HiddenLast hidden_last(union HiddenLast value);
struct First first(struct First value);
struct Last last(struct Last value);
struct Array array(struct Array value);
struct Hidden hidden(struct Hidden value);
struct Wide wide(struct Wide value);
struct Large large(struct Large value);
void variadic(int* expected, ...);

#endif
