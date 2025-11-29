Title: Overview of actions in GTK
Slug: actions

This chapter describes in detail how GTK uses actions to connect
activatable UI elements to callbacks. GTK inherits the underlying
architecture of `GAction` and `GMenu` for describing abstract actions
and menus from the GIO library.

## Basics about actions

A `GAction` is essentially a way to tell the toolkit about a piece of
functionality in your program, and to give it a name.

Actions are purely functional. They do not contain any presentational
information.

An action has four pieces of information associated with it:

- a name as an identifier (usually all-lowercase, untranslated
  English string)
- an enabled flag indicating if the action can be activated or not
  (like the "sensitive" property on widgets)
- an optional state value, for stateful actions (like a boolean for
  toggles)
- an optional parameter type, used when activating the action

An action supports two operations. You can activate it, which requires
passing a parameter of the correct type And you can request to change
the actions state (for stateful actions) to a new state value of the
correct type.

Here are some rules about an action:

- the name is immutable (in the sense that it will never change) and
  it is never %NULL
- the enabled flag can change
- the parameter type is immutable
- the parameter type is optional: it can be %NULL
- if the parameter type is %NULL then action activation must be done
  without a parameter (ie: a %NULL GVariant pointer)
- if the parameter type is non-%NULL then the parameter must have this
  type
- the state can change, but it cannot change type
- if the action was stateful when it was created, it will always have a
  state and it will always have exactly the same type (such as boolean
  or string)
- if the action was stateless when it was created, it can never have a
  state
- you can only request state changes on stateful actions and it is only
  possible to request that the state change to a value of the same type
  as the existing state

An action does not have any sort of presentational information such as
a label, an icon or a way of creating a widget from it.

## Action state and parameters

Most actions in your application will be stateless actions with no
parameters. These typically appear as menu items with no special
decoration. An example is "quit".

Stateful actions are used to represent an action which has a
closely-associated state of some kind. A good example is a "fullscreen"
action. For this case, you would expect to see a checkmark next to the
menu item when the fullscreen option is active. This is usually called
a toggle action, and it has a boolean state. By convention, toggle actions
have no parameter type for activation: activating the action always toggles
the state.

Another common case is to have an action representing a enumeration of
possible values of a given type (typically string). This is often called
a radio action and is usually represented in the user interface with radio
buttons or radio menu items, or sometimes a combobox. A good example is
"text-justify" with possible values "left", "center", and "right". By
convention, these types of actions have a parameter type equal to their
state type, and activating them with a particular parameter value is
equivalent to changing their state to that value.

This approach to handling radio buttons is different than many other
action systems such as `GtkAction`. With `GAction`, there is only one action
for "text-justify" and "left", "center" and "right" are possible states on
that action. There are not three separate "justify-left", "justify-center"
and "justify-right" actions.

The final common type of action is a stateless action with a parameter.
This is typically used for actions like "open-bookmark" where the parameter
to the action would be the identifier of the bookmark to open.

Because some types of actions cannot be invoked without a parameter, it is
often important to specify a parameter when referring to the action from
a place where it will be invoked (such as from a radio button that sets
the state to a particular value or from a menu item that opens a specific
bookmark). In these contexts, the value used for the action parameter is
typically called the target of the action.

Even though toggle actions have a state, they do not have a parameter.
Therefore, a target value is not needed when referring to them — they
will always be toggled on activation.

Most APIs that allow using a `GAction` (such as `GMenuModel` and `GtkActionable`)
allow use of detailed action names. This is a convenient way of specifying
an action name and an action target with a single string.

In the case that the action target is a string with no unusual characters
(ie: only alphanumeric, plus '-' and '.') then you can use a detailed
action name of the form "justify::left" to specify the justify action with
a target of left.

In the case that the action target is not a string, or contains unusual
characters, you can use the more general format "action-name(5)", where the
"5" here is any valid text-format GVariant (ie: a string that can be parsed
by g_variant_parse()). Another example is "open-bookmark('http://gnome.org/')".

