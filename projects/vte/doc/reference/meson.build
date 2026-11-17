# Copyright © 2021, 2022 Christian Persch
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

assert(get_option('gir'), 'Introspection must be enabled to build docs')

gidocgen = find_program('gi-docgen', required: true)

doc_conf = configuration_data()
doc_conf.set('version', meson.project_version())

doc_sources = [
  'licence.md',
]

docdir = vte_datadir / 'doc'

if get_option('gtk3')

  libvte_gtk3_docs_toml = configure_file(
    configuration: doc_conf,
    input: 'vte-gtk3.toml.in',
    install: true,
    install_dir: docdir / vte_gtk3_api_name,
    output: vte_gtk3_api_name + '.toml',
  )

  libvte_gtk3_docs = custom_target(
    vte_gtk3_api_name,
    command: [
      gidocgen,
      'generate',
      '--config', '@INPUT0@',
      '--content-dir', meson.current_source_dir(),
      '--fatal-warnings',
      '--no-namespace-dir',
      '--output-dir', '@OUTPUT@',
      '--quiet',
      '@INPUT1@',
    ],
    depend_files: [doc_sources,],
    input: [
      libvte_gtk3_docs_toml,
      libvte_gtk3_gir[0],
    ],
    install: true,
    install_dir: docdir,
    output: vte_gtk3_api_name,
  )

endif # gtk3

if get_option('gtk4')

  libvte_gtk4_docs_toml = configure_file(
    configuration: doc_conf,
    input: 'vte-gtk4.toml.in',
    install: true,
    install_dir: docdir / vte_gtk4_api_name,
    output: vte_gtk4_api_name + '.toml',
  )

  libvte_gtk4_docs = custom_target(
    vte_gtk4_api_name,
    command: [
      gidocgen,
      'generate',
      '--config', '@INPUT0@',
      '--content-dir', meson.current_source_dir(),
      '--fatal-warnings',
      '--no-namespace-dir',
      '--output-dir', '@OUTPUT@',
      '--quiet',
      '@INPUT1@',
    ],
    depend_files: [doc_sources,],
    input: [
      libvte_gtk4_docs_toml,
      libvte_gtk4_gir[0],
    ],
    install: true,
    install_dir: docdir,
    output: vte_gtk4_api_name,
  )

endif # gtk4
