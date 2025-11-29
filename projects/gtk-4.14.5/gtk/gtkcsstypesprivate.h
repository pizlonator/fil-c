/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#pragma once

#include <glib-object.h>
#include <gsk/gsk.h>

#include <gtk/gtkenums.h>

G_BEGIN_DECLS

typedef struct _GtkCssNode GtkCssNode;
typedef struct _GtkCssNodeDeclaration GtkCssNodeDeclaration;
typedef struct _GtkCssStyle GtkCssStyle;
typedef struct _GtkCssStaticStyle GtkCssStaticStyle;

#define GTK_CSS_CHANGE_CLASS                          (1ULL <<  0)
#define GTK_CSS_CHANGE_NAME                           (1ULL <<  1)
#define GTK_CSS_CHANGE_ID                             (1ULL <<  2)
#define GTK_CSS_CHANGE_FIRST_CHILD                    (1ULL <<  3)
#define GTK_CSS_CHANGE_LAST_CHILD                     (1ULL <<  4)
#define GTK_CSS_CHANGE_NTH_CHILD                      (1ULL <<  5)
#define GTK_CSS_CHANGE_NTH_LAST_CHILD                 (1ULL <<  6)
#define GTK_CSS_CHANGE_STATE                          (1ULL <<  7)
#define GTK_CSS_CHANGE_HOVER                          (1ULL <<  8)
#define GTK_CSS_CHANGE_DISABLED                       (1ULL <<  9)
#define GTK_CSS_CHANGE_BACKDROP                       (1ULL << 10)
#define GTK_CSS_CHANGE_SELECTED                       (1ULL << 11)

#define GTK_CSS_CHANGE_SIBLING_SHIFT 12

#define GTK_CSS_CHANGE_SIBLING_CLASS                  (1ULL << 12)
#define GTK_CSS_CHANGE_SIBLING_NAME                   (1ULL << 13)
#define GTK_CSS_CHANGE_SIBLING_ID                     (1ULL << 14)
#define GTK_CSS_CHANGE_SIBLING_FIRST_CHILD            (1ULL << 15)
#define GTK_CSS_CHANGE_SIBLING_LAST_CHILD             (1ULL << 16)
#define GTK_CSS_CHANGE_SIBLING_NTH_CHILD              (1ULL << 17)
#define GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD         (1ULL << 18)
#define GTK_CSS_CHANGE_SIBLING_STATE                  (1ULL << 19)
#define GTK_CSS_CHANGE_SIBLING_HOVER                  (1ULL << 20)
#define GTK_CSS_CHANGE_SIBLING_DISABLED               (1ULL << 21)
#define GTK_CSS_CHANGE_SIBLING_BACKDROP               (1ULL << 22)
#define GTK_CSS_CHANGE_SIBLING_SELECTED               (1ULL << 23)

#define GTK_CSS_CHANGE_PARENT_SHIFT (GTK_CSS_CHANGE_SIBLING_SHIFT + GTK_CSS_CHANGE_SIBLING_SHIFT)

#define GTK_CSS_CHANGE_PARENT_CLASS                   (1ULL << 24)
#define GTK_CSS_CHANGE_PARENT_NAME                    (1ULL << 25)
#define GTK_CSS_CHANGE_PARENT_ID                      (1ULL << 26)
#define GTK_CSS_CHANGE_PARENT_FIRST_CHILD             (1ULL << 27)
#define GTK_CSS_CHANGE_PARENT_LAST_CHILD              (1ULL << 28)
#define GTK_CSS_CHANGE_PARENT_NTH_CHILD               (1ULL << 29)
#define GTK_CSS_CHANGE_PARENT_NTH_LAST_CHILD          (1ULL << 30)
#define GTK_CSS_CHANGE_PARENT_STATE                   (1ULL << 31)
#define GTK_CSS_CHANGE_PARENT_HOVER                   (1ULL << 32)
#define GTK_CSS_CHANGE_PARENT_DISABLED                (1ULL << 33)
#define GTK_CSS_CHANGE_PARENT_BACKDROP                (1ULL << 34)
#define GTK_CSS_CHANGE_PARENT_SELECTED                (1ULL << 35)

