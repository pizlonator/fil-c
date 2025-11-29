/*
 * Copyright (C) 2007  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <Cocoa/Cocoa.h>
#include <macos/gdkmacos.h>

#include "gtksearchenginequartzprivate.h"

/* This file is a mixture of an objective-C object and a GObject,
 * so be careful to not confuse yourself.
 */

#define QUARTZ_POOL_ALLOC NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define QUARTZ_POOL_RELEASE [pool release]


/* Definition of objective-c object */
@interface ResultReceiver : NSObject
{
  int submitted_hits;
  GtkSearchEngine *engine;
}

- (void) setEngine:(GtkSearchEngine *)quartz_engine;

- (void) queryUpdate:(id)sender;
- (void) queryProgress:(id)sender;
- (void) queryFinished:(id)sender;

@end


/* Definition of GObject */
struct _GtkSearchEngineQuartzPrivate
{
  GtkQuery *query;

  ResultReceiver *receiver;
  NSMetadataQuery *ns_query;

  gboolean query_finished;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSearchEngineQuartz, _gtk_search_engine_quartz, GTK_TYPE_SEARCH_ENGINE)


/* Implementation of the objective-C object */
@implementation ResultReceiver

- (void) setEngine:(GtkSearchEngine *)quartz_engine
{
  g_return_if_fail (GTK_IS_SEARCH_ENGINE (quartz_engine));

  engine = quartz_engine;
  submitted_hits = 0;
}

- (void) submitHits:(NSMetadataQuery *)ns_query
{
  int i;
  GList *hits = NULL;
  /* The max was originally set to 1000 to mimic something called "the
   * boogie backend". submitted_hits contains the number of hits we've
   * processed in previous calls to this function.
   */
  const unsigned int max_hits = 1000 - submitted_hits;
  const unsigned int max_iter = [ns_query resultCount];

  for (i = 0; i < max_iter && i < max_hits; ++i)
    {
      id result = [ns_query resultAtIndex:i];
      const char *result_path;
      GFile *file;
      GtkSearchHit *hit;

      result_path = [[result valueForAttribute:@"kMDItemPath"] UTF8String];

      if (result_path == NULL)
        continue;

      file = g_file_new_for_path (result_path);

      hit = g_new (GtkSearchHit, 1);
      hit->file = file;
      hit->info = NULL;

      hits = g_list_prepend (hits, hit);
    }

  _gtk_search_engine_hits_added (engine, hits);
  g_list_free_full (hits, (GDestroyNotify) _gtk_search_hit_free);

  if (max_iter >= max_hits)
    [ns_query stopQuery];

  submitted_hits += max_iter;
}

- (void) queryUpdate:(id)sender
{
  NSMetadataQuery *ns_query = [sender object];

  [self submitHits:ns_query];
}

- (void) queryProgress:(id)sender
{
  NSMetadataQuery *ns_query = [sender object];

  [self submitHits:ns_query];
}

- (void) queryFinished:(id)sender
{
  NSMetadataQuery *ns_query = [sender object];

  [self submitHits:ns_query];

  _gtk_search_engine_finished (engine, submitted_hits > 0);
  submitted_hits = 0;
}

@end

/* Implementation of the GObject */

static void
gtk_search_engine_quartz_finalize (GObject *object)
{
  GtkSearchEngineQuartz *quartz;

  QUARTZ_POOL_ALLOC;

  quartz = GTK_SEARCH_ENGINE_QUARTZ (object);

  [[NSNotificationCenter defaultCenter] removeObserver:quartz->priv->receiver];

  [quartz->priv->ns_query release];
  [quartz->priv->receiver release];

  QUARTZ_POOL_RELEASE;

  if (quartz->priv->query)
    {
      g_object_unref (quartz->priv->query);
      quartz->priv->query = NULL;
    }

  G_OBJECT_CLASS (_gtk_search_engine_quartz_parent_class)->finalize (object);
}

