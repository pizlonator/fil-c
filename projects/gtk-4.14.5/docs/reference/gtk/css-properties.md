Title: GTK CSS Properties
Slug: css-properties

GTK supports CSS properties and shorthands as far as they can be applied
in the context of widgets, and adds its own properties only when needed.
All GTK-specific properties have a -gtk prefix.

## Basic types

All properties support the following keywords: inherit, initial, unset,
with the same meaning as defined in the
[CSS Cascading and Inheritance](https://www.w3.org/TR/css3-cascade/#defaulting-keywords)
spec.

The following units are supported for basic datatypes:

Length
: px, pt, em, ex, rem, pc, in, cm, mm, calc()

Percentage
: %, calc()

Angle
: deg, grad, turn, calc()

Time
: s, ms, calc()

Length values with the em or ex units are resolved using the font
size value, unless they occur in setting the font-size itself, in
which case they are resolved using the inherited font size value.

The rem unit is resolved using the initial font size value, which is
not quite the same as the CSS definition of rem.

The calc() notation adds considerable expressive power. There are limits
on what types can be combined in such an expression (e.g. it does not make
sense to add a number and a time). For the full details, see the
[CSS3 Values and Units](https://www.w3.org/TR/css3-values/#calc-notation)
spec.

A common pattern among shorthand properties (called 'four sides') is one
where one to four values can be specified, to determine a value for each
side of an area. In this case, the specified values are interpreted as
follows:

4 values:
: top right bottom left

3 values:
: top horizontal bottom

2 values:
: vertical horizontal

1 value:
: all

## Colors

GTK extends the CSS syntax with several additional ways to specify colors.

The first is a reference to a color defined via a @define-color rule in CSS.
The syntax for @define-color rules is as follows:

```
@define-color Name Color
```

To refer to the color defined by a @define-color rule, prefix the name with @.

GTK also supports color expressions, which allow colors to be transformed to
new ones. Color expressions can be nested, providing a rich language to
define colors. Color expressions resemble functions, taking 1 or more colors
and in some cases a number as arguments.

`lighter(Color)`
 : produces a brighter variant of Color

`darker(Color)`
 : produces a darker variant of Color

`shade(Color, Number)`
 : changes the lightness of Color. The number ranges from 0 for black to 2 for white.

`alpha(Color, Number)`
 : replaces the alpha value of color with number (between 0 and 1)

`mix(Color1, Color2, Number)`
 : interpolates between the two colors

## Images

GTK extends the CSS syntax for images and also uses it for specifying icons.
To load a themed icon, use

```
-gtk-icontheme(Name)
```

The specified icon name is used to look up a themed icon, while taking into
account the values of the -gtk-icon-palette property. This kind of image is
mainly used as value of the -gtk-icon-source property. 

Symbolic icons from the icon theme are recolored according to the
-gtk-icon-palette property, which defines a list of named colors.
The recognized names for colors in symbolic icons are error, warning
and success. The default palette maps these three names to symbolic
colors with the names @error_color, @warning_color and @success_color
respectively. The syntax for defining a custom palette is a comma-separated
list of name-color pairs, e.g.

```
success blue, warning #fc3, error magenta
```

Recoloring is sometimes needed for images that are not part of an icon theme,
and the

```
-gtk-recolor(uri, palette)
```

syntax makes this available. -gtk-recolor requires a url as first argument.
The remaining arguments specify the color palette to use. If the palette is
not explicitly specified, the current value of the -gtk-icon-palette property
is used. 

GTK supports scaled rendering on hi-resolution displays. This works best if
images can specify normal and hi-resolution variants. From CSS, this can be
done with

```
-gtk-scaled(Image1, Image2)
```

## GTK CSS Properties

| Property   | Reference | Notes |
|:-----------|:----------|:------|
|color       | [CSS Color Level 3](https://www.w3.org/TR/css3-color/#foreground) | |
|opacity     | [CSS Color Level 3](https://www.w3.org/TR/css3-color/#opacity) | |
|filter      | [CSS Filter Effect Level 1](https://drafts.fxtf.org/filters/#FilterProperty) | |
|font-family | [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-family-prop) | defaults to gtk-font-name setting |
|font-size   | [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-size-prop) | defaults to gtk-font-name setting |
|font-style  | [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-style-prop) | |
|font-variant| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#descdef-font-variant) | only CSS2 values supported |
|font-weight | [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-weight-prop) | |
|font-stretch| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-stretch-prop) | |
|font-kerning| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-kerning-prop) | |
|font-variant-ligatures| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-ligatures-prop) | |
|font-variant-position| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-position-prop) | |
|font-variant-caps| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-position-prop) | |
|font-variant-numeric| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-numeric-prop) | |
|font-variant-alternates| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-alternates-prop) | |
|font-variant-east-asian| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-east-asian-prop) | |
|font-feature-settings| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-feature-settings-prop) | |
|font-variation-settings| [CSS Fonts Level 4](https://www.w3.org/TR/css-fonts-4/#font-variation-settings-def) | |
|-gtk-dpi|[Number](https://www.w3.org/TR/css3-values/#number-value) | defaults to screen resolution |
|font| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-prop) | CSS allows line-height, etc |
|font-variant| [CSS Fonts Level 3](https://www.w3.org/TR/css3-fonts/#font-variant-prop) | |
|caret-color|[CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#caret-color) | CSS allows an auto value |
|-gtk-secondary-caret-color|[Color](https://www.w3.org/TR/css-color-3/#valuea-def-color) | used for the secondary caret in bidirectional text |
|letter-spacing| [CSS Text Level 3](https://www.w3.org/TR/css3-text/#letter-spacing) | |
|text-transform| [CSS Text Level 3](https://www.w3.org/TR/css-text-3/#text-transform-property) | CSS allows full-width and full-size-kana. Since 4.6 |
|line-height| [CSS Inline Layout Level 3](https://www.w3.org/TR/css-inline-3/#line-height-property) | Since 4.6 |
|text-decoration-line| [CSS Text Decoration Level 3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-line-property) | |
|text-decoration-color| [CSS Text Decoration Level 3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-color-property) | |
|text-decoration-style| [CSS Text Decoration Level 3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-style-property) | CSS allows dashed and dotted |
|text-shadow| [CSS Text Decoration Level 3](https://www.w3.org/TR/css-text-decor-3/#text-shadow-property) | |
|text-decoration| [CSS Text Decoration Level 3](https://www.w3.org/TR/css-text-decor-3/#text-decoration-property) | |
|-gtk-icon-source| [Image](https://www.w3.org/TR/css-backgrounds-3/#typedef-image), `builtin` or `none` | used for builtin icons in buttons and expanders |
|-gtk-icon-size| [Length](https://www.w3.org/TR/css3-values/#length-value) | size used for builtin icons in buttons and expanders |
|-gtk-icon-style| `requested`, `regular` or `symbolic` | preferred style for application-loaded icons |
|-gtk-icon-transform| [Transform list](https://www.w3.org/TR/css-transforms-1/#typedef-transform-list) or `none` | applied to builtin and application-loaded icons |
|-gtk-icon-palette| Color palette, as explained above | used to recolor symbolic icons |
|-gtk-icon-shadow| [Shadow](https://www.w3.org/TR/css-backgrounds-3/#typedef-shadow) or `none` | applied to builtin and application-loaded icons |
|-gtk-icon-filter| [Filter value list](https://www.w3.org/TR/filter-effects-1/#typedef-filter-value-list) or `none` | applied to builtin and application-loaded icons |
|transform| [CSS Transforms Level 1](https://www.w3.org/TR/css-transforms-1/#transform-property) | |
|transform-origin| [CSS Transforms Level 1](https://www.w3.org/TR/css-transforms-1/#transform-origin-property) | CSS allows specifying a z component|
|min-width| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#min-width) | CSS allows percentages |
|min-height| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#min-height) | CSS allows percentages |
|margin-top| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#margin-top) | CSS allows percentages or auto |
|margin-right| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#margin-right) | CSS allows percentages or auto |
|margin-bottom| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#margin-bottom) | CSS allows percentages or auto |
|margin-left| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#margin-left) | CSS allows percentages or auto |
|padding-top| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#padding-top) | CSS allows percentages |
|padding-right| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#padding-right) | CSS allows percentages |
|padding-bottom| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#padding-bottom) | CSS allows percentages |
|padding-left| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#padding-left) | CSS allows percentages |
|margin| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#margin) | a 'four sides' property |
|padding| [CSS Box Model Level 3](https://www.w3.org/TR/css3-box/#padding) | a 'four sides' property |
|border-top-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
|border-right-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
|border-bottom-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
|border-left-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-width) | CSS allows other values |
|border-top-style| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-style) | |
|border-right-style| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-style) | |
|border-bottom-style| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-style) | |
|border-left-style| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-style) | |
|border-top-right-radius| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-radius) | |
|border-bottom-right-radius| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-radius) | |
|border-bottom-left-radius| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-radius) | |
|border-top-left-radius| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-radius) | |
|border-top-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-color) | |
|border-right-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-color) | |
|border-bottom-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-color) | |
|border-left-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-color) | |
|border-image-source| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-image-source) | |
|border-image-repeat| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-image-repeat) | |
|border-image-slice| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-image-slice) | a 'four sides' property |
|border-image-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-image-width) | a 'four sides' property |
|border-width| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-width) | a 'four sides' property |
|border-style| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#the-border-style) | a 'four sides' property |
|border-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-color) | a 'four sides' property |
|border-top| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-top) | |
|border-right| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-right) | |
|border-bottom| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-bottom) | |
|border-left| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-left) | |
|border| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border) | |
|border-radius| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-radius) | |
|border-image| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#border-image) | |
|outline-style| [CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#outline-style) | initial value is none, auto is not supported |
|outline-width| [CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#outline-width) | |
|outline-color| [CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#outline-color) | initial value is currentColor, invert is not supported |
|outline-offset| [CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#outline-offset) | |
|outline| [CSS Basic User Interface Level 3](https://www.w3.org/TR/css3-ui/#propdef-outline) | |
|background-color| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-color) | |
|background-clip| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-clip) | |
|background-origin| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-origin) | |
|background-size| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-size) | |
|background-position| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-position) | |
|background-repeat| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-repeat) | |
|background-image| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background-image) | not supported: urls without quotes |
|box-shadow| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#box-shadow) | |
|background-blend-mode| [CSS Compositing and Blending Level 1](https://www.w3.org/TR/compositing-1/#propdef-background-blend-mode) | only affects multiple backgrounds |
|background| [CSS Backgrounds and Borders Level 3](https://www.w3.org/TR/css3-background/#background) | |
|transition-property| [CSS Transitions](https://www.w3.org/TR/css3-transitions/#transition-property) | |
|transition-duration| [CSS Transitions](https://www.w3.org/TR/css3-transitions/#transition-duration) | |
|transition-timing-function| [CSS Transitions](https://www.w3.org/TR/css3-transitions/#transition-timing-function) | |
|transition-delay| [CSS Transitions](https://www.w3.org/TR/css3-transitions/#transition-delay) | |
|transition| [CSS Transitions](https://www.w3.org/TR/css3-transitions/#transition) | |
|animation-name| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-name) | |
|animation-duration| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-duration) | |
|animation-timing-function| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-timing-function) | |
|animation-iteration-count| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-iteration-count) | |
|animation-direction| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-direction) | |
|animation-play-state| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-play-state) | |
|animation-delay| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-delay) | |
|animation-fill-mode| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation-fill-mode) | |
|animation| [CSS Animations Level 1](https://www.w3.org/TR/css3-animations/#animation) | |
|border-spacing| [CSS Table Level 3](https://www.w3.org/TR/css-tables-3/#border-spacing-property) | respected by GtkBoxLayout, GtkGridLayout, GtkCenterLayout |
