char mod7_data;

void
mod7_function (void (*f) (void))
{
  /* Make sure this is not a tail call and unwind information is
     therefore needed.  */
  f ();
  f ();
}
