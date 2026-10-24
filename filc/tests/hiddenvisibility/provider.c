/* Deliberately collide with private names in libconsumer. */
#define PUBLIC __attribute__((visibility("default")))

PUBLIC int hidden_function(void) { return 91; }
PUBLIC int protected_function(void) { return 93; }
PUBLIC int default_function(void) { return 97; }
PUBLIC int hidden_global = 113;
PUBLIC __thread int hidden_tls = 111;
PUBLIC __thread int protected_tls = 115;

PUBLIC int *provider_tls(void) { return &hidden_tls; }
PUBLIC int *provider_protected_tls(void) { return &protected_tls; }
PUBLIC int *provider_global(void) { return &hidden_global; }