#define GTK_CSS_CHANGE_PARENT_SIBLING_SHIFT (GTK_CSS_CHANGE_PARENT_SHIFT + GTK_CSS_CHANGE_SIBLING_SHIFT)

#define GTK_CSS_CHANGE_PARENT_SIBLING_CLASS           (1ULL << 36)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NAME            (1ULL << 37)
#define GTK_CSS_CHANGE_PARENT_SIBLING_ID              (1ULL << 38)
#define GTK_CSS_CHANGE_PARENT_SIBLING_FIRST_CHILD     (1ULL << 39)
#define GTK_CSS_CHANGE_PARENT_SIBLING_LAST_CHILD      (1ULL << 40)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NTH_CHILD       (1ULL << 41)
#define GTK_CSS_CHANGE_PARENT_SIBLING_NTH_LAST_CHILD  (1ULL << 42)
#define GTK_CSS_CHANGE_PARENT_SIBLING_STATE           (1ULL << 43)
#define GTK_CSS_CHANGE_PARENT_SIBLING_HOVER           (1ULL << 44)
#define GTK_CSS_CHANGE_PARENT_SIBLING_DISABLED        (1ULL << 45)
#define GTK_CSS_CHANGE_PARENT_SIBLING_BACKDROP        (1ULL << 46)
#define GTK_CSS_CHANGE_PARENT_SIBLING_SELECTED        (1ULL << 47)

/* add more */
#define GTK_CSS_CHANGE_SOURCE                         (1ULL << 48)
#define GTK_CSS_CHANGE_PARENT_STYLE                   (1ULL << 49)
#define GTK_CSS_CHANGE_TIMESTAMP                      (1ULL << 50)
#define GTK_CSS_CHANGE_ANIMATIONS                     (1ULL << 51)

#define GTK_CSS_CHANGE_RESERVED_BIT                   (1ULL << 62)

typedef guint64 GtkCssChange;

#define GTK_CSS_CHANGE_POSITION (GTK_CSS_CHANGE_FIRST_CHILD | \
                                 GTK_CSS_CHANGE_LAST_CHILD  | \
                                 GTK_CSS_CHANGE_NTH_CHILD   | \
                                 GTK_CSS_CHANGE_NTH_LAST_CHILD)
#define GTK_CSS_CHANGE_SIBLING_POSITION (GTK_CSS_CHANGE_POSITION << GTK_CSS_CHANGE_SIBLING_SHIFT)

#define GTK_CSS_CHANGE_ANY_SELF (GTK_CSS_CHANGE_CLASS    | \
                                 GTK_CSS_CHANGE_NAME     | \
                                 GTK_CSS_CHANGE_ID       | \
                                 GTK_CSS_CHANGE_POSITION | \
                                 GTK_CSS_CHANGE_STATE    | \
                                 GTK_CSS_CHANGE_DISABLED | \
                                 GTK_CSS_CHANGE_BACKDROP | \
                                 GTK_CSS_CHANGE_SELECTED | \
                                 GTK_CSS_CHANGE_HOVER)
#define GTK_CSS_CHANGE_ANY_SIBLING (GTK_CSS_CHANGE_ANY_SELF << GTK_CSS_CHANGE_SIBLING_SHIFT)
#define GTK_CSS_CHANGE_ANY_PARENT (GTK_CSS_CHANGE_ANY_SELF << GTK_CSS_CHANGE_PARENT_SHIFT)
#define GTK_CSS_CHANGE_ANY_PARENT_SIBLING (GTK_CSS_CHANGE_ANY_SELF << GTK_CSS_CHANGE_PARENT_SIBLING_SHIFT)

#define GTK_CSS_CHANGE_ANY (GTK_CSS_CHANGE_ANY_SELF           | \
                            GTK_CSS_CHANGE_ANY_SIBLING        | \
                            GTK_CSS_CHANGE_ANY_PARENT         | \
                            GTK_CSS_CHANGE_ANY_PARENT_SIBLING | \
                            GTK_CSS_CHANGE_SOURCE             | \
                            GTK_CSS_CHANGE_PARENT_STYLE       | \
                            GTK_CSS_CHANGE_TIMESTAMP          | \
                            GTK_CSS_CHANGE_ANIMATIONS)

