.. _tablet-support:

==============================================================================
Tablet support
==============================================================================

This page provides details about the graphics tablet
support in libinput. Note that the term "tablet" in libinput refers to
graphics tablets only (e.g. Wacom Intuos), not to tablet devices like the
Apple iPad.

.. figure:: tablet.svg
    :align: center

    Illustration of a graphics tablet

.. _tablet-tools:

------------------------------------------------------------------------------
Pad buttons vs. tablet tools
------------------------------------------------------------------------------

Most tablets provide two types of devices. The physical tablet often
provides a number of buttons and a touch ring or strip. Interaction on the
drawing surface of the tablet requires a tool, usually in the shape of a
stylus.  The libinput interface exposed by devices with the
**LIBINPUT_DEVICE_CAP_TABLET_TOOL** capability applies only to events generated
by tools.

Buttons, rings or strips on the physical tablet hardware (the "pad") are
exposed by devices with the **LIBINPUT_DEVICE_CAP_TABLET_PAD** capability.
Pad events do not require a tool to be in proximity. Note that both
capabilities may exist on the same device though usually they are split
across multiple kernel devices.

.. figure:: tablet-interfaces.svg
    :align: center

    Difference between Pad and Tool buttons

Touch events on the tablet integrated into a screen itself are exposed
through the **LIBINPUT_DEVICE_CAP_TOUCH** capability. Touch events on a
standalone tablet are exposed through the **LIBINPUT_DEVICE_CAP_POINTER**
capability.  In both cases, the kernel usually provides a separate event
node for the touch device, resulting in a separate libinput device.
See **libinput_device_get_device_group()** for information on how to associate
the touch part with other devices exposed by the same physical hardware.

.. _tablet-tip:

------------------------------------------------------------------------------
Tool tip events vs. tool button events
------------------------------------------------------------------------------

The primary use of a tablet tool is to draw on the surface of the tablet.
When the tool tip comes into contact with the surface, libinput sends an
event of type **LIBINPUT_EVENT_TABLET_TOOL_TIP**, and again when the tip
ceases contact with the surface.

Tablet tools may send button events; these are exclusively for extra buttons
unrelated to the tip. A button event is independent of the tip and can while
the tip is down or up.

Some tablet tools' pressure detection is too sensitive, causing phantom
touches when the user only slightly brushes the surfaces. For example, some
tools are capable of detecting 1 gram of pressure.

libinput uses a device-specific pressure threshold to determine when the tip
is considered logically down. As a result, libinput may send a nonzero
pressure value while the tip is logically up. Most application can and
should ignore pressure information until they receive the event of type
**LIBINPUT_EVENT_TABLET_TOOL_TIP**. Applications that require extremely
fine-grained pressure sensitivity should use the pressure data instead of
the tip events to determine a logical tip down state and treat the tip
events like axis events otherwise.

Note that the pressure threshold to trigger a logical tip event may be zero
on some devices. On tools without pressure sensitivity, determining when a
tip is down is device-specific.

.. _tablet-relative-motion:

------------------------------------------------------------------------------
Relative motion for tablet tools
------------------------------------------------------------------------------

libinput calculates the relative motion vector for each event and converts
it to the same coordinate space that a normal mouse device would use. For
the caller, this means that the delta coordinates returned by
**libinput_event_tablet_tool_get_dx()** and
**libinput_event_tablet_tool_get_dy()** can be used identical to the delta
coordinates from any other pointer event. Any resolution differences between
the x and y axes are accommodated for, a delta of N/N represents a 45 degree
diagonal move on the tablet.

The delta coordinates are available for all tablet events, it is up to the
caller to decide when a tool should be used in relative mode. It is
recommended that mouse and lens cursor tool default to relative mode and
all pen-like tools to absolute mode.

If a tool in relative mode must not use pointer acceleration, callers
should use the absolute coordinates returned by
**libinput_event_tablet_tool_get_x()** and libinput_event_tablet_tool_get_y()
and calculate the delta themselves. Callers that require exact physical
distance should also use these functions to calculate delta movements.

.. _tablet-axes:

------------------------------------------------------------------------------
Special axes on tablet tools
------------------------------------------------------------------------------

