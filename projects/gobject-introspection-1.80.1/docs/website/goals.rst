=====
Goals
=====

The introspection project has two major goals, and a variety of more minor
ones.


Two level applications - C and <your favorite runtime>
------------------------------------------------------

It makes sense to build many kinds of applications using (at least) two
different levels and languages — one for the low level elements, interfacing
with the OS and/or the hardware; and one for the high level application logic.
C is good for graphics, multimedia, and lower level systems work. However,
writing complex software in C is difficult and error-prone. A managed runtime
such as `JavaScript <https://wiki.gnome.org/JavaScript>`__, Python, Perl,
Java, Lua, .NET, Scheme etc. makes a lot of sense for non-fast-path
application logic such as configuration, layout, dialogs, etc.


.. note::

  To achieve this goal you need to write your code using GObject convention.
  For more information about that, see the `GObject tutorial
  <https://docs.gtk.org/gobject/tutorial.html>`__

Thus, one of the major goals of the GObject introspection project is to be a
convenient bridge between these two worlds, and allow you to choose the right
tool for the job, rather than being limited inside one or the other. With the
introspection project, you can write for example a ClutterActor or GtkWidget
subclass in C, and then without any additional work use that class inside the
high level language of your choice.


Sharing binding infrastructure work, and making the platform even more binding-friendly
---------------------------------------------------------------------------------------

Historically in GNOME, the core platform has been relatively binding-friendly,
but there are several details not captured in the C+GObject layer that
bindings have needed. For example, reference counting semantics and the item
type inside GList's. Up until now various language bindings such as Python,
Mono, java-gnome etc. had duplicated copies of hand-maintained metadata, and
this led to a situation where bindings tended to lag behind until these manual
fixups were done, or were simply wrong, and your application would crash when
calling a more obscure function.

The introspection project solves this by putting all of the metadata inside
the GObject library itself, using annotations in the comments. This will lead
to less duplicate work from binding authors, and a more reliable experience
for binding consumers.

Additionally, because the introspection build process will occur inside the
GObject libraries themselves, a goal is to encourage GObject authors to
consider shaping their APIs to be more binding friendly from the start, rather
than as an afterthought.


Additional goals and uses
-------------------------

* API verification - Sometimes the API of a library in our stack changes by
  accident. Usually by a less experienced developer making a change without
  realizing it will break applications. Introspecting the available API in
  each release of the library and comparing it to the last one makes it easy
  to see what changed
* Documentation tools - The tools written inside of the GObjectIntrospection
  can easily be reused to improve that problem. Essentially; replacing
  gtk-doc. We want to document what we export so it makes sense to glue this
  together with API verification mentioned above
* UI Designer infrastructure
* Serialization/RPC/DBus
