# Copyright © 2019, 2020, 2021 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

public_icons_themes = [
  'hicolor',
]

public_icons_sources = [
  'hicolor_apps_scalable_org.gnome.Terminal.svg',
  'hicolor_apps_scalable_org.gnome.Terminal.Preferences.svg',
  'hicolor_apps_symbolic_org.gnome.Terminal-symbolic.svg',
  'hicolor_apps_symbolic_org.gnome.Terminal.Preferences-symbolic.svg',
]

public_icons_renames = []

foreach icon : public_icons_sources
  components = icon.split('_')
  public_icons_renames += components[0] / components[2] / components[1] / components[3]
endforeach

icondir = gt_datadir / 'icons'
pkgicondir = gt_pkgdatadir / 'icons'

install_data(
  sources: public_icons_sources,
  rename: public_icons_renames,
  install_dir: icondir,
  install_tag: 'runtime',
)

foreach theme : public_icons_themes
  meson.add_install_script(
    'meson_updateiconcache.py',
    icondir / theme,
  )
endforeach