/*
 * GtkCssAffects:
 * @GTK_CSS_AFFECTS_CONTENT: The content rendering is affected.
 *   This does not include things that affect the font. For those,
 *   see @GTK_CSS_AFFECTS_TEXT.
 * @GTK_CSS_AFFECTS_BACKGROUND: The background rendering is affected.
 * @GTK_CSS_AFFECTS_BORDER: The border styling is affected.
 * @GTK_CSS_AFFECTS_TEXT_ATTRS: Text attributes are affected.
 * @GTK_CSS_AFFECTS_TEXT_SIZE: Text size is affected.
 * @GTK_CSS_AFFECTS_TEXT_CONTENT: Text rendering is affected, but size or
 *   attributes are not.
 * @GTK_CSS_AFFECTS_ICON_SIZE: Icon size is affected.
 * @GTK_CSS_AFFECTS_ICON_TEXTURE: The icon texture has changed and needs to be
 *   reloaded.
 * @GTK_CSS_AFFECTS_ICON_REDRAW: Icons need to be redrawn (both symbolic and
 *   non-symbolic).
 * @GTK_CSS_AFFECTS_ICON_REDRAW_SYMBOLIC: Symbolic icons need to be redrawn.
 * @GTK_CSS_AFFECTS_OUTLINE: The outline styling is affected.
 * @GTK_CSS_AFFECTS_SIZE: Changes in this property may have an effect
 *   on the allocated size of the element. Changes in these properties
 *   should cause a recomputation of the element's allocated size.
 * @GTK_CSS_AFFECTS_POSTEFFECT: An effect is applied after drawing that changes
 * @GTK_CSS_AFFECTS_TEXT: Affects anything related to text rendering.
 * @GTK_CSS_AFFECTS_REDRAW: Affects anything that requires redraw.
 * @GTK_CSS_AFFECTS_TRANSFORM: Affects the element transformation.
 *
 * The generic effects that a CSS property can have. If a value is
 * set, then the property will have an influence on that feature.
 *
 * Note that multiple values can be set.
 */
typedef enum {
  GTK_CSS_AFFECTS_CONTENT              = (1 << 0),
  GTK_CSS_AFFECTS_BACKGROUND           = (1 << 1),
  GTK_CSS_AFFECTS_BORDER               = (1 << 2),
  GTK_CSS_AFFECTS_TEXT_ATTRS           = (1 << 3),
  GTK_CSS_AFFECTS_TEXT_SIZE            = (1 << 4),
  GTK_CSS_AFFECTS_TEXT_CONTENT         = (1 << 5),
  GTK_CSS_AFFECTS_ICON_SIZE            = (1 << 6),
  GTK_CSS_AFFECTS_ICON_TEXTURE         = (1 << 7),
  GTK_CSS_AFFECTS_ICON_REDRAW          = (1 << 8),
  GTK_CSS_AFFECTS_ICON_REDRAW_SYMBOLIC = (1 << 9),
  GTK_CSS_AFFECTS_OUTLINE              = (1 << 10),
  GTK_CSS_AFFECTS_SIZE                 = (1 << 11),
  GTK_CSS_AFFECTS_POSTEFFECT           = (1 << 12),
  GTK_CSS_AFFECTS_TRANSFORM            = (1 << 13),
} GtkCssAffects;

#define GTK_CSS_AFFECTS_REDRAW (GTK_CSS_AFFECTS_CONTENT |       \
                                GTK_CSS_AFFECTS_BACKGROUND |    \
                                GTK_CSS_AFFECTS_BORDER |        \
                                GTK_CSS_AFFECTS_OUTLINE |       \
                                GTK_CSS_AFFECTS_POSTEFFECT)

#define GTK_CSS_AFFECTS_TEXT (GTK_CSS_AFFECTS_TEXT_SIZE | \
                              GTK_CSS_AFFECTS_TEXT_CONTENT)


