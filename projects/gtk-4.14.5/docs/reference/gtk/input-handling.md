Title: Overview of GTK input and event handling
Slug: input-overview

This chapter describes in detail how GTK handles input. If you are interested
in what happens to translate a key press or mouse motion of the users into a
change of a GTK widget, you should read this chapter. This knowledge will also
be useful if you decide to implement your own widgets.

## Devices and events

The most basic input devices that every computer user has interacted with are
keyboards and mice; beyond these, GTK supports touchpads, touchscreens and
more exotic input devices such as graphics tablets. Inside GTK, every such
input device is represented by a `GdkDevice` object.

To simplify dealing with the variability between these input devices, GTK
has a concept of logical and physical devices. The concrete physical devices that
have many different characteristics (mice may have 2 or 3 or 8 buttons,
keyboards have different layouts and may or may not have a separate number
block, etc) are represented as physical devices. Each physical device is
associated with a virtual logical device. Logical devices always come in
pointer/keyboard pairs - you can think of such a pair as a 'seat'.

GTK widgets generally deal with the logical devices, and thus can be used
with any pointing device or keyboard.

When a user interacts with an input device (e.g. moves a mouse or presses
a key on the keyboard), GTK receives events from the windowing system.
These are typically directed at a specific surface - for pointer events,
the surface under the pointer (grabs complicate this), for keyboard events,
the surface with the keyboard focus.

GDK translates these raw windowing system events into `GdkEvents`.
Typical input events are button clicks, pointer motion, key presses
or touch events. These are all represented as `GdkEvents`, but you can
differentiate between different events by looking at their type, using
gdk_event_get_event_type().

Some events, such as touch events or button press-release pairs,
are connected in to each other in an “event sequence” that
univocally identifies events that are related to the same
interaction.

When GTK creates a `GdkSurface`, it connects to the ::event
signal on it, which receives all of these input events. Surfaces
have signals and properties, e.g. to deal with window management
related events.

## Event propagation

The function which initially receives input events on the GTK
side is responsible for a number of tasks.

1. Find the widget which got the event.
2. Generate crossing (i.e. enter and leave) events when the focus or
   hover location change from one widget to another.
3. Send the event to widgets.

An event is propagated down and up the widget hierarchy in three phases
towards a target widget.

![Event propagation phases](capture-bubble.png)

For key events, the top-level window gets a first shot at activating
mnemonics and accelerators. If that does not consume the events,
the target widget for event propagation is window's current focus
widget (see gtk_window_get_focus()).

For pointer events, the target widget is determined by picking
the widget at the events coordinates (see gtk_widget_pick()).

In the first phase (the “capture” phase) the event is delivered to
each widget from the top-most (the top-level `GtkWindow` or grab widget)
down to the target widget.
[Event controllers](event-controllers-and-gestures) that are attached
with %GTK_PHASE_CAPTURE get a chance to react to the event.

After the “capture” phase, the widget that was intended to be the
destination of the event will run event controllers attached to
it with %GTK_PHASE_TARGET. This is known as the “target” phase,
and only happens on that widget.

In the last phase (the “bubble” phase), the event is delivered
to each widget from the target to the top-most, and event
controllers attached with %GTK_PHASE_BUBBLE are run.

Events are not delivered to a widget which is insensitive or unmapped.

Any time during the propagation phase, a controller may indicate
that a received event was consumed and propagation should
therefore be stopped. If gestures are used, this may happen
when the gesture claims the event touch sequence (or the
pointer events) for its own. See the “gesture states” section
below to learn more about gestures and sequences.

## Keyboard input

Every `GtkWindow` maintains a single focus location (in the :focus-widget
property). The focus widget is the target widget for key events sent to
the window. Only widgets which have :focusable set to %TRUE can become
the focus. Typically these are input controls such as entries or text
fields, but e.g. buttons can take the focus too.

Input widgets can be given the focus by clicking on them, but focus
can also be moved around with certain key events (this is known as
“keyboard navigation”).
GTK reserves the <kbd>Tab</kbd> key to move the focus to the next location,
and <kbd>Shift</kbd>+<kbd>Tab</kbd> to move it back to the previous one.
In addition many containers allow “directional navigation” with the arrow keys.

