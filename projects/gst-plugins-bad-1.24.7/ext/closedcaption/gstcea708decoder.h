/* GStreamer
 * Copyright (C) 2013 CableLabs, Louisville, CO 80027
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *     @Author: Chengjun Wang <cjun.wang@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef __GST_CEA708_DEC_H__
#define __GST_CEA708_DEC_H__

#include <gst/gst.h>
#include <pango/pangocairo.h>

G_BEGIN_DECLS
/* from ATSC A/53 Part 4
 * DTVCC packets are 128 bytes MAX, length is only 6 bits, header is 2 bytes,
 * the last byte is flag-fill, that leaves 125 possible bytes of data to be
 * represented in 6 bits, hence the length encoding
 */
/* should never be more than 128 */
#define DTVCC_LENGTH       128
#define DTVCC_PKT_SIZE(sz_byte)  (((sz_byte) == 0) ? 127 : ((sz_byte)  * 2) -1)
#define CCTYPE_VALID_MASK  0x04
#define CCTYPE_TYPE_MASK   0x03
#define NUM_608_CCTYPES 2
/* CEA-708-B commands */
/* EndOfText */
#define CC_COMMAND_ETX 0x03
/* SetCurrentWindow0 */
#define CC_COMMAND_CW0 0x80
#define CC_COMMAND_CW1 0x81
#define CC_COMMAND_CW2 0x82
#define CC_COMMAND_CW3 0x83
#define CC_COMMAND_CW4 0x84
#define CC_COMMAND_CW5 0x85
#define CC_COMMAND_CW6 0x86
#define CC_COMMAND_CW7 0x87
/* ClearWindows */
#define CC_COMMAND_CLW 0x88
/* DisplayWindows */
#define CC_COMMAND_DSW 0x89
/* HideWindows */
#define CC_COMMAND_HDW 0x8A
/* ToggleWindows */
#define CC_COMMAND_TGW 0x8B
/* DeleteWindows */
#define CC_COMMAND_DLW 0x8C
/* Delay */
#define CC_COMMAND_DLY 0x8D
/* DelayCancel */
#define CC_COMMAND_DLC 0x8E
/* Reset */
#define CC_COMMAND_RST 0x8F
/* SetPenAttributes */
#define CC_COMMAND_SPA 0x90
/* SetPenColor */
#define CC_COMMAND_SPC 0x91
/* SetPenLocation */
#define CC_COMMAND_SPL 0x92
/* SetWindowAttributes */
#define CC_COMMAND_SWA 0x97
/* DefineWindow0 */
#define CC_COMMAND_DF0 0x98
#define CC_COMMAND_DF1 0x99
#define CC_COMMAND_DF2 0x9A
#define CC_COMMAND_DF3 0x9B
#define CC_COMMAND_DF4 0x9C
#define CC_COMMAND_DF5 0x9D
#define CC_COMMAND_DF6 0x9E
#define CC_COMMAND_DF7 0x9F
/* music note unicode */
#define CC_SPECIAL_CODE_MUSIC_NOTE    0x266a
#define CC_UTF8_MAX_LENGTH   6
#define CC_MAX_CODE_SET_SIZE   96
/* Per CEA-708 spec there may be 8 CC windows */
#define MAX_708_WINDOWS  8
/* Each 708 window contains a grid of character positions. These are the
  * max limits defined, but each window has a row/col count which is typically
  * smaller than the limits. Note this is just one window, not the entire screen.
  */
/* max row count */
#define WINDOW_MAX_ROWS 15
/* max column width */
#define WINDOW_MAX_COLS 42
/* The linebuffer contains text for 1 line pango text corresponding to 1 line of 708 text.
  * The linebuffer could be a lot larger than the window text because of required markup.
  * example <u> </u> for underline.
  * The size given is an estimate, to be changed if determined that a larger
  * buffer is needed
  */
#define LINEBUFFER_SIZE 1024
/* The screen width/height defined by 708 - not character units, these are
  * used only to determine the position of the anchor on the screen.
  */
#define SCREEN_WIDTH_16_9    209
#define SCREEN_HEIGHT_16_9   74
#define SCREEN_WIDTH_4_3    159
#define SCREEN_HEIGHT_4_3    74

/* raw bytes of "define window" command */
#define WIN_DEF_SIZE 6
/* The maximum size of a 708 window in character units. This is used to
  * calculate the position of windows based on window anchor positions.
  */
