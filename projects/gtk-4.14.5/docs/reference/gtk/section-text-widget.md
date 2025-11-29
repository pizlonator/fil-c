Title: Text Widget Overview
Slug: gtk-textview

GTK has an extremely powerful framework for multiline text editing.  The
primary objects involved in the process are `GtkTextBuffer`, which represents the
text being edited, and `GtkTextView`, a widget which can display a `GtkTextBuffer`.
Each buffer can be displayed by any number of views.

One of the important things to remember about text in GTK is that it's in
the UTF-8 encoding. This means that one character can be encoded as multiple
bytes. Character counts are usually referred to as _offsets_, while byte
counts are called _indexes_. If you confuse these two, things will work fine
with ASCII, but as soon as your buffer contains multibyte characters, bad
things will happen.

Text in a buffer can be marked with _tags_. A tag is an attribute that can
be applied to some range of text. For example, a tag might be called "bold"
and make the text inside the tag bold. However, the tag concept is more
general than that; tags don't have to affect appearance. They can instead
affect the behavior of mouse and key presses, "lock" a range of text so the
user can't edit it, or countless other things. A tag is represented by a
`GtkTextTag` object. One `GtkTextTag` can be applied to any number of text
ranges in any number of buffers.

Each tag is stored in a `GtkTextTagTable`. A tag table defines a set of
tags that can be used together. Each buffer has one tag table associated with
it; only tags from that tag table can be used with the buffer. A single tag
table can be shared between multiple buffers, however.

Tags can have names, which is convenient sometimes (for example, you can name
your tag that makes things bold "bold"), but they can also be anonymous (which
is convenient if you're creating tags on-the-fly).

Most text manipulation is accomplished with _iterators_, represented by a
`GtkTextIter`. An iterator represents a position between two characters in
the text buffer. `GtkTextIter` is a struct designed to be allocated on the
stack; it's guaranteed to be copiable by value and never contain any
heap-allocated data. Iterators are not valid indefinitely; whenever the
buffer is modified in a way that affects the number of characters in the
buffer, all outstanding iterators become invalid. (Note that deleting 5
characters and then reinserting 5 still invalidates iterators, though you
end up with the same number of characters you pass through a state with a
different number).

Because of this, iterators can't be used to preserve positions across buffer
modifications. To preserve a position, the `GtkTextMark` object is ideal. You
can think of a mark as an invisible cursor or insertion point; it floats in
the buffer, saving a position. If the text surrounding the mark is deleted,
the mark remains in the position the text once occupied; if text is inserted
at the mark, the mark ends up either to the left or to the right of the new
text, depending on its _gravity_. The standard text cursor in left-to-right
languages is a mark with right gravity, because it stays to the right of
inserted text.

Like tags, marks can be either named or anonymous. There are two marks
built-in to `GtkTextBuffer`; these are named "insert" and "selection_bound"
and refer to the insertion point and the boundary of the selection which
is not the insertion point, respectively. If no text is selected, these
two marks will be in the same position. You can manipulate what is selected
and where the cursor appears by moving these marks around.

If you want to place the cursor in response to a user action, be sure to use
gtk_text_buffer_place_cursor(), which moves both at once without causing a
temporary selection (moving one then the other temporarily selects the range in
between the old and new positions).

Text buffers always contain at least one line, but may be empty (that
is, buffers can contain zero characters). The last line in the text
buffer never ends in a line separator (such as newline); the other
lines in the buffer always end in a line separator. Line separators
count as characters when computing character counts and character
offsets. Note that some Unicode line separators are represented with
multiple bytes in UTF-8, and the two-character sequence "\r\n" is also
considered a line separator.

Text buffers support undo and redo if gtk_text_buffer_set_enable_undo()
has been set to %TRUE. Use gtk_text_buffer_undo() or gtk_text_buffer_redo()
to perform the necessary action. Note that these operations are ignored if
the buffer is not editable. Developers may want some operations to not be
undoable. To do this, wrap your changes in
gtk_text_buffer_begin_irreversible_action() and
gtk_text_buffer_end_irreversible_action().

## Simple Example

The simplest usage of `GtkTextView` might look like this:

``` {.c}
GtkWidget *view;
GtkTextBuffer *buffer;

view = gtk_text_view_new ();

buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

gtk_text_buffer_set_text (buffer, "Hello, this is some text", -1);

/* Now you might put the view in a container and display it on the
 * screen; when the user edits the text, signals on the buffer
 * will be emitted, such as "changed", "insert_text", and so on.
 */
```

In many cases it's also convenient to first create the buffer with
gtk_text_buffer_new(), then create a widget for that buffer with
gtk_text_view_new_with_buffer(). Or you can change the buffer the widget
displays after the widget is created with gtk_text_view_set_buffer().

## Example of Changing Text Attributes

The way to affect text attributes in `GtkTextView` is to
apply tags that change the attributes for a region of text.
For text features that come from the theme — such as font and
foreground color — use CSS to override their default values.

```
GtkWidget *view;
GtkTextBuffer *buffer;
GtkTextIter start, end;
PangoFontDescription *font_desc;
GdkRGBA rgba;
GtkTextTag *tag;
GtkCssProvider *provider;
GtkStyleContext *context;

view = gtk_text_view_new ();

buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

gtk_text_buffer_set_text (buffer, "Hello, this is some text", -1);

/* Change default font and color throughout the widget */
provider = gtk_css_provider_new ();
gtk_css_provider_load_from_data (provider,
                                 "textview {"
                                 " font: 15px serif;"
                                 "  color: green;"
                                 "}",
                                 -1);
context = gtk_widget_get_style_context (view);
gtk_style_context_add_provider (context,
                                GTK_STYLE_PROVIDER (provider),
                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

/* Change left margin throughout the widget */
gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 30);

/* Use a tag to change the color for just one part of the widget */
tag = gtk_text_buffer_create_tag (buffer, "blue_foreground",
                                  "foreground", "blue",
                                  NULL);
gtk_text_buffer_get_iter_at_offset (buffer, &start, 7);
gtk_text_buffer_get_iter_at_offset (buffer, &end, 12);
gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
```

The `gtk4-demo` application that comes with
GTK contains more example code for `GtkTextView`.
