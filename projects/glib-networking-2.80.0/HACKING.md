# Code Style

In order to keep the code nice and clean we have a few requirements you'll
need to stick to in order to get your patch accepted:

 * Use GNU-style indentation:

   ```
   if (condition)
     {
       // body
     }
   ```

 * No braces for one line control clauses, except when another clause in the
   chain contains more than one line:

  ```
  if (condition)
    look_no_braces ();

  if (condition)
    {
      // Use braces even though it's only one statement, because
      // the condition is multiple lines long.
      function_call_with_many_arguments (arg1, arg2, arg3,
                                         arg4, arg5, arg6);
    }
  else
    {
      // Use braces because the clause above did.
    }
  ```

 * Callback functions have a suffix _cb. TODO: ensure existing code follows this
   rule.

 * Use `char`/`int`/`double`/…, not `gchar`/`gint`/`gdouble`/… types, except
   when implementing GLib vfuncs that use these types. TODO: ensure existing
   code follows this rule.

 * All implementation files must include first `"config.h"`, followed by
   the primary header, followed by a blank line, followed by all the
   local headers sorted alphabetically, followed by a blank line,
   followed by all the system headers sorted alphabetically. Headers
   should follow the same pattern excluding the config.h and
   self file section, for obvious reasons. TODO: ensure existing code follows
   this rule.

 * There's no space between a type cast and the variable name:  Right:
   `(int *)foo`. Wrong: `(int*) foo`.

 * Avoid explicit comparisons against TRUE, FALSE, and NULL. Right:
   `if (!condition)`, `if (!pointer)`, `if (integer == 0)`. Wrong:
   `if (condition == FALSE)`, `if (pointer == NULL)`, `if (!integer)`.
   Exception: `pointer != NULL` may be used to convert to gboolean since some
   developers find this more natural than `!!pointer`.
