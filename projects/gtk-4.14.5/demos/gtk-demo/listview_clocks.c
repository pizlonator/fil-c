/* Lists/Clocks
 * #Keywords: GtkGridView, GtkListItemFactory, GListModel
 *
 * This demo displays the time in different timezones.
 *
 * The goal is to show how to set up expressions that track changes
 * in objects and make them update widgets. For that, we create a
 * clock object that updates its time every second and then use
 * various ways to display that time.
 *
 * Typically, this will be done using GtkBuilder .ui files with the
 * help of the <binding> tag, but this demo shows the code that runs
 * behind that.
 */

#include <gtk/gtk.h>

#define GTK_TYPE_CLOCK (gtk_clock_get_type ())
G_DECLARE_FINAL_TYPE (GtkClock, gtk_clock, GTK, CLOCK, GObject)

/* This is our object. It's just a timezone */
typedef struct _GtkClock GtkClock;
struct _GtkClock
{
  GObject parent_instance;

  /* We allow this to be NULL for the local timezone */
  GTimeZone *timezone;
  /* Name of the location we're displaying time for */
  char *location;
};

enum {
  PROP_0,
  PROP_LOCATION,
  PROP_TIME,
  PROP_TIMEZONE,

  N_PROPS
};

/* This function returns the current time in the clock's timezone.
 * Note that this returns a new object every time, so we need to
 * remember to unref it after use.
 */
static GDateTime *
gtk_clock_get_time (GtkClock *clock)
{
  if (clock->timezone)
    return g_date_time_new_now (clock->timezone);
  else
    return g_date_time_new_now_local ();
}

/* Here, we implement the functionality required by the GdkPaintable
 * interface. This way we have a trivial way to display an analog clock.
 * It also allows demonstrating how to directly use objects in the
 * listview later by making this object do something interesting.
 */
static void
gtk_clock_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  GtkClock *self = GTK_CLOCK (paintable);
  GDateTime *time;
  GskRoundedRect outline;

#define BLACK ((GdkRGBA) { 0, 0, 0, 1 })

  /* save/restore() is necessary so we can undo the transforms we start
   * out with.
   */
  gtk_snapshot_save (snapshot);

  /* First, we move the (0, 0) point to the center of the area so
   * we can draw everything relative to it.
   */
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2, height / 2));

  /* Next we scale it, so that we can pretend that the clock is
   * 100px in size. That way, we don't need to do any complicated
   * math later. We use MIN() here so that we use the smaller
   * dimension for sizing. That way we don't overdraw but keep
   * the aspect ratio.
   */
  gtk_snapshot_scale (snapshot, MIN (width, height) / 100.0, MIN (width, height) / 100.0);

  /* Now we have a circle with diameter 100px (and radius 50px) that
   * has its (0, 0) point at the center. Let's draw a simple clock into it.
   */
  time = gtk_clock_get_time (self);

  /* First, draw a circle. This is a neat little trick to draw a circle
   * without requiring Cairo.
   */
  gsk_rounded_rect_init_from_rect (&outline, &GRAPHENE_RECT_INIT(-50, -50, 100, 100), 50);
  gtk_snapshot_append_border (snapshot,
                              &outline,
                              (float[4]) { 4, 4, 4, 4 },
                              (GdkRGBA [4]) { BLACK, BLACK, BLACK, BLACK });

  /* Next, draw the hour hand.
   * We do this using transforms again: Instead of computing where the angle
   * points to, we just rotate everything and then draw the hand as if it
   * was :00. We don't even need to care about am/pm here because rotations
   * just work.
   */
  gtk_snapshot_save (snapshot);
  gtk_snapshot_rotate (snapshot, 30 * g_date_time_get_hour (time) + 0.5 * g_date_time_get_minute (time));
  gsk_rounded_rect_init_from_rect (&outline, &GRAPHENE_RECT_INIT(-2, -23, 4, 25), 2);
  gtk_snapshot_push_rounded_clip (snapshot, &outline);
  gtk_snapshot_append_color (snapshot, &BLACK, &outline.bounds);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);

  /* And the same as above for the minute hand. Just make this one longer
   * so people can tell the hands apart.
   */
  gtk_snapshot_save (snapshot);
  gtk_snapshot_rotate (snapshot, 6 * g_date_time_get_minute (time));
  gsk_rounded_rect_init_from_rect (&outline, &GRAPHENE_RECT_INIT(-2, -43, 4, 45), 2);
  gtk_snapshot_push_rounded_clip (snapshot, &outline);
  gtk_snapshot_append_color (snapshot, &BLACK, &outline.bounds);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);

  /* and finally, the second indicator. */
  gtk_snapshot_save (snapshot);
  gtk_snapshot_rotate (snapshot, 6 * g_date_time_get_second (time));
  gsk_rounded_rect_init_from_rect (&outline, &GRAPHENE_RECT_INIT(-2, -43, 4, 10), 2);
  gtk_snapshot_push_rounded_clip (snapshot, &outline);
  gtk_snapshot_append_color (snapshot, &BLACK, &outline.bounds);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);

  /* And finally, don't forget to restore the initial save() that
   * we did for the initial transformations.
   */
  gtk_snapshot_restore (snapshot);

  g_date_time_unref (time);
}

