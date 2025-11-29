#include <gtk/gtk.h>

void
print_attribute (PangoAttribute *attr, GString *string)
{
  GEnumClass *class;
  GEnumValue *value;
  PangoAttrString *str;
  PangoAttrLanguage *lang;
  PangoAttrInt *integer;
  PangoAttrFloat *flt;
  PangoAttrFontDesc *font;
  PangoAttrColor *color;
  PangoAttrShape *shape;

  g_string_append_printf (string, "[%d,%d]", attr->start_index, attr->end_index);

  class = g_type_class_ref (pango_attr_type_get_type ());
  value = g_enum_get_value (class, attr->klass->type);
  g_string_append_printf (string, "%s=", value->value_nick);
  g_type_class_unref (class);

  if ((str = pango_attribute_as_string (attr)) != NULL)
    g_string_append (string, str->value);
  else if ((lang = pango_attribute_as_language (attr)) != NULL)
    g_string_append (string, pango_language_to_string (lang->value));
  else if ((integer = pango_attribute_as_int (attr)) != NULL)
    g_string_append_printf (string, "%d", integer->value);
  else if ((flt = pango_attribute_as_float (attr)) != NULL)
    {
        char val[20];

        g_ascii_formatd (val, 20, "%f", flt->value);
        g_string_append (string, val);
     }
  else if ((font = pango_attribute_as_font_desc (attr)) != NULL)
    {
      char *text = pango_font_description_to_string (font->desc);
      g_string_append (string, text);
      g_free (text);
    }
  else if ((color = pango_attribute_as_color (attr)) != NULL)
    {
      char *text = pango_color_to_string (&color->color);
      g_string_append (string, text);
      g_free (text);
    }
  else if ((shape = pango_attribute_as_shape (attr)) != NULL)
    {
      g_assert_true (shape != NULL);
      g_string_append_printf (string, "shape");
    }
  else
    g_assert_not_reached ();
}

void
print_attr_list (PangoAttrList *attrs, GString *string)
{
  PangoAttrIterator *iter;

  if (!attrs)
    return;

  iter = pango_attr_list_get_iterator (attrs);
  do {
    int start, end;
    GSList *list, *l;

    pango_attr_iterator_range (iter, &start, &end);
    g_string_append_printf (string, "range %d %d\n", start, end);
    list = pango_attr_iterator_get_attrs (iter);
    for (l = list; l; l = l->next)
      {
        PangoAttribute *attr = l->data;
        print_attribute (attr, string);
        g_string_append (string, "\n");
      }
    g_slist_free_full (list, (GDestroyNotify)pango_attribute_destroy);
  } while (pango_attr_iterator_next (iter));

  pango_attr_iterator_destroy (iter);
}

static void
test_label_markup (void)
{
  GtkWidget *window;
  GtkWidget *label;
  PangoAttrList *attrs;
  GString *str;
  const char *text;

  window = gtk_window_new ();
  label = gtk_label_new ("");

  gtk_window_set_child (GTK_WINDOW (window), label);
  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);

  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_label (GTK_LABEL (label), "<a href=\"test\"><span font_style=\"italic\">abc</span> _def</a>");

  g_assert_cmpuint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, 'd');

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  str = g_string_new ("");
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,4]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 4\n"
    "[0,4]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "range 4 5\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[4,5]underline=3\n"
    "range 5 8\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[5,8]underline=1\n"
    "range 8 2147483647\n");


  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), FALSE);

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  g_string_set_size (str, 0);
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,7]underline=1\n"
    "[0,7]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 7\n"
    "[0,7]underline=1\n"
    "[0,7]foreground=#1b1b6a6acbcb\n"
    "range 7 2147483647\n");

  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);
  gtk_label_set_use_underline (GTK_LABEL (label), FALSE);

  text = pango_layout_get_text (gtk_label_get_layout (GTK_LABEL (label)));
  g_assert_cmpstr (text, ==, "abc _def");

  attrs = pango_layout_get_attributes (gtk_label_get_layout (GTK_LABEL (label)));
  g_string_set_size (str, 0);
  print_attr_list (attrs, str);

  g_assert_cmpstr (str->str, ==,
    "range 0 3\n"
    "[0,8]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "[0,3]style=2\n"
    "range 3 8\n"
    "[0,8]underline=1\n"
    "[0,8]foreground=#1b1b6a6acbcb\n"
    "range 8 2147483647\n");

  g_string_free (str, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_label_underline (void)
{
  GtkWidget *window;
  GtkWidget *label;

  window = gtk_window_new ();

  label = gtk_label_new ("");

  gtk_window_set_child (GTK_WINDOW (window), label);
  gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);

  gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_label (GTK_LABEL (label), "tes_t & no markup <<");

  g_assert_cmpint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, GDK_KEY_t);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_label_parse_more (void)
{
  struct {
    const char *input;
    gboolean use_underline;
    gboolean use_markup;
    const char *text;
    guint accel;
  } tests[] = {
    { "tes_t m__e mo_re", TRUE, FALSE, "test m_e more", GDK_KEY_t },
    { "test m__e mo_re", TRUE, FALSE, "test m_e more", GDK_KEY_r },
    { "tes_t m__e mo_re", FALSE, FALSE, "tes_t m__e mo_re", GDK_KEY_VoidSymbol },
    { "<span font='test_font'>test <a href='bla'>w_ith</a> bla</span>", TRUE, TRUE, "test with bla", GDK_KEY_i },
    { "<span font='test_font'>test <a href='bla'>w_ith</a> bla</span>", FALSE, TRUE, "test w_ith bla", GDK_KEY_VoidSymbol },
  };
  GtkWidget *label;

  label = gtk_label_new ("");

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      gtk_label_set_use_underline (GTK_LABEL (label), tests[i].use_underline);
      gtk_label_set_use_markup (GTK_LABEL (label), tests[i].use_markup);
      gtk_label_set_label (GTK_LABEL (label), tests[i].input);

      g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, tests[i].input);
      g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (label)), ==, tests[i].text);
      g_assert_cmpuint (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)), ==, tests[i].accel);
    }

  g_object_ref_sink (label);
  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/label/markup-parse", test_label_markup);
  g_test_add_func ("/label/underline-parse", test_label_underline);
  g_test_add_func ("/label/parse-more", test_label_parse_more);

  return g_test_run ();
}
