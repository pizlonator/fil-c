#include "config.h"

#include "broadway-server.h"

#include "broadway-output.h"

#include <glib.h>
#include <glib/gprintf.h>
#include "gdktypes.h"
#include "gdkdeviceprivate.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined (G_OS_WIN32)
#include <io.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef G_OS_UNIX
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#include <string.h>
#endif

typedef struct {
  int id;
  guint32 tag;
} BroadwayOutstandingRoundtrip;

typedef struct BroadwayInput BroadwayInput;
typedef struct BroadwaySurface BroadwaySurface;
struct _BroadwayServer {
  GObject parent_instance;

  char *address;
  int port;
  char *ssl_cert;
  char *ssl_key;
  GSocketService *service;
  BroadwayOutput *output;
  guint32 id_counter;
  guint32 saved_serial;
  guint64 last_seen_time;
  BroadwayInput *input;
  GList *input_messages;
  guint process_input_idle;

  GHashTable *surface_id_hash;
  GList *surfaces;
  BroadwaySurface *root;
  gint32 focused_surface_id; /* -1 => none */
  int show_keyboard;

  guint32 next_texture_id;
  GHashTable *textures;

  guint32 screen_scale;

  gint32 mouse_in_surface_id;
  int last_x, last_y; /* in root coords */
  guint32 last_state;
  gint32 real_mouse_in_surface_id; /* Not affected by grabs */

  /* Explicit pointer grabs: */
  gint32 pointer_grab_surface_id; /* -1 => none */
  gint32 pointer_grab_client_id; /* -1 => none */
  guint32 pointer_grab_time;
  gboolean pointer_grab_owner_events;

  /* Future data, from the currently queued events */
  int future_root_x;
  int future_root_y;
  guint32 future_state;
  int future_mouse_in_surface;

  GList *outstanding_roundtrips;
};

struct _BroadwayServerClass
{
  GObjectClass parent_class;
};

typedef struct HttpRequest {
  BroadwayServer *server;
  GSocketConnection *socket_connection;
  GIOStream *connection;
  GDataInputStream *data;
  GString *request;
}  HttpRequest;

struct BroadwayInput {
  BroadwayServer *server;
  BroadwayOutput *output;
  GIOStream *connection;
  GByteArray *buffer;
  GSource *source;
  gboolean seen_time;
  gint64 time_base;
  gboolean active;
};

struct BroadwaySurface {
  guint32 owner;
  gint32 id;
  gint32 x;
  gint32 y;
  gint32 width;
  gint32 height;
  gboolean visible;
  gint32 transient_for;
  guint32 texture;
  gboolean modal_hint;
  BroadwayNode *nodes;
  GHashTable *node_lookup;
};

struct _BroadwayTexture {
  grefcount refcount;
  guint32 id;
  GBytes *bytes;
};

static void broadway_server_resync_surfaces (BroadwayServer *server);
static void send_outstanding_roundtrips (BroadwayServer *server);

static void broadway_server_ref_texture (BroadwayServer   *server,
                                         guint32           id);

static GType broadway_server_get_type (void);

G_DEFINE_TYPE (BroadwayServer, broadway_server, G_TYPE_OBJECT)

static void
broadway_texture_free (BroadwayTexture *texture)
{
  g_bytes_unref (texture->bytes);
  g_free (texture);
}

static void
broadway_node_unref (BroadwayServer *server,
                     BroadwayNode *node)
{
  int i;

  if (g_ref_count_dec (&node->refcount))
    {
      for (i = 0; i < node->n_children; i++)
        broadway_node_unref (server, node->children[i]);

      if (node->texture_id)
        broadway_server_release_texture (server, node->texture_id);

      g_free (node);
    }
}

static BroadwayNode *
broadway_node_ref (BroadwayNode *node)
{
  g_ref_count_inc (&node->refcount);

  return node;
}

gboolean
broadway_node_equal (BroadwayNode     *a,
                     BroadwayNode     *b)
{
  int i;

  if (a->type != b->type)
    return FALSE;

  if (a->n_data != b->n_data)
    return FALSE;

  /* Don't check data for containers, that is just n_children, which
     we don't want to compare for a shallow equal */
  if (a->type != BROADWAY_NODE_CONTAINER)
    {
      for (i = 0; i < a->n_data; i++)
        if (a->data[i] != b->data[i])
          return FALSE;
    }

  return TRUE;
}

gboolean
broadway_node_deep_equal (BroadwayNode     *a,
                          BroadwayNode     *b)
{
  int i;

  if (a->hash != b->hash)
    return FALSE;

  if (!broadway_node_equal (a,b))
    return FALSE;

  if (a->n_children != b->n_children)
    return FALSE;

  for (i = 0; i < a->n_children; i++)
    if (!broadway_node_deep_equal (a->children[i], b->children[i]))
      return FALSE;

  return TRUE;
}


void
broadway_node_mark_deep_reused (BroadwayNode    *node,
                                gboolean         reused)
{
  node->reused = reused;
  for (int i = 0; i < node->n_children; i++)
    broadway_node_mark_deep_reused (node->children[i], reused);
}

void
broadway_node_mark_deep_consumed (BroadwayNode    *node,
                                  gboolean         consumed)
{
  node->consumed = consumed;
  for (int i = 0; i < node->n_children; i++)
    broadway_node_mark_deep_consumed (node->children[i], consumed);
}

void
broadway_node_add_to_lookup (BroadwayNode    *node,
                             GHashTable      *node_lookup)
{
  g_hash_table_insert (node_lookup, GINT_TO_POINTER(node->id), node);
  for (int i = 0; i < node->n_children; i++)
    broadway_node_add_to_lookup (node->children[i], node_lookup);
}

static void
broadway_server_init (BroadwayServer *server)
{
  BroadwaySurface *root;

  server->service = g_socket_service_new ();
  server->pointer_grab_surface_id = -1;
  server->saved_serial = 1;
  server->last_seen_time = 1;
  server->surface_id_hash = g_hash_table_new (NULL, NULL);
  server->id_counter = 0;
  server->textures = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                            (GDestroyNotify)broadway_texture_free);

  root = g_new0 (BroadwaySurface, 1);
  root->id = server->id_counter++;
  root->width = 1024;
  root->height = 768;
  root->visible = TRUE;

  server->root = root;
  server->screen_scale = 1;

  g_hash_table_insert (server->surface_id_hash,
                       GINT_TO_POINTER (root->id),
                       root);
}

static void
broadway_server_finalize (GObject *object)
{
  BroadwayServer *server = BROADWAY_SERVER (object);

  g_free (server->address);
  g_free (server->ssl_cert);
  g_free (server->ssl_key);
  g_hash_table_destroy (server->textures);

  G_OBJECT_CLASS (broadway_server_parent_class)->finalize (object);
}

static void
broadway_server_class_init (BroadwayServerClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = broadway_server_finalize;
}

static void
broadway_surface_free (BroadwayServer *server,
                       BroadwaySurface *surface)
{
  if (surface->nodes)
    broadway_node_unref (server, surface->nodes);
  g_hash_table_unref (surface->node_lookup);
  g_free (surface);
}

