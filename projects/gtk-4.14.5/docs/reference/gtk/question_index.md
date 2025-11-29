Title: Common Questions
Slug: gtk-question-index

This is an "index" of the reference manual organized by common "How do
I..." questions. If you aren't sure which documentation to read for
the question you have, this list is a good place to start.

## General Questions

*  How do I get started with GTK?

    The GTK [website](https://www.gtk.org) offers some
    [tutorials](https://www.gtk.org/documentation.php) and other documentation
    This reference manual also contains a introductory
    [Getting Started](#gtk-getting-started) part.

    More documentation ranging from whitepapers to online books can be found at
    the [GNOME developer's site](https://developer.gnome.org). After studying these
    materials you should be well prepared to come back to this reference manual for details.

*  Where can I get help with GTK, submit a bug report, or make a feature request?

    See the [documentation](#gtk-resources) on this topic.

*  How do I port from one GTK version to another?

    Every major version of GTK comes with a [migration guide](#migrating). You may also
    find useful information in the documentation for specific widgets and functions. If
    you have a question not covered in the manual, feel free to ask, and please
    [file a bug report](https://gitlab.gnome.org/GNOME/gtk/issues/) against the
    documentation.

*  Should I maintain parallel versions of my UI in GTK x and GTK y?

    At the end of the day, that is up to you.

    Our experience is that it is a lot of work, and usually not a good idea.

    If you are not ready to make the jump to the next major version of GTK,
    it is perfectly fine to stick with the stable release. We maintain them
    for that reason.

*  How does memory management work in GTK? Should I free data returned from functions?

    See the documentation for `GObject` and `GInitiallyUnowned`. For `GObject` note
    specifically `g_object_ref()` and `g_object_unref()`. `GInitiallyUnowned` is a
    subclass of `GObject` so the same points apply, except that it has a "floating"
    state (explained in its documentation).

    In a widget tree, each container owns a reference to its children. The root
    object (typically a `GtkWindow`) is owned by GTK. GTK will drop its reference
    when you call [method@Gtk.Window.destroy].

    For strings returned from functions, they will be declared "const" if they should
    not be freed. Non-const strings should be freed with `g_free()`. Arrays follow the
    same rule. If you find an undocumented exception to the rules, please
    [file a bug report.](https://gitlab.gnome.org/GNOME/gtk/issues/).

    The transfer annotations for gobject-introspection that are part of the
    documentation can provide useful hints for memory handling semantics as well.

*  Why does my program leak memory, if I destroy a widget immediately
    after creating it?

    If `GtkFoo` isn't a toplevel window, then

        foo = gtk_foo_new ();
        g_object_unref (foo);

    is a memory leak, because no one assumed the initial floating reference
    (you will get a warning about this too). If you are using a widget and
    you aren't immediately packing it into a container, then you probably
    want standard reference counting, not floating reference counting.

    To get this, you must acquire a reference to the widget and drop the
    floating reference (_ref and sink_ in `GObject` parlance) after creating it:

        foo = gtk_foo_new ();
        g_object_ref_sink (foo);

    When you immediately add a widget to a container, it takes care of assuming
    the initial floating reference and you don't have to worry about reference
    counting at all ... just remove the widget from the container to get rid of it.

*  How do I use GTK with threads?

    GTK requires that all GTK API calls are made from the same thread in which
    the `GtkApplication` was created, or `gtk_init()` was called (the _main thread_).

    If you want to take advantage of multi-threading in a GTK application,
    it is usually best to send long-running tasks to worker threads, and feed
    the results back to the main thread using `g_idle_add()` or `GAsyncQueue`. GIO
    offers useful tools for such an approach such as `GTask`.

*  How do I internationalize a GTK program?

    Most people use [GNU gettext](https://www.gnu.org/software/gettext/),
    already required in order to install GLib. On a UNIX or Linux system with
    gettext installed, type `info gettext` to read the documentation.

    The short checklist on how to use gettext is: call bindtextdomain() so
    gettext can find the files containing your translations, call textdomain()
    to set the default translation domain, call bind_textdomain_codeset() to
    request that all translated strings are returned in UTF-8, then call
    gettext() to look up each string to be translated in the default domain.

    `gi18n.h` provides the following shorthand macros for convenience.
    Conventionally, people define macros as follows for convenience:

        #define  _(x)     gettext (x)
        #define N_(x)     x
        #define C_(ctx,x) pgettext (ctx, x)

    You use `N_()` (N stands for no-op) to mark a string for translation in
    a location where a function call to gettext() is not allowed, such as
    in an array initializer. You eventually have to call gettext() on the
    string to actually fetch the translation. `_()` both marks the string for
    translation and actually translates it. The `C_()` macro (C stands for
    context) adds an additional context to the string that is marked for
    translation, which can help to disambiguate short strings that might
    need different translations in different parts of your program.

    Code using these macros ends up looking like this:

        #include <gi18n.h>

        static const char *global_variable = N_("Translate this string");

        static void
        make_widgets (void)
        {
          GtkWidget *label1;
          GtkWidget *label2;

          label1 = gtk_label_new (_("Another string to translate"));
          label2 = gtk_label_new (_(global_variable));
        ...

    Libraries using gettext should use dgettext() instead of gettext(),
    which allows them to specify the translation domain each time they
    ask for a translation. Libraries should also avoid calling textdomain(),
    since they will be specifying the domain instead of using the default.

    With the convention that the macro `GETTEXT_PACKAGE` is defined to hold
    your libraries translation domain, `gi18n-lib.h` can be included to provide
    the following convenience:

        #define _(x) dgettext (GETTEXT_PACKAGE, x)

*  How do I use non-ASCII characters in GTK programs ?

    GTK uses [Unicode](http://www.unicode.org) (more exactly UTF-8) for all text.
    UTF-8 encodes each Unicode codepoint as a sequence of one to six bytes and
    has a number of nice properties which make it a good choice for working with
    Unicode text in C programs:

    - ASCII characters are encoded by their familiar ASCII codepoints.
    - ASCII characters never appear as part of any other character.
    - The zero byte doesn't occur as part of a character, so that UTF-8
      string can be manipulated with the usual C library functions for
      handling zero-terminated strings.

    More information about Unicode and UTF-8 can be found in the
    [UTF-8 and Unicode FAQ](https://www.cl.cam.ac.uk/~mgk25/unicode.html).
    GLib provides functions for converting strings between UTF-8 and other
    encodings, see g_locale_to_utf8() and g_convert().

    Text coming from external sources (e.g. files or user input), has to be
    converted to UTF-8 before being handed over to GTK. The following example
    writes the content of a IS0-8859-1 encoded text file to `stdout`:

        char *text, *utf8_text;
        gsize length;
        GError *error = NULL;

        if (g_file_get_contents (filename, &amp;text, &amp;length, NULL))
          {
            utf8_text = g_convert (text, length, "UTF-8", "ISO-8859-1",
                                   NULL, NULL, &error);
            if (error != NULL)
              {
                fprintf ("Couldn't convert file %s to UTF-8\n", filename);
                g_error_free (error);
              }
            else
              g_print (utf8_text);
          }
        else
          fprintf (stderr, "Unable to read file %s\n", filename);

    For string literals in the source code, there are several alternatives
    for handling non-ASCII content:

    - Direct UTF-8

      If your editor and compiler are capable of handling UTF-8 encoded
      sources, it is very convenient to simply use UTF-8 for string literals,
      since it allows you to edit the strings in "wysiwyg". Note that choosing
      this option may reduce the portability of your code.

    - Escaped UTF-8

      Even if your toolchain can't handle UTF-8 directly, you can still
      encode string literals in UTF-8 by using octal or hexadecimal escapes
      like `\212` or `\xa8` to encode each byte. This is portable, but
      modifying the escaped strings is not very convenient. Be careful when
      mixing hexadecimal escapes with ordinary text; `"\xa8abcd"`  is a string
      of length 1 !

    - Runtime conversion

      If the string literals can be represented in an encoding which your
      toolchain can handle (e.g. IS0-8859-1), you can write your source
      files in that encoding and use g_convert() to convert the strings
      to UTF-8 at runtime. Note that this has some runtime overhead, so
      you may want to move the conversion out of inner loops.

    Here is an example showing the three approaches using the copyright
    sign © which has Unicode and ISO-8859-1 codepoint 169 and is represented
    in UTF-8 by the two bytes 194, 169, or `"\302\251"` as a string literal:

        g_print ("direct UTF-8: ©");
        g_print ("escaped UTF-8: \302\251");
        text = g_convert ("runtime conversion: ©", -1,
                          "ISO-8859-1", "UTF-8", NULL, NULL, NULL);
        g_print (text);
        g_free (text);

    If you are using gettext() to localize your application, you need
    to call bind_textdomain_codeset() to ensure that translated strings
    are returned in UTF-8 encoding.

*  How do I use GTK with C++?

    There are two ways to approach this. The GTK header files use the subset
    of C that's also valid C++, so you can simply use the normal GTK API
    in a C++ program. Alternatively, you can use a "C++ binding" such as
    [gtkmm](https://www.gtkmm.org/) which provides a native C++ API.

    When using GTK directly, keep in mind that only functions can be
    connected to signals, not methods. So you will need to use global
    functions or "static" class functions for signal connections.

    Another common issue when using GTK directly is that C++ will not
    implicitly convert an integer to an enumeration. This comes up when
    using bitfields; in C you can write the following code:

        gdk_surface_set_events (gdk_surface,
                                GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    while in C++ you must write:

        gdk_surface_set_events (gdk_surface,
                                (GdkEventMask) GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    There are very few functions that require this cast, however.

*  How do I use GTK with other non-C languages?

    See the list of [language bindings](https://www.gtk.org/language-bindings.php)
    on the GTK [website](https://www.gtk.org).

*  How do I load an image or animation from a file?

    To load an image file straight into a display widget, use
    [ctor@Gtk.Picture.new_for_file] or [ctor@Gtk.Picture.new_for_filename].
    To load an image for another purpose, use [ctor@Gdk.Texture.new_from_file].
    To load a video from a file, use [ctor@Gtk.MediaFile.new_for_file].

    Note that [class@Gtk.Image] is meant for fixed-size icons. For arbitrary
    image files, you should use [class@Gtk.Picture].

*  How do I draw text?

    If you just want to put text into your user interface somewhere, it is
    usually easiest to just use one of ready-made widgets for this purpose,
    such as [class@Gtk.Label].

    To draw a piece of text onto a cairo surface, use a Pango layout and
    [func@PangoCairo.show_layout].

        layout = gtk_widget_create_pango_layout (widget, text);
        fontdesc = pango_font_description_from_string ("Luxi Mono 12");
        pango_layout_set_font_description (layout, fontdesc);
        pango_cairo_show_layout (cr, layout);
        pango_font_description_free (fontdesc);
        g_object_unref (layout);

    See also the [Cairo Rendering](https://docs.gtk.org/PangoCairo/pango_cairo.html)
    section of the [Pango documentation](https://docs.gtk.org/Pango/).

    To draw a piece of text in a widget [vfunc@Gtk.Widget.snapshot] implementation,
    use [method@Gtk.Snapshot.append_layout].

*  How do I measure the size of a piece of text?

    To obtain the size of a piece of text, use a Pango layout and
    [method@Pango.Layout.get_pixel_size], using code like the following:

        layout = gtk_widget_create_pango_layout (widget, text);
        fontdesc = pango_font_description_from_string ("Luxi Mono 12");
        pango_layout_set_font_description (layout, fontdesc);
        pango_layout_get_pixel_size (layout, &amp;width, &amp;height);
        pango_font_description_free (fontdesc);
        g_object_unref (layout);

    See also the [Layout Objects](https://docs.gtk.org/Pango/class.Layout.html)
    section of the [Pango documentation](https://docs.gtk.org/Pango/).

*  Why are types not registered if I use their `GTK_TYPE_BLAH` macro?

    The %GTK_TYPE_BLAH macros are defined as calls to gtk_blah_get_type(), and
    the `_get_type()` functions are declared as %G_GNUC_CONST which allows the
    compiler to optimize the call away if it appears that the value is not
    being used.

    GLib provides the `g_type_ensure()` function to work around this problem.

        g_type_ensure (GTK_TYPE_BLAH);

*  How do I create a transparent toplevel window?

    Any toplevel window can be transparent. It is just a matter of setting a
    transparent background in the CSS style for it.

## Which widget should I use...

*  ...for lists and trees?

    This question has different answers, depending on the size of the dataset
    and the required formatting flexibility.

    If you want to display a large amount of data in a uniform way, your best
    option is a [class@Gtk.ListView] widget. The list view can have different
    types of models: [class@Gtk.TreeListModel] can serve as a model for a tree
    structure, while a simple [iface@Gio.ListModel] can be used for simple lists.
    The section [List Widget Overview](section-list-widget.html) helps you get started.
    It replaces [class@Gtk.TreeView], which has been deprecated since GTK 4.10.

    If you want to display a small amount of items, but need flexible formatting
    and widgetry inside the list, then you probably want to use a [class@Gtk.ListBox],
    which uses regular widgets for display.

*  ...for multi-line text display or editing?

    See the [text widget overview](#TextWidget) -- you should use the
    [class@Gtk.TextView] widget.

    If you only have a small amount of text, [class@Gtk.Label] may also be appropriate
    of course. It can be made selectable with [method@Gtk.Label.set_selectable]. For a
    single-line text entry, see [class@Gtk.Entry].

*  ...to display an image or animation?

    GTK has two widgets that are dedicated to displaying images. [class@Gtk.Image], for
    small, fixed-size icons and [class@Gtk.Picture] for content images.

    Both can display images in just about any format GTK understands.
    You can also use [class@Gtk.DrawingArea] if you need to do something more complex,
    such as draw text or graphics over the top of the image.

    Both [class@Gtk.Image] and [class@Gtk.Picture] can display animations and videos as well.
    To show an webm file, load it with the [class@Gtk.MediaFile] API and then use
    it as a paintable:

        mediafile = gtk_media_file_new_for_filename ("example.webm");
        picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (mediafile));

*  ...for presenting a set of mutually-exclusive choices, where Windows
    would use a combo box?

    With GTK, a [class@Gtk.DropDown] is the recommended widget to use for this use case.

## Questions about GtkWidget

*  How do I change the color of a widget?

    The background color of a widget is determined by the CSS style that applies
    to it. To change that, you can set style classes on the widget, and provide
    custom CSS to change the appearance. Such CSS can be loaded with
    [method@Gtk.CssProvider.load_from_file] and its variants.
    See [method@Gtk.StyleContext.add_provider].

*  How do I change the font of a widget?

    If you want to make the text of a label larger, you can use
    gtk_label_set_markup():

        gtk_label_set_markup (label, "<big>big tex</big>");

    This is preferred for many apps because it's a relative size to the
    user's chosen font size. See `g_markup_escape_text()` if you are
    constructing such strings on the fly.

    You can also change the font of a widget by putting

        .my-widget-class {
          font: Sans 30;
        }

    in a CSS file, loading it with [method@Gtk.CssProvider.load_from_file], and
    adding the provider with [func@Gtk.StyleContext.add_provider_for_display].
    To associate this style information with your widget, set a style class
    on the widget using [method@Gtk.Widget.add_css_class]. The advantage
    of this approach is that users can then override the font you have chosen.
    See the `GtkStyleContext` documentation for more discussion.

*  How do I disable/ghost/desensitize a widget?

    In GTK a disabled widget is termed _insensitive_.
    See [method@Gtk.Widget.set_sensitive].

## GtkTextView questions

*  How do I get the contents of the entire text widget as a string?

    See [method@Gtk.TextBuffer.get_bounds] and [method@Gtk.TextBuffer.get_text]
    or [method@Gtk.TextIter.get_text].

        GtkTextIter start, end;
        GtkTextBuffer *buffer;
        char *text;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_bounds (buffer, &start, &end);
        text = gtk_text_iter_get_text (&start, &end);
        /* use text */
        g_free (text);

*  How do I make a text widget display its complete contents in a specific font?

    If you use [method@Gtk.TextBuffer.insert_with_tags] with appropriate tags to
    select the font, the inserted text will have the desired appearance, but
    text typed in by the user before or after the tagged block will appear in
    the default style.

*  How do I make a text view scroll to the end of the buffer automatically ?

    A good way to keep a text buffer scrolled to the end is to place a
    [mark](#GtkTextMark) at the end of the buffer, and give it right gravity.
    The gravity has the effect that text inserted at the mark gets inserted
    *before*, keeping the mark at the end.

    To ensure that the end of the buffer remains visible, use
    [method@Gtk.TextView.scroll_to_mark] to scroll to the mark after
    inserting new text.

    The gtk4-demo application contains an example of this technique.

## GtkTreeView questions

*  How do I associate some data with a row in the tree?

    Remember that the [iface@Gtk.TreeModel] columns don't necessarily have to be
    displayed. So you can put non-user-visible data in your model just
    like any other data, and retrieve it with [method@Gtk.TreeModel.get].
    See the [tree widget overview](#TreeWidget).

*  How do I put an image and some text in the same column?

    You can pack more than one [class@Gtk.CellRenderer] into a single [class@Gtk.TreeViewColumn]
    using [method@Gtk.TreeViewColumn.pack_start] or [method@Gtk.TreeViewColumn.pack_end].
    So pack both a [class@Gtk.CellRendererPixbuf] and a [class@Gtk.CellRendererText] into the
    column.

*  I can set data easily on my [class@Gtk.TreeStore] or [class@Gtk.ListStore] models using
    [method@Gtk.ListStore.set] and [method@Gtk.TreeStore.set], but can't read it back?

    Both the [class@Gtk.TreeStore] and the [class@Gtk.ListStore] implement the [iface@Gtk.TreeModel]
    interface. As a consequence, you can use any function this interface
    implements. The easiest way to read a set of data back is to use
    [method@Gtk.TreeModel.get].

*  How do I change the way that numbers are formatted by `GtkTreeView`?

    Use [method@Gtk.TreeView.insert_column_with_data_func] or
    [method@Gtk.TreeViewColumn.set_cell_data_func] and do the conversion
    from number to string yourself (with, say, `g_strdup_printf()`).

    The following example demonstrates this:

        enum
        {
          DOUBLE_COLUMN,
          N_COLUMNS
        };

        GtkListStore *mycolumns;

        GtkTreeView *treeview;

        void
        my_cell_double_to_text (GtkTreeViewColumn *tree_column,
	                            GtkCellRenderer   *cell,
                                GtkTreeModel      *tree_model,
	                            GtkTreeIter       *iter,
                                gpointer           data)
        {
          GtkCellRendererText *cell_text = (GtkCellRendererText *)cell;
          double d;
          char *text;

          /* Get the double value from the model. */
          gtk_tree_model_get (tree_model, iter, (int)data, &d, -1);
          /* Now we can format the value ourselves. */
          text = g_strdup_printf ("%.2f", d);
          g_object_set (cell, "text", text, NULL);
          g_free (text);
        }

        void
        set_up_new_columns (GtkTreeView *myview)
        {
          GtkCellRendererText *renderer;
          GtkTreeViewColumn *column;
          GtkListStore *mycolumns;

          /* Create the data model and associate it with the given TreeView */
          mycolumns = gtk_list_store_new (N_COLUMNS, G_TYPE_DOUBLE);
          gtk_tree_view_set_model (myview, GTK_TREE_MODEL (mycolumns));

          /* Create a GtkCellRendererText */
          renderer = gtk_cell_renderer_text_new ();

          /* Create a new column that has a title ("Example column"),
           * uses the above created renderer that will render the double
           * value into text from the associated model's rows.
           */
          column = gtk_tree_view_column_new ();
          gtk_tree_view_column_set_title  (column, "Example column");
          renderer = gtk_cell_renderer_text_new ();
          gtk_tree_view_column_pack_start (column, renderer, TRUE);

          /* Append the new column after the GtkTreeView's previous columns. */
          gtk_tree_view_append_column (GTK_TREE_VIEW (myview), column);
          /* Since we created the column by hand, we can set it up for our
           * needs, e.g. set its minimum and maximum width, etc.
           */
          /* Set up a custom function that will be called when the column content
           * is rendered. We use the func_data pointer as an index into our
           * model. This is convenient when using multi column lists.
           */
          gtk_tree_view_column_set_cell_data_func (column, renderer,
	                                               my_cell_double_to_text,
                                                   (gpointer)DOUBLE_COLUMN, NULL);
        }

*  How do I hide the expander arrows in my tree view?

    Set the expander-column property of the tree view to a hidden column.
    See [method@Gtk.TreeView.set_expander_column] and [method@Gtk.TreeViewColumn.set_visible].

## Using cairo with GTK

*  How do I use cairo to draw in GTK applications?

    [class@Gtk.DrawingArea] is a ready-made widget for drawing with cairo.

    If you implement a custom widget, use [method@Gtk.Snapshot.append_cairo]
    in your [vfunc@Gtk.Widget.snapshot] vfunc to obtain a cairo context and draw with that.

*  Can I improve the performance of my application by using another backend
    of cairo (such as GL)?

    No. Most drawing in GTK is not done via cairo anymore (but instead
    by the GL or Vulkan renderers of GSK).

    If you use cairo for drawing your own widgets, [method@Gtk.Snapshot.append_cairo]
    will choose the most appropriate surface type for you.

    If you are interested in using GL for your own drawing, see [class@Gtk.GLArea].

*  Can I use cairo to draw on a `GdkPixbuf`?

    No. The cairo image surface does not support the pixel format used by `GdkPixbuf`.

    If you need to get cairo drawing into a format that can be displayed efficiently
    by GTK, you may want to use an image surface and [ctor@Gdk.MemoryTexture.new].
