.. _architecture:

==============================================================================
libinput's internal architecture
==============================================================================

This page provides an outline of libinput's internal architecture. The goal
here is to get the high-level picture across and point out the components
and their interplay to new developers.

The public facing API is in ``libinput.c``, this file is thus the entry point
for almost all API calls. General device handling is in ``evdev.c`` with the
device-type-specific implementations in ``evdev-<type>.c``. It is not
necessary to understand all of libinput to contribute a patch.

As of libinput 1.29 libinput has an internal plugin pipeline that modifies
the event stream before libinput proper sees it, see
:ref:`architecture-plugins`.

:ref:`architecture-contexts` is the only user-visible implementation detail,
everything else is purely internal implementation and may change when
required.

.. _architecture-contexts:

------------------------------------------------------------------------------
The udev and path contexts
------------------------------------------------------------------------------

The first building block is the "context" which can be one of
two types, "path" and "udev". See **libinput_path_create_context()** and
**libinput_udev_create_context()**. The path/udev specific bits are in
``path-seat.c`` and ``udev-seat.c``. This includes the functions that add new
devices to a context.


.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      libudev [label="libudev 'add' event"]
      udev [label="**libinput_udev_create_context()**"];
      udev_backend [label="udev-specific backend"];
      context [label="libinput context"]
      udev -> udev_backend;
      libudev -> udev_backend;
      udev_backend -> context;
    }


The udev context provides automatic device hotplugging as udev's "add"
events are handled directly by libinput. The path context requires that the
caller adds devices.


.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      path [label="**libinput_path_create_context()**"];
      path_backend [label="path-specific backend"];
      xdriver [label="**libinput_path_add_device()**"]
      context [label="libinput context"]
      path -> path_backend;
      xdriver -> path_backend;
      path_backend -> context;
    }


As a general rule: all Wayland compositors use a udev context, the X.org
stack uses a path context.

Which context was initialized only matters for creating/destroying a context
and adding devices. The device handling itself is the same for both types of
context.

.. _architecture-device:

------------------------------------------------------------------------------
Device initialization
------------------------------------------------------------------------------

libinput only supports evdev devices, all the device initialization is done
in ``evdev.c``. Much of the libinput public API is also a thin wrapper around
the matching implementation in the evdev device.

There is a 1:1 mapping between libinput devices and ``/dev/input/eventX``
device nodes.



.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      devnode [label="/dev/input/event0"]

      libudev [label="libudev 'add' event"]
      xdriver [label="**libinput_path_add_device()**"]
      context [label="libinput context"]

      evdev [label="evdev_device_create()"]

      devnode -> xdriver;
      devnode -> libudev;
      xdriver -> context;
      libudev -> context;

      context->evdev;

    }


Entry point for all devices is ``evdev_device_create()``, this function
decides to create a ``struct evdev_device`` for the given device node.
Based on the udev tags (e.g. ``ID_INPUT_TOUCHPAD``), a
:ref:`architecture-dispatch` is initialized. All event handling is then in this
dispatch.

Rejection of devices and the application of quirks is generally handled in
``evdev.c`` as well. Common functionality shared across multiple device types
(like button-scrolling) is also handled here.

.. _architecture-dispatch:

------------------------------------------------------------------------------
Device-type specific event dispatch
------------------------------------------------------------------------------

Depending on the device type, ``evdev_configure_device`` creates the matching
``struct evdev_dispatch``. This dispatch interface contains the function
pointers to handle events. Four such dispatch methods are currently
implemented: touchpad, tablet, tablet pad, and the fallback dispatch which
handles mice, keyboards and touchscreens.

.. graphviz::

    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      evdev [label="evdev_device_create()"]

      fallback [label="evdev-fallback.c"]
      touchpad [label="evdev-mt-touchpad.c"]
      tablet [label="evdev-tablet.c"]
      pad [label="evdev-tablet-pad.c"]

      evdev -> fallback;
      evdev -> touchpad;
      evdev -> tablet;
      evdev -> pad;

    }


Event dispatch is done per "evdev frame", a collection of events up until including
the ``SYN_REPORT``. One such ``struct evdev_frame`` represents all state **updates**
to the previous frame.

While ``evdev.c`` pulls the event out of libevdev, the actual handling of the
events is performed within the dispatch method.

.. graphviz::

    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      evdev [label="evdev_device_dispatch()"]

      plugins [label="plugin pipline"]

      fallback [label="fallback_interface_process()"];
      touchpad [label="tp_interface_process()"]
      tablet [label="tablet_process()"]
      pad [label="pad_process()"]

      evdev -> plugins;
      plugins -> fallback;
      plugins -> touchpad;
      plugins -> tablet;
      plugins -> pad;
    }


The dispatch methods then look at the ``struct evdev_frame`` and proceed to
update the state.

.. _architecture-plugins:

------------------------------------------------------------------------------
The Plugin Pipeline
------------------------------------------------------------------------------