enum { /*< skip >*/
  GTK_CSS_PROPERTY_COLOR,
  GTK_CSS_PROPERTY_DPI,
  GTK_CSS_PROPERTY_FONT_SIZE,
  GTK_CSS_PROPERTY_ICON_PALETTE,
  GTK_CSS_PROPERTY_BACKGROUND_COLOR,
  GTK_CSS_PROPERTY_FONT_FAMILY,
  GTK_CSS_PROPERTY_FONT_STYLE,
  GTK_CSS_PROPERTY_FONT_WEIGHT,
  GTK_CSS_PROPERTY_FONT_STRETCH,
  GTK_CSS_PROPERTY_LETTER_SPACING,
  GTK_CSS_PROPERTY_TEXT_DECORATION_LINE,
  GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR,
  GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE,
  GTK_CSS_PROPERTY_TEXT_TRANSFORM,
  GTK_CSS_PROPERTY_FONT_KERNING,
  GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES,
  GTK_CSS_PROPERTY_FONT_VARIANT_POSITION,
  GTK_CSS_PROPERTY_FONT_VARIANT_CAPS,
  GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC,
  GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES,
  GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN,
  GTK_CSS_PROPERTY_TEXT_SHADOW,
  GTK_CSS_PROPERTY_BOX_SHADOW,
  GTK_CSS_PROPERTY_MARGIN_TOP,
  GTK_CSS_PROPERTY_MARGIN_LEFT,
  GTK_CSS_PROPERTY_MARGIN_BOTTOM,
  GTK_CSS_PROPERTY_MARGIN_RIGHT,
  GTK_CSS_PROPERTY_PADDING_TOP,
  GTK_CSS_PROPERTY_PADDING_LEFT,
  GTK_CSS_PROPERTY_PADDING_BOTTOM,
  GTK_CSS_PROPERTY_PADDING_RIGHT,
  GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
  GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
  GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
  GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
  GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
  GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_STYLE,
  GTK_CSS_PROPERTY_OUTLINE_WIDTH,
  GTK_CSS_PROPERTY_OUTLINE_OFFSET,
  GTK_CSS_PROPERTY_BACKGROUND_CLIP,
  GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
  GTK_CSS_PROPERTY_BACKGROUND_SIZE,
  GTK_CSS_PROPERTY_BACKGROUND_POSITION,
  GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
  GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
  GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
  GTK_CSS_PROPERTY_OUTLINE_COLOR,
  GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
  GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
  GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
  GTK_CSS_PROPERTY_ICON_SOURCE,
  GTK_CSS_PROPERTY_ICON_SIZE,
  GTK_CSS_PROPERTY_ICON_SHADOW,
  GTK_CSS_PROPERTY_ICON_STYLE,
  GTK_CSS_PROPERTY_ICON_TRANSFORM,
  GTK_CSS_PROPERTY_ICON_FILTER,
  GTK_CSS_PROPERTY_BORDER_SPACING,
  GTK_CSS_PROPERTY_TRANSFORM,
  GTK_CSS_PROPERTY_TRANSFORM_ORIGIN,
  GTK_CSS_PROPERTY_MIN_WIDTH,
  GTK_CSS_PROPERTY_MIN_HEIGHT,
  GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
  GTK_CSS_PROPERTY_TRANSITION_DURATION,
  GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_TRANSITION_DELAY,
  GTK_CSS_PROPERTY_ANIMATION_NAME,
  GTK_CSS_PROPERTY_ANIMATION_DURATION,
  GTK_CSS_PROPERTY_ANIMATION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_ANIMATION_ITERATION_COUNT,
  GTK_CSS_PROPERTY_ANIMATION_DIRECTION,
  GTK_CSS_PROPERTY_ANIMATION_PLAY_STATE,
  GTK_CSS_PROPERTY_ANIMATION_DELAY,
  GTK_CSS_PROPERTY_ANIMATION_FILL_MODE,
  GTK_CSS_PROPERTY_OPACITY,
  GTK_CSS_PROPERTY_FILTER,
  GTK_CSS_PROPERTY_CARET_COLOR,
  GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR,
  GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS,
  GTK_CSS_PROPERTY_FONT_VARIATION_SETTINGS,
  GTK_CSS_PROPERTY_LINE_HEIGHT,
  /* add more */
  GTK_CSS_PROPERTY_N_PROPERTIES
};

