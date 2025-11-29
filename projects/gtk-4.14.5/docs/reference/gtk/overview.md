Title: Overview

GTK is a library for creating graphical user interfaces. It works on many
UNIX-like platforms, Windows, and macOS. GTK is released under the terms of
the [GNU Library General Public License][gnu-lgpl], which allows for flexible
licensing of client applications. GTK has a C-based, object-oriented
architecture that allows for maximum flexibility and portability; there are
bindings for many other languages, including C++, Objective-C, Guile/Scheme, Perl,
Python, JavaScript, Rust, Go, TOM, Ada95, Free Pascal, and Eiffel.

The GTK toolkit contains "widgets": GUI components such as buttons, text
input, or windows.

GTK depends on the following libraries:

 - **GLib**: a general-purpose utility library, not specific to graphical
   user interfaces. GLib provides many useful data types, macros, type
   conversions, string utilities, file utilities, a main loop abstraction,
   and so on. More information available on the [GLib website][glib].
 - **GObject**: A library that provides a type system, a collection of
   fundamental types including an object type, and a signal system. More
   information available on the [GObject website][gobject].
 - **GIO**: A modern, easy-to-use VFS API including abstractions for files,
   drives, volumes, stream IO, as well as network programming and IPC though
   DBus. More information available on the [GIO website][gio].
 - **Cairo**: Cairo is a 2D graphics library with support for multiple
   output devices. More information available on the [Cairo website][cairo].
 - **OpenGL**: OpenGL is the premier environment for developing portable,
   interactive 2D and 3D graphics applications. More information available
   on the [Khronos website][opengl].
 - **Vulkan**: Vulkan is the a newer graphics API, that can be considered
   the successor of OpenGL.  More information available on the
   [Khronos website][vulkan].
 - **Pango**: Pango is a library for internationalized text handling. It
   centers around the `PangoLayout` object, representing a paragraph of
   text.  Pango provides the engine for `GtkTextView`, `GtkLabel`,
   `GtkEntry`, and all GTK widgets that display text. More information
   available on the [Pango website][pango].
 - **gdk-pixbuf**: A small, portable library which allows you to create
   `GdkPixbuf` ("pixel buffer") objects from image data or image files. You
   can use `GdkPixbuf` in combination with widgets like `GtkImage` to
   display images. More information available on the
   [gdk-pixbuf website][gdkpixbuf].
 - **graphene**: A small library which provides vector and matrix
   datatypes and operations. Graphene provides optimized implementations
   using various SIMD instruction sets such as SSE and ARM NEON. More
   information available on the [Graphene website][graphene]

GTK is divided into three parts:

 - **GDK**: GDK is the abstraction layer that allows GTK to support multiple
   windowing systems. GDK provides window system facilities on Wayland, X11,
   Microsoft Windows, and Apple macOS.
 - **GSK**: GSK is an API for creating a scene graph from drawing operation,
   called "nodes", and rendering it using different backends. GSK provides
   renderers for OpenGL, Vulkan and Cairo.
 - **GTK**: The GUI toolkit, containing UI elements, layout managers, data
   storage types for efficient use in GUI applications, and much more.

[gnu-lgpl]: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html
[glib]: https://docs.gtk.org/glib/
[gobject]: https://docs.gtk.org/gobject/
[gio]: https://docs.gtk.org/gio/
[cairo]: https://www.cairographics.org/manual/
[opengl]: https://www.opengl.org/about/
[vulkan]: https://www.vulkan.org/
[pango]: https://docs.gtk.org/pango/
[gdkpixbuf]: https://docs.gtk.org/gdk-pixbuf/
[graphene]: https://ebassi.github.io/graphene/