#define SCREEN_HEIGHT_708  15
#define SCREEN_WIDTH_708   32
/* cea708 minimum color list */
#define CEA708_COLOR_INVALID   0xFF
#define CEA708_COLOR_BLACK     0x00
#define CEA708_COLOR_WHITE     0x2A
#define CEA708_COLOR_RED       0x20
#define CEA708_COLOR_GREEN     0x08
#define CEA708_COLOR_BLUE      0x02
#define CEA708_COLOR_YELLOW    0x28
#define CEA708_COLOR_MAGENTA   0x22
#define CEA708_COLOR_CYAN      0x0A
#define CEA708_PANGO_SPAN_MARKUP_START                      "<span"
#define CEA708_PANGO_SPAN_MARKUP_END                        "</span>"
#define CEA708_PANGO_SPAN_ATTRIBUTES_UNDERLINE_SINGLE       " underline='single'"
#define CEA708_PANGO_SPAN_ATTRIBUTES_STYLE_ITALIC           " style='italic'"
#define CEA708_PANGO_SPAN_ATTRIBUTES_FONT                   " font_desc="
#define CEA708_PANGO_SPAN_ATTRIBUTES_FOREGROUND             " foreground="
#define CEA708_PANGO_SPAN_ATTRIBUTES_BACKGROUND             " background="
#define MINIMUM_OUTLINE_OFFSET 1.0
#define WINDOW_IN_LIST_IS_ACTIVE(list) (list & 0x1)
typedef struct _Cea708Dec Cea708Dec;

typedef enum
{
  COLOR_TYPE_BLACK = 0,
  COLOR_TYPE_WHITE,
  COLOR_TYPE_RED,
  COLOR_TYPE_GREEN,
  COLOR_TYPE_BLUE,
  COLOR_TYPE_YELLOW,
  COLOR_TYPE_MAGENTA,
  COLOR_TYPE_CYAN,
  COLOR_TYPE_RESEVER
} Cea708ColorType;

typedef enum
{
  NO_CHANGE = 0,
  SWITCH_TO_HIDE,
  SWITCH_TO_SHOW,
  TOGGLE
} VisibilityControl;

typedef enum
{
  SOLID = 0,
  FLASH,
  TRANSLUCENT,
  TRANSPARENT
} Opacity;

typedef enum
{
  WIN_STYLE_NORMAL = 1,
  WIN_STYLE_TRANSPARENT,
  WIN_STYLE_NORMAL_CENTERED,
  WIN_STYLE_NORMAL_WORD_WRAP,
  WIN_STYLE_TRANSPARENT_WORD_WRAP,
  WIN_STYLE_TRANSPARENT_CENTERED,
  WIN_STYLE_ROTATED
} WindowStyle;

typedef enum
{
  PEN_STYLE_DEFAULT = 1,
  PEN_STYLE_MONO_SERIF,
  PEN_STYLE_PROP_SERIF,
  PEN_STYLE_MONO_SANS,
  PEN_STYLE_PROP_SANS,
  PEN_STYLE_MONO_SANS_TRANSPARENT,
  PEN_STYLE_PROP_SANS_TRANSPARENT
} PenStyle;

typedef enum
{
  ANCHOR_PT_TOP_LEFT = 0,
  ANCHOR_PT_TOP_CENTER,
  ANCHOR_PT_TOP_RIGHT,
  ANCHOR_PT_MIDDLE_LEFT,
  ANCHOR_PT_CENTER,
  ANCHOR_PT_MIDDLE_RIGHT,
  ANCHOR_PT_BOTTOM_LEFT,
  ANCHOR_PT_BOTTOM_CENTER,
  ANCHOR_PT_BOTTOM_RIGHT,
} AnchorPoint;

typedef enum
{
  TAG_DIALOG = 0,
  TAG_SPEAKER_ID,
  TAG_ELECTRONIC_VOICE,
  TAG_ALT_LANGUAGE_DIALOG,
  TAG_VOICEOVER,
  TAG_AUDIBLE_TRANSLATION,
  TAG_SUBTITLE_TRANSLATION,
  TAG_VOICE_QUALITY_DESCRIPTION,
  TAG_SONG_LYRICS,
  TAG_SOUND_EFFECT_DESCRIPTION,
  TAG_MUSICAL_SCORE_DESCRIPTION,
  TAG_EXPLETIVE,
  TAG_UNDEF1,
  TAG_UNDEF2,
  TAG_UNDEF3,
  TAG_NOT_DISPLAYED
} TagType;

