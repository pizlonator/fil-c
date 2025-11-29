#include "gtktexthistoryprivate.h"

#if 0
# define DEBUG_COMMANDS
#endif

typedef struct
{
  GtkTextHistory *history;
  GString *buf;
  struct {
    int insert;
    int bound;
  } selection;
  guint can_redo : 1;
  guint can_undo : 1;
  guint is_modified : 1;
} Text;

enum {
  IGNORE = 0,
  SET = 1,
  UNSET = 2,
};

enum {
  IGNORE_SELECT = 0,
  DO_SELECT = 1,
};

enum {
  INSERT = 1,
  INSERT_SEQ,
  BACKSPACE,
  DELETE_KEY,
  UNDO,
  REDO,
  BEGIN_IRREVERSIBLE,
  END_IRREVERSIBLE,
  BEGIN_USER,
  END_USER,
  MODIFIED,
  UNMODIFIED,
  SELECT,
  CHECK_SELECT,
  SET_MAX_UNDO,
};

typedef struct
{
  int kind;
  int location;
  int end_location;
  const char *text;
  const char *expected;
  int can_undo;
  int can_redo;
  int is_modified;
  int select;
} Command;

static void
do_change_state (gpointer funcs_data,
                 gboolean is_modified,
                 gboolean can_undo,
                 gboolean can_redo)
{
  Text *text = funcs_data;

  text->is_modified = is_modified;
  text->can_undo = can_undo;
  text->can_redo = can_redo;
}

static void
do_insert (gpointer    funcs_data,
           guint       begin,
           guint       end,
           const char *text,
           guint       len)
{
  Text *t = funcs_data;

#ifdef DEBUG_COMMANDS
  g_printerr ("Insert Into '%s' (begin=%u end=%u text=%s)\n",
              t->buf->str, begin, end, text);
#endif

  g_string_insert_len (t->buf, begin, text, len);
}

static void
do_delete (gpointer     funcs_data,
           guint        begin,
           guint        end,
           const char *expected_text,
           guint        len)
{
  Text *t = funcs_data;

#ifdef DEBUG_COMMANDS
  g_printerr ("Delete(begin=%u end=%u expected_text=%s)\n", begin, end, expected_text);
#endif

  if (end < begin)
    {
      guint tmp = end;
      end = begin;
      begin = tmp;
    }

  g_assert_cmpint (memcmp (t->buf->str + begin, expected_text, len), ==, 0);

  if (end >= t->buf->len)
    {
      t->buf->len = begin;
      t->buf->str[begin] = 0;
      return;
    }

  memmove (t->buf->str + begin,
           t->buf->str + end,
           t->buf->len - end);
  g_string_truncate (t->buf, t->buf->len - (end - begin));
}

static void
do_select (gpointer funcs_data,
           int      selection_insert,
           int      selection_bound)
{
  Text *text = funcs_data;

  text->selection.insert = selection_insert;
  text->selection.bound = selection_bound;
}

static GtkTextHistoryFuncs funcs = {
  do_change_state,
  do_insert,
  do_delete,
  do_select,
};

static Text *
text_new (void)
{
  Text *text = g_new0 (Text, 1);

  text->history = gtk_text_history_new (&funcs, text);
  text->buf = g_string_new (NULL);
  text->selection.insert = -1;
  text->selection.bound = -1;

  return text;
}

static void
text_free (Text *text)
{
  g_object_unref (text->history);
  g_string_free (text->buf, TRUE);
  g_free (text);
}

static void
command_insert (const Command *cmd,
                Text          *text)
{
  do_insert (text,
             cmd->location,
             cmd->location + g_utf8_strlen (cmd->text, -1),
             cmd->text,
             strlen (cmd->text));
  gtk_text_history_text_inserted (text->history, cmd->location, cmd->text, -1);
}

static void
command_delete_key (const Command *cmd,
                    Text          *text)
{
  do_delete (text,
             cmd->location,
             cmd->end_location,
             cmd->text,
             strlen (cmd->text));
  gtk_text_history_text_deleted (text->history,
                                 cmd->location,
                                 cmd->end_location,
                                 cmd->text,
                                 ABS (cmd->end_location - cmd->location));
}

static void
command_undo (const Command *cmd,
              Text          *text)
{
  gtk_text_history_undo (text->history);
}

static void
command_redo (const Command *cmd,
              Text          *text)
{
  gtk_text_history_redo (text->history);
}

