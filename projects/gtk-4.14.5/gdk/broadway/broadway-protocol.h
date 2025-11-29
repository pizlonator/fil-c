#pragma once

#include <glib.h>

typedef struct  {
    gint32 x, y;
    gint32 width, height;
} BroadwayRect;

typedef enum { /* Sync changes with broadway.js and node_type_is_container() */
  BROADWAY_NODE_TEXTURE = 0,
  BROADWAY_NODE_CONTAINER = 1,
  BROADWAY_NODE_COLOR = 2,
  BROADWAY_NODE_BORDER = 3,
  BROADWAY_NODE_OUTSET_SHADOW = 4,
  BROADWAY_NODE_INSET_SHADOW = 5,
  BROADWAY_NODE_ROUNDED_CLIP = 6,
  BROADWAY_NODE_LINEAR_GRADIENT = 7,
  BROADWAY_NODE_SHADOW = 8,
  BROADWAY_NODE_OPACITY = 9,
  BROADWAY_NODE_CLIP = 10,
  BROADWAY_NODE_TRANSFORM = 11,
  BROADWAY_NODE_DEBUG = 12,
  BROADWAY_NODE_REUSE = 13,
} BroadwayNodeType;

typedef enum { /* Sync changes with broadway.js */
  BROADWAY_NODE_OP_INSERT_NODE = 0,
  BROADWAY_NODE_OP_REMOVE_NODE = 1,
  BROADWAY_NODE_OP_MOVE_AFTER_CHILD = 2,
  BROADWAY_NODE_OP_PATCH_TEXTURE = 3,
  BROADWAY_NODE_OP_PATCH_TRANSFORM = 4,
} BroadwayNodeOpType;

static const char *broadway_node_type_names[] G_GNUC_UNUSED =  {
  "TEXTURE",
  "CONTAINER",
  "COLOR",
  "BORDER",
  "OUTSET_SHADOW",
  "INSET_SHADOW",
  "ROUNDED_CLIP",
  "LINEAR_GRADIENT",
  "SHADOW",
  "OPACITY",
  "CLIP",
  "TRANSFORM",
  "DEBUG",
  "REUSE",
};

typedef enum {
  BROADWAY_EVENT_ENTER = 0,
  BROADWAY_EVENT_LEAVE = 1,
  BROADWAY_EVENT_POINTER_MOVE = 2,
  BROADWAY_EVENT_BUTTON_PRESS = 3,
  BROADWAY_EVENT_BUTTON_RELEASE = 4,
  BROADWAY_EVENT_TOUCH = 5,
  BROADWAY_EVENT_SCROLL = 6,
  BROADWAY_EVENT_KEY_PRESS = 7,
  BROADWAY_EVENT_KEY_RELEASE = 8,
  BROADWAY_EVENT_GRAB_NOTIFY = 9,
  BROADWAY_EVENT_UNGRAB_NOTIFY = 10,
  BROADWAY_EVENT_CONFIGURE_NOTIFY = 11,
  BROADWAY_EVENT_SCREEN_SIZE_CHANGED = 12,
  BROADWAY_EVENT_FOCUS = 13,
  BROADWAY_EVENT_ROUNDTRIP_NOTIFY = 14,
} BroadwayEventType;

typedef enum {
  BROADWAY_OP_GRAB_POINTER = 0,
  BROADWAY_OP_UNGRAB_POINTER = 1,
  BROADWAY_OP_NEW_SURFACE = 2,
  BROADWAY_OP_SHOW_SURFACE = 3,
  BROADWAY_OP_HIDE_SURFACE = 4,
  BROADWAY_OP_RAISE_SURFACE = 5,
  BROADWAY_OP_LOWER_SURFACE = 6,
  BROADWAY_OP_DESTROY_SURFACE = 7,
  BROADWAY_OP_MOVE_RESIZE = 8,
  BROADWAY_OP_SET_TRANSIENT_FOR = 9,
  BROADWAY_OP_DISCONNECTED = 10,
  BROADWAY_OP_SURFACE_UPDATE = 11,
  BROADWAY_OP_SET_SHOW_KEYBOARD = 12,
  BROADWAY_OP_UPLOAD_TEXTURE = 13,
  BROADWAY_OP_RELEASE_TEXTURE = 14,
  BROADWAY_OP_SET_NODES = 15,
  BROADWAY_OP_ROUNDTRIP = 16,
} BroadwayOpType;

typedef struct {
  guint32 type;
  guint32 serial;
  guint64 time;
} BroadwayInputBaseMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 mouse_surface_id; /* The real surface, not taking grabs into account */
  guint32 event_surface_id;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} BroadwayInputPointerMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 mode;
} BroadwayInputCrossingMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 button;
} BroadwayInputButtonMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  gint32 dir;
} BroadwayInputScrollMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 touch_type;
  guint32 event_surface_id;
  guint32 sequence_id;
  guint32 is_emulated;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} BroadwayInputTouchMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 surface_id;
  guint32 state;
  gint32 key;
} BroadwayInputKeyMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 res;
} BroadwayInputGrabReply;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
  gint32 x;
  gint32 y;
  gint32 width;
  gint32 height;
} BroadwayInputConfigureNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
  guint32 tag;
  guint32 local;
} BroadwayInputRoundtripNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 width;
  guint32 height;
  guint32 scale;
} BroadwayInputScreenResizeNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
} BroadwayInputDeleteNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 new_id;
  gint32 old_id;
} BroadwayInputFocusMsg;