static void
gtk_search_engine_quartz_start (GtkSearchEngine *engine)
{
  GtkSearchEngineQuartz *quartz;

  QUARTZ_POOL_ALLOC;

  quartz = GTK_SEARCH_ENGINE_QUARTZ (engine);

  [quartz->priv->ns_query startQuery];

  QUARTZ_POOL_RELEASE;
}

static void
gtk_search_engine_quartz_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineQuartz *quartz;

  QUARTZ_POOL_ALLOC;

  quartz = GTK_SEARCH_ENGINE_QUARTZ (engine);

  [quartz->priv->ns_query stopQuery];

  QUARTZ_POOL_RELEASE;
}

static void
gtk_search_engine_quartz_set_query (GtkSearchEngine *engine, 
				    GtkQuery        *query)
{
  GtkSearchEngineQuartz *quartz;
  const char* path = NULL;
  GFile *location = NULL;

  QUARTZ_POOL_ALLOC;

  quartz = GTK_SEARCH_ENGINE_QUARTZ (engine);

  if (query)
    g_object_ref (query);

  if (quartz->priv->query)
    g_object_unref (quartz->priv->query);

  quartz->priv->query = query;
  location = gtk_query_get_location (query);

  if (location)
    path = g_file_peek_path (location);

  /* We create a query to look for ".*text.*" in the text contents of
   * all indexed files.  (Should we also search for text in file and folder
   * names?).
   */

  if (path)
    {
      NSString *ns_path = [[NSString string] initWithUTF8String:path];
      [quartz->priv->ns_query setSearchScopes:@[ns_path]];
    }
  else
    {
      [quartz->priv->ns_query setSearchScopes:@[NSMetadataQueryLocalComputerScope]];
    }

  [quartz->priv->ns_query setSearchItems:@[(NSString*)kMDItemTextContent,
                                           (NSString*)kMDItemFSName]];
  [quartz->priv->ns_query setPredicate:
    [NSPredicate predicateWithFormat:
      [NSString stringWithFormat:@"(kMDItemTextContent LIKE[cd] \"*%s*\")",
                                 gtk_query_get_text (query)]]];

  QUARTZ_POOL_RELEASE;
}

static void
_gtk_search_engine_quartz_class_init (GtkSearchEngineQuartzClass *class)
{
  GObjectClass *gobject_class;
  GtkSearchEngineClass *engine_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = gtk_search_engine_quartz_finalize;
  
  engine_class = GTK_SEARCH_ENGINE_CLASS (class);
  engine_class->set_query = gtk_search_engine_quartz_set_query;
  engine_class->start = gtk_search_engine_quartz_start;
  engine_class->stop = gtk_search_engine_quartz_stop;
}

static void
_gtk_search_engine_quartz_init (GtkSearchEngineQuartz *engine)
{
  QUARTZ_POOL_ALLOC;

  engine->priv = _gtk_search_engine_quartz_get_instance_private (engine);

  engine->priv->ns_query = [[NSMetadataQuery alloc] init];
  engine->priv->receiver = [[ResultReceiver alloc] init];

  [engine->priv->receiver setEngine:GTK_SEARCH_ENGINE (engine)];

  [[NSNotificationCenter defaultCenter] addObserver:engine->priv->receiver
				        selector:@selector(queryUpdate:)
				        name:@"NSMetadataQueryDidUpdateNotification"
				        object:engine->priv->ns_query];
  [[NSNotificationCenter defaultCenter] addObserver:engine->priv->receiver
				        selector:@selector(queryFinished:)
				        name:@"NSMetadataQueryDidFinishGatheringNotification"
				        object:engine->priv->ns_query];

  [[NSNotificationCenter defaultCenter] addObserver:engine->priv->receiver
				        selector:@selector(queryProgress:)
				        name:@"NSMetadataQueryGatheringProgressNotification"
				        object:engine->priv->ns_query];

  QUARTZ_POOL_RELEASE;
}

GtkSearchEngine *
_gtk_search_engine_quartz_new (void)
{
#ifdef GDK_WINDOWING_MACOS
  return g_object_new (GTK_TYPE_SEARCH_ENGINE_QUARTZ, NULL);
#else
  return NULL;
#endif
}
