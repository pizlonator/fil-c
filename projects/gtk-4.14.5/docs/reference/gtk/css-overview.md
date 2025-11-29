Title: CSS in GTK
Slug: css

This chapter describes how GTK uses CSS for styling and layout.
It is not meant to be an explanation of CSS from first principles,
but focuses on listing supported CSS features and differences
between Web CSS and GTK.

There is plenty of introductory documentation available that
can be used to learn about CSS in general. In the tables below
we include links to the official specs that can be used to look
up the definition of individual selectors and properties.

## CSS nodes

GTK applies the style information found in style sheets by matching
the selectors against a tree of nodes. Each node in the tree has a
name, a state and possibly style classes. The children of each node
are linearly ordered.

Every widget has one or more of these CSS nodes, and determines their
name, state, style classes and how they are laid out as children and
siblings in the overall node tree. The documentation for each widget
explains what CSS nodes it has.

### The CSS nodes of a GtkScale

```
scale[.fine-tune]
├── marks.top
│   ├── mark
┊   ┊
│   ╰── mark
├── trough
│   ├── slider
│   ├── [highlight]
│   ╰── [fill]
╰── marks.bottom
    ├── mark
    ┊
    ╰── mark
```

## Selectors

Selectors work very similar to the way they do on the web.

All widgets have one or more CSS nodes with element names and style
classes. When style classes are used in selectors, they have to be prefixed
with a period. Widget names can be used in selectors like IDs. When used
in a selector, widget names must be prefixed with a &num; character.

### GTK CSS Selectors

| Pattern | Reference | Notes |
|:--------|:----------|:------|
| *       | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#universal-selector) | |
| E       | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#type-selectors) | |
| E.class | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#class-html) | |
| E#id    | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#id-selectors) | |
| E:nth-child(n) | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | |
| E:nth-last-child(n) | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | |
| E:first-child | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | |
| E:last-child | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | |
| E:only-child | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#structural-pseudos) | |
| E:link, E:visited | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#link) | Corresponds to GTK_STATE_FLAG_LINK and GTK_STATE_FLAGS_VISITED |
| E:active, E:hover, E:focus | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#useraction-pseudos) | Correspond to GTK_STATE_FLAG_ACTIVE, GTK_STATE_FLAG_PRELIGHT, GTK_STATE_FLAGS_FOCUSED |
| E:focus-within | [CSS Selector Level 4](https://drafts.csswg.org/selectors/#focus-within-pseudo) | Set on all ancestors of the focus widget, unlike CSS |
| E:focus-visible | [CSS Selector Level 4](https://drafts.csswg.org/selectors/#focus-within-pseudo) | Set on focus widget and all ancestors, unlike CSS |
| E:disabled | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#UIstates) | Corresponds to GTK_STATE_FLAG_INSENSITIVE |
| E:checked | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#UIstates) | Corresponds to GTK_STATE_FLAG_CHECKED |
| E:indeterminate | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#indeterminate) | Corresponds to GTK_STATE_FLAG_INCONSISTENT |
| E:backdrop, E:selected | | Corresponds to GTK_STATE_FLAG_BACKDROP, GTK_STATE_FLAG_SELECTED |
| E:not(selector) | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#negation) | |
| E:dir(ltr), E:dir(rtl) | [CSS Selector Level 4](https://drafts.csswg.org/selectors/#the-dir-pseudo) | |
| E:drop(active) | [CSS Selector Level 4](https://drafts.csswg.org/selectors/#drag-pseudos) | |
| E F | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#descendent-combinators) | |
| E > F | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#child-combinators) | |
| E ~ F | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#general-sibling-combinators) | |
| E + F | [CSS Selector Level 3](https://www.w3.org/TR/css3-selectors/#adjacent-sibling-combinators) | |
