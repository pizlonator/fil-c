#ifndef _LIBRFB_DECODER_H_
#define _LIBRFB_DECODER_H_

#include <gio/gio.h>

#include <glib.h>

G_BEGIN_DECLS

enum
{
  SECURITY_FAIL = 0,
  SECURITY_NONE,
  SECURITY_VNC,
};

#define IS_VERSION(x, ma, mi)   ((x->protocol_major == ma) && (x->protocol_minor == mi))
#define IS_VERSION_3_3(x)       IS_VERSION(x, 3, 3)
#define IS_VERSION_3_7(x)       IS_VERSION(x, 3, 7)
#define IS_VERSION_3_8(x)       IS_VERSION(x, 3, 8)

#define MESSAGE_TYPE_FRAMEBUFFER_UPDATE     0

#define ENCODING_TYPE_RAW                   0
#define ENCODING_TYPE_COPYRECT              1
#define ENCODING_TYPE_RRE                   2
#define ENCODING_TYPE_CORRE                 4
#define ENCODING_TYPE_HEXTILE               5

#define SUBENCODING_RAW                     1
#define SUBENCODING_BACKGROUND              2
#define SUBENCODING_FOREGROUND              4
#define SUBENCODING_ANYSUBRECTS             8
#define SUBENCODING_SUBRECTSCOLORED         16

typedef struct _RfbDecoder RfbDecoder;

struct _RfbDecoder
{
  /* callbacks */
  gboolean (*state) (RfbDecoder * decoder);

  gpointer buffer_handler_data;

  GSocketClient *socket_client;
  GSocketConnection *connection;
  GCancellable *cancellable;

  guint8 *data;
  guint32 data_len;
  gpointer decoder_private;
  guint8 *frame;
  guint8 *prev_frame;

  GError *error;

  /* settable properties */
  gboolean shared_flag;

  /* readable properties */
  gboolean inited;

  guint protocol_major;
  guint protocol_minor;
  guint security_type;

  gchar *password;
  gboolean use_copyrect;

  guint width;
  guint height;
  guint bpp;
  guint depth;
  gboolean big_endian;
  gboolean true_colour;
  guint red_max;
  guint green_max;
  guint blue_max;
  guint red_shift;
  guint green_shift;
  guint blue_shift;

  gchar *name;

  /* information if we don't want to update the whole screen */
  guint offset_x;
  guint offset_y;
  guint rect_width;
  guint rect_height;

  gint n_rects;

  /* some many used values */
  guint bytespp;
  guint line_size;

  /* Seriliaze writes operations */
  GMutex write_lock;
};

RfbDecoder *rfb_decoder_new (void);
void rfb_decoder_free (RfbDecoder * decoder);
gboolean rfb_decoder_connect_tcp (RfbDecoder * decoder,
    gchar * host, guint port);
void rfb_decoder_disconnect (RfbDecoder * decoder);
gboolean rfb_decoder_iterate (RfbDecoder * decoder);
void rfb_decoder_send_update_request (RfbDecoder * decoder,
    gboolean incremental, gint x, gint y, gint width, gint height);
void rfb_decoder_send_key_event (RfbDecoder * decoder,
    guint key, gboolean down_flag);
void rfb_decoder_send_pointer_event (RfbDecoder * decoder,
    gint button_mask, gint x, gint y);

G_END_DECLS
#endif