You can convert between detailed action names and split-out action names
and target values using g_action_parse_detailed_name() and
g_action_print_detailed_name() but usually you will not need to. Most APIs
will provide both ways of specifying actions with targets.

## Action scopes

Actions are always scoped to a particular object on which they operate.

In GTK, actions are typically scoped to either an application or a window,
but any widget can have actions associated with it.

Actions scoped to windows should be the actions that specifically impact
that window. These are actions like "fullscreen" and "close", or in the
case that a window contains a document, "save" and "print".

Actions that impact the application as a whole rather than one specific
window are scoped to the application. These are actions like "about" and
"preferences".

If a particular action is scoped to a window then it is scoped to a
specific window. Another way of saying this: if your application has a
"fullscreen" action that applies to windows and it has three windows,
then it will have three fullscreen actions: one for each window.

Having a separate action per-window allows for each window to have a
separate state for each instance of the action as well as being able to
control the enabled state of the action on a per-window basis.

Actions are added to their relevant scope (application, window or widget)
either using the `GActionMap` interface, or by using
gtk_widget_insert_action_group(). Actions that will be the same for all
instances of a widget class can be added globally using
gtk_widget_class_install_action().

## Action groups and action maps

Actions rarely occurs in isolation. It is common to have groups
of related actions, which are represented by instances of the
`GActionGroup` interface.

Action maps are a variant of action groups that allow to change
the name of the action as it is looked up. In GTK, the convention
is to add a prefix to the action name to indicate the scope of
the actions, such as "app." for the actions with application scope
or "win." for those with window scope.

When referring to actions on a GActionMap only the name of the
action itself is used (ie: "quit", not "app.quit"). The
"app.quit" form is only used when referring to actions from
places like a `GMenu` or `GtkActionable` widget where the scope
of the action is not already known.

`GtkApplication` and `GtkApplicationWindow` implement the `GActionMap`
interface, so you can just add actions directly to them. For
other widgets, use gtk_widget_insert_action_group() to add
actions to it.

If you want to insert several actions at the same time, it is
typically faster and easier to use `GActionEntry`.

## Connecting actions to widgets

Any widget that implements the `GtkActionable` interface can
be connected to an action just by setting the ::action-name
property. If the action has a parameter, you will also need
to set the ::action-target property. Widgets that implement
`GtkActionable` include `GtkSwitch`, `GtkButton`, and their
respective subclasses.

Another way of obtaining widgets that are connected to actions
is to create a menu using a `GMenu` menu model. `GMenu` provides an
abstract way to describe typical menus: nested groups of items
where each item can have a label, and icon, and an action.

A typical use of `GMenu` inside GTK is to set up an application
menubar with gtk_application_set_menubar(). Another, maybe more
common use is to create a popover for a menubutton, using
gtk_menu_button_set_menu_model().

Unlike traditional menus, those created from menu models don't
have keyboard accelerators associated with menu items. Instead,
`GtkApplication` offers the gtk_application_set_accels_for_action()
API to associate keyboard shortcuts with actions.

## Activation

When a widget with a connected action is activated, GTK finds
the action to activate by walking up the widget hierarchy,
looking for a matching action, ending up at the `GtkApplication`.

## Built-in Actions

GTK uses actions for its own purposes in a number places. These
built-in actions can sometimes be activated by applications, and
you should avoid naming conflicts with them when creating your
own actions.

default.activate
: Activates the default widget in a context (typically a `GtkWindow`,
  `GtkDialog` or `GtkPopover`)

clipboard.cut, clipboard.copy, clipboard.paste
: Clipboard operations on entries, text view and labels, typically
  used in the context menu

selection.delete, selection.select-all
: Selection operations on entries, text view and labels

color.select, color.customize
: Operate on colors in a `GtkColorChooserWidget`. These actions are
  unusual in that they have the non-trivial parameter type (dddd).
