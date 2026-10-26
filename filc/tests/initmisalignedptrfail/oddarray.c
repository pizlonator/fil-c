/* An underaligned union compiles, but the second element of this array
 * places its pointer word at byte 9. */
int anchor;
union __attribute__((packed)) Odd { int *pointer; char bytes[9]; };
union Odd odd_global[2] = { { .pointer = &anchor }, { .pointer = &anchor } };
_Thread_local union Odd odd_tls[2] = { { .pointer = &anchor }, { .pointer = &anchor } };
union Odd *expose(void) { return odd_global; }
union Odd *expose_tls(void) { return odd_tls; }