typedef enum /*< skip >*/ {
  GTK_CSS_AREA_BORDER_BOX,
  GTK_CSS_AREA_PADDING_BOX,
  GTK_CSS_AREA_CONTENT_BOX
} GtkCssArea;

typedef enum /*< skip >*/ {
  GTK_CSS_DIRECTION_NORMAL,
  GTK_CSS_DIRECTION_REVERSE,
  GTK_CSS_DIRECTION_ALTERNATE,
  GTK_CSS_DIRECTION_ALTERNATE_REVERSE
} GtkCssDirection;

typedef enum /*< skip >*/ {
  GTK_CSS_PLAY_STATE_RUNNING,
  GTK_CSS_PLAY_STATE_PAUSED
} GtkCssPlayState;

typedef enum /*< skip >*/ {
  GTK_CSS_FILL_NONE,
  GTK_CSS_FILL_FORWARDS,
  GTK_CSS_FILL_BACKWARDS,
  GTK_CSS_FILL_BOTH
} GtkCssFillMode;

typedef enum /*< skip >*/ {
  GTK_CSS_ICON_STYLE_REQUESTED,
  GTK_CSS_ICON_STYLE_REGULAR,
  GTK_CSS_ICON_STYLE_SYMBOLIC
} GtkCssIconStyle;

typedef enum /*< skip >*/ {
  /* relative font sizes */
  GTK_CSS_FONT_SIZE_SMALLER,
  GTK_CSS_FONT_SIZE_LARGER,
  /* absolute font sizes */
  GTK_CSS_FONT_SIZE_XX_SMALL,
  GTK_CSS_FONT_SIZE_X_SMALL,
  GTK_CSS_FONT_SIZE_SMALL,
  GTK_CSS_FONT_SIZE_MEDIUM,
  GTK_CSS_FONT_SIZE_LARGE,
  GTK_CSS_FONT_SIZE_X_LARGE,
  GTK_CSS_FONT_SIZE_XX_LARGE
} GtkCssFontSize;

typedef enum /*< skip >*/ {
  GTK_CSS_TEXT_DECORATION_LINE_NONE         = 1 << 0,
  GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE    = 1 << 1,
  GTK_CSS_TEXT_DECORATION_LINE_OVERLINE     = 1 << 2,
  GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH = 1 << 3
} GtkTextDecorationLine;

typedef enum /*< skip >*/ {
  GTK_CSS_TEXT_DECORATION_STYLE_SOLID,
  GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE,
  GTK_CSS_TEXT_DECORATION_STYLE_WAVY
} GtkTextDecorationStyle;

typedef enum /*< skip >*/ {
  GTK_CSS_TEXT_TRANSFORM_NONE,
  GTK_CSS_TEXT_TRANSFORM_LOWERCASE,
  GTK_CSS_TEXT_TRANSFORM_UPPERCASE,
  GTK_CSS_TEXT_TRANSFORM_CAPITALIZE,
} GtkTextTransform;

/* for the order in arrays */
typedef enum /*< skip >*/ {
  GTK_CSS_TOP,
  GTK_CSS_RIGHT,
  GTK_CSS_BOTTOM,
  GTK_CSS_LEFT
} GtkCssSide;

typedef enum /*< skip >*/ {
  GTK_CSS_DIMENSION_PERCENTAGE,
  GTK_CSS_DIMENSION_NUMBER,
  GTK_CSS_DIMENSION_LENGTH,
  GTK_CSS_DIMENSION_ANGLE,
  GTK_CSS_DIMENSION_TIME
} GtkCssDimension;

