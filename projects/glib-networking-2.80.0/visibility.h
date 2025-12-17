#pragma once

#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(GLIB_NETWORKING_STATIC_COMPILATION)
#  define GLIB_NETWORKING_EXPORT __declspec(dllexport)
#elif __GNUC__ >= 4
#  define GLIB_NETWORKING_EXPORT __attribute__((visibility("default")))
#else
#  define GLIB_NETWORKING_EXPORT
#endif
