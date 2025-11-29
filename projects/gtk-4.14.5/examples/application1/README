Step 1: A trivial application

To make gnome-shell use the desktop file and icon for this example
while running it uninstalled, do the following:

mkdir -p ~/.local/share/applications
sed -e "s#@bindir@#$PWD#" org.gtk.exampleapp.desktop \
  > ~/.local/share/applications/org.gtk.exampleapp.desktop

mkdir -p ~/.local/share/icons/hicolor/48x48/apps
cp exampleapp.png ~/.local/share/icons/hicolor/48x48/apps
