=================
Meson Integration
=================

Support for generating GObject introspection data is included in Meson
directly  and accessible through the ``gnome.generate_gir()`` function. See
the `meson documentation
<https://mesonbuild.com/Gnome-module.html#gnomegenerate_gir>`__ for details.

For some real examples, see the meson build definitions of various GNOME
modules:

Pango:
  https://gitlab.gnome.org/GNOME/pango/blob/master/pango/meson.build

  .. code-block:: python

    pango_gir = gnome.generate_gir(
      libpango,
      sources: pango_sources + pango_headers + [ pango_enum_h ],
      namespace: 'Pango',
      nsversion: pango_api_version,
      identifier_prefix: 'Pango',
      symbol_prefix: 'pango',
      export_packages: 'pango',
      includes: [ 'GObject-2.0', 'cairo-1.0', ],
      header: 'pango/pango.h',
      install: true,
      extra_args: gir_args,
    )

json-glib:
  https://gitlab.gnome.org/GNOME/json-glib/blob/master/json-glib/meson.build

  .. code-block:: python

    json_glib_gir = gnome.generate_gir(
      json_lib,
      sources: source_c + source_h + json_glib_enums + [ json_version_h ],
      namespace: 'Json',
      nsversion: json_api_version,
      identifier_prefix: 'Json',
      symbol_prefix: 'json',
      export_packages: json_api_name,
      includes: [ 'GObject-2.0', 'Gio-2.0', ],
      header: 'json-glib/json-glib.h',
      install: true,
      extra_args: gir_args,
    )
