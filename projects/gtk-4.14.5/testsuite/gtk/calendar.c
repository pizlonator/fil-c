#include <gtk/gtk.h>

static void
test_calendar_get_set_properties (void)
{
  GtkWidget *calendar;
  int year, month, day;

  calendar = gtk_calendar_new ();
  g_object_set (calendar, "year", 2024, NULL);
  g_object_get (calendar, "year", &year, NULL);
  g_assert_cmpint (year, ==, 2024);
  g_object_set (calendar, "month", 0, NULL); /* January */
  g_object_get (calendar, "month", &month, NULL);
  g_assert_cmpint (month, ==, 0);
  g_object_set (calendar, "day", 15, NULL);
  g_object_get (calendar, "day", &day, NULL);
  g_assert_cmpint (day, ==, 15);
}

static void
test_calendar_select_day (void)
{
  GtkWidget *cal;
  GTimeZone *tz;
  GDateTime *dt;
  GDateTime *dt2;

  cal = gtk_calendar_new ();

  tz = g_time_zone_new_identifier ("Europe/Brussels");
  g_assert_nonnull (tz);
  dt = g_date_time_new (tz, 1970, 3, 1, 0, 0, 0);
  g_assert_nonnull (dt);

  gtk_calendar_select_day (GTK_CALENDAR (cal), dt);

  dt2 = gtk_calendar_get_date (GTK_CALENDAR (cal));
  g_assert_true (g_date_time_equal (dt, dt2));

  g_date_time_unref (dt);
  g_date_time_unref (dt2);
  g_time_zone_unref (tz);
}

static void
test_calendar_get_date (void)
{
  GtkWidget *cal;
  GDateTime *dt2;

  cal = gtk_calendar_new ();

  g_object_set (cal,
                "year", 1970,
                "month", 2, /* March */
                "day", 1,
                NULL);

  dt2 = gtk_calendar_get_date (GTK_CALENDAR (cal));
  g_assert_cmpint (g_date_time_get_year (dt2), ==, 1970);
  g_assert_cmpint (g_date_time_get_month (dt2), ==, 3);
  g_assert_cmpint (g_date_time_get_day_of_month (dt2), ==, 1);

  g_date_time_unref (dt2);
}

static void
test_calendar_set_get_year (void)
{
  GtkWidget *calendar;
  int year;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10); /* avoid days that don't exist in all years */

  gtk_calendar_set_year (GTK_CALENDAR (calendar), 2024);
  year = gtk_calendar_get_year (GTK_CALENDAR (calendar));
  g_assert_cmpint (year, ==, 2024);
}

static void
test_calendar_set_get_month (void)
{
  GtkWidget *calendar;
  int month;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10); /* avoid days that don't exist in all months */

  gtk_calendar_set_month (GTK_CALENDAR (calendar), 1); /* February */
  month = gtk_calendar_get_month (GTK_CALENDAR (calendar));
  g_assert_cmpint (month, ==, 1);
}

static void
test_calendar_set_get_day (void)
{
  GtkWidget *calendar;
  int day;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10);

  gtk_calendar_set_day (GTK_CALENDAR (calendar), 11);
  day = gtk_calendar_get_day (GTK_CALENDAR (calendar));
  g_assert_cmpint (day, ==, 11);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/calendar/get_set_properties", test_calendar_get_set_properties);
  g_test_add_func ("/calendar/select_day", test_calendar_select_day);
  g_test_add_func ("/calendar/test_calendar_get_date", test_calendar_get_date);
  g_test_add_func ("/calendar/set_get_day", test_calendar_set_get_day);
  g_test_add_func ("/calendar/set_get_month", test_calendar_set_get_month);
  g_test_add_func ("/calendar/set_get_year", test_calendar_set_get_year);

  return g_test_run ();
}