typedef union {
  BroadwayInputBaseMsg base;
  BroadwayInputPointerMsg pointer;
  BroadwayInputCrossingMsg crossing;
  BroadwayInputButtonMsg button;
  BroadwayInputScrollMsg scroll;
  BroadwayInputTouchMsg touch;
  BroadwayInputKeyMsg key;
  BroadwayInputGrabReply grab_reply;
  BroadwayInputConfigureNotify configure_notify;
  BroadwayInputRoundtripNotify roundtrip_notify;
  BroadwayInputDeleteNotify delete_notify;
  BroadwayInputScreenResizeNotify screen_resize_notify;
  BroadwayInputFocusMsg focus;
} BroadwayInputMsg;

typedef enum {
  BROADWAY_REQUEST_NEW_SURFACE,
  BROADWAY_REQUEST_FLUSH,
  BROADWAY_REQUEST_SYNC,
  BROADWAY_REQUEST_QUERY_MOUSE,
  BROADWAY_REQUEST_DESTROY_SURFACE,
  BROADWAY_REQUEST_SHOW_SURFACE,
  BROADWAY_REQUEST_HIDE_SURFACE,
  BROADWAY_REQUEST_SET_TRANSIENT_FOR,
  BROADWAY_REQUEST_MOVE_RESIZE,
  BROADWAY_REQUEST_GRAB_POINTER,
  BROADWAY_REQUEST_UNGRAB_POINTER,
  BROADWAY_REQUEST_FOCUS_SURFACE,
  BROADWAY_REQUEST_SET_SHOW_KEYBOARD,
  BROADWAY_REQUEST_UPLOAD_TEXTURE,
  BROADWAY_REQUEST_RELEASE_TEXTURE,
  BROADWAY_REQUEST_SET_NODES,
  BROADWAY_REQUEST_ROUNDTRIP,
  BROADWAY_REQUEST_SET_MODAL_HINT,
} BroadwayRequestType;

typedef struct {
  guint32 size;
  guint32 serial;
  guint32 type;
} BroadwayRequestBase, BroadwayRequestFlush, BroadwayRequestSync, BroadwayRequestQueryMouse;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
} BroadwayRequestDestroySurface, BroadwayRequestShowSurface, BroadwayRequestHideSurface, BroadwayRequestFocusSurface;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 tag;
} BroadwayRequestRoundtrip;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 parent;
} BroadwayRequestSetTransientFor;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 offset;
  guint32 size;
} BroadwayRequestUploadTexture;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
} BroadwayRequestReleaseTexture;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 data[1];
} BroadwayRequestSetNodes;


typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 owner_events;
  guint32 event_mask;
  guint32 time_;
} BroadwayRequestGrabPointer;

typedef struct {
  BroadwayRequestBase base;
  guint32 time_;
} BroadwayRequestUngrabPointer;

typedef struct {
  BroadwayRequestBase base;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
} BroadwayRequestNewSurface;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 with_move;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
} BroadwayRequestMoveResize;

typedef struct {
  BroadwayRequestBase base;
  guint32 show_keyboard;
} BroadwayRequestSetShowKeyboard;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  gboolean modal_hint;
} BroadwayRequestSetModalHint;

typedef union {
  BroadwayRequestBase base;
  BroadwayRequestNewSurface new_surface;
  BroadwayRequestFlush flush;
  BroadwayRequestSync sync;
  BroadwayRequestRoundtrip roundtrip;
  BroadwayRequestQueryMouse query_mouse;
  BroadwayRequestDestroySurface destroy_surface;
  BroadwayRequestShowSurface show_surface;
  BroadwayRequestHideSurface hide_surface;
  BroadwayRequestSetTransientFor set_transient_for;
  BroadwayRequestMoveResize move_resize;
  BroadwayRequestGrabPointer grab_pointer;
  BroadwayRequestUngrabPointer ungrab_pointer;
  BroadwayRequestFocusSurface focus_surface;
  BroadwayRequestSetShowKeyboard set_show_keyboard;
  BroadwayRequestUploadTexture upload_texture;
  BroadwayRequestReleaseTexture release_texture;
  BroadwayRequestSetNodes set_nodes;
  BroadwayRequestSetModalHint set_modal_hint;
} BroadwayRequest;

typedef enum {
  BROADWAY_REPLY_EVENT,
  BROADWAY_REPLY_SYNC,
  BROADWAY_REPLY_QUERY_MOUSE,
  BROADWAY_REPLY_NEW_SURFACE,
  BROADWAY_REPLY_GRAB_POINTER,
  BROADWAY_REPLY_UNGRAB_POINTER
} BroadwayReplyType;

typedef struct {
  guint32 size;
  guint32 in_reply_to;
  guint32 type;
} BroadwayReplyBase, BroadwayReplySync;

typedef struct {
  BroadwayReplyBase base;
  guint32 id;
} BroadwayReplyNewSurface;

typedef struct {
  BroadwayReplyBase base;
  guint32 status;
} BroadwayReplyGrabPointer, BroadwayReplyUngrabPointer;

typedef struct {
  BroadwayReplyBase base;
  guint32 surface;
  gint32 root_x;
  gint32 root_y;
  guint32 mask;
} BroadwayReplyQueryMouse;

typedef struct {
  BroadwayReplyBase base;
  BroadwayInputMsg msg;
} BroadwayReplyEvent;

typedef union {
  BroadwayReplyBase base;
  BroadwayReplySync sync;
  BroadwayReplyEvent event;
  BroadwayReplyQueryMouse query_mouse;
  BroadwayReplyNewSurface new_surface;
  BroadwayReplyGrabPointer grab_pointer;
  BroadwayReplyUngrabPointer ungrab_pointer;
} BroadwayReply;

