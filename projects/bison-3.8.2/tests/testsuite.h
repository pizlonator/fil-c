/* Common definitions to include in the generated parsers.  See
   AT_DATA_SOURCE_PROLOGUE. */

/* In C++ pre C++11 it is standard practice to use 0 for the null
   pointer.  But GCC -std=c++98 with -Wzero-as-null-pointer-constant
   warns about this, although this is just no alternative!  Warning
   introduced in GCC 4.7. */
#if defined __cplusplus && __cplusplus < 201103L
# if defined __GNUC__ && ! defined __clang__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
#  pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
# endif
#endif