typedef enum
{
  JUSTIFY_LEFT = 0,
  JUSTIFY_RIGHT,
  JUSTIFY_CENTER,
  JUSTIFY_FULL
} JUSTIFY_MODE;

typedef enum
{
  PRINT_DIR_LEFT_TO_RIGHT = 0,
  PRINT_DIR_RIGHT_TO_LEFT,
  PRINT_DIR_TOP_TO_BOTTOM,
  PRINT_DIR_BOTTOM_TO_TOP
} PRINT_DIRECTION;

typedef enum
{
  SCROLL_DIR_LEFT_TO_RIGHT = 0,
  SCROLL_DIR_RIGHT_TO_LEFT,
  SCROLL_DIR_TOP_TO_BOTTOM,
  SCROLL_DIR_BOTTOM_TO_TOP
} SCROLL_DIRECTION;

typedef enum
{
  DISPLAY_EFFECT_SNAP = 0,
  DISPLAY_EFFECT_FADE,
  DISPLAY_EFFECT_WIPE
} DisplayEffect;

typedef enum
{
  EFFECT_DIR_LEFT_TO_RIGHT = 0,
  EFFECT_DIR_RIGHT_TO_LEFT,
  EFFECT_DIR_TOP_TO_BOTTOM,
  EFFECT_DIR_BOTTOM_TO_TOP
} EFFECT_DIRECTION;

typedef enum
{
  BORDER_TYPE_NONE = 0,
  BORDER_TYPE_RAISED,
  BORDER_TYPE_DEPRESSED,
  BORDER_TYPE_UNIFORM
} BORDER_TYPE;

typedef enum
{
  PEN_SIZE_SMALL = 0,
  PEN_SIZE_STANDARD,
  PEN_SIZE_LARGE,
  PEN_SIZE_INVALID
} PenSize;

typedef enum
{
  PEN_OFFSET_SUBSCRIPT = 0,
  PEN_OFFSET_NORMAL,
  PEN_OFFSET_SUPERSCRIPT,
  PEN_OFFSET_INVALID
} PenOffset;

typedef enum
{
  EDGE_TYPE_NONE = 0,
  EDGE_TYPE_RAISED,
  EDGE_TYPE_DEPRESSED,
  EDGE_TYPE_UNIFORM,
  EDGE_TYPE_LEFT_DROP_SHADOW,
  EDGE_TYPE_RIGHT_DROP_SHADOW,
  EDGE_TYPE_INVALID_1,
  EDGE_TYPE_INVALID_2
} EdgeType;

typedef enum
{
  FONT_STYLE_DEFAULT = 0,
  FONT_STYLE_MONO_SERIF,
  FONT_STYLE_PROP_SERIF,
  FONT_STYLE_MONO_SANS,
  FONT_STYLE_PROP_SANS,
  FONT_STYLE_CASUAL,
  FONT_STYLE_CURSIVE,
  FONT_STYLE_SMALLCAPS
} FontStyle;

typedef struct
{
  guint8 fg_color;
  guint8 fg_opacity;
  guint8 bg_color;
  guint8 bg_opacity;
  guint8 edge_color;
} cea708PenColor;

typedef struct
{
  gboolean span_start_flag;
  gboolean span_end_flag;
  gboolean span_txt_flag;

  gboolean span_next_flag;

  gboolean underline;
  gboolean italics;

  guint8 size;
  guint8 fg_color;
  guint8 bg_color;
  FontStyle font_style;
} cea708PangoSpanControl;

typedef struct
{
  PenSize pen_size;
  FontStyle font_style;
  TagType text_tag;
  PenOffset offset;
  gboolean italics;
  gboolean underline;
  EdgeType edge_type;
} cea708PenAttributes;

/* The char records one cell location in the window, with the character and all of its attributes */
typedef struct
{
  cea708PenColor pen_color;
  cea708PenAttributes pen_attributes;
  guint8 justify_mode;
  gunichar c;
} cea708char;


/* This struct keeps track of one cea-708 CC window. There are up to 8. As new
  * windows are created, the text they contain is visible on the screen (if the
  * window visible flag is set). When a window is deleted, all text within the
  * window is erased from the screen. Windows may be initialized and made visible
  * then hidden. Each transition should cause new text cues to be emitted as
  * text is displayed and removed from the screen.
  */