static void
set_selection (Text *text,
               int   begin,
               int   end)
{
  gtk_text_history_selection_changed (text->history, begin, end);
}

static void
run_test (const Command *commands,
          guint          n_commands,
          guint          max_undo)
{
  Text *text = text_new ();

  if (max_undo)
    gtk_text_history_set_max_undo_levels (text->history, max_undo);

  for (guint i = 0; i < n_commands; i++)
    {
      const Command *cmd = &commands[i];

#ifdef DEBUG_COMMANDS
      g_printerr ("%d: %d\n", i, cmd->kind);
#endif

      switch (cmd->kind)
        {
        case INSERT:
          command_insert (cmd, text);
          break;

        case INSERT_SEQ:
          for (guint j = 0; cmd->text[j]; j++)
            {
              const char seqtext[2] = { cmd->text[j], 0 };
              Command seq = { INSERT, cmd->location + j, 1, seqtext, NULL };
              command_insert (&seq, text);
            }
          break;

        case DELETE_KEY:
          if (cmd->select == DO_SELECT)
            set_selection (text, cmd->location, cmd->end_location);
          else if (strlen (cmd->text) == 1)
            set_selection (text, cmd->location, -1);
          command_delete_key (cmd, text);
          break;

        case BACKSPACE:
          if (cmd->select == DO_SELECT)
            set_selection (text, cmd->location, cmd->end_location);
          else if (strlen (cmd->text) == 1)
            set_selection (text, cmd->end_location, -1);
          command_delete_key (cmd, text);
          break;

        case UNDO:
          command_undo (cmd, text);
          break;

        case REDO:
          command_redo (cmd, text);
          break;

        case BEGIN_USER:
          gtk_text_history_begin_user_action (text->history);
          break;

        case END_USER:
          gtk_text_history_end_user_action (text->history);
          break;

        case BEGIN_IRREVERSIBLE:
          gtk_text_history_begin_irreversible_action (text->history);
          break;

        case END_IRREVERSIBLE:
          gtk_text_history_end_irreversible_action (text->history);
          break;

        case MODIFIED:
          gtk_text_history_modified_changed (text->history, TRUE);
          break;

        case UNMODIFIED:
          gtk_text_history_modified_changed (text->history, FALSE);
          break;

        case SELECT:
          gtk_text_history_selection_changed (text->history,
                                              cmd->location,
                                              cmd->end_location);
          break;

        case CHECK_SELECT:
          g_assert_cmpint (text->selection.insert, ==, cmd->location);
          g_assert_cmpint (text->selection.bound, ==, cmd->end_location);
          break;

        case SET_MAX_UNDO:
          /* Not ideal use of location, but fine */
          gtk_text_history_set_max_undo_levels (text->history, cmd->location);
          break;

        default:
          break;
        }

      if (cmd->expected)
        g_assert_cmpstr (text->buf->str, ==, cmd->expected);

      if (cmd->can_redo == SET)
        g_assert_cmpint (text->can_redo, ==, TRUE);
      else if (cmd->can_redo == UNSET)
        g_assert_cmpint (text->can_redo, ==, FALSE);

      if (cmd->can_undo == SET)
        g_assert_cmpint (text->can_undo, ==, TRUE);
      else if (cmd->can_undo == UNSET)
        g_assert_cmpint (text->can_undo, ==, FALSE);

      if (cmd->is_modified == SET)
        g_assert_cmpint (text->is_modified, ==, TRUE);
      else if (cmd->is_modified == UNSET)
        g_assert_cmpint (text->is_modified, ==, FALSE);
    }

  text_free (text);
}

