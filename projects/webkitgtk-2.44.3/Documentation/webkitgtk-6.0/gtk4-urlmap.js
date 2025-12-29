const baseURLApiLevelSuffix = (function (uri) {
  const slashPos = uri.lastIndexOf("/");
  const dashPos = uri.lastIndexOf("-", slashPos);
  return uri.substring(dashPos, slashPos + 1);
})(window.location.href);
baseURLs = [
  ["GLib", "https://docs.gtk.org/glib/"],
  ["GObject", "https://docs.gtk.org/gobject/"],
  ["Gio", "https://docs.gtk.org/gio/"],
  ["Gdk", "https://docs.gtk.org/gdk4/"],
  ["Gtk", "https://docs.gtk.org/gtk4/"],
  ["JavaScriptCore", "../javascriptcoregtk" + baseURLApiLevelSuffix],
  ["Soup", "https://libsoup.org/libsoup-3.0/"],
  ["WebKit", "../webkitgtk" + baseURLApiLevelSuffix],
  ["WebKitWebProcessExtension", "../webkitgtk-web-process-extension" + baseURLApiLevelSuffix]
]
