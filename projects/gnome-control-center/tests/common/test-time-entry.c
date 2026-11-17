#include <locale.h>
#include <gtk/gtk.h>
/* #include "cc-datetime-resources.h" */

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#include "cc-time-entry.c"

static void
test_time (CcTimeEntry *time_entry,
           guint        hour,
           guint        minute,
           gboolean     is_24h)
{
  g_autofree char *str = NULL;
  const char *entry_str;
  guint entry_hour, entry_minute;

  g_assert_true (CC_IS_TIME_ENTRY (time_entry));
  g_assert_cmpint (hour, >=, 0);
  g_assert_cmpint (hour, <= , 23);
  g_assert_cmpint (minute, >=, 0);
  g_assert_cmpint (minute, <= , 59);

  entry_hour = cc_time_entry_get_hour (time_entry);
  g_assert_cmpint (entry_hour, ==, hour);

  entry_minute = cc_time_entry_get_minute (time_entry);
  g_assert_cmpint (entry_minute, ==, minute);

  /* Convert 24 hour time to 12 hour */
  if (!is_24h)
    {
      /* 00:00 is 12:00 AM */
      if (hour == 0)
        hour = 12;
      else if (hour > 12)
        hour = hour - 12;
    }

  str = g_strdup_printf ("%02d:%02d", hour, minute);
  entry_str = gtk_entry_get_text (GTK_ENTRY (time_entry));
  g_assert_cmpstr (entry_str, ==, str);
}

