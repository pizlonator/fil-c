Title: Initializing GTK

## Library initialization and main loop

Before using GTK, you need to initialize it using [func@Gtk.init]; this
connects to the windowing system, sets up the locale and performs other
initialization tasks. [func@Gtk.init] exits the application if errors occur;
to avoid this, you can use [func@Gtk.init_check], which allows you to recover
from a failed GTK initialization; for instance, you might start up your
application in text mode instead.

Like most GUI toolkits, GTK uses an event-driven programming model. When the
application is doing nothing, GTK sits in the “main loop” and waits for input.
If the user performs some action - say, a mouse click - then the main loop
“wakes up” and delivers an event to GTK. GTK forwards the event to one or
more widgets.

When widgets receive an event, they frequently emit one or more “signals”.
Signals notify your program that "something interesting happened" by invoking
functions you’ve connected to the signal with `g_signal_connect()`. Functions
connected to a signal are often called “callbacks”.

When your callbacks are invoked, you would typically take some action - for
example, when an Open button is clicked you might display a [class@Gtk.FileChooserDialog].
After a callback finishes, GTK will return to the main loop and await more
user input.

### The `main()` function for a simple GTK application

```c
int
main (int argc, char **argv)
{
 GtkWidget *window;
  // Initialize i18n support with bindtextdomain(), etc.

  // ...

  // Initialize the widget set
  gtk_init ();

  // Create the main window
  window = gtk_window_new ();

  // Set up our GUI elements

  // ...

  // Show the application window
  gtk_window_present (GTK_WINDOW (window));

  // Enter the main event loop, and wait for user interaction
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  // The user lost interest
  return 0;
}
```

It's important to note that if you use [class@Gtk.Application], the
application class will take care of initializing GTK for you, as well
as spinning the main loop.

### See also

- the GLib manual, especially `GMainLoop`
- signal-related functions, such as `g_signal_connect()` in GObject
