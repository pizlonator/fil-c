/* Overload resolution builds temporary call expressions with no arguments
   (CallExpr::CreateTemporary) when it tries a conversion function.  For a
   conversion function with an explicit object parameter, CallExpr::getBeginLoc
   used to assert that the first argument exists.  Binding a const int& to an
   S object through S's explicit-object conversion function used to crash the
   compiler; it must compile and run instead. */

#include <stdio.h>

struct S {
    int value;
    operator int(this S& self) { return self.value; }
};

int main(void)
{
    S s;
    s.value = 42;
    const int& r = s;
    if (r != 42)
        return 1;
    printf("explicitobjtempcall ok\n");
    return 0;
}