A tablet tool usually provides additional information beyond x/y positional
information and the tip state. A tool may provide the distance to the tablet
surface and the pressure exerted on the tip when in contact. Some tablets
additionally provide tilt information along the x and y axis.

.. figure:: tablet-axes.svg
    :align: center

    Illustration of the distance, pressure and tilt axes

The granularity and precision of the distance and pressure axes varies
between tablet devices and cannot usually be mapped into a physical unit.
libinput normalizes distance and pressure into the [0, 1] range.

While the normalization range is identical for these axes, a caller should
not interpret identical values as identical across axes, i.e. a value v1 on
the distance axis has no relation to the same value v1 on the pressure axis.

The tilt axes provide the angle in degrees between a vertical line out of
the tablet and the top of the stylus. The angle is measured along the x and
y axis, respectively, a positive tilt angle thus means that the stylus' top
is tilted towards the logical right and/or bottom of the tablet.

.. _tablet-fake-proximity:

------------------------------------------------------------------------------
Handling of proximity events
------------------------------------------------------------------------------

libinput's **LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY** events notify a caller
when a tool comes into sensor range or leaves the sensor range. On some
tools this range does not represent the physical range but a reduced
tool-specific logical range. If the range is reduced, this is done
transparent to the caller.

For example, the Wacom mouse and lens cursor tools are usually
used in relative mode, lying flat on the tablet. Movement typically follows
the interaction normal mouse movements have, i.e. slightly lift the tool and
place it in a separate location. The proximity detection on Wacom
tablets however extends further than the user may lift the mouse, i.e. the
tool may not be lifted out of physical proximity. For such tools, libinput
provides software-emulated proximity.

Events from the pad do not require proximity, they may be sent any time.

.. _tablet-pressure-offset:

------------------------------------------------------------------------------
Pressure offset on worn-out tools
------------------------------------------------------------------------------

When a tool is used for an extended period it can wear down physically. A
worn-down tool may never return a zero pressure value. Even when hovering
above the surface, the pressure value returned by the tool is nonzero,
creating a fake surface touch and making interaction with the tablet less
predictable.

libinput automatically detects pressure offsets and rescales the remaining
pressure range into the available range, making pressure-offsets transparent
to the caller. A tool with a pressure offset will thus send a 0 pressure
value for the detected offset and nonzero pressure values for values higher
than that offset.

Some limitations apply to avoid misdetection of pressure offsets,
specifically:

- pressure offset is only detected on proximity in, and if a device is
  capable of detection distances,
- pressure offset is only detected if the distance between the tool and the
  tablet is high enough,
- pressure offset is only used if it is 50% or less of the pressure range
  available to the tool. A pressure offset higher than 50% indicates either
  a misdetection or a tool that should be replaced, and
- if a pressure value less than the current pressure offset is seen, the
  offset resets to that value.

Pressure offsets are not detected on **LIBINPUT_TABLET_TOOL_TYPE_MOUSE**
and **LIBINPUT_TABLET_TOOL_TYPE_LENS** tools.


.. _tablet-pressure-range:

------------------------------------------------------------------------------
Custom tablet tool pressure ranges
------------------------------------------------------------------------------

On tablets supporting pressure, libinput provides that hardware pressure as
a logical range of ``0.0`` up to ``1.0`` for the maximum supported pressure.
By default, the hardware range thus maps into the following logical range::


           hw minimum                                  hw maximum
  hw range:      |------|-----------------------------------|
  logical range:   |----|-----------------------------------|
                  0.0   |                                  1.0
                       Tip

Note that libinput always has some built-in thresholds to filter out erroneous
touches with near-zero pressure but otherwise the hardware range maps as-is
into the logical range. The :ref:`tip event <tablet-tip>` threshold is defined
within this range.

For some use-cases the full hardware range is not suitable, it may require either
too light a pressure for the user to interact or it may require too hard a
pressure before the logical maximum is reached. libinput provides
the **libinput_tablet_tool_config_pressure_range_set()** function that allows
reducing the usable range of the tablet::

           hw minimum                                  hw maximum
  hw range:      |----------|-------------------------------|
  adjusted range:    |------|---------------------|
  logical range:       |----|---------------------|
                      0.0   |                    1.0
                           Tip

A reduced range as shown above will result in

