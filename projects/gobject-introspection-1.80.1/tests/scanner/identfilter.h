#ifndef __IDENTFILTER_H__
#define __IDENTFILTER_H__

typedef struct _identfilter identfilter_t;
typedef struct _identfilter_object identfilter_object_t;

identfilter_object_t *identfilter_object_new (void);
void identfilter_object_foo_method (identfilter_object_t *self);
void identfilter_object_free (identfilter_object_t *self);

#endif
