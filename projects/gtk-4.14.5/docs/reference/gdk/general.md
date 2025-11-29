Title: Library initialization and versioning

The GDK and GTK headers annotate deprecated APIs in a way that produces
compiler warnings if these deprecated APIs are used. The warnings
can be turned off by defining the macro `GDK_DISABLE_DEPRECATION_WARNINGS`
before including the `gdk.h` header.

GDK and GTK also provide support for building applications against defined
subsets of deprecated or new APIs. You can define the macro
`GDK_VERSION_MIN_REQUIRED` to specify up to what version you want to receive
warnings about deprecated APIs; and the macro `GDK_VERSION_MAX_ALLOWED` to
specify the newest version whose API you want to use. If you attempt to use
a function deprecated before the version of GTK specified in
`GDK_VERSION_MIN_REQUIRED`, or a function introduced after the version of
GTK specified in `GDK_VERSION_MAX_ALLOWED`, the compiler will warn you when
building your code.