static BroadwaySurface *
broadway_server_lookup_surface (BroadwayServer   *server,
                                guint32           id)
{
  return g_hash_table_lookup (server->surface_id_hash,
                              GINT_TO_POINTER (id));
}

static void start (BroadwayInput *input);

static void
http_request_free (HttpRequest *request)
{
  g_object_unref (request->socket_connection);
  g_object_unref (request->connection);
  g_object_unref (request->data);
  g_string_free (request->request, TRUE);
  g_free (request);
}

static void
broadway_input_free (BroadwayInput *input)
{
  g_object_unref (input->connection);
  g_byte_array_free (input->buffer, FALSE);
  g_source_destroy (input->source);
  g_free (input);
}

static void
update_event_state (BroadwayServer *server,
                    BroadwayInputMsg *message)
{
  BroadwaySurface *surface;

  switch (message->base.type) {
  case BROADWAY_EVENT_ENTER:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_surface_id = message->pointer.mouse_surface_id;

    /* TODO: Unset when it dies */
    server->mouse_in_surface_id = message->pointer.event_surface_id;
    break;
  case BROADWAY_EVENT_LEAVE:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_surface_id = message->pointer.mouse_surface_id;

    server->mouse_in_surface_id = 0;
    break;
  case BROADWAY_EVENT_POINTER_MOVE:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_surface_id = message->pointer.mouse_surface_id;
    break;
  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    if (message->base.type == BROADWAY_EVENT_BUTTON_PRESS &&
        server->focused_surface_id != message->pointer.mouse_surface_id &&
        server->pointer_grab_surface_id == -1)
      {
        broadway_server_surface_raise (server, message->pointer.mouse_surface_id);
        broadway_server_focus_surface (server, message->pointer.mouse_surface_id);
        broadway_server_flush (server);
      }

    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_surface_id = message->pointer.mouse_surface_id;
    break;
  case BROADWAY_EVENT_SCROLL:
    server->last_x = message->pointer.root_x;
    server->last_y = message->pointer.root_y;
    server->last_state = message->pointer.state;
    server->real_mouse_in_surface_id = message->pointer.mouse_surface_id;
    break;
  case BROADWAY_EVENT_TOUCH:
    if (message->touch.touch_type == 0 && message->touch.is_emulated &&
        server->focused_surface_id != message->touch.event_surface_id)
      {
        broadway_server_surface_raise (server, message->touch.event_surface_id);
        broadway_server_focus_surface (server, message->touch.event_surface_id);
        broadway_server_flush (server);
      }

    if (message->touch.is_emulated)
      {
        server->last_x = message->pointer.root_x;
        server->last_y = message->pointer.root_y;
      }

    server->last_state = message->touch.state;
    break;
  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    server->last_state = message->key.state;
    break;
  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    break;
  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    surface = broadway_server_lookup_surface (server, message->configure_notify.id);
    if (surface != NULL)
      {
        surface->x = message->configure_notify.x;
        surface->y = message->configure_notify.y;

	if (server->focused_surface_id != message->configure_notify.id &&
	    server->pointer_grab_surface_id == -1 && surface->modal_hint)
	{
	  broadway_server_surface_raise (server, message->configure_notify.id);
	  broadway_server_focus_surface (server, message->configure_notify.id);
	  broadway_server_flush (server);
	}
      }
    break;
  case BROADWAY_EVENT_ROUNDTRIP_NOTIFY:
    break;
  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    server->root->width = message->screen_resize_notify.width;
    server->root->height = message->screen_resize_notify.height;
    server->screen_scale = message->screen_resize_notify.scale;
    break;

  default:
    g_printerr ("update_event_state - Unknown input command %c\n", message->base.type);
    break;
  }
}

gboolean
broadway_server_lookahead_event (BroadwayServer  *server,
                                 const char      *types)
{
  BroadwayInputMsg *message;
  GList *l;

  for (l = server->input_messages; l != NULL; l = l->next)
    {
      message = l->data;
      if (strchr (types, message->base.type) != NULL)
        return TRUE;
    }

  return FALSE;
}

static gboolean
is_pointer_event (BroadwayInputMsg *message)
{
  return
    message->base.type == BROADWAY_EVENT_ENTER ||
    message->base.type == BROADWAY_EVENT_LEAVE ||
    message->base.type == BROADWAY_EVENT_POINTER_MOVE ||
    message->base.type == BROADWAY_EVENT_BUTTON_PRESS ||
    message->base.type == BROADWAY_EVENT_BUTTON_RELEASE ||
    message->base.type == BROADWAY_EVENT_SCROLL ||
    message->base.type == BROADWAY_EVENT_GRAB_NOTIFY ||
    message->base.type == BROADWAY_EVENT_UNGRAB_NOTIFY;
}

static void
process_input_message (BroadwayServer *server,
                       BroadwayInputMsg *message)
{
  gint32 client;
  BroadwaySurface *surface;

  update_event_state (server, message);


  switch (message->base.type) {
  case BROADWAY_EVENT_ENTER:
  case BROADWAY_EVENT_LEAVE:
  case BROADWAY_EVENT_POINTER_MOVE:
  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
  case BROADWAY_EVENT_SCROLL:
  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    surface = broadway_server_lookup_surface (server, message->pointer.event_surface_id);
    break;
  case BROADWAY_EVENT_TOUCH:
    surface = broadway_server_lookup_surface (server, message->touch.event_surface_id);
    break;
  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    surface = broadway_server_lookup_surface (server, message->configure_notify.id);
    break;
  case BROADWAY_EVENT_ROUNDTRIP_NOTIFY:
    surface = broadway_server_lookup_surface (server, message->roundtrip_notify.id);
    break;
  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    /* TODO: Send to keys focused clients only... */
  case BROADWAY_EVENT_FOCUS:
  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
  default:
    surface = NULL;
    break;
  }

  if (surface)
    client = surface->owner;
  else
    client = -1;

  if (is_pointer_event (message) &&
      server->pointer_grab_surface_id != -1)
    client = server->pointer_grab_client_id;

  broadway_events_got_input (message, client);
}

static void
process_input_messages (BroadwayServer *server)
{
  BroadwayInputMsg *message;

  while (server->input_messages)
    {
      message = server->input_messages->data;
      server->input_messages =
        g_list_delete_link (server->input_messages,
                            server->input_messages);

      if (message->base.serial == 0)
        {
          /* This was sent before we got any requests, but we don't want the
             daemon serials to go backwards, so we fix it up to be the last used
             serial */
          message->base.serial = server->saved_serial - 1;
        }

      process_input_message (server, message);
      g_free (message);
    }
}

static void
fake_configure_notify (BroadwayServer *server,
                       BroadwaySurface *surface)
{
  BroadwayInputMsg ev = { {0} };

  ev.base.type = BROADWAY_EVENT_CONFIGURE_NOTIFY;
  ev.base.serial = server->saved_serial - 1;
  ev.base.time = server->last_seen_time;
  ev.configure_notify.id = surface->id;
  ev.configure_notify.x = surface->x;
  ev.configure_notify.y = surface->y;
  ev.configure_notify.width = surface->width;
  ev.configure_notify.height = surface->height;

  process_input_message (server, &ev);
}