typedef struct
{
  /* The current attributes which will be used for the next text string */
  cea708PenColor pen_color;
  cea708PenAttributes pen_attributes;

  /* true to indicate the window has not been created.
   * set to true on delete, false on subsequent define command
   * if true, reset pen position to 0,0 on window creation
   */
  gboolean deleted;

  /* Text position */
  guint16 pen_row;
  guint16 pen_col;
  /* window display priority */
  guint8 priority;
  /* window position on screen 0-8 */
  guint8 anchor_point;
  /* 1 = anchor vertical/horizontal coordinates, 0 = physical screen coordinate, aka. rp */
  guint8 relative_position;
  /* vertical position of windows anchor point, 0-74 or if rp=1 then 0-99 */
  guint8 anchor_vertical;
  /* horz position of window anchor point, 0-209(16:9) 0-159(4:3) or if rp=1 then 0-99 */
  guint8 anchor_horizontal;
  /* vert position of upper left corner of window */
  gfloat screen_vertical;
  /* horz position of upper left corner of window */
  gfloat screen_horizontal;
  /* virtual rows of text - 1, (ex. rc=2 means there are 3 rows) */
  guint8 row_count;
  /* virtual columns of text, 0-41(16:9) 0-31(4:3) - 1 */
  guint8 column_count;
  /* 1 = fixes #rows of caption text, 0 = more rows may be added */
  guint8 row_lock;
  /* 1 = fixes #columns of caption text, 0 = more columns may be added */
  guint8 column_lock;
  /* TRUE = window is visible, FALSE = window not visible */
  gboolean visible;
  /* specifies 1 of 7 static preset window. attribute styles, during window create,
   * 0 = use style #1, during window update, 0 = no window, attributes will be changed
   */
  guint8 style_id;
  /* specifies 1 of 7 static preset pen attributes, during window create,
   * 0 = use pen style #1, during window update, 0 = do not change pen attributes
   */
  guint8 pen_style_id;
  /* timestamp when this window became visible */
  guint64 start_time;

  /* window attributes */
  guint8 justify_mode;
  guint8 print_direction;
  guint8 scroll_direction;
  gboolean word_wrap;
  guint8 display_effect;
  guint8 effect_direction;
  guint8 effect_speed;
  guint8 fill_color;
  guint8 fill_opacity;
  guint8 border_type;
  guint8 border_color;

  /* Character position offsets for the upper left corner of the window */
  guint v_offset;
  guint h_offset;

  /* The char array that text is written into, using the current pen position */
  cea708char text[WINDOW_MAX_ROWS][WINDOW_MAX_COLS];

  PangoLayout *layout;
  gdouble shadow_offset;
  gdouble outline_offset;
  guchar *text_image;
  gint image_width;
  gint image_height;
  gboolean updated;
} cea708Window;

struct _Cea708Dec
{
  /* output data storage */
  GSList *text_list;

  /* simulation of 708 CC windows */
  cea708Window *cc_windows[MAX_708_WINDOWS];
  guint8 current_window;
  gchar *default_font_desc;
  PangoContext *pango_context;

  /* a counter used to ignore bytes in CC text stream following commands */
  gint8 output_ignore;
  /* most recent timestamp from userdata */
  guint64 current_time;

  /* desired_service selects the service that will be decoded. If
   * desired_service = -1 (default) no decoding based on service number will
   * occur. Service #0 is reserved, and the valid range of service numbers
   * is 1-7. with 1 being primary caption service and 2 being the secondary
   * language service. If service_number is 7, then the extended_service_number is added and used instead of the service_number */
  gint8 desired_service;

  gboolean use_ARGB;
  gint width;
  gint height;
};

Cea708Dec *gst_cea708dec_create (PangoContext * pango_context);

void       gst_cea708dec_free (Cea708Dec *dec);

void
gst_cea708dec_set_service_number (Cea708Dec * decoder, gint8 desired_service);
gboolean
gst_cea708dec_process_dtvcc_packet (Cea708Dec * decoder, guint8 * dtvcc_buffer, gsize dtvcc_size);
void
gst_cea708dec_set_video_width_height (Cea708Dec * decoder, gint width, gint height);
void gst_cea708_decoder_init_debug(void);

  G_END_DECLS
#endif /* __GST_CEA708_DEC_H__ */
