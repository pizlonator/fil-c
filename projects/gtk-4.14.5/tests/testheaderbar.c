#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
unset_title (GtkWidget *window)
{
  GtkWidget *box;

  g_assert (GTK_IS_WINDOW (window));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_hide (box);

  gtk_window_set_titlebar (GTK_WINDOW (window), box);
}

static void
load_css (GtkWidget  *widget,
          const char *css)
{
  GtkCssProvider *provider;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), 800);
}

static void
create_regular (GtkApplication *app)
{
  GtkWidget *window, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Regular window");

  label = gtk_label_new ("This window has no titlebar set");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_headerbar_as_titlebar (GtkApplication *app)
{
  GtkWidget *window, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar as titlebar");

  header = gtk_header_bar_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  label = gtk_label_new ("This window has a headerbar set as a titlebar");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_headerbar_inside_window (GtkApplication *app)
{
  GtkWidget *window, *box, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar inside window");
  unset_title (window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  header = gtk_header_bar_new ();
  gtk_box_append (GTK_BOX (box), header);

  label = gtk_label_new ("This window has a headerbar inside the window and no titlebar");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_headerbar_overlay (GtkApplication *app)
{
  GtkWidget *window, *overlay, *sw, *box, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar overlaying content");
  unset_title (window);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  header = gtk_header_bar_new ();
  gtk_widget_set_valign (header, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), header);
  load_css (header, "headerbar { background: alpha(shade(@theme_bg_color, .9), .8); }");

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request (sw, 300, 250);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), sw);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);
  gtk_widget_set_size_request (sw, 300, 250);

  label = gtk_label_new ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                         "Nulla innn urna ac dui malesuada ornare. Nullam dictum "
                         "tempor mi et tincidunt. Aliquam metus nulla, auctor "
                         "vitae pulvinar nec, egestas at mi. Class aptent taciti "
                         "sociosqu ad litora torquent per conubia nostra, per "
                         "inceptos himenaeos. Aliquam sagittis, tellus congue "
                         "cursus congue, diam massa mollis enim, sit amet gravida "
                         "magna turpis egestas sapien. Aenean vel molestie nunc. "
                         "In hac habitasse platea dictumst. Suspendisse lacinia"
                         "mi eu ipsum vestibulum in venenatis enim commodo. "
                         "Vivamus non malesuada ligula.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new ("This window has a headerbar inside an overlay, so the text is visible underneath it");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_hiding_headerbar (GtkApplication *app)
{
  GtkWidget *window, *box, *revealer, *header, *label, *hbox, *toggle;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Hiding headerbar");
  unset_title (window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  revealer = gtk_revealer_new ();
  gtk_box_append (GTK_BOX (box), revealer);

  header = gtk_header_bar_new ();
  gtk_revealer_set_child (GTK_REVEALER (revealer), header);

  label = gtk_label_new ("This window's headerbar can be shown and hidden with animation");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (hbox, 12);
  gtk_widget_set_margin_bottom (hbox, 12);
  gtk_widget_set_margin_start (hbox, 12);
  gtk_widget_set_margin_end (hbox, 12);
  gtk_box_append (GTK_BOX (box), hbox);

  toggle = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (toggle), TRUE);
  gtk_box_append (GTK_BOX (hbox), toggle);
  g_object_bind_property (toggle, "active",
                          revealer, "reveal-child",
                          G_BINDING_SYNC_CREATE);

  label = gtk_label_new ("Show headerbar");
  gtk_box_append (GTK_BOX (hbox), label);

  gtk_window_present (GTK_WINDOW (window));
}

static void
create_fake_headerbar (GtkApplication *app)
{
  GtkWidget *window, *handle, *box, *center_box, *controls, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Fake headerbar");
  unset_title (window);

  handle = gtk_window_handle_new ();
  gtk_window_set_child (GTK_WINDOW (window), handle);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_handle_set_child (GTK_WINDOW_HANDLE (handle), box);

  center_box = gtk_center_box_new ();
  gtk_box_append (GTK_BOX (box), center_box);

  label = gtk_label_new ("Fake headerbar");
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (center_box), label);

  controls = gtk_window_controls_new (GTK_PACK_START);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (center_box), controls);

  controls = gtk_window_controls_new (GTK_PACK_END);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (center_box), controls);

  label = gtk_label_new ("This window's titlebar is just a centerbox with a label and window controls.\nThe whole window is draggable.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_window_present (GTK_WINDOW (window));
}

/* split headerbar  */

static void
split_decorations (GtkSettings *settings,
                   GParamSpec  *pspec,
                   GtkBuilder  *builder)
{
  GtkWidget *sheader, *mheader;
  char *layout, *p1, *p2;
  char **p;

  sheader = (GtkWidget *)gtk_builder_get_object (builder, "sidebar-header");
  mheader = (GtkWidget *)gtk_builder_get_object (builder, "main-header");

  g_object_get (settings, "gtk-decoration-layout", &layout, NULL);

  p = g_strsplit (layout, ":", -1);

  p1 = g_strconcat ("", p[0], ":", NULL);

  if (g_strv_length (p) >= 2)
    p2 = g_strconcat (":", p[1], NULL);
  else
    p2 = g_strdup ("");

  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (sheader), p1);
  gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (mheader), p2);
 
  g_free (p1);
  g_free (p2);
  g_strfreev (p);
  g_free (layout);
}