/* Our desired size is 100px. That sounds okay for an analog clock */
static int
gtk_clock_get_intrinsic_width (GdkPaintable *paintable)
{
  return 100;
}

static int
gtk_clock_get_intrinsic_height (GdkPaintable *paintable)
{
  return 100;
}

/* Initialize the paintable interface. This way we turn our clocks
 * into objects that can be drawn. There are more functions to this
 * interface to define desired size, but this is enough.
 */
static void
gtk_clock_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_clock_snapshot;
  iface->get_intrinsic_width = gtk_clock_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_clock_get_intrinsic_height;
}

/* Finally, we define the type. The important part is adding the
 * paintable interface, so GTK knows that this object can indeed
 * be drawn.
 */
G_DEFINE_TYPE_WITH_CODE (GtkClock, gtk_clock, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_clock_paintable_init))

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_clock_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkClock *self = GTK_CLOCK (object);

  switch (property_id)
    {
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;

    case PROP_TIME:
      g_value_take_boxed (value, gtk_clock_get_time (self));
      break;

    case PROP_TIMEZONE:
      g_value_set_boxed (value, self->timezone);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_clock_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkClock *self = GTK_CLOCK (object);

  switch (property_id)
    {
    case PROP_LOCATION:
      self->location = g_value_dup_string (value);
      break;

    case PROP_TIMEZONE:
      self->timezone = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/* This is the list of all the ticking clocks */
static GSList *ticking_clocks = NULL;

/* This is the ID of the timeout source that is updating all
 * ticking clocks.
 */
static guint ticking_clock_id = 0;

/* Every second, this function is called to tell everybody that
 * the clocks are ticking.
 */
static gboolean
gtk_clock_tick (gpointer unused)
{
  GSList *l;

  for (l = ticking_clocks; l; l = l->next)
    {
      GtkClock *clock = l->data;

      /* We will now return a different value for the time property,
       * so notify about that.
       */
      g_object_notify_by_pspec (G_OBJECT (clock), properties[PROP_TIME]);

      /* We will also draw the hands of the clock differently.
       * So notify about that, too.
       */
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (clock));
    }

  return G_SOURCE_CONTINUE;
}

static void
gtk_clock_stop_ticking (GtkClock *self)
{
  ticking_clocks = g_slist_remove (ticking_clocks, self);

  /* If no clock is remaining, stop running the tick updates */
  if (ticking_clocks == NULL && ticking_clock_id != 0)
    g_clear_handle_id (&ticking_clock_id, g_source_remove);
}

static void
gtk_clock_start_ticking (GtkClock *self)
{
  /* if no clock is ticking yet, start */
  if (ticking_clock_id == 0)
    ticking_clock_id = g_timeout_add_seconds (1, gtk_clock_tick, NULL);

  ticking_clocks = g_slist_prepend (ticking_clocks, self);
}

static void
gtk_clock_finalize (GObject *object)
{
  GtkClock *self = GTK_CLOCK (object);

  gtk_clock_stop_ticking (self);

  g_free (self->location);
  g_clear_pointer (&self->timezone, g_time_zone_unref);

  G_OBJECT_CLASS (gtk_clock_parent_class)->finalize (object);
}

static void
gtk_clock_class_init (GtkClockClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_clock_get_property;
  gobject_class->set_property = gtk_clock_set_property;
  gobject_class->finalize = gtk_clock_finalize;

  properties[PROP_LOCATION] =
    g_param_spec_string ("location", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  properties[PROP_TIME] =
    g_param_spec_boxed ("time", NULL, NULL, G_TYPE_DATE_TIME, G_PARAM_READABLE);
  properties[PROP_TIMEZONE] =
    g_param_spec_boxed ("timezone", NULL, NULL, G_TYPE_TIME_ZONE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_clock_init (GtkClock *self)
{
  gtk_clock_start_ticking (self);
}

static GtkClock *
gtk_clock_new (const char *location,
               GTimeZone  *_tz)
{
  GtkClock *result;

  result = g_object_new (GTK_TYPE_CLOCK,
                         "location", location,
                         "timezone", _tz,
                         NULL);

  g_clear_pointer (&_tz, g_time_zone_unref);

  return result;
}

static GListModel *
create_clocks_model (void)
{
  GListStore *result;
  GtkClock *clock;

  result = g_list_store_new (GTK_TYPE_CLOCK);

  /* local time */
  clock = gtk_clock_new ("local", NULL);
  g_list_store_append (result, clock);
  g_object_unref (clock);
  /* UTC time */
  clock = gtk_clock_new ("UTC", g_time_zone_new_utc ());
  g_list_store_append (result, clock);
  g_object_unref (clock);
  /* A bunch of timezones with GTK hackers */
  clock = gtk_clock_new ("San Francisco", g_time_zone_new ("America/Los_Angeles"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("Xalapa", g_time_zone_new ("America/Mexico_City"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("Boston", g_time_zone_new ("America/New_York"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("London", g_time_zone_new ("Europe/London"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("Berlin", g_time_zone_new ("Europe/Berlin"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("Moscow", g_time_zone_new ("Europe/Moscow"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("New Delhi", g_time_zone_new ("Asia/Kolkata"));
  g_list_store_append (result, clock);
  g_object_unref (clock);
  clock = gtk_clock_new ("Shanghai", g_time_zone_new ("Asia/Shanghai"));
  g_list_store_append (result, clock);
  g_object_unref (clock);

  return G_LIST_MODEL (result);
}

static char *
convert_time_to_string (GObject   *image,
                        GDateTime *time,
                        gpointer   unused)
{
  return g_date_time_format (time, "%x\n%X");
}

/* And this function is the crux for this whole demo.
 * It shows how to use expressions to set up bindings.
 */
static void
setup_listitem_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *box, *picture, *location_label, *time_label;
  GtkExpression *clock_expression, *expression;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_list_item_set_child (list_item, box);

  /* First, we create an expression that gets us the clock from the listitem:
   * 1. Create an expression that gets the list item.
   * 2. Use that expression's "item" property to get the clock
   */
  expression = gtk_constant_expression_new (GTK_TYPE_LIST_ITEM, list_item);
  clock_expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, expression, "item");

  /* Bind the clock's location to a label.
   * This is easy: We just get the "location" property of the clock.
   */
  expression = gtk_property_expression_new (GTK_TYPE_CLOCK,
                                            gtk_expression_ref (clock_expression),
                                            "location");
  /* Now create the label and bind the expression to it. */
  location_label = gtk_label_new (NULL);
  gtk_expression_bind (expression, location_label, "label", location_label);
  gtk_box_append (GTK_BOX (box), location_label);


  /* Here we bind the item itself to a GdkPicture.
   * This is simply done by using the clock expression itself.
   */
  expression = gtk_expression_ref (clock_expression);
  /* Now create the widget and bind the expression to it. */
  picture = gtk_picture_new ();
  gtk_expression_bind (expression, picture, "paintable", picture);
  gtk_box_append (GTK_BOX (box), picture);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (picture),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, location_label, NULL,
                                  -1);


  /* And finally, everything comes together.
   * We create a label for displaying the time as text.
   * For that, we need to transform the "GDateTime" of the
   * time property into a string so that the label can display it.
   */
  expression = gtk_property_expression_new (GTK_TYPE_CLOCK,
                                            gtk_expression_ref (clock_expression),
                                            "time");
  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL,
                                            1, (GtkExpression *[1]) { expression },
                                            G_CALLBACK (convert_time_to_string),
                                            NULL, NULL);
  /* Now create the label and bind the expression to it. */
  time_label = gtk_label_new (NULL);
  gtk_expression_bind (expression, time_label, "label", time_label);
  gtk_box_append (GTK_BOX (box), time_label);

  gtk_expression_unref (clock_expression);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_clocks (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *gridview, *sw;
      GtkListItemFactory *factory;
      GtkSelectionModel *model;

      /* This is the normal window setup code every demo does */
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Clocks");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      /* List widgets go into a scrolled window. Always. */
      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);

      /* Create the factory that creates the listitems. Because we
       * used bindings above during setup, we only need to connect
       * to the setup signal.
       * The bindings take care of the bind step.
       */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);

      model = GTK_SELECTION_MODEL (gtk_no_selection_new (create_clocks_model ()));
      gridview = gtk_grid_view_new (model, factory);
      gtk_accessible_update_property (GTK_ACCESSIBLE (gridview),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Clocks",
                                      -1);
      gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (gridview), GTK_SCROLL_NATURAL);
      gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (gridview), GTK_SCROLL_NATURAL);

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), gridview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
