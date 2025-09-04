int an_int;
char *a_char_ptr;
typedef int (*a_typedef) (int main);
struct struct_forward;
enum enum_forward;
union union_forward;
typedef int an_array[50];
struct a_struct { int foo; };
union a_union { int bar; };
enum an_enum { FOO };

a_typedef a;
struct struct_forward *x;
union union_forward *y;
enum enum_forward *z;
struct a_struct *xx;
union a_union *yy;
enum an_enum *zz;
an_array ar;