static void
create_split_headerbar (GtkApplication *app)
{
  GtkBuilder *builder;
  GtkSettings *settings;
  GtkWidget *win;
  GtkWidget *entry;
  GtkWidget *check;
  GtkWidget *header;
  const char *ui = "tests/testsplitheaders.ui";

  if (!g_file_test (ui, G_FILE_TEST_EXISTS))
    {
      g_warning ("Can't find %s", ui);
      return;
    }

  builder = gtk_builder_new_from_file (ui);

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  gtk_window_set_application (GTK_WINDOW (win), app);

  settings = gtk_widget_get_settings (win);

  g_signal_connect (settings, "notify::gtk-decoration-layout",
                    G_CALLBACK (split_decorations), builder);
  split_decorations (settings, NULL, builder);

  entry = (GtkWidget *)gtk_builder_get_object (builder, "layout-entry");
  g_object_bind_property (settings, "gtk-decoration-layout",
                          entry, "text",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  check = (GtkWidget *)gtk_builder_get_object (builder, "decorations");
  header = (GtkWidget *)gtk_builder_get_object (builder, "sidebar-header");
  g_object_bind_property (check, "active",
                          header, "show-title-buttons",
                          G_BINDING_DEFAULT);
  header = (GtkWidget *)gtk_builder_get_object (builder, "main-header");
  g_object_bind_property (check, "active",
                          header, "show-title-buttons",
			  G_BINDING_DEFAULT);
  gtk_window_present (GTK_WINDOW (win));
}

/* stacked headers */

static void
back_to_main (GtkButton *button,
              GtkWidget *win)
{
  GtkWidget *header_stack;
  GtkWidget *page_stack;

  header_stack = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "header-stack"));
  page_stack = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "page-stack"));

  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "main");
  gtk_stack_set_visible_child_name (GTK_STACK (page_stack), "page1");
}

static void
go_to_secondary (GtkButton *button,
                 GtkWidget *win)
{
  GtkWidget *header_stack;
  GtkWidget *page_stack;

  header_stack = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "header-stack"));
  page_stack = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "page-stack"));

  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "secondary");
  gtk_stack_set_visible_child_name (GTK_STACK (page_stack), "secondary");
}

static void
create_stacked_headerbar (GtkApplication *app)
{
  GtkBuilder *builder;
  GtkWidget *win;
  GtkWidget *new_btn;
  GtkWidget *back_btn;
  GtkWidget *header_stack;
  GtkWidget *page_stack;
  const char *ui = "tests/teststackedheaders.ui";

  if (!g_file_test (ui, G_FILE_TEST_EXISTS))
    {
      g_warning ("Can't find %s", ui);
      return;
    }

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui, NULL);

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  gtk_window_set_application (GTK_WINDOW (win), app);

  header_stack = (GtkWidget *)gtk_builder_get_object (builder, "header_stack");
  page_stack = (GtkWidget *)gtk_builder_get_object (builder, "page_stack");

  g_object_set_data (G_OBJECT (win), "header-stack", header_stack);
  g_object_set_data (G_OBJECT (win), "page-stack", page_stack);

  new_btn = (GtkWidget *)gtk_builder_get_object (builder, "new_btn");
  back_btn = (GtkWidget *)gtk_builder_get_object (builder, "back_btn");

  g_signal_connect (new_btn, "clicked", G_CALLBACK (go_to_secondary), win);
  g_signal_connect (back_btn, "clicked", G_CALLBACK (back_to_main), win);

  gtk_window_present (GTK_WINDOW (win));
}

/* controls */
static void
create_controls (GtkApplication *app)
{
  GtkBuilder *builder;
  GtkWidget *win;
  const char *ui = "tests/testheadercontrols.ui";

  if (!g_file_test (ui, G_FILE_TEST_EXISTS))
    {
      g_warning ("Can't find %s", ui);
      return;
    }

  builder = gtk_builder_new_from_file (ui);

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  gtk_window_set_application (GTK_WINDOW (win), app);

  gtk_window_present (GTK_WINDOW (win));
}

/* technorama */

static const char css[] =
 ".main.background { "
 " background-image: linear-gradient(to bottom, red, blue);"
 " border-width: 0px; "
 "}"
 ".titlebar.backdrop { "
 " background-image: none; "
 " background-color: @bg_color; "
 " border-radius: 10px 10px 0px 0px; "
 "}"
 ".titlebar { "
 " background-image: linear-gradient(to bottom, white, @bg_color);"
 " border-radius: 10px 10px 0px 0px; "
 "}";

