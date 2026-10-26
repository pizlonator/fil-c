/* Global initializers store pointer leaves without access checks. A pointer
 * at byte 1 would put its capability across two aux words, so reject it
 * rather than assert or fall back to the unchecked recursive store. */
int anchor;
struct __attribute__((packed)) Packed { char prefix; int *pointer; };
struct Packed packed_global = { 1, &anchor };
struct Packed *expose(void) { return &packed_global; }
