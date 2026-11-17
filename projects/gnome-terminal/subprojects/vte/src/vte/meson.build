# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019 Christian Persch
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

vte_inc = include_directories('.')

libvte_common_public_headers = files(
  'vte.h',
  'vtedeprecated.h',
  'vteenums.h',
  'vteglobals.h',
  'vtemacros.h',
  'vtepty.h',
  'vteregex.h',
  'vteterminal.h',
  'vteuuid.h',
  'vtetypebuiltins.h',
)

libvte_common_enum_headers = files(
  # These files contain enums to be extracted by glib-mkenums
  'vtedeprecated.h',
  'vteenums.h',
)

# Version header

vteversion_conf = configuration_data()
vteversion_conf.set('VTE_MAJOR_VERSION', vte_major_version)
vteversion_conf.set('VTE_MINOR_VERSION', vte_minor_version)
vteversion_conf.set('VTE_MICRO_VERSION', vte_micro_version)

libvte_common_public_headers += configure_file(
  input: 'vteversion.h.in',
  output: '@BASENAME@',
  configuration: vteversion_conf,
  install: false,
)

# Install headers, and create the type builtin files.
# Note that we cannot use gnome.mkenums() to create the type builtins
# files, since we need to install the generated header for both gtk3
# and gtk4, and gnome.mkenums does not work with install_header()
# [https://github.com/mesonbuild/meson/issues/1687]. However, neither does
# custom_target() itself.
# So we need to generate differently-named files for gtk3 and gtk4, and
# install them sepearately, with an extra header that includes the right
# one. And since gnome.mkenums() does not allow specifying the output names
# when using templates, we need to use custom_target() for that.
glib_mkenums = find_program('glib-mkenums')

if get_option('gtk3')

  libvte_gtk3_public_headers = libvte_common_public_headers

  install_headers(
    libvte_gtk3_public_headers,
    subdir: vte_gtk3_api_path
  )

  libvte_gtk3_public_headers += custom_target(
    'vtetypebuiltins-gtk3.h',
    command: [
      glib_mkenums,
      '--output', '@OUTPUT@',
      '--template', meson.current_source_dir() / '..' / 'vtetypebuiltins.h.template',
      '@INPUT@',
    ],
    input: libvte_common_enum_headers,
    install: true,
    install_dir: vte_includedir / vte_gtk3_api_path,
    output: 'vtetypebuiltins-gtk3.h',
  )

  libvte_gtk3_enum_sources = [custom_target(
    'vtetypebuiltins-gtk3.cc',
    command: [
      glib_mkenums,
      '--output', '@OUTPUT@',
      '--template', meson.current_source_dir() / '..' / 'vtetypebuiltins.cc.template',
      '@INPUT@',
    ],
    input: libvte_common_enum_headers,
    install: false,
    output: 'vtetypebuiltins-gtk3.cc',
  ),]
endif

if get_option('gtk4')

  libvte_gtk4_public_headers = libvte_common_public_headers

  install_headers(
    libvte_gtk4_public_headers,
    subdir: vte_gtk4_api_path
  )

  libvte_gtk4_public_headers += custom_target(
    'vtetypebuiltins-gtk4.h',
    command: [
      glib_mkenums,
      '--output', '@OUTPUT@',
      '--template', meson.current_source_dir() / '..' / 'vtetypebuiltins.h.template',
      '@INPUT@',
    ],
    input: libvte_common_enum_headers,
    install: true,
    install_dir: vte_includedir / vte_gtk4_api_path,
    output: 'vtetypebuiltins-gtk4.h',
  )

  libvte_gtk4_enum_sources = [custom_target(
    'vtetypebuiltins-gtk4.cc',
    command: [
      glib_mkenums,
      '--output', '@OUTPUT@',
      '--template', meson.current_source_dir() / '..' / 'vtetypebuiltins.cc.template',
      '@INPUT@',
    ],
    input: libvte_common_enum_headers,
    install: false,
    output: 'vtetypebuiltins-gtk4.cc',
  ),]
endif
