# The Node file format

GSK render nodes can be serialized and deserialized using APIs such as `gsk_render_node_serialize()` and `gsk_render_node_deserialize()`. The intended use for this is development - primarily the development of GTK - by allowing things such as creating testsuites and benchmarks, exchanging nodes in bug reports. GTK includes the `gtk4-node-editor` application for creating such test files.

The format is a text format that follows the [CSS syntax rules](https://drafts.csswg.org/css-syntax-3/). In particular, this means that every array of bytes will produce a render node when parsed, as there is a defined error recovery method. For more details on error handling, please refer to the documentation of the parsing APIs.

The grammar of a node text representation using [the CSS value definition syntax](https://drafts.csswg.org/css-values-3/#value-defs) looks like this:
**document**: `<node>\*`
**node**: container [ "name" ] { <document> } | `<node-type> [ "name" ] { <property>* }` | "name"
**property**: `<property-name>: <node> | <value> ;`

Each node has its own `<node-type>` and supports a custom set of properties, each with their own `<property-name>` and syntax. The following paragraphs document each of the nodes and their properties.

When serializing and the value of a property equals the default value, this value will not be serialized. Serialization aims to produce an output as small as possible.

To embed newlines in strings, use \A. To break a long string into multiple lines, escape the newline with a \.

# Names

### Nodes

Nodes can be given a name by adding a string after the `<node-type>` in their definition. That same node can then be used further down in the document by specifying just the name identifying the node.

### Textures

Just like nodes, textures can be referenced by name. When defining a named texture, the name has to be placed in front of the URL.

# Nodes

### container

The **container** node is a special node that allows specifying a list of child nodes. Its contents follow the same rules as an empty document.

### blend

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bottom   | `<node>`         | color { }              | always      |
| mode     | `<blend-mode>`   | normal                 | non-default |
| top      | `<node>`         | color { }              | always      |

Creates a node like `gsk_blend_node_new()` with the given properties.

### blur

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| blur     | `<number>`       | 1                      | non-default |
| child    | `<node>`         | color { }              | always      |

Creates a node like `gsk_blur_node_new()` with the given properties.

### border

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| colors   | `<color>{1,4}`   | black                  | non-default |
| outline  | `<rounded-rect>` | 50                     | always      |
| widths   | `<number>{1,4}`  | 1                      | non-default |

Creates a node like `gsk_border_node_new()` with the given properties.

For the color and width properties, the values follow the typical CSS order
of top, right, bottom, left. If the last/left value isn't given, the 2nd/right
value is used. If the 3rd/bottom value isn't given, the 1st/top value is used.
And if the 2nd/right value also isn't given, the 1st/top value is used for
every 4 values.

### cairo

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | none                   | always      |
| pixels   | `<url>`          | none                   | non-default |
| script   | `<url>`          | none                   | non-default |

The pixels are a base64-encoded data url of png data. The script is
a base64-encoded data url of a cairo script.

### clip

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| clip     | `<rounded-rect>` | 50                     | always      |

Creates a node like `gsk_clip_node_new()` with the given properties.

As an extension, this node allows specifying a rounded rectangle for the
clip property. If that rectangle is indeed rounded, a node like
`gsk_rounded_clip_node_new()` will be created instead.

### color

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| color    | `<color>`        | #FF00CC                | always      |

Creates a node like `gsk_color_node_new()` with the given properties.

The color is chosen as an error pink so it is visible while also reminding
people to change it.

### color-matrix

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| matrix   | `<transform>`    | none                   | non-default |
| offset   | `<number>{4}`    | 0 0 0 0                | non-default |

Creates a node like `gsk_color_matrix_node_new()` with the given properties.

The matrix property accepts a <transform> for compatibility purposes, but you
should be aware that the allowed values are meant to be used on 3D transformations,
so their naming might appear awkward. However, it is always possible to use the
matrix3d() production to specify all 16 values individually.

### conic-gradient

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| center   | `<point>`        | 25, 25                 | always      |
| rotation | `<number>`       | 0                      | always      |
| stops    | `<color-stop>`   | 0 #AF0, 1 #F0C         | always      |

Creates a node like `gsk_conic_gradient_node_new()` with the given properties.

### cross-fade

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| end      | `<node>`         | color { }              | always      |
| progress | `<number>`       | 0.5                    | non-default |
| start    | `<node>`         | color { }              | always      |

Creates a node like `gsk_cross_fade_node_new()` with the given properties.

### debug

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| message  | `<string>`       | ""                     | non-default |

Creates a node like `gsk_debug_node_new()` with the given properties.

### fill

| property  | syntax          | default                | printed     |
| --------- | --------------- | ---------------------- | ----------- |
| child     | `<node>`        | *see below*            | always      |
| path      | `<string>`      | ""                     | always      |
| fill-rule | `<fill-rule>`   | winding                | always      |

Creates a node like `gsk_fill_node_new()` with the given properties.

The default child node is the default color node, but created with the
bounds of the path.

### glshader

| property   | syntax             | default                | printed     |
| ---------- | ------------------ | ---------------------- | ----------- |
| bounds     | `<rect>`           | 50                     | always      |
| sourcecode | `<string>`         | ""                     | always      |
| args       | `<uniform values>` | none                   | non-default |
| child1     | `<node>`           | none                   | non-default |
| child2     | `<node>`           | none                   | non-default |
| child3     | `<node>`           | none                   | non-default |
| child4     | `<node>`           | none                   | non-default |

Creates a GLShader node. The `sourcecode` must be a GLSL fragment shader.
The `args` must match the uniforms of simple types declared in that shader,
in order and comma-separated. The `child` properties must match the sampler
uniforms in the shader.

### inset-shadow

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| blur     | `<number>`       | 0                      | non-default |
| color    | `<color>`        | black                  | non-default |
| dx       | `<number>`       | 1                      | non-default |
| dy       | `<number>`       | 1                      | non-default |
| outline  | `<rounded-rect>` | 50                     | always      |
| spread   | `<number>`       | 0                      | non-default |

Creates a node like `gsk_inset_shadow_node_new()` with the given properties.

### linear-gradient

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| start    | `<point>`        | 0 0                    | always      |
| end      | `<point>`        | 0 50                   | always      |
| stops    | `<color-stop>`   | 0 #AF0, 1 #F0C         | always      |

Creates a node like `gsk_linear_gradient_node_new()` with the given properties.

### mask

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| source   | `<node>`         | color { }              | always      |
| mode     | `<mask-mode>`    | alpha                  | non-default |
| mask     | `<node>`         | color { }              | always      |

Creates a node like `gsk_mask_node_new()` with the given properties.

### opacity

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| opacity  | `<number>`       | 0.5                    | non-default |

Creates a node like `gsk_transform_node_new()` with the given properties.

### outset-shadow

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| blur     | `<number>`       | 0                      | non-default |
| color    | `<color>`        | black                  | non-default |
| dx       | `<number>`       | 1                      | non-default |
| dy       | `<number>`       | 1                      | non-default |
| outline  | `<rounded-rect>` | 50                     | always      |
| spread   | `<number>`       | 0                      | non-default |

Creates a node like `gsk_outset_shadow_node_new()` with the given properties.

### radial-gradient

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| center   | `<point>`        | 25 25                  | always      |
| hradius  | `<number>`       | 25                     | always      |
| vradius  | `<number>`       | 25                     | always      |
| start    | `<number>`       | 0                      | always      |
| end      | `<number>`       | 1                      | always      |
| stops    | `<color-stop>`   | 0 #AF0, 1 #F0C         | always      |

Creates a node like `gsk_radial_gradient_node_new()` with the given properties.

### repeat

| property    | syntax           | default                | printed     |
| ----------- | ---------------- | ---------------------- | ----------- |
| bounds      | `<rect>`         | *bounds of child node* | non-default |
| child       | `<node>`         | color { }              | always      |
| child-bounds| `<rect>`         | *bounds of child node* | non-default |

Creates a node like `gsk_repeat_node_new()` with the given properties.

### repeating-linear-gradient

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| start    | `<point>`        | 0 0                    | always      |
| end      | `<point>`        | 0 50                   | always      |
| stops    | `<color-stop>`   | 0 #AF0, 1 #F0C         | always      |

Creates a node like `gsk_repeating_linear_gradient_node_new()` with the given
properties.

### repeating radial-gradient

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| center   | `<point>`        | 25 25                  | always      |
| hradius  | `<number>`       | 25                     | always      |
| vradius  | `<number>`       | 25                     | always      |
| start    | `<number>`       | 0                      | always      |
| end      | `<number>`       | 1                      | always      |
| stops    | `<color-stop>`   | 0 #AF0, 1 #F0C         | always      |

Creates a node like `gsk_repeating_radial_gradient_node_new()` with the given
properties.

### rounded-clip

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| clip     | `<rounded-rect>` | 50                     | always      |

Creates a node like `gsk_rounded_clip_node_new()` with the given properties.

### shadow

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| shadows  | `<shadow>`       | black 1 1              | always      |

Creates a node like `gsk_shadow_node_new()` with the given properties.

### stroke

| property    | syntax             | default           | printed     |
| ----------- | ------------------ | ----------------- | ----------- |
| child       | `<node>`           | *see below*       | always      |
| path        | `<string>`         | ""                | always      |
| line-width  | `<number>`         | 0                 | non-default |
| line-cap    | `<line-cap>`       | butt              | always      |
| line-join   | `<line-join>`      | miter             | always      |
| miter-limit | `<number>`         | 4                 | non-default |
| dash        | `<number>{+}|none` | none              | non-default |
| dash-offset | `<number>`         | 0                 | non-default |

Creates a node like `gsk_stroke_node_new()` with the given properties.

The default child node is the default color node, but created with the
stroke bounds of the path.

### text

| property     | syntax              | default             | printed     |
| ------------ | ------------------- | ------------------- | ----------- |
| color        | `<color>`           | black               | non-default |
| font         | `<string>` `<url>`? | "Cantarell 15px"    | always      |
| glyphs       | `<glyphs>`          | "Hello"             | always      |
| offset       | `<point>`           | 0 0                 | non-default |
| hint-style   | `<hint style>`      | slight              | non-default |
| antialias    | `<antialias>`       | gray                | non-default |

Creates a node like `gsk_text_node_new()` with the given properties.

If a url is specified for the font, it must point to a font file for the
font that is specified in the string. It can be either a data url containing
a base64-encoded font file, or a regular url that points to a font file.

Glyphs can be specified as an ASCII string, or as a comma-separated list of
their glyph IDs. Optionally, the advance width, x and y offsets and flags can
be specified as well, like this: 40 10 0 0 color.

If the given font does not exist or the given glyphs are invalid for the given
font, an error node will be returned.

Possible values for hint-style are none, slight or full.
Possible value for antialias are none or gray.

### texture

| property | syntax              | default                | printed     |
| -------- | ------------------- | ---------------------- | ----------- |
| bounds   | `<rect>`            | 50                     | always      |
| texture  | `<string>`?`<url>`? | *see below*            | always      |

Creates a node like `gsk_texture_node_new()` with the given properties.

If a string is specified for the texture, it will be used as a name for the text.
Textures can be reused by specifying the name of a previously used texture. In
that case, the url can be omitted.

The default texture is a 10x10 checkerboard with the top left and bottom right
5x5 being in the color #FF00CC and the other part being transparent. A possible
representation for this texture is `url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+9AAAABmJLR0QA/wD/AP+gvaeTAAAAKUlEQVQYlWP8z3DmPwMaYGQwYUQXY0IXwAUGUCGGoxkYGBiweXAoeAYAz44F3e3U1xUAAAAASUVORK5CYII=")
`.

### texture-scale

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| bounds   | `<rect>`         | 50                     | always      |
| texture  | `<url>`          | *see below*            | always      |
| filter   | `filter`         | *see below*            | non-default |

Creates a node like `gsk_texture_scale_node_new()` with the given properties.

The default texture is a 10x10 checkerboard, just like for texture.

The possible filter values are `linear`, `nearest` and `trilinear`, with
`linear` being the default.

### transform

| property | syntax           | default                | printed     |
| -------- | ---------------- | ---------------------- | ----------- |
| child    | `<node>`         | color { }              | always      |
| transform| `<transform>`    | none                   | non-default |

Creates a node like `gsk_transform_node_new()` with the given properties.
