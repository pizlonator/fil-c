#include "ruby.h"
#include "rubyspec.h"

#ifdef __cplusplus
extern "C" {
#endif

static VALUE language_spec_switch(VALUE self, VALUE value) {
  if (value == ID2SYM(rb_intern("undef"))) {
    value = Qundef;
  }

  switch ((uintptr_t)value) {
    case (uintptr_t)Qtrue:
      return ID2SYM(rb_intern("true"));
    case (uintptr_t)Qfalse:
      return ID2SYM(rb_intern("false"));
    case (uintptr_t)Qnil:
      return ID2SYM(rb_intern("nil"));
    case (uintptr_t)Qundef:
      return ID2SYM(rb_intern("undef"));
    default:
      return ID2SYM(rb_intern("default"));
  }
}

/* Defining a local variable rb_mProcess which already exists as a global variable
 * For instance eventmachine does this in Init_rubyeventmachine() */
static VALUE language_spec_global_local_var(VALUE self) {
  VALUE rb_mProcess = rb_const_get(rb_cObject, rb_intern("Process"));
  return rb_mProcess;
}

void Init_language_spec(void) {
  VALUE cls = rb_define_class("CApiLanguageSpecs", rb_cObject);
  rb_define_method(cls, "switch", language_spec_switch, 1);
  rb_define_method(cls, "global_local_var", language_spec_global_local_var, 0);
}

#ifdef __cplusplus
}
#endif
