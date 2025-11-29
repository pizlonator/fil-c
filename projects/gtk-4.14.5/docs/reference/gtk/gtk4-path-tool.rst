.. _gtk4-path-tool(1):

=================
gtk4-path-tool
=================

-----------------------
GskPath Utility
-----------------------

SYNOPSIS
--------
|   **gtk4-path-tool** <COMMAND> [OPTIONS...] <PATH>
|
|   **gtk4-path-tool** decompose [OPTIONS...] <PATH>
|   **gtk4-path-tool** show [OPTIONS...] <PATH>
|   **gtk4-path-tool** render [OPTIONS...] <PATH>
|   **gtk4-path-tool** reverse [OPTIONS...] <PATH>
|   **gtk4-path-tool** info [OPTIONS...] <PATH>

DESCRIPTION
-----------

``gtk4-path-tool`` can perform various tasks on paths. Paths are specified
in SVG syntax, as strings like "M 100 100 C 100 200 200 200 200 100 Z".

To read a path from a file, use a filename that starts with a '.' or a '/'.
To read a path from stdin, use '-'.

COMMANDS
--------

Decomposing
^^^^^^^^^^^

The ``decompose`` command approximates the path by one with simpler elements.
When used without options, the curves of the path are approximated by line
segments.

``--allow-quad``

  Allow quadratic Bézier curves to be used in the generated path.

``--allow-cubic``

  Allow cubic Bézier curves to be used in the generated path.

``--allow-conic``

  Allow conic Bézier curves to be used in the generated path.

Showing
^^^^^^^

The ``show`` command displays the given path in a window. The interior
of the path is filled.

``--fill``

  Fill the path (this is the default).

``--stroke``

  Stroke the path instead of filling it.

``--points``

  Show points on the path.

``--controls``

  Show control points.

``--fill-rule=VALUE``

  The fill rule that is used to determine what areas are inside the path.
  The possible values are ``winding`` or ``even-odd``. The default is ``winding``.

``--fg-color=COLOR``

  The color that is used to fill the interior of the path or stroke the path.
  If not specified, black is used.

``--bg-color=COLOR``

  The color that is used to render the background behind the path.
  If not specified, white is used.

``--point-color=COLOR``

  The color that is used to render the points.
  If not specified, red is used.

``--line-width=VALUE``

  The line width to use for the stroke. ``VALUE`` must be a positive number.
  The default line width is 1.

``--line-cap=VALUE``

  The cap style to use at line ends. The possible values are ``butt``, ``round``
  or ``square``. See the SVG specification for details on these styles.
  The default cap style is ``butt``.

``--line-join=VALUE``

  The join style to use at line joins. The possible values are ``miter``,
  ``miter-clip``, ``round``, ``bevel`` or ``arcs``. See the SVG specification
  for details on these styles.
  The default join style is ``miter``.

``--miter-limit=VALUE``

  The limit at which to clip miters at line joins. The default value is 4.

``--dashes=VALUE``

  The dash pattern to use for this stroke. A dash pattern is specified by
  a comma-separated list of alternating non-negative numbers. Each number
  provides the length of alternate "on" and "off" portions of the stroke.
  If the dash pattern is empty, dashing is disabled, which is the default.
  See the SVG specification for details on dashing.

``--dash-offset=VALUE``

  The offset into the dash pattern where dashing should begin.
  The default value is 0.

Rendering
^^^^^^^^^

The ``render`` command renders the given path as a PNG image.
The interior of the path is filled.

``--fill``

  Fill the path (this is the default).

``--stroke``

  Stroke the path instead of filling it.

``--points``

  Show points on the path.

``--controls``

  Show control points.

``--fill-rule=VALUE``

  The fill rule that is used to determine what areas are inside the path.
  The possible values are ``winding`` or ``even-odd``. The default is ``winding``.

``--fg-color=COLOR``

  The color that is used to fill the interior of the path or stroke the path.
  If not specified, black is used.

``--bg-color=COLOR``

  The color that is used to render the background behind the path.
  If not specified, white is used.

``--point-color=COLOR``

  The color that is used to render the points.
  If not specified, red is used.

``--output-file=FILE``

  The file to save the PNG image to.
  If not specified, "path.png" is used.

``--line-width=VALUE``

  The line width to use for the stroke. ``VALUE`` must be a positive number.
  The default line width is 1.

``--line-cap=VALUE``

  The cap style to use at line ends. The possible values are ``butt``, ``round``
  or ``square``. See the SVG specification for details on these styles.
  The default cap style is ``butt``.

``--line-join=VALUE``

  The join style to use at line joins. The possible values are ``miter``,
  ``miter-clip``, ``round``, ``bevel`` or ``arcs``. See the SVG specification
  for details on these styles.
  The default join style is ``miter``.

``--miter-limit=VALUE``

  The limit at which to clip miters at line joins. The default value is 4.

``--dashes=VALUE``

  The dash pattern to use for this stroke. A dash pattern is specified by
  a comma-separated list of alternating non-negative numbers. Each number
  provides the length of alternate "on" and "off" portions of the stroke.
  If the dash pattern is empty, dashing is disabled, which is the default.
  See the SVG specification for details on dashing.

``--dash-offset=VALUE``

  The offset into the dash pattern where dashing should begin.
  The default value is 0.

Reversing
^^^^^^^^^

The ``reverse`` command changes the direction of the path. The resulting
paths starts where the original path ends.

Info
^^^^

The ``info`` command shows various information about the given path,
such as its bounding box.

REFERENCES
----------

- SVG Path Specification, https://www.w3.org/TR/SVG2/paths.html
