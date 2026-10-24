#include <stdfil.h>

#define PUBLIC __attribute__((visibility("default")))
#define PROTECTED __attribute__((visibility("protected")))
#define NOINLINE __attribute__((noinline))

/* The command-line default must apply to every generated entrypoint. */
NOINLINE int hidden_function(void) { return 37; }
PROTECTED NOINLINE int protected_function(void) { return 43; }
PUBLIC NOINLINE int default_function(void) { return 53; }
int hidden_global = 19;
__thread int hidden_tls = 17;
PROTECTED __thread int protected_tls = 23;

PUBLIC int read_hidden(void) { return hidden_function(); }
PUBLIC int read_protected(void) { return protected_function(); }
PUBLIC int read_default(void) { return default_function(); }
PUBLIC int (*hidden_pointer(void))(void) { return hidden_function; }
PUBLIC int (*protected_pointer(void))(void) { return protected_function; }
PUBLIC int (*default_pointer(void))(void) { return default_function; }
PUBLIC int *consumer_tls(void) { return &hidden_tls; }
PUBLIC int *consumer_protected_tls(void) { return &protected_tls; }
PUBLIC int *consumer_global(void) { return &hidden_global; }

/* An exported source alias may intentionally name a hidden implementation. */
PUBLIC int public_alias(void) __attribute__((alias("hidden_function")));

/* zcallee implementations keep local linkage, hence default IR visibility. */
NOINLINE void *hidden_callee(void) { return zcallee(); }
PUBLIC int check_callee(void) { return hidden_callee() == hidden_callee; }
