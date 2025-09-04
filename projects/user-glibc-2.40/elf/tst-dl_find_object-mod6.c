/* Large to get different layouts.  */
char mod6_data[4096];

void
mod6_function (void (*f) (void))
{
  /* Make sure this is not a tail call and unwind information is
     therefore needed.  */
  f ();
  f ();
}