static guint32 *
parse_pointer_data (guint32 *p, BroadwayInputPointerMsg *data)
{
  data->mouse_surface_id = ntohl (*p++);
  data->event_surface_id = ntohl (*p++);
  data->root_x = ntohl (*p++);
  data->root_y = ntohl (*p++);
  data->win_x = ntohl (*p++);
  data->win_y = ntohl (*p++);
  data->state = ntohl (*p++);

  return p;
}

static guint32 *
parse_touch_data (guint32 *p, BroadwayInputTouchMsg *data)
{
  data->touch_type = ntohl (*p++);
  data->event_surface_id = ntohl (*p++);
  data->sequence_id = ntohl (*p++);
  data->is_emulated = ntohl (*p++);
  data->root_x = ntohl (*p++);
  data->root_y = ntohl (*p++);
  data->win_x = ntohl (*p++);
  data->win_y = ntohl (*p++);
  data->state = ntohl (*p++);

  return p;
}

static void
update_future_pointer_info (BroadwayServer *server, BroadwayInputPointerMsg *data)
{
  server->future_root_x = data->root_x;
  server->future_root_y = data->root_y;
  server->future_state = data->state;
  server->future_mouse_in_surface = data->mouse_surface_id;
}

static void
queue_input_message (BroadwayServer *server, BroadwayInputMsg *msg)
{
  server->input_messages = g_list_append (server->input_messages, g_memdup2 (msg, sizeof (BroadwayInputMsg)));
}

static void
parse_input_message (BroadwayInput *input, const unsigned char *message)
{
  BroadwayServer *server = input->server;
  BroadwayInputMsg msg;
  guint32 *p;
  gint64 time_;
  GList *l;

  memset (&msg, 0, sizeof (msg));

  p = (guint32 *) message;

  msg.base.type = ntohl (*p++);
  msg.base.serial = ntohl (*p++);

  time_ = ntohl (*p++);

  if (time_ == 0) {
    time_ = server->last_seen_time;
  } else {
    if (!input->seen_time) {
      input->seen_time = TRUE;
      /* Calculate time base so that any following times are normalized to start
         5 seconds after last_seen_time, to avoid issues that could appear when
         a long hiatus due to a reconnect seems to be instant */
      input->time_base = time_ - (server->last_seen_time + 5000);
    }
    time_ = time_ - input->time_base;
  }

  server->last_seen_time = time_;

  msg.base.time = time_;

  switch (msg.base.type) {
  case BROADWAY_EVENT_ENTER:
  case BROADWAY_EVENT_LEAVE:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.crossing.mode = ntohl (*p++);
    break;

  case BROADWAY_EVENT_POINTER_MOVE: /* Mouse move */
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    break;

  case BROADWAY_EVENT_BUTTON_PRESS:
  case BROADWAY_EVENT_BUTTON_RELEASE:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.button.button = ntohl (*p++);
    break;

  case BROADWAY_EVENT_SCROLL:
    p = parse_pointer_data (p, &msg.pointer);
    update_future_pointer_info (server, &msg.pointer);
    msg.scroll.dir = ntohl (*p++);
    break;

  case BROADWAY_EVENT_TOUCH:
    p = parse_touch_data (p, &msg.touch);
    break;

  case BROADWAY_EVENT_KEY_PRESS:
  case BROADWAY_EVENT_KEY_RELEASE:
    msg.key.surface_id = server->focused_surface_id;
    msg.key.key = ntohl (*p++);
    msg.key.state = ntohl (*p++);
    break;

  case BROADWAY_EVENT_GRAB_NOTIFY:
  case BROADWAY_EVENT_UNGRAB_NOTIFY:
    msg.grab_reply.res = ntohl (*p++);
    break;

  case BROADWAY_EVENT_CONFIGURE_NOTIFY:
    msg.configure_notify.id = ntohl (*p++);
    msg.configure_notify.x = ntohl (*p++);
    msg.configure_notify.y = ntohl (*p++);
    msg.configure_notify.width = ntohl (*p++);
    msg.configure_notify.height = ntohl (*p++);
    break;

  case BROADWAY_EVENT_ROUNDTRIP_NOTIFY:
    msg.roundtrip_notify.id = ntohl (*p++);
    msg.roundtrip_notify.tag = ntohl (*p++);
    msg.roundtrip_notify.local = FALSE;

    /* Remove matched outstanding roundtrips */
    for (l = server->outstanding_roundtrips; l != NULL; l = l->next)
      {
        BroadwayOutstandingRoundtrip *rt = l->data;

        if (rt->id == msg.roundtrip_notify.id &&
            rt->tag == msg.roundtrip_notify.tag)
          break;
      }

    if (l == NULL)
      g_warning ("Got unexpected roundtrip reply for id %d, tag %d\n", msg.roundtrip_notify.id, msg.roundtrip_notify.tag);
    else
      {
        BroadwayOutstandingRoundtrip *rt = l->data;

        server->outstanding_roundtrips = g_list_delete_link (server->outstanding_roundtrips, l);
        g_free (rt);
      }

    break;

  case BROADWAY_EVENT_SCREEN_SIZE_CHANGED:
    msg.screen_resize_notify.width = ntohl (*p++);
    msg.screen_resize_notify.height = ntohl (*p++);
    msg.screen_resize_notify.scale = ntohl (*p++);
    break;

  default:
    g_printerr ("parse_input_message - Unknown input command %c (%s)\n", msg.base.type, message);
    break;
  }

  queue_input_message (server, &msg);
}

static inline void
hex_dump (guchar *data, gsize len)
{
#ifdef DEBUG_WEBSOCKETS
  gsize i, j;
  for (j = 0; j < len + 15; j += 16)
    {
      fprintf (stderr, "0x%.4x  ", j);
      for (i = 0; i < 16; i++)
        {
          if ((j + i) < len)
            fprintf (stderr, "%.2x ", data[j+i]);
          else
            fprintf (stderr, "  ");
          if (i == 8)
            fprintf (stderr, " ");
        }
      fprintf (stderr, " | ");

      for (i = 0; i < 16; i++)
        if ((j + i) < len && g_ascii_isalnum(data[j+i]))
          fprintf (stderr, "%c", data[j+i]);
        else
          fprintf (stderr, ".");
      fprintf (stderr, "\n");
    }
#endif
}