typedef enum /*< skip >*/ {
  /* CSS term: <number> */
  GTK_CSS_NUMBER,
  /* CSS term: <percentage> */
  GTK_CSS_PERCENT,
  /* CSS term: <length> */
  GTK_CSS_PX,
  GTK_CSS_PT,
  GTK_CSS_EM,
  GTK_CSS_EX,
  GTK_CSS_REM,
  GTK_CSS_PC,
  GTK_CSS_IN,
  GTK_CSS_CM,
  GTK_CSS_MM,
  /* CSS term: <angle> */
  GTK_CSS_RAD,
  GTK_CSS_DEG,
  GTK_CSS_GRAD,
  GTK_CSS_TURN,
  /* CSS term: <time> */
  GTK_CSS_S,
  GTK_CSS_MS,
} GtkCssUnit;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_KERNING_AUTO,
  GTK_CSS_FONT_KERNING_NORMAL,
  GTK_CSS_FONT_KERNING_NONE
} GtkCssFontKerning;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL                     = 1 << 0,
  GTK_CSS_FONT_VARIANT_LIGATURE_NONE                       = 1 << 1,
  GTK_CSS_FONT_VARIANT_LIGATURE_COMMON_LIGATURES           = 1 << 2,
  GTK_CSS_FONT_VARIANT_LIGATURE_NO_COMMON_LIGATURES        = 1 << 3,
  GTK_CSS_FONT_VARIANT_LIGATURE_DISCRETIONARY_LIGATURES    = 1 << 4,
  GTK_CSS_FONT_VARIANT_LIGATURE_NO_DISCRETIONARY_LIGATURES = 1 << 5,
  GTK_CSS_FONT_VARIANT_LIGATURE_HISTORICAL_LIGATURES       = 1 << 6,
  GTK_CSS_FONT_VARIANT_LIGATURE_NO_HISTORICAL_LIGATURES    = 1 << 7,
  GTK_CSS_FONT_VARIANT_LIGATURE_CONTEXTUAL                 = 1 << 8,
  GTK_CSS_FONT_VARIANT_LIGATURE_NO_CONTEXTUAL              = 1 << 9
} GtkCssFontVariantLigature;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_POSITION_NORMAL,
  GTK_CSS_FONT_VARIANT_POSITION_SUB,
  GTK_CSS_FONT_VARIANT_POSITION_SUPER
} GtkCssFontVariantPosition;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL             = 1 << 0,
  GTK_CSS_FONT_VARIANT_NUMERIC_LINING_NUMS        = 1 << 1,
  GTK_CSS_FONT_VARIANT_NUMERIC_OLDSTYLE_NUMS      = 1 << 2,
  GTK_CSS_FONT_VARIANT_NUMERIC_PROPORTIONAL_NUMS  = 1 << 3,
  GTK_CSS_FONT_VARIANT_NUMERIC_TABULAR_NUMS       = 1 << 4,
  GTK_CSS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS = 1 << 5,
  GTK_CSS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS  = 1 << 6,
  GTK_CSS_FONT_VARIANT_NUMERIC_ORDINAL            = 1 << 7,
  GTK_CSS_FONT_VARIANT_NUMERIC_SLASHED_ZERO       = 1 << 8
} GtkCssFontVariantNumeric;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_CAPS_NORMAL,
  GTK_CSS_FONT_VARIANT_CAPS_SMALL_CAPS,
  GTK_CSS_FONT_VARIANT_CAPS_ALL_SMALL_CAPS,
  GTK_CSS_FONT_VARIANT_CAPS_PETITE_CAPS,
  GTK_CSS_FONT_VARIANT_CAPS_ALL_PETITE_CAPS,
  GTK_CSS_FONT_VARIANT_CAPS_UNICASE,
  GTK_CSS_FONT_VARIANT_CAPS_TITLING_CAPS
} GtkCssFontVariantCaps;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL,
  GTK_CSS_FONT_VARIANT_ALTERNATE_HISTORICAL_FORMS
} GtkCssFontVariantAlternate;

typedef enum /*< skip >*/ {
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL       = 1 << 0,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS78        = 1 << 1,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS83        = 1 << 2,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS90        = 1 << 3,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS04        = 1 << 4,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED   = 1 << 5,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL  = 1 << 6,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH   = 1 << 7,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_PROPORTIONAL = 1 << 8,
  GTK_CSS_FONT_VARIANT_EAST_ASIAN_RUBY         = 1 << 9
} GtkCssFontVariantEastAsian;

