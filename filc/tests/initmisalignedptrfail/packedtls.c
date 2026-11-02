int anchor;
struct __attribute__((packed)) Packed { char prefix; int *pointer; };
_Thread_local struct Packed packed_tls = { 1, &anchor };
/* Thread-local initialization stores null pointer leaves too. */
_Thread_local struct Packed null_tls = { 1, 0 };
struct Packed *expose(void) { return &packed_tls; }
struct Packed *expose_null(void) { return &null_tls; }