static void
parse_input (BroadwayInput *input)
{
  if (!input->buffer->len)
    return;

  hex_dump (input->buffer->data, input->buffer->len);

  while (input->buffer->len > 2)
    {
      gsize len, payload_len;
      BroadwayWSOpCode code;
      gboolean is_mask, fin;
      guchar *buf, *data, *mask;

      buf = input->buffer->data;
      len = input->buffer->len;

#ifdef DEBUG_WEBSOCKETS
      g_print ("Parse input first byte 0x%2x 0x%2x\n", buf[0], buf[1]);
#endif

      fin = buf[0] & 0x80;
      code = buf[0] & 0x0f;
      payload_len = buf[1] & 0x7f;
      is_mask = buf[1] & 0x80;
      data = buf + 2;

      if (payload_len == 126)
        {
          if (len < 4)
            return;
          payload_len = GUINT16_FROM_BE( *(guint16 *) data );
          data += 2;
        }
      else if (payload_len == 127)
        {
          if (len < 10)
            return;
          payload_len = GUINT64_FROM_BE( *(guint64 *) data );
          data += 8;
        }

      mask = NULL;
      if (is_mask)
        {
          if (data - buf + 4 > len)
            return;
          mask = data;
          data += 4;
        }

      if (data - buf + payload_len > len)
        return; /* wait to accumulate more */

      if (is_mask)
        {
          gsize i;
          for (i = 0; i < payload_len; i++)
            data[i] ^= mask[i%4];
        }

      switch (code) {
      case BROADWAY_WS_CNX_CLOSE:
        break; /* hang around anyway */
      case BROADWAY_WS_BINARY:
        if (!fin)
          {
#ifdef DEBUG_WEBSOCKETS
            g_warning ("can't yet accept fragmented input");
#endif
          }
        else
          {
            parse_input_message (input, data);
          }
        break;
      case BROADWAY_WS_CNX_PING:
        broadway_output_pong (input->output);
        break;
      case BROADWAY_WS_CNX_PONG:
        break; /* we never send pings, but tolerate pongs */
      case BROADWAY_WS_TEXT:
      case BROADWAY_WS_CONTINUATION:
      default:
        {
          g_warning ("fragmented or unknown input code 0x%2x with fin set", code);
          break;
        }
      }

      g_byte_array_remove_range (input->buffer, 0, data - buf + payload_len);
    }
}


static gboolean
process_input_idle_cb (BroadwayServer *server)
{
  server->process_input_idle = 0;
  process_input_messages (server);
  return G_SOURCE_REMOVE;
}

static void
queue_process_input_at_idle (BroadwayServer *server)
{
  if (server->process_input_idle == 0)
    server->process_input_idle =
      g_idle_add_full (G_PRIORITY_DEFAULT, (GSourceFunc)process_input_idle_cb, server, NULL);
}

static gboolean
broadway_server_read_all_input_nonblocking (BroadwayInput *input)
{
  GInputStream *in;
  gssize res;
  guint8 buffer[1024];
  GError *error = NULL;

  if (input == NULL)
    return FALSE;

  in = g_io_stream_get_input_stream (input->connection);

  res = g_pollable_input_stream_read_nonblocking (G_POLLABLE_INPUT_STREAM (in),
                                                  buffer, sizeof (buffer), NULL, &error);

  if (res <= 0)
    {
      if (res < 0 &&
          g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        {
          g_error_free (error);
          return TRUE;
        }

      if (input->server->input == input)
        {
          send_outstanding_roundtrips (input->server);

          input->server->input = NULL;
        }
      broadway_input_free (input);
      if (res < 0)
        {
          g_printerr ("input error %s\n", error->message);
          g_error_free (error);
        }
      return FALSE;
    }

  g_byte_array_append (input->buffer, buffer, res);

  parse_input (input);
  return TRUE;
}

static void
broadway_server_consume_all_input (BroadwayServer *server)
{
  broadway_server_read_all_input_nonblocking (server->input);

  /* Since we're parsing input but not processing the resulting messages
     we might not get a readable callback on the stream, so queue an idle to
     process the messages */
  queue_process_input_at_idle (server);
}


static gboolean
input_data_cb (GObject  *stream,
               BroadwayInput *input)
{
  BroadwayServer *server = input->server;

  if (!broadway_server_read_all_input_nonblocking (input))
    return FALSE;

  if (input->active)
    process_input_messages (server);

  return TRUE;
}

guint32
broadway_server_get_next_serial (BroadwayServer *server)
{
  if (server->output)
    return broadway_output_get_next_serial (server->output);

  return server->saved_serial;
}

void
broadway_server_get_screen_size (BroadwayServer   *server,
                                 guint32          *width,
                                 guint32          *height,
                                 guint32          *scale)
{
  *width = server->root->width;
  *height = server->root->height;
  *scale = server->screen_scale;
}

static void
broadway_server_fake_roundtrip_reply (BroadwayServer *server,
                                      int             id,
                                      guint32         tag)
{
  BroadwayInputMsg msg;

  msg.base.type = BROADWAY_EVENT_ROUNDTRIP_NOTIFY;
  msg.base.serial = 0;
  msg.base.time = server->last_seen_time;
  msg.roundtrip_notify.id = id;
  msg.roundtrip_notify.tag = tag;
  msg.roundtrip_notify.local = 1;

  queue_input_message (server, &msg);
  queue_process_input_at_idle (server);
}

void
broadway_server_flush (BroadwayServer *server)
{
  if (server->output &&
      !broadway_output_flush (server->output))
    {
      server->saved_serial = broadway_output_get_next_serial (server->output);
      broadway_output_free (server->output);
      server->output = NULL;
      send_outstanding_roundtrips (server);
    }
}

void
broadway_server_roundtrip (BroadwayServer *server,
                           int             id,
                           guint32         tag)
{
  if (server->output)
    {
      BroadwayOutstandingRoundtrip *rt = g_new0 (BroadwayOutstandingRoundtrip, 1);
      rt->id = id;
      rt->tag = tag;
      server->outstanding_roundtrips = g_list_prepend (server->outstanding_roundtrips, rt);

      broadway_output_roundtrip (server->output, id, tag);
    }
  else
    broadway_server_fake_roundtrip_reply (server, id, tag);
}

static const char *
parse_line (const char *line, const char *key)
{
  const char *p;

  if (g_ascii_strncasecmp (line, key, strlen (key)) != 0)
    return NULL;
  p = line + strlen (key);
  if (*p != ':')
    return NULL;
  p++;
  /* Skip optional initial space */
  if (*p == ' ')
    p++;
  return p;
}

static void
send_error (HttpRequest *request,
            int error_code,
            const char *reason)
{
  char *res;

  res = g_strdup_printf ("HTTP/1.0 %d %s\r\n\r\n"
                         "<html><head><title>%d %s</title></head>"
                         "<body>%s</body></html>",
                         error_code, reason,
                         error_code, reason,
                         reason);

  /* TODO: This should really be async */
  g_output_stream_write_all (g_io_stream_get_output_stream (request->connection),
                             res, strlen (res), NULL, NULL, NULL);

  g_free (res);
  http_request_free (request);
}

/* magic from: http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-17 */
#define SEC_WEB_SOCKET_KEY_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* 'x3JJHMbDL1EzLkh9GBhXDw==' generates 'HSmrc0sMlYUkAGmm5OPpG2HaGWk=' */
static char *
generate_handshake_response_wsietf_v7 (const char *key)
{
  gsize digest_len = 20;
  guchar digest[20];
  GChecksum *checksum;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, (guchar *)key, -1);
  g_checksum_update (checksum, (guchar *)SEC_WEB_SOCKET_KEY_MAGIC, -1);

  g_checksum_get_digest (checksum, digest, &digest_len);
  g_checksum_free (checksum);

  g_assert (digest_len == 20);

  return g_base64_encode (digest, digest_len);
}

