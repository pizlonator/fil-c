# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019, 2020, 2021 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.9.0')

if get_option('gtk3')
  libvte_gtk3_gir_includes = [
    'cairo-1.0',
    'Gdk-3.0',
    'Gtk-3.0',
    'Pango-1.0',
  ]

  libvte_gtk3_gir = gnome.generate_gir(
    libvte_gtk3,
    sources: libvte_gtk3_public_headers + libvte_common_doc_sources,
    includes: libvte_gtk3_gir_includes,
    dependencies: libvte_gtk3_dep,
    extra_args: '-DVTE_COMPILATION',
    nsversion: vte_api_version,
    namespace: 'Vte',
    export_packages: vte_gtk3_api_name,
    header: 'vte' / 'vte.h',
    install: true,
  )
endif

if get_option('gtk4')
  libvte_gtk4_gir_includes = [
    'cairo-1.0',
    'Graphene-1.0',
    'Gsk-4.0',
    'Gdk-4.0',
    'Gtk-4.0',
    'Pango-1.0',
  ]

  # Ideally, the gir would be named something like "VteGtk4" instead,
  # but it seems that's not possible. So work around it using "Vte"
  # as namespace with this nsversion hack:
  vte_gtk4_gir_nsversion = '@0@.@1@'.format(vte_api_major_version + 1, vte_api_minor_version)

  libvte_gtk4_gir = gnome.generate_gir(
    libvte_gtk4,
    dependencies: libvte_gtk4_dep,
    export_packages: vte_gtk4_api_name,
    extra_args: '-DVTE_COMPILATION',
    header: 'vte' / 'vte.h',
    includes: libvte_gtk4_gir_includes,
    install: true,
    namespace: 'Vte',
    nsversion: vte_gtk4_gir_nsversion,
    sources: libvte_gtk4_public_headers + libvte_common_doc_sources,
    symbol_prefix: 'vte',
  )
endif
