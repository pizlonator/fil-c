#ifndef GI_SAMPLE_H
#define GI_SAMPLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GI_TYPE_SAMPLE_THING (gi_sample_thing_get_type())

G_DECLARE_FINAL_TYPE (GISampleThing, gi_sample_thing, GI, SAMPLE_THING, GObject)

GISampleThing    *gi_sample_thing_new           (void);
void              gi_sample_thing_print_message (GISampleThing *self);

G_END_DECLS

#endif /* GI_SAMPLE_H */
