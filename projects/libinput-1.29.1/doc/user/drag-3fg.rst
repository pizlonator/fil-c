.. _drag_3fg:

==============================================================================
Three-finger drag
==============================================================================

Three-finger drag is a feature available on touchpads that emulates logical
button presses if three fingers are moving on the touchpad.

Three-finger drag is independent from :ref:`tapping` though some specific
behaviors may change when both features are enabled. For example, with
tapping *disabled* a three-finger gesture will virtually always be a three-finger
drag. With tapping *enabled* a three finger gesture may be a three finger drag
and a short delay is required to disambiguate between the two.


The exact behavior of three-finger drag is implementation defined and may
subtly change. As a general rule, the following constraints can be expected:

- three fingers down and movement trigger a button down and subsequent motion
  events (i.e. a drag)
- releasing one finger while keeping two fingers down will keep the drag
  and *not* switch to :ref:`twofinger_scrolling`.
- releasing two fingers while keeping one finger down will end the drag
  (and thus release the button) and switch to normal pointer motion
- releasing all three fingers and putting three fingers back on the touchpad
  immediately will keep the drag (i.e. behave as if the fingers were
  never lifted)

  - if tapping is enabled: a three finger tap immediately after a three-finger
    drag will *not* tap, the user needs to wait past the timeout to
    three-finger tap

- releasing all three fingers and putting one or two fingers back on
  the touchpad will end the drag (and thus release the button)
  and proceed with pointer motion or two-finger scrolling, if applicable

  - if tapping is enabled: a one or two finger tap immediately after a
    three-finger drag will trigger a one or two finger tap. The user does
    not have to wait past the drag release timeout