static void
test1 (void)
{
  static const Command commands[] = {
    { INSERT, 0, -1, "test", "test", SET, UNSET },
    { INSERT, 2, -1, "s", "tesst", SET, UNSET },
    { INSERT, 3, -1, "ss", "tesssst", SET, UNSET },
    { DELETE_KEY, 2, 5, "sss", "test", SET, UNSET },
    { UNDO, -1, -1, NULL, "tesssst", SET, SET },
    { REDO, -1, -1, NULL, "test", SET, UNSET },
    { UNDO, -1, -1, NULL, "tesssst", SET, SET },
    { DELETE_KEY, 0, 7, "tesssst", "", SET, UNSET },
    { INSERT, 0, -1, "z", "z", SET, UNSET },
    { UNDO, -1, -1, NULL, "", SET, SET },
    { UNDO, -1, -1, NULL, "tesssst", SET, SET },
    { UNDO, -1, -1, NULL, "test", SET, SET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test2 (void)
{
  static const Command commands[] = {
    { BEGIN_IRREVERSIBLE, -1, -1, NULL, "", UNSET, UNSET },
    { INSERT, 0, -1, "this is a test", "this is a test", UNSET, UNSET },
    { END_IRREVERSIBLE, -1, -1, NULL, "this is a test", UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test", UNSET, UNSET },
    { REDO, -1, -1, NULL, "this is a test", UNSET, UNSET },
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET },
    { INSERT, 0, -1, "first", "firstthis is a test", UNSET, UNSET },
    { INSERT, 5, -1, " ", "first this is a test", UNSET, UNSET },
    { END_USER, -1, -1, NULL, "first this is a test", SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test", UNSET, SET },
    { UNDO, -1, -1, NULL, "this is a test", UNSET, SET },
    { REDO, -1, -1, NULL, "first this is a test", SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test", UNSET, SET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test3 (void)
{
  static const Command commands[] = {
    { INSERT_SEQ, 0, -1, "this is a test of insertions.", "this is a test of insertions.", SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test of", SET, SET },
    { UNDO, -1, -1, NULL, "this is a test", SET, SET },
    { UNDO, -1, -1, NULL, "this is a", SET, SET },
    { UNDO, -1, -1, NULL, "this is", SET, SET },
    { UNDO, -1, -1, NULL, "this", SET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET },
    { UNDO, -1, -1, NULL, "" , UNSET, SET },
    { REDO, -1, -1, NULL, "this", SET, SET },
    { REDO, -1, -1, NULL, "this is", SET, SET },
    { REDO, -1, -1, NULL, "this is a", SET, SET },
    { REDO, -1, -1, NULL, "this is a test", SET, SET },
    { REDO, -1, -1, NULL, "this is a test of", SET, SET },
    { REDO, -1, -1, NULL, "this is a test of insertions.", SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test4 (void)
{
  static const Command commands[] = {
    { INSERT, 0, -1, "initial text", "initial text", SET, UNSET },
    /* Barrier */
    { BEGIN_IRREVERSIBLE, -1, -1, NULL, NULL, UNSET, UNSET },
    { END_IRREVERSIBLE, -1, -1, NULL, NULL, UNSET, UNSET },
    { INSERT, 0, -1, "more text ", "more text initial text", SET, UNSET },
    { UNDO, -1, -1, NULL, "initial text", UNSET, SET },
    { UNDO, -1, -1, NULL, "initial text", UNSET, SET },
    { REDO, -1, -1, NULL, "more text initial text", SET, UNSET },
    /* Barrier */
    { BEGIN_IRREVERSIBLE, UNSET, UNSET },
    { END_IRREVERSIBLE, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "more text initial text", UNSET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test5 (void)
{
  static const Command commands[] = {
    { INSERT, 0, -1, "initial text", "initial text", SET, UNSET },
    { DELETE_KEY, 0, 12, "initial text", "", SET, UNSET },
    /* Add empty nested user action (should get ignored) */
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET },
      { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET },
        { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET },
        { END_USER, -1, -1, NULL, NULL, UNSET, UNSET },
      { END_USER, -1, -1, NULL, NULL, UNSET, UNSET },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET },
    { UNDO, -1, -1, NULL, "initial text" },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test6 (void)
{
  static const Command commands[] = {
    { INSERT_SEQ, 0, -1, " \t\t    this is some text", " \t\t    this is some text", SET, UNSET },
    { UNDO, -1, -1, NULL, " \t\t    this is some", SET, SET },
    { UNDO, -1, -1, NULL, " \t\t    this is", SET, SET },
    { UNDO, -1, -1, NULL, " \t\t    this", SET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test7 (void)
{
  static const Command commands[] = {
    { MODIFIED, -1, -1, NULL, NULL, UNSET, UNSET, SET },
    { UNMODIFIED, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { INSERT, 0, -1, "foo bar", "foo bar", SET, UNSET, UNSET },
    { MODIFIED, -1, -1, NULL, NULL, SET, UNSET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET, UNSET },
    { REDO, -1, -1, NULL, "foo bar", SET, UNSET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET, UNSET },
    { REDO, -1, -1, NULL, "foo bar", SET, UNSET, SET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test8 (void)
{
  static const Command commands[] = {
    { INSERT, 0, -1, "foo bar", "foo bar", SET, UNSET, UNSET },
    { MODIFIED, -1, -1, NULL, NULL, SET, UNSET, SET },
    { INSERT, 0, -1, "f", "ffoo bar", SET, UNSET, SET },
    { UNMODIFIED, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "foo bar", SET, SET, SET },
    { UNDO, -1, -1, NULL, "", UNSET, SET, SET },
    { REDO, -1, -1, NULL, "foo bar", SET, SET, SET },
    { REDO, -1, -1, NULL, "ffoo bar", SET, UNSET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test9 (void)
{
  static const Command commands[] = {
    { INSERT, 0, -1, "foo bar", "foo bar", SET, UNSET, UNSET },
    { DELETE_KEY, 0, 3, "foo", " bar", SET, UNSET, UNSET, DO_SELECT },
    { DELETE_KEY, 0, 4, " bar", "", SET, UNSET, UNSET, DO_SELECT },
    { UNDO, -1, -1, NULL, " bar", SET, SET, UNSET },
    { CHECK_SELECT, 0, 4, NULL, " bar", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "foo bar", SET, SET, UNSET },
    { CHECK_SELECT, 0, 3, NULL, "foo bar", SET, SET, UNSET },
    { BEGIN_IRREVERSIBLE, -1, -1, NULL, "foo bar", UNSET, UNSET, UNSET },
    { END_IRREVERSIBLE, -1, -1, NULL, "foo bar", UNSET, UNSET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test10 (void)
{
  static const Command commands[] = {
    { BEGIN_USER }, { INSERT, 0, -1, "t", "t", UNSET, UNSET, UNSET }, { END_USER },
    { BEGIN_USER }, { INSERT, 1, -1, " ", "t ", UNSET, UNSET, UNSET }, { END_USER },
    { BEGIN_USER }, { INSERT, 2, -1, "t", "t t", UNSET, UNSET, UNSET }, { END_USER },
    { BEGIN_USER }, { INSERT, 3, -1, "h", "t th", UNSET, UNSET, UNSET }, { END_USER },
    { BEGIN_USER }, { INSERT, 4, -1, "i", "t thi", UNSET, UNSET, UNSET }, { END_USER },
    { BEGIN_USER }, { INSERT, 5, -1, "s", "t this", UNSET, UNSET, UNSET }, { END_USER },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test11 (void)
{
  /* Test backspace */
  static const Command commands[] = {
    { INSERT_SEQ, 0, -1, "insert some text", "insert some text", SET, UNSET, UNSET },
    { BACKSPACE, 15, 16, "t", "insert some tex", SET, UNSET, UNSET },
    { BACKSPACE, 14, 15, "x", "insert some te", SET, UNSET, UNSET },
    { BACKSPACE, 13, 14, "e", "insert some t", SET, UNSET, UNSET },
    { BACKSPACE, 12, 13, "t", "insert some ", SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "insert some text", SET, SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test12 (void)
{
  static const Command commands[] = {
    { INSERT_SEQ, 0, -1, "this is a test\nmore", "this is a test\nmore", SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "", UNSET, SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test13 (void)
{
  static const Command commands[] = {
    { INSERT_SEQ, 0, -1, "this is a test\nmore", "this is a test\nmore", SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test", SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a", UNSET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a", UNSET, SET, UNSET },
    { SET_MAX_UNDO, 2, -1, NULL, "this is a", UNSET, SET, UNSET },
    { REDO, -1, -1, NULL, "this is a test", SET, SET, UNSET },
    { REDO, -1, -1, NULL, "this is a test\n", SET, UNSET, UNSET },
    { REDO, -1, -1, NULL, "this is a test\n", SET, UNSET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 3);
}

static void
test14 (void)
{
  char *fill = g_strnfill (1024, 'x');
  char *fill_after = g_strnfill (1025, 'x');
  char *fill_after_2 = g_strdup_printf ("%s word", fill_after);
  const Command commands[] = {
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { INSERT, 0, -1, fill, fill, UNSET, UNSET, UNSET },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { INSERT, 0, -1, "x", fill_after, UNSET, UNSET, UNSET },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { INSERT_SEQ, strlen(fill_after), -1, " word", fill_after_2, UNSET, UNSET, UNSET },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, fill_after, SET, SET, UNSET },
    { UNDO, -1, -1, NULL, fill, SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "", UNSET, SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);

  g_free (fill);
  g_free (fill_after);
  g_free (fill_after_2);
}

static void
test_issue_4276 (void)
{
  const Command commands[] = {
    { INSERT, 0, -1, "this is some text", "this is some text", SET, UNSET, UNSET },
    { SELECT, 0, 17, NULL, "this is some text", SET, UNSET, UNSET },
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { DELETE_KEY, 0, 17, "this is some text", "", UNSET, UNSET, UNSET },
    { INSERT, 0, -1, "z", "z", UNSET, UNSET, UNSET },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { INSERT, 1, -1, "zzz", "zzzz", SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "z", SET, SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test_issue_4575 (void)
{
  const Command commands[] = {
    { INSERT, 0, -1, "this is some text", "this is some text", SET, UNSET, UNSET },
    { SELECT, 5, 8, NULL, NULL, SET, UNSET, UNSET },
    { BEGIN_USER, -1, -1, NULL, NULL, UNSET, UNSET, UNSET },
    { DELETE_KEY, 5, 8, "is ", "this some text", UNSET, UNSET, UNSET, IGNORE_SELECT },
    { END_USER, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is some text", SET, SET, UNSET },
    { CHECK_SELECT, 5, 8, NULL, "this is some text", SET, SET, UNSET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 0);
}

static void
test_issue_5777 (void)
{
  static const Command commands[] = {
    { MODIFIED, -1, -1, NULL, NULL, UNSET, UNSET, SET },
    { INSERT_SEQ, 0, -1, "this is a test\nmore", "this is a test\nmore", SET, UNSET, SET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, SET },
    { UNMODIFIED, -1, -1, NULL, NULL, SET, SET, UNSET },
    { REDO, -1, -1, NULL, "this is a test\nmore", SET, UNSET, SET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, UNSET },
    { REDO, -1, -1, NULL, "this is a test\nmore", SET, UNSET, SET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, UNSET },
    { MODIFIED, -1, -1, NULL, NULL, SET, SET, SET },
    { REDO, -1, -1, NULL, "this is a test\nmore", SET, UNSET, SET },
    { UNMODIFIED, -1, -1, NULL, NULL, SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, SET },
    { REDO, -1, -1, NULL, "this is a test\nmore", SET, UNSET, UNSET },
    { UNDO, -1, -1, NULL, "this is a test\n", SET, SET, SET },
    { UNDO, -1, -1, NULL, "this is a test", SET, SET, SET },
    { UNMODIFIED, -1, -1, NULL, NULL, SET, SET, UNSET },
    { UNDO, -1, -1, NULL, "this is a", SET, SET, SET },
    { REDO, -1, -1, NULL, "this is a test", SET, SET, UNSET },
    { REDO, -1, -1, NULL, "this is a test\n", SET, SET, SET },
    { REDO, -1, -1, NULL, "this is a test\nmore", SET, UNSET, SET },
  };

  run_test (commands, G_N_ELEMENTS (commands), 4);
}

int
main (int   argc,
      char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/Gtk/TextHistory/test1", test1);
  g_test_add_func ("/Gtk/TextHistory/test2", test2);
  g_test_add_func ("/Gtk/TextHistory/test3", test3);
  g_test_add_func ("/Gtk/TextHistory/test4", test4);
  g_test_add_func ("/Gtk/TextHistory/test5", test5);
  g_test_add_func ("/Gtk/TextHistory/test6", test6);
  g_test_add_func ("/Gtk/TextHistory/test7", test7);
  g_test_add_func ("/Gtk/TextHistory/test8", test8);
  g_test_add_func ("/Gtk/TextHistory/test9", test9);
  g_test_add_func ("/Gtk/TextHistory/test10", test10);
  g_test_add_func ("/Gtk/TextHistory/test11", test11);
  g_test_add_func ("/Gtk/TextHistory/test12", test12);
  g_test_add_func ("/Gtk/TextHistory/test13", test13);
  g_test_add_func ("/Gtk/TextHistory/test14", test14);
  g_test_add_func ("/Gtk/TextHistory/issue_4276", test_issue_4276);
  g_test_add_func ("/Gtk/TextHistory/issue_4575", test_issue_4575);
  g_test_add_func ("/Gtk/TextHistory/issue_5777", test_issue_5777);

  return g_test_run ();
}