static void
start_input (HttpRequest *request)
{
  char **lines;
  const char *p;
  int i;
  char *res;
  const char *origin, *host;
  BroadwayInput *input;
  const void *data_buffer;
  gsize data_buffer_size;
  GInputStream *in;
  const char *key;
  GSocket *socket;
  int flag = 1;

#ifdef DEBUG_WEBSOCKETS
  g_print ("incoming request:\n%s\n", request->request->str);
#endif
  lines = g_strsplit (request->request->str, "\n", 0);

  key = NULL;
  origin = NULL;
  host = NULL;
  for (i = 0; lines[i] != NULL; i++)
    {
      if ((p = parse_line (lines[i], "Sec-WebSocket-Key")))
        key = p;
      else if ((p = parse_line (lines[i], "Origin")))
        origin = p;
      else if ((p = parse_line (lines[i], "Host")))
        host = p;
      else if ((p = parse_line (lines[i], "Sec-WebSocket-Origin")))
        origin = p;
    }

  if (host == NULL)
    {
      g_strfreev (lines);
      send_error (request, 400, "Bad websocket request");
      return;
    }

  if (key != NULL)
    {
      char* accept = generate_handshake_response_wsietf_v7 (key);
      res = g_strdup_printf ("HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: %s\r\n"
                             "%s%s%s"
                             "Sec-WebSocket-Location: ws://%s/socket\r\n"
                             "Sec-WebSocket-Protocol: broadway\r\n"
                             "\r\n", accept,
                             origin?"Sec-WebSocket-Origin: ":"", origin?origin:"", origin?"\r\n":"",
                             host);
      g_free (accept);

#ifdef DEBUG_WEBSOCKETS
      g_print ("v7 proto response:\n%s", res);
#endif

      g_output_stream_write_all (g_io_stream_get_output_stream (request->connection),
                                 res, strlen (res), NULL, NULL, NULL);
      g_free (res);
    }
  else
    {
      g_strfreev (lines);
      send_error (request, 400, "Bad websocket request");
      return;
    }

  socket = g_socket_connection_get_socket (request->socket_connection);
  setsockopt (g_socket_get_fd (socket), IPPROTO_TCP,
              TCP_NODELAY, (char *) &flag, sizeof(int));

  input = g_new0 (BroadwayInput, 1);
  input->server = request->server;
  input->connection = g_object_ref (request->connection);

  data_buffer = g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (request->data), &data_buffer_size);
  input->buffer = g_byte_array_sized_new (data_buffer_size);
  g_byte_array_append (input->buffer, data_buffer, data_buffer_size);

  input->output =
    broadway_output_new (g_io_stream_get_output_stream (request->connection), 0);

  /* This will free and close the data input stream, but we got all the buffered content already */
  http_request_free (request);

  in = g_io_stream_get_input_stream (input->connection);

  input->source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (in), NULL);
  g_source_set_callback (input->source, (GSourceFunc)input_data_cb, input, NULL);
  g_source_attach (input->source, NULL);

  start (input);

  /* Process any data in the pipe already */
  parse_input (input);

  g_strfreev (lines);
}

static void
send_outstanding_roundtrips (BroadwayServer *server)
{
  GList *l;

  for (l = server->outstanding_roundtrips; l != NULL; l = l->next)
    {
      BroadwayOutstandingRoundtrip *rt = l->data;
      broadway_server_fake_roundtrip_reply (server, rt->id, rt->tag);
    }

  g_list_free_full (server->outstanding_roundtrips, g_free);
  server->outstanding_roundtrips = NULL;
}

static void
start (BroadwayInput *input)
{
  BroadwayServer *server;

  input->active = TRUE;

  server = BROADWAY_SERVER (input->server);

  if (server->output)
    {
      send_outstanding_roundtrips (server);
      broadway_output_disconnected (server->output);
      broadway_output_flush (server->output);
    }

  if (server->input != NULL)
    {
      send_outstanding_roundtrips (server);
      broadway_input_free (server->input);
      server->input = NULL;
    }

  server->input = input;

  if (server->output)
    {
      server->saved_serial = broadway_output_get_next_serial (server->output);
      broadway_output_free (server->output);
    }
  server->output = input->output;

  broadway_output_set_next_serial (server->output, server->saved_serial);
  broadway_output_flush (server->output);

  broadway_server_resync_surfaces (server);

  if (server->pointer_grab_surface_id != -1)
    broadway_output_grab_pointer (server->output,
                                  server->pointer_grab_surface_id,
                                  server->pointer_grab_owner_events);

  process_input_messages (server);
}

static void
send_data (HttpRequest *request,
           const char *mimetype,
           const char *data, gsize len)
{
  char *res;

  res = g_strdup_printf ("HTTP/1.0 200 OK\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %"G_GSIZE_FORMAT"\r\n"
                         "\r\n",
                         mimetype, len);

  /* TODO: This should really be async */
  g_output_stream_write_all (g_io_stream_get_output_stream (request->connection),
                             res, strlen (res), NULL, NULL, NULL);
  g_free (res);
  g_output_stream_write_all (g_io_stream_get_output_stream (request->connection),
                             data, len, NULL, NULL, NULL);
  http_request_free (request);
}

#include "clienthtml.h"
#include "broadwayjs.h"

static void
got_request (HttpRequest *request)
{
  char *start, *escaped, *tmp, *version, *query;

  if (!g_str_has_prefix (request->request->str, "GET "))
    {
      send_error (request, 501, "Only GET implemented");
      return;
    }

  start = request->request->str + 4; /* Skip "GET " */

  while (*start == ' ')
    start++;

  for (tmp = start; *tmp != 0 && *tmp != ' ' && *tmp != '\n'; tmp++)
    ;
  escaped = g_strndup (start, tmp - start);
  version = NULL;
  if (*tmp == ' ')
    {
      start = tmp;
      while (*start == ' ')
        start++;
      for (tmp = start; *tmp != 0 && *tmp != ' ' && *tmp != '\n'; tmp++)
        ;
      version = g_strndup (start, tmp - start);
    }

  query = strchr (escaped, '?');
  if (query)
    *query = 0;

  if (strcmp (escaped, "/client.html") == 0 || strcmp (escaped, "/") == 0)
    send_data (request, "text/html", client_html, G_N_ELEMENTS(client_html) - 1);
  else if (strcmp (escaped, "/broadway.js") == 0)
    send_data (request, "text/javascript", broadway_js, G_N_ELEMENTS(broadway_js) - 1);
  else if (strcmp (escaped, "/socket") == 0)
    start_input (request);
  else
    send_error (request, 404, "File not found");

  g_free (escaped);
  g_free (version);
}

static void
got_http_request_line (GInputStream *stream,
                       GAsyncResult *result,
                       HttpRequest *request)
{
  char *line;

  line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (stream), result, NULL, NULL);
  if (line == NULL)
    {
      http_request_free (request);
      g_printerr ("Error reading request lines\n");
      return;
    }
  if (strlen (line) == 0)
    got_request (request);
  else
    {
      /* Protect against overflow in request length */
      if (request->request->len > 1024 * 5)
        {
          send_error (request, 400, "Request too long");
        }
      else
        {
          g_string_append_printf (request->request, "%s\n", line);
          g_data_input_stream_read_line_async (request->data, 0, NULL,
                                               (GAsyncReadyCallback)got_http_request_line, request);
        }
    }
  g_free (line);
}

