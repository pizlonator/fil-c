.. _gtk4-rendernode-tool(1):

====================
gtk4-rendernode-tool
====================

-----------------------
GskRenderNode Utility
-----------------------

SYNOPSIS
--------
|   **gtk4-rendernode-tool** <COMMAND> [OPTIONS...] <FILE>
|
|   **gtk4-rendernode-tool** benchmark [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** compare [OPTIONS...] <FILE1> <FILE2>
|   **gtk4-rendernode-tool** info [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** render [OPTIONS...] <FILE> [<FILE>]
|   **gtk4-rendernode-tool** show [OPTIONS...] <FILE>

DESCRIPTION
-----------

``gtk4-rendernode-tool`` can perform various operations on serialized rendernodes.

COMMANDS
--------

Information
^^^^^^^^^^^

The ``info`` command shows general information about the rendernode, such
as the number of nodes, and the depth of the tree.

Showing
^^^^^^^

The ``show`` command displays the rendernode.

``--undecorated``

Removes window decorations. This is meant for rendering of exactly the rendernode
without any titlebar.

Rendering
^^^^^^^^^

The ``render`` command saves a rendering of the rendernode as a png or tiff image.
The name of the file to write can be specified as a second FILE argument.

``--renderer=RENDERER``

  Use the given renderer. Use ``--renderer=help`` to get a information
  about poassible values for the ``RENDERER``.

Benchmark
^^^^^^^^^

The ``benchmark`` command benchmarks rendering of a node with the existing renderers
and prints the runtimes.

``--renderer=RENDERER``

  Add the given renderer. This argument can be passed multiple times to test multiple
  renderers. By default, all major GTK renderers are run.

``--runs=RUNS``

  Number of times to render the node on each renderer. By default, this is 3 times.
  Keep in mind that the first run is often used to populate caches and might be
  significantly slower.

``--no-download``

  Do not attempt to download the result. This may cause the measurement to not include
  the execution of the commands on the GPU. It can be useful to use this flag to test
  command submission performance.

Compare
^^^^^^^

The ``compare`` command compares the rendering of a node with a reference image,
or the renderings of two nodes, or two images. If any differences are found, the
exit code is 1. If the images are identical, it is 0.

``--renderer=RENDERER``

  Use the given renderer.

``--output=FILE``

  Save the differences as a png image in ``FILE``.

``--quiet``

  Don't write results to stdout.`