static inline GtkCssChange
_gtk_css_change_for_sibling (GtkCssChange match)
{
#define BASE_STATES ( GTK_CSS_CHANGE_CLASS \
                    | GTK_CSS_CHANGE_NAME \
                    | GTK_CSS_CHANGE_ID \
                    | GTK_CSS_CHANGE_FIRST_CHILD \
                    | GTK_CSS_CHANGE_LAST_CHILD \
                    | GTK_CSS_CHANGE_NTH_CHILD \
                    | GTK_CSS_CHANGE_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_STATE \
                    | GTK_CSS_CHANGE_HOVER \
                    | GTK_CSS_CHANGE_DISABLED \
                    | GTK_CSS_CHANGE_SELECTED \
                    | GTK_CSS_CHANGE_BACKDROP)

#define KEEP_STATES ( ~(BASE_STATES|GTK_CSS_CHANGE_SOURCE|GTK_CSS_CHANGE_PARENT_STYLE) \
                    | GTK_CSS_CHANGE_NTH_CHILD \
                    | GTK_CSS_CHANGE_NTH_LAST_CHILD)

  return (match & KEEP_STATES) | ((match & BASE_STATES) << GTK_CSS_CHANGE_SIBLING_SHIFT);

#undef BASE_STATES
#undef KEEP_STATES
}

static inline GtkCssChange
_gtk_css_change_for_child (GtkCssChange match)
{
#define BASE_STATES ( GTK_CSS_CHANGE_CLASS \
                    | GTK_CSS_CHANGE_NAME \
                    | GTK_CSS_CHANGE_ID \
                    | GTK_CSS_CHANGE_FIRST_CHILD \
                    | GTK_CSS_CHANGE_LAST_CHILD \
                    | GTK_CSS_CHANGE_NTH_CHILD \
                    | GTK_CSS_CHANGE_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_STATE \
                    | GTK_CSS_CHANGE_HOVER \
                    | GTK_CSS_CHANGE_DISABLED \
                    | GTK_CSS_CHANGE_BACKDROP \
                    | GTK_CSS_CHANGE_SELECTED \
                    | GTK_CSS_CHANGE_SIBLING_CLASS \
                    | GTK_CSS_CHANGE_SIBLING_NAME \
                    | GTK_CSS_CHANGE_SIBLING_ID \
                    | GTK_CSS_CHANGE_SIBLING_FIRST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_LAST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_NTH_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_STATE \
                    | GTK_CSS_CHANGE_SIBLING_HOVER \
                    | GTK_CSS_CHANGE_SIBLING_DISABLED \
                    | GTK_CSS_CHANGE_SIBLING_BACKDROP \
                    | GTK_CSS_CHANGE_SIBLING_SELECTED)

#define KEEP_STATES (~(BASE_STATES|GTK_CSS_CHANGE_SOURCE|GTK_CSS_CHANGE_PARENT_STYLE))

  return (match & KEEP_STATES) | ((match & BASE_STATES) << GTK_CSS_CHANGE_PARENT_SHIFT);

#undef BASE_STATES
#undef KEEP_STATES
}

GtkCssDimension         gtk_css_unit_get_dimension               (GtkCssUnit         unit) G_GNUC_CONST;

char *                  gtk_css_change_to_string                 (GtkCssChange       change) G_GNUC_MALLOC;
void                    gtk_css_change_print                     (GtkCssChange       change,
                                                                  GString           *string);

const char *            gtk_css_pseudoclass_name                 (GtkStateFlags      flags) G_GNUC_CONST;

/* These hash functions are selected so they achieve 2 things:
 * 1. collision free among each other
 *    Hashing the CSS selectors "button", ".button" and "#button" should give different results.
 *    So we multiply the hash values with distinct prime numbers.
 * 2. generate small numbers
 *    It's why the code uses quarks instead of interned strings. Interned strings are random
 *    pointers, quarks are numbers increasing from 0.
 * Both of these goals should achieve a bloom filter for selector matching that is as free
 * of collisions as possible.
 */
static inline guint
gtk_css_hash_class (GQuark klass)
{
  return klass * 5;
}
static inline guint
gtk_css_hash_name (GQuark name)
{
  return name * 7;
}
static inline guint
gtk_css_hash_id (GQuark id)
{
  return id * 11;
}

G_END_DECLS