- all hw pressure below the new minimum to register as logical pressure ``0.0``
- all hw pressure above the new maximum to register as logical pressure ``1.0``
- the tip event threshold to be relative to the new minimum

In other words, adjusting the pressure range of a tablet tool is equivalent to
reducing the hardware range of said tool. Note that where a custom pressure
range is set, detection of :ref:`tablet-pressure-offset` is disabled.

.. _tablet-serial-numbers:

------------------------------------------------------------------------------
Tracking unique tools
------------------------------------------------------------------------------

Some tools provide hardware information that enables libinput to uniquely
identify the physical device. For example, tools compatible with the Wacom
Intuos 4, Intuos 5, Intuos Pro and Cintiq series are uniquely identifiable
through a serial number. libinput does not specify how a tool can be
identified uniquely, a caller should use **libinput_tablet_tool_is_unique()** to
check if the tool is unique.

libinput creates a struct libinput_tablet_tool on the first proximity in of
this tool. By default, this struct is destroyed on proximity out and
re-initialized on the next proximity in. If a caller keeps a reference to
the tool by using **libinput_tablet_tool_ref()** libinput re-uses this struct
whenever that same physical tool comes into proximity on any tablet
recognized by libinput. It is possible to attach tool-specific virtual state
to the tool. For example, a graphics program such as the GIMP may assign a
specific color to each tool, allowing the artist to use the tools like
physical pens of different color. In multi-tablet setups it is also
possible to track the tool across devices.

If the tool does not have a unique identifier, libinput creates a single
struct libinput_tablet_tool per tool type on each tablet the tool is used
on.

.. _tablet-tool-types:

------------------------------------------------------------------------------
Vendor-specific tablet tool types
------------------------------------------------------------------------------

libinput supports a number of high-level tool types that describe the
general interaction expected with the tool. For example, a user would expect
a tool of type **LIBINPUT_TABLET_TOOL_TYPE_PEN** to interact with a
graphics application taking pressure and tilt into account. The default
virtual tool assigned should be a drawing tool, e.g. a virtual pen or brush.
A tool of type **LIBINPUT_TABLET_TOOL_TYPE_ERASER** would normally be
mapped to an eraser-like virtual tool. See **libinput_tablet_tool_type**
for the list of all available tools.

Vendors may provide more fine-grained information about the tool in use by
adding a hardware-specific tool ID. libinput provides this ID to the caller
with **libinput_tablet_tool_get_tool_id()** but makes no promises about the
content or format of the ID.

libinput currently supports Wacom-style tool IDs as provided on the Wacom
Intuos 3, 4, 5, Wacon Cintiq and Wacom Intuos Pro series. The tool ID can
be used to distinguish between e.g. a Wacom Classic Pen or a Wacom Pro Pen.
It is  the caller's responsibility to interpret the tool ID.

.. _tablet-bounds:

------------------------------------------------------------------------------
Out-of-bounds motion events
------------------------------------------------------------------------------

Some tablets integrated into a screen (e.g. Wacom Cintiq 24HD, 27QHD and
13HD series, etc.) have a sensor larger than the display area. libinput uses
the range advertised by the kernel as the valid range unless device-specific
quirks are present. Events outside this range will produce coordinates that
may be negative or larger than the tablet's width and/or height. It is up to
the caller to ignore these events.

.. figure:: tablet-out-of-bounds.svg
    :align: center

    Illustration of the out-of-bounds area on a tablet

In the image above, the display area is shown in black. The red area around
the display illustrates the sensor area that generates input events. Events
within this area will have negative coordinate or coordinates larger than
the width/height of the tablet.

If events outside the logical bounds of the input area are scaled into a
custom range with **libinput_event_tablet_tool_get_x_transformed()** and
**libinput_event_tablet_tool_get_y_transformed()** the resulting value may be
less than 0 or larger than the upper range provided. It is up to the caller
to test for this and handle or ignore these events accordingly.

.. _tablet-pad-buttons:

------------------------------------------------------------------------------
Tablet pad button numbers
------------------------------------------------------------------------------

Tablet Pad buttons are numbered sequentially, starting with button 0. Thus
button numbers returned by **libinput_event_tablet_pad_get_button_number()**
have no semantic meaning, a notable difference to the button codes returned
by other libinput interfaces (e.g. **libinput_event_tablet_tool_get_button()**).