static void
test_time_24h (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_object_ref_sink (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  for (guint i = 0; i <= 25; i++)
    {
      guint hour;
      gboolean is_am;

      cc_time_entry_set_time (time_entry, i, 0);
      g_assert_false (cc_time_entry_get_am_pm (time_entry));

      test_time (time_entry, i < 24 ? i : 23, 0, TRUE);

      hour = cc_time_entry_get_hour (time_entry);
      is_am = cc_time_entry_get_is_am (time_entry);
      if (hour < 12)
        g_assert_true (is_am);
      else
        g_assert_false (is_am);
    }

  g_object_unref (entry);
}

static void
test_time_12h (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_object_ref_sink (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  cc_time_entry_set_am_pm (time_entry, FALSE);
  g_assert_false (cc_time_entry_get_am_pm (time_entry));
  cc_time_entry_set_am_pm (time_entry, TRUE);
  g_assert_true (cc_time_entry_get_am_pm (time_entry));

  for (guint i = 0; i <= 25; i++)
    {
      guint hour;
      gboolean is_am;

      cc_time_entry_set_time (time_entry, i, 0);

      test_time (time_entry, i < 24 ? i : 23, 0, FALSE);

      hour = cc_time_entry_get_hour (time_entry);
      is_am = cc_time_entry_get_is_am (time_entry);

      if (hour < 12)
        g_assert_true (is_am);
      else
        g_assert_false (is_am);
    }

  g_object_unref (entry);
}

static void
test_time_hour_24h (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;
  int hour;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), 0);

  for (guint i = 1; i <= 25; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      hour = i;
      if (hour >= 24)
        hour = hour - 24;

      test_time (time_entry, hour, 0, TRUE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  hour = 0;

  for (int i = 25; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      hour--;
      if (hour < 0)
        hour = 23;

      test_time (time_entry, hour, 0, TRUE);
    }

  /* Put cursor at the one’s place and repeat the tests */
  gtk_editable_set_position (GTK_EDITABLE (entry), 1);
  cc_time_entry_set_time (time_entry, 0, 0);

  for (guint i = 1; i <= 25; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      hour = i;
      if (hour >= 24)
        hour = hour - 24;

      test_time (time_entry, hour, 0, TRUE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  hour = 0;

  for (int i = 25; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      hour--;
      if (hour < 0)
        hour = 23;

      test_time (time_entry, hour, 0, TRUE);
    }

  g_object_ref_sink (entry);
  g_object_unref (entry);
}

static void
test_time_minute_24h (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;
  int minute;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  cc_time_entry_set_time (time_entry, 0, 0);
  /* Set cursor at 10’s place of minute */
  gtk_editable_set_position (GTK_EDITABLE (entry), SEPARATOR_INDEX + 1);

  for (guint i = 1; i <= 61; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      minute = i;
      if (minute >= 60)
        minute = minute - 60;

      test_time (time_entry, 0, minute, TRUE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), SEPARATOR_INDEX + 1);
  minute = 0;

  for (int i = 61; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      minute--;
      if (minute < 0)
        minute = 59;

      test_time (time_entry, 0, minute, TRUE);
    }

  /* Put cursor at the minute one’s place and repeat the tests */
  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), SEPARATOR_INDEX + 2);

  for (guint i = 1; i <= 61; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      minute = i;
      if (minute >= 60)
        minute = minute - 60;

      test_time (time_entry, 0, minute, TRUE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), SEPARATOR_INDEX + 2);
  minute = 0;

  for (int i = 61; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      minute--;
      if (minute < 0)
        minute = 59;

      test_time (time_entry, 0, minute, TRUE);
    }

  g_object_ref_sink (entry);
  g_object_unref (entry);
}

static void
test_time_hour_12h (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;
  int hour;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  cc_time_entry_set_am_pm (time_entry, TRUE);
  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), 0);

  for (guint i = 1; i <= 14; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      hour = i;
      if (hour >= 12)
        hour = hour - 12;

      test_time (time_entry, hour, 0, FALSE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  hour = 12;

  for (int i = 23; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      hour--;
      if (hour < 0)
        hour = 11;  /* Hour varies from 0 to 11 (0 is 12) */

      test_time (time_entry, hour, 0, FALSE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  /* Put cursor at the one’s place and repeat the tests */
  gtk_editable_set_position (GTK_EDITABLE (entry), 1);

  for (guint i = 1; i <= 14; i++)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_UP);

      /* Wrap if above limit */
      hour = i;
      if (hour >= 12)
        hour = hour - 12;

      test_time (time_entry, hour, 0, FALSE);
    }

  cc_time_entry_set_time (time_entry, 0, 0);
  hour = 0;

  for (int i = 23; i >= 0; i--)
    {
      g_signal_emit_by_name (entry, "change-value", GTK_SCROLL_STEP_DOWN);

      /* Wrap if below limit */
      hour--;
      if (hour < 0)
        hour = 11;  /* Hour varies from 0 to 11 (0 is 12) */

      test_time (time_entry, hour, 0, FALSE);
    }

  g_object_ref_sink (entry);
  g_object_unref (entry);
}

static void
test_time_insertion (void)
{
  GtkWidget *entry;
  CcTimeEntry *time_entry;
  int position;

  entry = cc_time_entry_new ();
  time_entry = CC_TIME_ENTRY (entry);
  g_assert (CC_IS_TIME_ENTRY (entry));

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_set_position (GTK_EDITABLE (entry), 0);

  /* Test hour */
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 0;
  /* 00:00 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "0", -1, &position);
  test_time (time_entry, 0, 0, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 0;
  /* 20:00 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "2", -1, &position);
  test_time (time_entry, 20, 0, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 0;
  /* 30:00 => 23:00 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "3", -1, &position);
  test_time (time_entry, 23, 0, TRUE);

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 1;
  /* 09:00 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "9", -1, &position);
  test_time (time_entry, 9, 0, TRUE);

  /* Test minute */
  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 1;
  /* 00:10 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "1", -1, &position);
  test_time (time_entry, 0, 10, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 1;
  /* 00:30 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "3", -1, &position);
  test_time (time_entry, 0, 30, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 1;
  /* 00:60 => 00:59 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "6", -1, &position);
  test_time (time_entry, 0, 59, TRUE);

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 2;
  /* 00:00 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "0", -1, &position);
  test_time (time_entry, 0, 0, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 2;
  /* 00:01 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "1", -1, &position);
  test_time (time_entry, 0, 1, TRUE);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = SEPARATOR_INDEX + 2;
  /* 00:09 */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "9", -1, &position);
  test_time (time_entry, 0, 9, TRUE);

  /* 12 Hour mode */
  cc_time_entry_set_am_pm (time_entry, TRUE);
  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 0;
  /* 12:00 AM */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "1", -1, &position);
  /* 12 AM Hour in 12H mode is 00 Hour in 24H mode */
  test_time (time_entry, 0, 0, FALSE);

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 1;
  /* 11:00 AM */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "1", -1, &position);
  test_time (time_entry, 11, 0, FALSE);

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 3;
  /* 12:10 AM */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "1", -1, &position);
  test_time (time_entry, 0, 10, FALSE);

  cc_time_entry_set_time (time_entry, 0, 0);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, 1);
  position = 4;
  /* 12:09 AM */
  gtk_editable_insert_text (GTK_EDITABLE (entry), "9", -1, &position);
  test_time (time_entry, 0, 9, FALSE);

  g_object_ref_sink (entry);
  g_object_unref (entry);
}

int
main (int    argc,
      char **argv)
{
  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);
  gtk_init (NULL, NULL);

  g_setenv ("G_DEBUG", "fatal_warnings", FALSE);

  g_test_add_func ("/datetime/time-24h", test_time_24h);
  g_test_add_func ("/datetime/time-12h", test_time_12h);
  g_test_add_func ("/datetime/time-hour-24h", test_time_hour_24h);
  g_test_add_func ("/datetime/time-minute-24h", test_time_minute_24h);
  g_test_add_func ("/datetime/time-hour-12h", test_time_hour_12h);
  g_test_add_func ("/datetime/time-insertion", test_time_insertion);

  return g_test_run ();
}
