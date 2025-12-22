/* GStreamer
 * Copyright (C) <2011> Stefan Sauer <ensonic@users.sf.net>
 *
 * gstdrawhelpers.h: simple drawing helpers
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
 
/* FIXME: add versions that don't ignore alpha */
 
#define draw_dot(_vd, _x, _y, _st, _c) G_STMT_START {                          \
  _vd[(_y * _st) + _x] = _c;                                                   \
} G_STMT_END

#define draw_dot_c(_vd, _x, _y, _st, _c) G_STMT_START {                        \
  _vd[(_y * _st) + _x] |= _c;                                                  \
} G_STMT_END

#define draw_dot_aa(_vd, _x, _y, _st, _c, _f)  G_STMT_START {                  \
  guint32 _oc, _c1, _c2, _c3;                                                  \
                                                                               \
  _oc = _vd[(_y * _st) + _x];                                                  \
  _c3 = (_oc & 0xff) + ((_c & 0xff) * _f);                                     \
  _c3 = MIN(_c3, 255);                                                         \
  _c2 = ((_oc & 0xff00) >> 8) + (((_c & 0xff00) >> 8) * _f);                   \
  _c2 = MIN(_c2, 255);                                                         \
  _c1 = ((_oc & 0xff0000) >> 16) + (((_c & 0xff0000) >> 16) * _f);             \
  _c1 = MIN(_c1, 255);                                                         \
  _vd[(_y * _st) + _x] = (_c1 << 16) | (_c2 << 8) | _c3;                       \
} G_STMT_END

#define draw_line(_vd, _x1, _x2, _y1, _y2, _st, _c) G_STMT_START {             \
  guint _i, _j, _x, _y;                                                        \
  gint _dx = _x2 - _x1, _dy = _y2 - _y1;                                       \
  gfloat _f;                                                                   \
                                                                               \
  _j = abs (_dx) > abs (_dy) ? abs (_dx) : abs (_dy);                          \
  for (_i = 0; _i < _j; _i++) {                                                \
    _f = (gfloat) _i / (gfloat) _j;                                            \
    _x = _x1 + _dx * _f;                                                       \
    _y = _y1 + _dy * _f;                                                       \
    draw_dot (_vd, _x, _y, _st, _c);                                           \
  }                                                                            \
} G_STMT_END

#define draw_line_aa(_vd, _x1, _x2, _y1, _y2, _st, _c) G_STMT_START {          \
  guint _i, _j, _x, _y;                                                        \
  gint _dx = _x2 - _x1, _dy = _y2 - _y1;                                       \
  gfloat _f, _rx, _ry, _fx, _fy;                                               \
                                                                               \
  _j = abs (_dx) > abs (_dy) ? abs (_dx) : abs (_dy);                          \
  for (_i = 0; _i < _j; _i++) {                                                \
    _f = (gfloat) _i / (gfloat) _j;                                            \
    _rx = _x1 + _dx * _f;                                                      \
    _ry = _y1 + _dy * _f;                                                      \
    _x = (guint)_rx;                                                           \
    _y = (guint)_ry;                                                           \
    _fx = _rx - (gfloat)_x;                                                    \
    _fy = _ry - (gfloat)_y;                                                    \
                                                                               \
    _f = ((1.0 - _fx) + (1.0 - _fy)) / 2.0;                                    \
    draw_dot_aa (_vd, _x, _y, _st, _c, _f);                                    \
                                                                               \
    _f = (_fx + (1.0 - _fy)) / 2.0;                                            \
    draw_dot_aa (_vd, (_x + 1), _y, _st, _c, _f);                              \
                                                                               \
    _f = ((1.0 - _fx) + _fy) / 2.0;                                            \
    draw_dot_aa (_vd, _x, (_y + 1), _st, _c, _f);                              \
                                                                               \
    _f = (_fx + _fy) / 2.0;                                                    \
    draw_dot_aa (_vd, (_x + 1), (_y + 1), _st, _c, _f);                        \
  }                                                                            \
} G_STMT_END

