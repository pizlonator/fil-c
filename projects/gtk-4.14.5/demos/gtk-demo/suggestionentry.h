#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define MATCH_TYPE_OBJECT                 (match_object_get_type ())
#define MATCH_OBJECT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), MATCH_TYPE_OBJECT, MatchObject))
#define MATCH_IS_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MATCH_TYPE_OBJECT))

typedef struct _MatchObject MatchObject;

GType            match_object_get_type (void) G_GNUC_CONST;

gpointer         match_object_get_item        (MatchObject *object);
const char *     match_object_get_string      (MatchObject *object);
guint            match_object_get_match_start (MatchObject *object);
guint            match_object_get_match_end   (MatchObject *object);
guint            match_object_get_score       (MatchObject *object);
void             match_object_set_match       (MatchObject *object,
                                               guint        start,
                                               guint        end,
                                               guint        score);

#define SUGGESTION_TYPE_ENTRY               (suggestion_entry_get_type ())
#define SUGGESTION_ENTRY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SUGGESTION_TYPE_ENTRY, SuggestionEntry))
#define SUGGESTION_IS_ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SUGGESTION_TYPE_ENTRY))

typedef struct _SuggestionEntry       SuggestionEntry;

GType           suggestion_entry_get_type (void) G_GNUC_CONST;

GtkWidget*      suggestion_entry_new                (void);

void            suggestion_entry_set_model          (SuggestionEntry     *self,
                                                     GListModel          *model);
GListModel *    suggestion_entry_get_model          (SuggestionEntry     *self);

void            suggestion_entry_set_factory        (SuggestionEntry     *self,
                                                     GtkListItemFactory  *factory);
GtkListItemFactory *
                suggestion_entry_get_factory        (SuggestionEntry     *self);

void            suggestion_entry_set_use_filter     (SuggestionEntry     *self,
                                                     gboolean             use_ilter);
gboolean        suggestion_entry_get_use_filter     (SuggestionEntry     *self);

void            suggestion_entry_set_expression     (SuggestionEntry     *self,
                                                     GtkExpression       *expression);
GtkExpression * suggestion_entry_get_expression     (SuggestionEntry     *self);

void            suggestion_entry_set_show_arrow     (SuggestionEntry     *self,
                                                     gboolean             show_arrow);
gboolean        suggestion_entry_get_show_arrow     (SuggestionEntry     *self);

typedef void (* SuggestionEntryMatchFunc)           (MatchObject *object,
                                                     const char  *search,
                                                     gpointer     user_data);

void            suggestion_entry_set_match_func     (SuggestionEntry          *self,
                                                     SuggestionEntryMatchFunc  func,
                                                     gpointer                  user_data,
                                                     GDestroyNotify            destroy);

G_END_DECLS
