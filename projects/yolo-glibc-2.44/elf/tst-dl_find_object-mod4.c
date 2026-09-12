char mod4_data;

void
mod4_function (void (*f) (void))
{
  /* Make sure this is not a tail call and unwind information is
     therefore needed.  */
  f ();
  f ();
}
