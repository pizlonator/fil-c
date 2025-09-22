#ifndef SHADOW_INCLUDE_LIB_ATTR_H_
#define SHADOW_INCLUDE_LIB_ATTR_H_


#include "config.h"


#if defined(__GNUC__)
# define MAYBE_UNUSED                __attribute__((unused))
# define NORETURN                    __attribute__((__noreturn__))
# define format_attr(type, fmt, va)  __attribute__((format(type, fmt, va)))
# define ATTR_ACCESS(...)            __attribute__((access(__VA_ARGS__)))
#else
# define MAYBE_UNUSED
# define NORETURN
# define format_attr(type, fmt, va)
# define ATTR_ACCESS(...)
#endif

#if (__GNUC__ >= 11) && !defined(__clang__)
# define ATTR_MALLOC(deallocator)    [[gnu::malloc(deallocator)]]
#else
# define ATTR_MALLOC(deallocator)
#endif

#if (__GNUC__ >= 14)
# define ATTR_STRING(...)       [[gnu::null_terminated_string_arg(__VA_ARGS__)]]
#else
# define ATTR_STRING(...)
#endif


#endif  // include guard