The Linux kernel requires all input events to have semantic event codes,
but generic buttons like those on a pad cannot easily be assigned semantic
codes. The kernel supports generic codes in the form of BTN_0 through to
BTN_9 and additional unnamed space up until code 0x10f. Additional generic
buttons are available as BTN_A in the range dedicated for gamepads and
joysticks. Thus, tablet with a large number of buttons have to map across
two semantic ranges, have to use unnamed kernel button codes or risk leaking
into an unrelated range. libinput transparently maps the kernel event codes
into a sequential button range on the pad. Callers should use external
sources like libwacom to associate button numbers to their position on the
tablet.

Some buttons may have expected default behaviors. For example, on Wacom
Intuos Pro series tablets, the button inside the touch ring is expected to
switch between modes, see :ref:`tablet-pad-modes`. Callers should use
external sources like libwacom to identify which buttons have semantic
behaviors.

.. _tablet-left-handed:

------------------------------------------------------------------------------
Tablets in left-handed mode
------------------------------------------------------------------------------

Left-handed mode on tablet devices usually means rotating the physical
tablet by 180 degrees to move the tablet pad button area to right side of
the tablet.  When left-handed mode is enabled on a tablet device (see
**libinput_device_config_left_handed_set()**) the tablet tool and tablet pad
behavior changes. In left-handed mode, the tools' axes are adjusted
so that the origin of each axis remains the logical north-east of
the physical tablet. For example, the x and y axes are inverted and the
positive x/y coordinates are down/right of the top-left corner of the tablet
in its current orientation. On a tablet pad, the ring and strip are
similarly adjusted. The origin of the ring and strips remain the top-most
point.

.. figure:: tablet-left-handed.svg
    :align: center

    Tablet axes in right- and left-handed mode

Pad buttons are not affected by left-handed mode; the number of each button
remains the same even when the perceived physical location of the button
changes. This is a conscious design decision:

- Tablet pad buttons do not have intrinsic semantic meanings. Re-ordering
  the button numbers would not change any functionality.
- Button numbers should not be exposed directly to the user but handled in
  the intermediate layers. Re-ordering button numbers thus has no
  user-visible effect.
- Re-ordering button numbers may complicate the intermediate layers.

Left-handed mode is only available on some tablets, some tablets are
symmetric and thus do not support left-handed mode. libinput requires
libwacom to determine if a tablet is capable of being switched to
left-handed mode.

.. _tablet-pad-modes:

------------------------------------------------------------------------------
Tablet pad modes
------------------------------------------------------------------------------

Tablet pad modes are virtual groupings of button, ring and strip
functionality. A caller may assign different functionalities depending on
the mode the tablet is in. For example, in mode 0 the touch ring may emulate
scrolling, in mode 1 the touch ring may emulate zooming, etc. libinput
handles the modes and mode switching but does not assign specific
functionality to buttons, rings or strips based on the mode. It is up to the
caller to decide whether the mode only applies to buttons, rings and strips
or only to rings and strips (this is the case with the Wacom OS X and
Windows driver).

The availability of modes on a touchpad usually depends on visual feedback
such as LEDs around the touch ring. If no visual feedback is available, only
one mode may be available.

Mode switching is controlled by libinput and usually toggled by one or
more buttons on the device. For example, on the Wacom Intuos 4, 5, and
Pro series tablets the mode button is the button centered in the touch
ring and toggles the modes sequentially. On the Wacom Cintiq 24HD the
three buttons next to each touch ring allow for directly changing the
mode to the desired setting.

Multiple modes may exist on the tablet, libinput uses the term "mode group"
for such groupings of buttons that share a mode and mode toggle. For
example, the Wacom Cintiq 24HD has two separate mode groups, one for the
left set of buttons, strips, and touch rings and one for the right set.
libinput handles the mode groups independently and returns the mode for each
button as appropriate. The mode group is static for the lifetime of the
device.

.. figure:: tablet-intuos-modes.svg
    :align: center

    Modes on an Intuos Pro-like tablet

