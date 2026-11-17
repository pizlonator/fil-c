/* Stub to replace gnome-settings-daemon's
 * gnome-settings-bus.h helpers */

/* Fil-C: the pizlonated gtk4 is built without the X11 backend
 * (-D x11-backend=false; pizlix is Wayland-only), so gdk/x11/gdkx.h does not
 * exist.  With GDK_WINDOWING_WAYLAND defined (always, in the pizlonated
 * gtk4) every display is a Wayland display. */

#ifdef GDK_WINDOWING_WAYLAND

static inline gboolean
gnome_settings_is_wayland (void)
{
  return TRUE;
}

#endif /* GDK_WINDOWING_WAYLAND */