As of libinput 1.29 libinput has an **internal** plugin pipeline. These plugins
logically sit between libevdev and the :ref:`architecture-dispatch` and modify
the device and/or event stream. The primary motivation of such plugins is that
modifying the event stream is often simpler than analyzing the state later.

Plugins are loaded on libinput context startup and are executed in-order. The last
plugin is the hardcoded `evdev-plugin.c` which takes the modified event stream and
passes the events to the dispatch.

.. graphviz::

    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      evdev [label="evdev_device_dispatch()"]

      p1 [label="P1"]
      p2 [label="P2"]
      p3 [label="P3"]
      ep [label="evdev-plugin"]

      fallback [label="fallback_interface_process()"];
      touchpad [label="tp_interface_process()"]
      tablet [label="tablet_process()"]
      pad [label="pad_process()"]

      evdev -> p1;
      p1 -> p2;
      p2 -> p3;
      p3 -> ep;
      ep -> fallback;
      ep -> touchpad;
      ep -> tablet;
      ep -> pad;
    }

Each plugin may not only modify the current event frame (this includes adding/removing events
from the frame), it may also append or prepend additional event frames. For
example the tablet proximity-timer plugin adds proximity in/out events to the
event stream.

.. graphviz::

    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]
      n0 [label= "", shape=none,height=.0,width=.0]
      n1 [label= "", shape=none,height=.0,width=.0]

      p1 [label="P1"]
      p2 [label="P2"]
      p3 [label="P3"]
      ep [label="evdev-plugin"]

      n0 -> p1 [label="F1"];
      p1 -> p2 [label="F1"];
      p2 -> p3 [label="F1,F2"];
      p3 -> ep [label="F3,F1,F2"];
      ep -> n1 [label="F3,F1,F2"];
    }

In the diagram above, the plugin ``P2`` *appends* a new frame (``F2``), the plugin ``P3``
*prepends* a new frame (``F3``). The original event frame ``F1`` thus becomes the event frame
sequence ``F3``, ``F1``, ``F2`` by the time it reaches the :ref:`architecture-dispatch`.

Note that each plugin only sees one event frame at a time, so ``P3`` would see ``F1`` first,
decides to prepend ``F3`` and passes ``F1`` through. It then sees ``F2`` but does nothing with
it (optionally modified in-place).

.. _architecture-configuration:

------------------------------------------------------------------------------
Device configuration
------------------------------------------------------------------------------

All device-specific configuration is handled through ``struct
libinput_device_config_FOO`` instances. These are set up during device init
and provide the function pointers for the ``get``, ``set``, ``get_default``
triplet of configuration queries (or more, where applicable).

For example, the ``struct tablet_dispatch`` for tablet devices has a
``struct libinput_device_config_accel``. This struct is set up with the
required function pointers to change the profiles.


.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      tablet [label="struct tablet_dispatch"]
      config [label="struct libinput_device_config_accel"];
      tablet_config [label="tablet_accel_config_set_profile()"];
      tablet->config;
      config->tablet_config;
    }


When the matching ``**libinput_device_config_set_FOO()**`` is called, this goes
through to the config struct and invokes the function there. Thus, it is
possible to have different configuration functions for a mouse vs a
touchpad, even though the interface is the same.


.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      libinput [label="**libinput_device_config_accel_set_profile()**"];
      tablet_config [label="tablet_accel_config_set_profile()"];
      libinput->tablet_config;
    }


.. _architecture-filter:

------------------------------------------------------------------------------
Pointer acceleration filters
------------------------------------------------------------------------------

All pointer acceleration is handled in the ``filter.c`` file and its
associated files.

The ``struct motion_filter`` is initialized during device init, whenever
deltas are available they are passed to ``filter_dispatch()``. This function
returns a set of :ref:`normalized coordinates <motion_normalization_customization>`.

All actual acceleration is handled within the filter, the device itself has
no further knowledge. Thus it is possible to have different acceleration
filters for the same device types (e.g. the Lenovo X230 touchpad has a
custom filter).


.. graphviz::


    digraph context
    {
      compound=true;
      rankdir="LR";
      node [
        shape="box";
      ]

      fallback [label="fallback deltas"];
      touchpad [label="touchpad deltas"];
      tablet [label="tablet deltas"];

      filter [label="filter_dispatch"];

      fallback->filter;
      touchpad->filter;
      tablet->filter;

      flat [label="accelerator_interface_flat()"];
      x230 [label="accelerator_filter_x230()"];
      pen [label="tablet_accelerator_filter_flat_pen()"];

      filter->flat;
      filter->x230;
      filter->pen;

    }


Most filters convert the deltas (incl. timestamps) to a motion speed and
then apply a so-called profile function. This function returns a factor that
is then applied to the current delta, converting it into an accelerated
delta. See :ref:`pointer-acceleration` for more details.
the current
