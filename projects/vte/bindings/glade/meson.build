# Copyright © 2020 Aleksandr Mezin
# Copyright © 2020 Christian Persch
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

gladedir = vte_datadir / 'glade'
cataloguedir =  gladedir / 'catalogs'
pixmapdir = gladedir / 'pixmaps'

catalog_conf = configuration_data()
catalog_conf.set('VTE_API_VERSION', vte_api_version)
catalog_conf.set('VERSION', vte_version)

configure_file(
  input: 'vte.xml.in',
  configuration: catalog_conf,
  install_dir: cataloguedir,
  output: vte_gtk3_api_name + '.xml',
)

install_data(
  'hicolor_actions_16x16_widget-vte-terminal.png',
  install_dir: pixmapdir / 'hicolor' / '16x16' / 'actions',
  rename: 'widget-vte-terminal.png',
)

install_data(
  'hicolor_actions_22x22_widget-vte-terminal.png',
  install_dir: pixmapdir / 'hicolor' / '22x22' / 'actions',
  rename: 'widget-vte-terminal.png',
)