static gboolean
handle_incoming_connection (GSocketService    *service,
                            GSocketConnection *connection,
                            GObject           *source_object)
{
  HttpRequest *request;
  GInputStream *in;

  request = g_new0 (HttpRequest, 1);
  request->socket_connection = g_object_ref (connection);
  request->server = BROADWAY_SERVER (source_object);
  request->request = g_string_new ("");

  if (request->server->ssl_cert && request->server->ssl_key)
    {
      GError *error = NULL;
      GTlsCertificate *certificate;

      certificate = g_tls_certificate_new_from_files (request->server->ssl_cert,
                                                      request->server->ssl_key,
                                                      &error);
      if (!certificate)
        {
          g_warning ("Cannot create TLS certificate: %s", error->message);
          g_error_free (error);
          return FALSE;
        }

      request->connection = g_tls_server_connection_new (G_IO_STREAM (connection),
                                                         certificate,
                                                         &error);
      if (!request->connection)
        {
          g_warning ("Cannot create TLS connection: %s", error->message);
          g_error_free (error);
          return FALSE;
        }

      if (!g_tls_connection_handshake (G_TLS_CONNECTION (request->connection),
                                       NULL, &error))
        {
          g_warning ("Cannot create TLS connection: %s", error->message);
          g_error_free (error);
          return FALSE;
        }
    }
  else
    {
      request->connection = G_IO_STREAM (g_object_ref (connection));
    }

  in = g_io_stream_get_input_stream (request->connection);

  request->data = g_data_input_stream_new (in);
  g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (request->data), FALSE);
  /* Be tolerant of input */
  g_data_input_stream_set_newline_type (request->data, G_DATA_STREAM_NEWLINE_TYPE_ANY);

  g_data_input_stream_read_line_async (request->data, 0, NULL,
                                       (GAsyncReadyCallback)got_http_request_line, request);
  return TRUE;
}

BroadwayServer *
broadway_server_new (char        *address,
                     int          port,
                     const char  *ssl_cert,
                     const char  *ssl_key,
                     GError     **error)
{
  BroadwayServer *server;
  GInetAddress *inet_address;
  GSocketAddress *socket_address;

  server = g_object_new (BROADWAY_TYPE_SERVER, NULL);
  server->port = port;
  server->address = g_strdup (address);
  server->ssl_cert = g_strdup (ssl_cert);
  server->ssl_key = g_strdup (ssl_key);

  if (address == NULL)
    {
      if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (server->service),
                                            server->port,
                                            G_OBJECT (server),
                                            error))
        {
          g_prefix_error (error, "Unable to listen to port %d: ", server->port);
          g_object_unref (server);
          return NULL;
        }
    }
  else
    {
      inet_address = g_inet_address_new_from_string (address);
      if (inet_address == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid ip address %s: ", address);
          g_object_unref (server);
          return NULL;
        }
      socket_address = g_inet_socket_address_new (inet_address, port);
      g_object_unref (inet_address);
      if (!g_socket_listener_add_address (G_SOCKET_LISTENER (server->service),
                                          socket_address,
                                          G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_TCP,
                                          G_OBJECT (server),
                                          NULL,
                                          error))
        {
          g_prefix_error (error, "Unable to listen to %s:%d: ", server->address, server->port);
          g_object_unref (socket_address);
          g_object_unref (server);
          return NULL;
        }
      g_object_unref (socket_address);
    }

  g_signal_connect (server->service, "incoming",
                    G_CALLBACK (handle_incoming_connection), NULL);
  return server;
}

BroadwayServer *
broadway_server_on_unix_socket_new (char *address, GError **error)
{
  BroadwayServer *server;
  GSocketAddress *socket_address = NULL;

  server = g_object_new (BROADWAY_TYPE_SERVER, NULL);
  server->port = -1;
  server->address = g_strdup (address);

  if (address == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Unspecified unix domain socket address");
      g_object_unref (server);
      return NULL;
    }
  else
    {
#ifdef HAVE_GIO_UNIX
      socket_address = g_unix_socket_address_new (address);
#endif
      if (socket_address == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid unix domain socket address %s: ", address);
          g_object_unref (server);
          return NULL;
        }
      if (!g_socket_listener_add_address (G_SOCKET_LISTENER (server->service),
                                          socket_address,
                                          G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_DEFAULT,
                                          G_OBJECT (server),
                                          NULL,
                                          error))
        {
          g_prefix_error (error, "Unable to listen to %s: ", server->address);
          g_object_unref (socket_address);
          g_object_unref (server);
          return NULL;
        }
      g_object_unref (socket_address);
    }

  g_signal_connect (server->service, "incoming",
                    G_CALLBACK (handle_incoming_connection), NULL);
  return server;
}

guint32
broadway_server_get_last_seen_time (BroadwayServer *server)
{
  broadway_server_consume_all_input (server);
  return (guint32) server->last_seen_time;
}

void
broadway_server_query_mouse (BroadwayServer *server,
                             guint32            *surface,
                             gint32             *root_x,
                             gint32             *root_y,
                             guint32            *mask)
{
  if (server->output)
    {
      broadway_server_consume_all_input (server);
      if (root_x)
        *root_x = server->future_root_x;
      if (root_y)
        *root_y = server->future_root_y;
      if (mask)
        *mask = server->future_state;
      if (surface)
        *surface = server->future_mouse_in_surface;
      return;
    }

  /* Fallback when unconnected */
  if (root_x)
    *root_x = server->last_x;
  if (root_y)
    *root_y = server->last_y;
  if (mask)
    *mask = server->last_state;
  if (surface)
    *surface = server->mouse_in_surface_id;
}

void
broadway_server_destroy_surface (BroadwayServer *server,
                                 int id,
                                 gboolean disconnected)
{
  BroadwaySurface *surface;
  gint32 transient_for = -1;

  if (server->mouse_in_surface_id == id)
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      server->mouse_in_surface_id = 0;
    }

  if (server->pointer_grab_surface_id == id)
    server->pointer_grab_surface_id = -1;

  if (server->output)
    broadway_output_destroy_surface (server->output, id);

  surface = broadway_server_lookup_surface (server, id);
  if (surface != NULL)
    {
      if (server->focused_surface_id == id)
	transient_for = surface->transient_for;

      server->surfaces = g_list_remove (server->surfaces, surface);
      g_hash_table_remove (server->surface_id_hash,
                           GINT_TO_POINTER (id));
      broadway_surface_free (server, surface);
    }

  if (transient_for != -1 && !disconnected)
    {
      surface = broadway_server_lookup_surface (server, transient_for);
      if (surface != NULL)
        {
	  broadway_server_focus_surface (server, transient_for);
	  broadway_server_flush (server);
	}
    }
}

gboolean
broadway_server_surface_show (BroadwayServer *server,
                              int id)
{
  BroadwaySurface *surface;
  gboolean sent = FALSE;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return FALSE;

  surface->visible = TRUE;

  if (server->output)
    {
      broadway_output_show_surface (server->output, surface->id);
      sent = TRUE;
    }

  return sent;
}

gboolean
broadway_server_surface_hide (BroadwayServer *server,
                              int id)
{
  BroadwaySurface *surface;
  gboolean sent = FALSE;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return FALSE;

  surface->visible = FALSE;

  if (server->mouse_in_surface_id == id)
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      server->mouse_in_surface_id = 0;
    }

  if (server->pointer_grab_surface_id == id)
    server->pointer_grab_surface_id = -1;

  if (server->output)
    {
      broadway_output_hide_surface (server->output, surface->id);
      sent = TRUE;
    }
  return sent;
}