static void
on_bookmark_clicked (GtkButton *button, gpointer data)
{
  GtkWindow *window = GTK_WINDOW (data);
  GtkWidget *chooser;

  chooser = gtk_file_chooser_dialog_new ("File Chooser Test",
                                         window,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

  g_signal_connect (chooser, "response",
                    G_CALLBACK (gtk_window_destroy), NULL);

  gtk_window_present (GTK_WINDOW (chooser));
}

static void
toggle_fullscreen (GtkButton *button, gpointer data)
{
  GtkWidget *window = GTK_WIDGET (data);
  static gboolean fullscreen = FALSE;

  if (fullscreen)
    {
      gtk_window_unfullscreen (GTK_WINDOW (window));
      fullscreen = FALSE;
    }
  else
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
      fullscreen = TRUE;
    }
}

static void
change_header (GtkButton *button, gpointer data)
{
  GtkWidget *window = GTK_WIDGET (data);
  GtkWidget *label;
  GtkWidget *widget;
  GtkWidget *header;

  if (button && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_add_css_class (header, "titlebar");
      gtk_widget_add_css_class (header, "header-bar");
      gtk_widget_set_margin_start (header, 10);
      gtk_widget_set_margin_end (header, 10);
      gtk_widget_set_margin_top (header, 10);
      gtk_widget_set_margin_bottom (header, 10);
      label = gtk_label_new ("Label");
      gtk_box_append (GTK_BOX (header), label);
      widget = gtk_level_bar_new ();
      gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), 0.4);
      gtk_widget_set_hexpand (widget, TRUE);
      gtk_box_append (GTK_BOX (header), widget);
    }
  else
    {
      header = gtk_header_bar_new ();
      gtk_widget_add_css_class (header, "titlebar");

      widget = gtk_button_new_with_label ("_Close");
      gtk_button_set_use_underline (GTK_BUTTON (widget), TRUE);
      gtk_widget_add_css_class (widget, "suggested-action");
      g_signal_connect_swapped (widget, "clicked", G_CALLBACK (gtk_window_destroy), window);

      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), widget);

      widget= gtk_button_new_from_icon_name ("bookmark-new-symbolic");
      g_signal_connect (widget, "clicked", G_CALLBACK (on_bookmark_clicked), window);

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), widget);
    }

  gtk_window_set_titlebar (GTK_WINDOW (window), header);
}

static void
create_technorama (GtkApplication *app)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *footer;
  GtkWidget *button;
  GtkWidget *content;
  GtkCssProvider *provider;

  window = gtk_window_new ();
  gtk_window_set_application (GTK_WINDOW (window), app);

  gtk_widget_add_css_class (window, "main");

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (window),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);


  change_header (NULL, window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  content = gtk_image_new_from_icon_name ("start-here-symbolic");
  gtk_image_set_pixel_size (GTK_IMAGE (content), 512);
  gtk_widget_set_vexpand (content, TRUE);

  gtk_box_append (GTK_BOX (box), content);

  footer = gtk_action_bar_new ();
  gtk_action_bar_set_center_widget (GTK_ACTION_BAR (footer), gtk_check_button_new_with_label ("Middle"));
  button = gtk_toggle_button_new_with_label ("Custom");
  g_signal_connect (button, "clicked", G_CALLBACK (change_header), window);
  gtk_action_bar_pack_start (GTK_ACTION_BAR (footer), button);
  button = gtk_button_new_with_label ("Fullscreen");
  gtk_action_bar_pack_end (GTK_ACTION_BAR (footer), button);
  g_signal_connect (button, "clicked", G_CALLBACK (toggle_fullscreen), window);
  gtk_box_append (GTK_BOX (box), footer);
  gtk_window_present (GTK_WINDOW (window));
}

struct {
  const char *name;
  void (*cb) (GtkApplication *app);
} buttons[] =
{
    { "Regular window", create_regular },
    { "Headerbar as titlebar", create_headerbar_as_titlebar },
    { "Headerbar inside window", create_headerbar_inside_window },
    { "Headerbar overlaying content", create_headerbar_overlay },
    { "Hiding headerbar", create_hiding_headerbar },
    { "Fake headerbar", create_fake_headerbar },
    { "Split headerbar", create_split_headerbar },
    { "Stacked headerbar", create_stacked_headerbar },
    { "Headerbar with controls", create_controls },
    { "Technorama", create_technorama },
};
int n_buttons = sizeof (buttons) / sizeof (buttons[0]);

static void
app_activate_cb (GtkApplication *app)
{
  GtkWidget *window, *box;
  int i;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar test");

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (box, "linked");
  gtk_window_set_child (GTK_WINDOW (window), box);

  for (i = 0; i < n_buttons; i++)
    {
      GtkWidget *btn;

      btn = gtk_button_new_with_label (buttons[i].name);
      g_signal_connect_object (btn,
                               "clicked",
                               G_CALLBACK (buttons[i].cb),
                               app,
                               G_CONNECT_SWAPPED);
      gtk_box_append (GTK_BOX (box), btn);
    }

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.headerbar2", 0);

  g_signal_connect (app,
                    "activate",
                    G_CALLBACK (app_activate_cb),
                    NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