In the image above, the Intuos Pro-like tablet provides 4 LEDs to indicate
the currently active modes. The button inside the touch ring cycles through
the modes in a clockwise fashion. The upper-right LED indicates that the
currently active mode is 1, based on 0-indexed mode numbering.
**libinput_event_tablet_pad_get_mode()** would thus return 1 for all button and
ring events on this tablet. When the center button is pressed, the mode
switches to mode 2, the LED changes to the bottom-right and
**libinput_event_tablet_pad_get_mode()** returns 2 for the center button event
and all subsequent events.

.. figure:: tablet-cintiq24hd-modes.svg
    :align: center

    Modes on an Cintiq 24HD-like tablet

In the image above, the Cintiq 24HD-like tablet provides 3 LEDs on each side
of the tablet to indicate the currently active mode for that group of
buttons and the respective ring. The buttons next to the touch ring select
the mode directly. The two LEDs indicate that the mode for the left set of
buttons is currently 0, the mode for the right set of buttons is currently
1, based on 0-indexed mode numbering. **libinput_event_tablet_pad_get_mode()**
would thus return 0 for all button and ring events on the left and 1 for all
button and ring events on the right. When one of the three mode toggle
buttons on the right is pressed, the right mode switches to that button's
mode but the left mode remains unchanged.

.. _tablet-touch-arbitration:

------------------------------------------------------------------------------
Tablet touch arbitration
------------------------------------------------------------------------------

"Touch arbitration" is the terminology used when touch events are suppressed
while the pen is in proximity. Since it is almost impossible to use a stylus
or other tool without triggering touches with the hand holding the tool,
touch arbitration serves to reduce the number of accidental inputs.
The wacom kernel driver currently provides touch arbitration but for other
devices arbitration has to be done in userspace.

libinput uses the **libinput_device_group** to decide on touch arbitration
and automatically discards touch events whenever a tool is in proximity.
The exact behavior is device-dependent.

.. _tablet-area:

------------------------------------------------------------------------------
Tablet area
------------------------------------------------------------------------------

External tablet devices such as e.g. the Wacom Intuos series can be configured
to reduce the available logical input area. Typically the logical input area
is equivalent to the physical input area but it can be reduced with the
**libinput_device_config_area_set_rectangle()** call. Once reduced, input
events outside the logical input area are ignored and the logical input area
acts as if it represented the extents of the physical tablet.

.. figure:: tablet-area.svg
   :align: center

   Tablet area configuration example

In the image above, the area is set to the rectangle 0.25/0.25 to 0.5/0.75.
Even though the tool is roughly at the physical position ``0.5 * width`` and
``0.75 * height``, the return values of
**libinput_event_tablet_tool_get_x_transformed()** and
**libinput_event_tablet_tool_get_y_transformed()** would be close to the
maximum provided in this call.

The size of the tablet reported by **libinput_device_get_size()** always reflects
the physical area, not the logical area.

.. _tablet-eraser-button:

------------------------------------------------------------------------------
Tablet eraser buttons
------------------------------------------------------------------------------

Tablet tools come in a variety of forms but the most common one is a
pen-like tool. Some of these pen-like tools have a virtual eraser at the
tip of the tool - inverting the tool brings the eraser into proximity.

.. figure:: tablet-eraser-invert.svg
    :align: center

    An pen-like tool used as pen and as eraser by inverting it

Having an eraser as a separate tool is beneficial in many applications as the
eraser tool can be assigned different functionality (colors, paint tools, etc.)
that is easily available.

However, a large proportion of tablet pens have an "eraser button". By
pressing the button the pen switches to be an eraser tool.
On the data level this is not done via a button event, instead the firmware
will pretend the pen tool going out of proximity and the eraser coming
into proximity immediately after - as if the tool was physically inverted.

.. figure:: tablet-eraser-button.svg
    :align: center

    An pen-like tool used as pen and as eraser by pressing the eraser button

Microsoft mandates this behavior (see
`Windows Pen States <https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/windows-pen-states>`_
for details) and thus the overwhelming majority of devices will have
an eraser button that virtually inverts the pen.

Enforcing an eraser button means that users have one button less on the
stylus that they would have otherwise. For some users the eraser button
is in an inconvenient location, others don't want an eraser button at all.

libinput provides an eraser button configuration that allows disabling the
eraser button and turning it into a normal button event. If the eraser button
is disabled, pressing that button will generate a normal tablet tool button
event.

This configuration is only available on pens with an eraser button, not on
with an invert-type eraser.