void
broadway_server_surface_raise (BroadwayServer *server,
                               int id)
{
  BroadwaySurface *surface;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return;

  server->surfaces = g_list_remove (server->surfaces, surface);
  server->surfaces = g_list_append (server->surfaces, surface);

  if (server->output)
    broadway_output_raise_surface (server->output, surface->id);
}

void
broadway_server_set_show_keyboard (BroadwayServer *server,
                                   gboolean show)
{
  server->show_keyboard = show;

  if (server->output)
    {
      broadway_output_set_show_keyboard (server->output, server->show_keyboard);
      broadway_server_flush (server);
    }
}

void
broadway_server_surface_lower (BroadwayServer *server,
                               int id)
{
  BroadwaySurface *surface;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return;

  server->surfaces = g_list_remove (server->surfaces, surface);
  server->surfaces = g_list_prepend (server->surfaces, surface);

  if (server->output)
    broadway_output_lower_surface (server->output, surface->id);
}

void
broadway_server_surface_set_transient_for (BroadwayServer *server,
                                           int id, int parent)
{
  BroadwaySurface *surface;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return;

  surface->transient_for = parent;

  if (server->output)
    {
      broadway_output_set_transient_for (server->output, surface->id, surface->transient_for);
      broadway_server_flush (server);
    }
}

void
broadway_server_surface_set_modal_hint (BroadwayServer *server,
                                        int id, gboolean modal_hint)
{
  BroadwaySurface *surface;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return;

  surface->modal_hint = modal_hint;
}

gboolean
broadway_server_has_client (BroadwayServer *server)
{
  return server->output != NULL;
}

#define NODE_SIZE_COLOR 1
#define NODE_SIZE_FLOAT 1
#define NODE_SIZE_POINT 2
#define NODE_SIZE_MATRIX 16
#define NODE_SIZE_SIZE 2
#define NODE_SIZE_RECT (NODE_SIZE_POINT + NODE_SIZE_SIZE)
#define NODE_SIZE_RRECT (NODE_SIZE_RECT + 4 * NODE_SIZE_SIZE)
#define NODE_SIZE_COLOR_STOP (NODE_SIZE_FLOAT + NODE_SIZE_COLOR)
#define NODE_SIZE_SHADOW (NODE_SIZE_COLOR + 3 * NODE_SIZE_FLOAT)

static guint32
rotl (guint32 value, int shift)
{
  if ((shift &= 32 - 1) == 0)
    return value;
  return (value << shift) | (value >> (32 - shift));
}

static BroadwayNode *
decode_nodes (BroadwayServer *server,
              BroadwaySurface *surface,
              int len,
              guint32 data[],
              GHashTable  *client_texture_map,
              int *pos)
{
  BroadwayNode *node;
  guint32 type, id;
  guint32 i, n_stops, n_shadows, n_chars;
  guint32 size, n_children;
  gint32 texture_offset;
  guint32 hash;
  guint32 transform_type;

  g_assert (*pos < len);

  size = 0;
  n_children = 0;
  texture_offset = -1;

  type = data[(*pos)++];
  id = data[(*pos)++];
  switch (type) {
  case BROADWAY_NODE_REUSE:
    node = g_hash_table_lookup (surface->node_lookup, GINT_TO_POINTER(id));
    g_assert (node != NULL);
    return broadway_node_ref (node);
    break;
  case BROADWAY_NODE_COLOR:
    size = NODE_SIZE_RECT + NODE_SIZE_COLOR;
    break;
  case BROADWAY_NODE_BORDER:
    size = NODE_SIZE_RRECT + 4 * NODE_SIZE_FLOAT + 4 * NODE_SIZE_COLOR;
    break;
  case BROADWAY_NODE_INSET_SHADOW:
  case BROADWAY_NODE_OUTSET_SHADOW:
    size = NODE_SIZE_RRECT + NODE_SIZE_COLOR + 4 * NODE_SIZE_FLOAT;
    break;
  case BROADWAY_NODE_TEXTURE:
    texture_offset = 4;
    size = 5;
    break;
  case BROADWAY_NODE_CONTAINER:
    size = 1;
    n_children = data[*pos];
    break;
  case BROADWAY_NODE_ROUNDED_CLIP:
    size = NODE_SIZE_RRECT;
    n_children = 1;
    break;
  case BROADWAY_NODE_CLIP:
    size = NODE_SIZE_RECT;
    n_children = 1;
    break;
  case BROADWAY_NODE_TRANSFORM:
    transform_type = data[(*pos)];
    size = 1;
    if (transform_type == 0) {
      size += NODE_SIZE_POINT;
    } else if (transform_type == 1) {
      size += NODE_SIZE_MATRIX;
    } else {
      g_assert_not_reached();
    }
    n_children = 1;
    break;
  case BROADWAY_NODE_LINEAR_GRADIENT:
    size = NODE_SIZE_RECT + 2 * NODE_SIZE_POINT;
    n_stops = data[*pos + size++];
    size += n_stops * NODE_SIZE_COLOR_STOP;
    break;
  case BROADWAY_NODE_SHADOW:
    size = 1;
    n_shadows = data[*pos];
    size += n_shadows * NODE_SIZE_SHADOW;
    n_children = 1;
    break;
  case BROADWAY_NODE_OPACITY:
    size = NODE_SIZE_FLOAT;
    n_children = 1;
    break;
  case BROADWAY_NODE_DEBUG:
    n_chars = data[*pos];
    size = 1 + (n_chars + 3) / 4;
    n_children = 1;
    break;
  default:
    g_assert_not_reached ();
  }

  node = g_malloc (sizeof(BroadwayNode) + (size - 1) * sizeof(guint32) + n_children * sizeof (BroadwayNode *));
  g_ref_count_init (&node->refcount);
  node->type = type;
  node->id = id;
  node->output_id = id;
  node->texture_id = 0;
  node->n_children = n_children;
  node->children = (BroadwayNode **)((char *)node + sizeof(BroadwayNode) + (size - 1) * sizeof(guint32));
  node->n_data = size;
  for (i = 0; i < size; i++)
    {
      node->data[i] = data[(*pos)++];
      if (i == texture_offset)
        {
          node->texture_id = GPOINTER_TO_INT (g_hash_table_lookup (client_texture_map, GINT_TO_POINTER (node->data[i])));
          broadway_server_ref_texture (server, node->texture_id);
          node->data[i] = node->texture_id;
        }
    }

  for (i = 0; i < n_children; i++)
    node->children[i] = decode_nodes (server, surface, len, data, client_texture_map, pos);

  hash = node->type << 16;

  for (i = 0; i < size; i++)
    hash ^= rotl (node->data[i], i);

  for (i = 0; i < n_children; i++)
    hash ^= rotl (node->children[i]->hash, i);

  node->hash = hash;

  return node;
}

