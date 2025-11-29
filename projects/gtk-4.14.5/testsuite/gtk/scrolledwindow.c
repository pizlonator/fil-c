#include <gtk/gtk.h>

#define MIN_SIZE 150
#define MAX_SIZE 300
#define BOX_SIZE 600

typedef enum
{
  MINIMUM_CONTENT = 1 << 0,
  MAXIMUM_CONTENT = 1 << 1
} TestProperty;

static void
test_size (gboolean       overlay,
           GtkPolicyType  policy,
           GtkOrientation orientation,
           TestProperty   prop)
{
  GtkWidget *scrolledwindow, *box;
  int min_size, max_size, child_size;
  int scrollbar_size = 0;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);

  scrolledwindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scrolledwindow), TRUE);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scrolledwindow), overlay);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), policy, policy);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), box);

  /* Testing the content-width property */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), MIN_SIZE);

          gtk_widget_measure (scrolledwindow, GTK_ORIENTATION_HORIZONTAL, -1,
                              &min_size, NULL, NULL, NULL);

        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_width (GTK_SCROLLED_WINDOW (scrolledwindow), MAX_SIZE);
          gtk_widget_set_size_request (box, BOX_SIZE, -1);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-width.
           */
          gtk_widget_measure (scrolledwindow, GTK_ORIENTATION_HORIZONTAL, -1,
                              NULL, &max_size, NULL, NULL);
          gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1,
                              &child_size, NULL, NULL, NULL);
        }

      /* If the relevant scrollbar is non-overlay and always shown, it is added
       * to the preferred size. When comparing to the expected size, we need to
       * to exclude that extra, as we are only interested in the content size */
      if (!overlay && policy == GTK_POLICY_ALWAYS)
        {
          GtkWidget *scrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scrolledwindow));
          gtk_widget_measure (scrollbar, GTK_ORIENTATION_HORIZONTAL, -1,
                              &scrollbar_size, NULL, NULL, NULL);
        }
    }
  /* Testing the content-height property */
  else
    {
      if (prop & MINIMUM_CONTENT)
        {
          gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), MIN_SIZE);

          gtk_widget_measure (scrolledwindow, GTK_ORIENTATION_VERTICAL, -1,
                              &min_size, NULL, NULL, NULL);
        }

      if (prop & MAXIMUM_CONTENT)
        {
          gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scrolledwindow), MAX_SIZE);
          gtk_widget_set_size_request (box, -1, BOX_SIZE);

          /*
           * Here, the content is purposely bigger than the scrolled window,
           * so it should grow up to max-content-height.
           */
          gtk_widget_measure (scrolledwindow, GTK_ORIENTATION_VERTICAL, -1,
                              NULL, &max_size, NULL, NULL);
          gtk_widget_measure (box, GTK_ORIENTATION_VERTICAL, -1,
                              &child_size, NULL, NULL, NULL);
        }

      if (!overlay && policy == GTK_POLICY_ALWAYS)
        {
          GtkWidget *scrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (scrolledwindow));
          gtk_widget_measure (scrollbar, GTK_ORIENTATION_VERTICAL, -1,
                              &scrollbar_size, NULL, NULL, NULL);
        }
    }

  if (prop & MINIMUM_CONTENT)
    {
      min_size -= scrollbar_size;
      g_assert_cmpint (min_size, ==, MIN_SIZE);
    }

  if (prop & MAXIMUM_CONTENT)
    {
      g_assert_cmpint (child_size, ==, BOX_SIZE);

      max_size -= scrollbar_size;
      g_assert_cmpint (max_size, ==, MAX_SIZE);
    }
}


static void
overlay_automatic_width_min (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT);
}

static void
overlay_automatic_height_min (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT);
}

static void
overlay_automatic_width_max (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MAXIMUM_CONTENT);
}

static void
overlay_automatic_height_max (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MAXIMUM_CONTENT);
}

static void
overlay_automatic_width_min_max (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
overlay_automatic_height_min_max (void)
{
  test_size (TRUE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
nonoverlay_automatic_width_min (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT);
}

static void
nonoverlay_automatic_height_min (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT);
}

static void
nonoverlay_automatic_width_max (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MAXIMUM_CONTENT);
}

static void
nonoverlay_automatic_height_max (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MAXIMUM_CONTENT);
}

static void
nonoverlay_automatic_width_min_max (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
nonoverlay_automatic_height_min_max (void)
{
  test_size (FALSE, GTK_POLICY_AUTOMATIC, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
overlay_always_width_min (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT);
}

static void
overlay_always_height_min (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT);
}

static void
overlay_always_width_max (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MAXIMUM_CONTENT);
}

static void
overlay_always_height_max (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MAXIMUM_CONTENT);
}

static void
overlay_always_width_min_max (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
overlay_always_height_min_max (void)
{
  test_size (TRUE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


static void
nonoverlay_always_width_min (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT);
}

static void
nonoverlay_always_height_min (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT);
}

static void
nonoverlay_always_width_max (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MAXIMUM_CONTENT);
}

static void
nonoverlay_always_height_max (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MAXIMUM_CONTENT);
}

static void
nonoverlay_always_width_min_max (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_HORIZONTAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}

static void
nonoverlay_always_height_min_max (void)
{
  test_size (FALSE, GTK_POLICY_ALWAYS, GTK_ORIENTATION_VERTICAL, MINIMUM_CONTENT | MAXIMUM_CONTENT);
}


int
main (int argc, char **argv)
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_width_min", overlay_automatic_width_min);
  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_height_min", overlay_automatic_height_min);
  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_width_max", overlay_automatic_width_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_height_max", overlay_automatic_height_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_width_min_max", overlay_automatic_width_min_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_automatic_height_min_max", overlay_automatic_height_min_max);

  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_width_min", nonoverlay_automatic_width_min);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_height_min", nonoverlay_automatic_height_min);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_width_max", nonoverlay_automatic_width_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_height_max", nonoverlay_automatic_height_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_width_min_max", nonoverlay_automatic_width_min_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_automatic_height_min_max", nonoverlay_automatic_height_min_max);

  g_test_add_func ("/sizing/scrolledwindow/overlay_always_width_min", overlay_always_width_min);
  g_test_add_func ("/sizing/scrolledwindow/overlay_always_height_min", overlay_always_height_min);
  g_test_add_func ("/sizing/scrolledwindow/overlay_always_width_max", overlay_always_width_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_always_height_max", overlay_always_height_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_always_width_min_max", overlay_always_width_min_max);
  g_test_add_func ("/sizing/scrolledwindow/overlay_always_height_min_max", overlay_always_height_min_max);

  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_width_min", nonoverlay_always_width_min);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_height_min", nonoverlay_always_height_min);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_width_max", nonoverlay_always_width_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_height_max", nonoverlay_always_height_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_width_min_max", nonoverlay_always_width_min_max);
  g_test_add_func ("/sizing/scrolledwindow/nonoverlay_always_height_min_max", nonoverlay_always_height_min_max);

  return g_test_run ();
}