Many widgets can be “activated” to trigger and action.
E.g., you can activate a button or switch by clicking on them,
but you can also activate them with the keyboard,
by using the <kbd>Enter</kbd> or <kbd>Space</kbd> keys.

Apart from keyboard navigation, activation and directly typing into
entries or text views, GTK widgets can use key events for activating
“shortcuts”. Shortcuts generally act as a quick way to move the focus
around or to activate a widget that does not currently have the focus.

GTK has traditionally supported different kinds of shortcuts:

- Accelerators are any other shortcuts that can be activated regardless
  of where the focus is, and typically trigger global actions, such as
  <kbd>Ctrl</kbd>+<kbd>Q</kbd> to quit an application.
- Mnemonics are usually triggered using <kbd>Alt</kbd>
  as a modifier for a letter.
  They are used in places where a label is associated with a control,
  and are indicated by underlining the letter in the label. As a special
  case, inside menus (i.e. inside `GtkPopoverMenu`), mnemonics can be
  triggered without the modifier.
- Key bindings are specific to individual widgets,
  such as <kbd>Ctrl</kbd>+<kbd>C</kbd> or <kbd>Ctrl</kbd>+<kbd>V</kbd>
  in an entry copy to or paste from the clipboard.
  They are only triggered when the widget has focus.

GTK handles accelerators and mnemonics in a global scope, during the
capture phase, and key bindings locally, during the target phase.

Under the hood, all shortcuts are represented as instances of `GtkShortcut`,
and they are managed by `GtkShortcutController`.

## Text input

When actual text input is needed (i.e. not just keyboard shortcuts),
input method support can be added to a widget by connecting an input
method context and listening to its `::commit` signal. To create a new
input method context, use gtk_im_multicontext_new(), to provide it with
input, use gtk_event_controller_key_set_im_context().

## Event controllers and gestures

Event controllers are standalone objects that can perform
specific actions upon received `GdkEvents`. These are tied
to a widget, and can be told of the event propagation phase
at which they will manage the events.

Gestures are a set of specific controllers that are prepared
to handle pointer and/or touch events, each gesture
implementation attempts to recognize specific actions out the
received events, notifying of the state/progress accordingly to
let the widget react to those. On multi-touch gestures, every
interacting touch sequence will be tracked independently.

Since gestures are “simple” units, it is not uncommon to tie
several together to perform higher level actions, grouped
gestures handle the same event sequences simultaneously, and
those sequences share a same state across all grouped
gestures. Some examples of grouping may be:

- A “drag” and a “swipe” gestures may want grouping.
  The former will report events as the dragging happens,
  the latter will tell the swipe X/Y velocities only after
  recognition has finished.
- Grouping a “drag” gesture with a “pan” gesture will only
  effectively allow dragging in the panning orientation, as
  both gestures share state.
- If “press” and “long press” are wanted simultaneously,
  those would need grouping.

Shortcuts are handled by `GtkShortcutController`, which is
a complex event handler that can either activate shortcuts
itself, or propagate them to another controller, depending
on its scope.

## Gesture states

Gestures have a notion of “state” for each individual touch
sequence. When events from a touch sequence are first received,
the touch sequence will have “none” state, this means the touch
sequence is being handled by the gesture to possibly trigger
actions, but the event propagation will not be stopped.

When the gesture enters recognition, or at a later point in time,
the widget may choose to claim the touch sequences (individually
or as a group), hence stopping event propagation after the event
is run through every gesture in that widget and propagation phase.
Anytime this happens, the touch sequences are cancelled downwards
the propagation chain, to let these know that no further events
will be sent.

Alternatively, or at a later point in time, the widget may choose
to deny the touch sequences, thus letting those go through again
in event propagation. When this happens in the capture phase, and
if there are no other claiming gestures in the widget,
a %GDK_TOUCH_BEGIN/%GDK_BUTTON_PRESS event will be emulated and
propagated downwards, in order to preserve consistency.

Grouped gestures always share the same state for a given touch
sequence, so setting the state on one does transfer the state to
the others. They also are mutually exclusive, within a widget
where may be only one gesture group claiming a given sequence.
If another gesture group claims later that same sequence, the
first group will deny the sequence.