/* passes ownership of nodes */
void
broadway_server_surface_update_nodes (BroadwayServer   *server,
                                      int               id,
                                      guint32          data[],
                                      int              len,
                                      GHashTable      *client_texture_map)
{
  BroadwaySurface *surface;
  int pos = 0;
  BroadwayNode *root;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return;

  root = decode_nodes (server, surface, len, data, client_texture_map, &pos);

  if (server->output != NULL)
    broadway_output_surface_set_nodes (server->output, surface->id,
                                       root,
                                       surface->nodes,
                                       surface->node_lookup);

  if (surface->nodes)
    broadway_node_unref (server, surface->nodes);

  surface->nodes = root;

  g_hash_table_remove_all (surface->node_lookup);
  broadway_node_add_to_lookup (root, surface->node_lookup);
}

guint32
broadway_server_upload_texture (BroadwayServer   *server,
                                GBytes           *bytes)
{
  BroadwayTexture *texture;

  texture = g_new0 (BroadwayTexture, 1);
  g_ref_count_init (&texture->refcount);
  texture->id = ++server->next_texture_id;
  texture->bytes = g_bytes_ref (bytes);

  g_hash_table_replace (server->textures,
                        GINT_TO_POINTER (texture->id),
                        texture);

  if (server->output)
    broadway_output_upload_texture (server->output, texture->id, texture->bytes);

  return texture->id;
}

static void
broadway_server_ref_texture (BroadwayServer   *server,
                             guint32           id)
{
  BroadwayTexture *texture;

  texture = g_hash_table_lookup (server->textures, GINT_TO_POINTER (id));
  if (texture)
    g_ref_count_inc (&texture->refcount);
}

void
broadway_server_release_texture (BroadwayServer   *server,
                                 guint32           id)
{
  BroadwayTexture *texture;

  texture = g_hash_table_lookup (server->textures, GINT_TO_POINTER (id));

  if (texture && g_ref_count_dec (&texture->refcount))
    {
      g_hash_table_remove (server->textures, GINT_TO_POINTER (id));

      if (server->output)
        broadway_output_release_texture (server->output, id);
    }
}

gboolean
broadway_server_surface_move_resize (BroadwayServer *server,
                                     int id,
                                     gboolean with_move,
                                     int x,
                                     int y,
                                     int width,
                                     int height)
{
  BroadwaySurface *surface;
  gboolean with_resize;
  gboolean sent = FALSE;

  surface = broadway_server_lookup_surface (server, id);
  if (surface == NULL)
    return FALSE;

  with_resize = width != surface->width || height != surface->height;
  surface->width = width;
  surface->height = height;

  if (server->output != NULL)
    {
      broadway_output_move_resize_surface (server->output,
                                           surface->id,
                                           with_move, x, y,
                                           with_resize, surface->width, surface->height);
      sent = TRUE;
    }
  else
    {
      if (with_move)
        {
          surface->x = x;
          surface->y = y;
        }

      fake_configure_notify (server, surface);
    }

  return sent;
}

void
broadway_server_focus_surface (BroadwayServer *server,
                               int new_focused_surface)
{
  BroadwayInputMsg focus_msg;

  if (server->focused_surface_id == new_focused_surface)
    return;

  memset (&focus_msg, 0, sizeof (focus_msg));
  focus_msg.base.type = BROADWAY_EVENT_FOCUS;
  focus_msg.base.time = broadway_server_get_last_seen_time (server);
  focus_msg.focus.old_id = server->focused_surface_id;
  focus_msg.focus.new_id = new_focused_surface;

  broadway_events_got_input (&focus_msg, -1);

  /* Keep track of the new focused surface */
  server->focused_surface_id = new_focused_surface;
}

guint32
broadway_server_grab_pointer (BroadwayServer *server,
                              int client_id,
                              int id,
                              gboolean owner_events,
                              guint32 event_mask,
                              guint32 time_)
{
  if (server->pointer_grab_surface_id != -1 &&
      time_ != 0 && server->pointer_grab_time > time_)
    return GDK_GRAB_ALREADY_GRABBED;

  if (time_ == 0)
    time_ = server->last_seen_time;

  server->pointer_grab_surface_id = id;
  server->pointer_grab_client_id = client_id;
  server->pointer_grab_owner_events = owner_events;
  server->pointer_grab_time = time_;

  if (server->output)
    {
      broadway_output_grab_pointer (server->output,
                                    id,
                                    owner_events);
      broadway_server_flush (server);
    }

  /* TODO: What about surface grab events if we're not connected? */

  return GDK_GRAB_SUCCESS;
}

guint32
broadway_server_ungrab_pointer (BroadwayServer *server,
                                guint32    time_)
{
  guint32 serial;

  if (server->pointer_grab_surface_id != -1 &&
      time_ != 0 && server->pointer_grab_time > time_)
    return 0;

  /* TODO: What about surface grab events if we're not connected? */

  if (server->output)
    {
      serial = broadway_output_ungrab_pointer (server->output);
      broadway_server_flush (server);
    }
  else
    {
      serial = server->saved_serial;
    }

  server->pointer_grab_surface_id = -1;

  return serial;
}

guint32
broadway_server_new_surface (BroadwayServer *server,
                             guint32 client,
                             int x,
                             int y,
                             int width,
                             int height)
{
  BroadwaySurface *surface;

  surface = g_new0 (BroadwaySurface, 1);
  surface->owner = client;
  surface->id = server->id_counter++;
  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;
  surface->node_lookup = g_hash_table_new (g_direct_hash, g_direct_equal);

  g_hash_table_insert (server->surface_id_hash,
                       GINT_TO_POINTER (surface->id),
                       surface);

  server->surfaces = g_list_append (server->surfaces, surface);

  if (server->output)
    broadway_output_new_surface (server->output,
                                 surface->id,
                                 surface->x,
                                 surface->y,
                                 surface->width,
                                 surface->height);
  else
    fake_configure_notify (server, surface);

  return surface->id;
}

static void
broadway_server_resync_surfaces (BroadwayServer *server)
{
  GHashTableIter iter;
  gpointer key, value;
  GList *l;

  if (server->output == NULL)
    return;

  /* First upload all textures */
  g_hash_table_iter_init (&iter, server->textures);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      BroadwayTexture *texture = value;
      broadway_output_upload_texture (server->output,
                                      GPOINTER_TO_INT (key),
                                      texture->bytes);
    }

  /* Then create all surfaces */
  for (l = server->surfaces; l != NULL; l = l->next)
    {
      BroadwaySurface *surface = l->data;

      if (surface->id == 0)
        continue; /* Skip root */

      broadway_output_new_surface (server->output,
                                   surface->id,
                                   surface->x,
                                   surface->y,
                                   surface->width,
                                   surface->height);
    }

  /* Then do everything that may reference other surfaces */
  for (l = server->surfaces; l != NULL; l = l->next)
    {
      BroadwaySurface *surface = l->data;

      if (surface->id == 0)
        continue; /* Skip root */

      if (surface->transient_for != -1)
        broadway_output_set_transient_for (server->output, surface->id,
                                           surface->transient_for);

      if (surface->nodes)
        broadway_output_surface_set_nodes (server->output, surface->id,
                                           surface->nodes,
                                           NULL, NULL);

      if (surface->visible)
        broadway_output_show_surface (server->output, surface->id);
    }

  if (server->show_keyboard)
    broadway_output_set_show_keyboard (server->output, TRUE);

  broadway_server_flush (server);
}
