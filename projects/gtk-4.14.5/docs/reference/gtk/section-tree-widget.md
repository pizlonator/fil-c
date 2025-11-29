Title: Tree and List Widget Overview
Slug: gtk-treeview

This document describes the `GtkTreeView` widget and auxiliary
classes, like tree models and cell renderers. All of these have
been deprecated and will be removed in GTK 5. Their replacements
are described in the [List Widget Overview](section-list-widget.html).

## Introduction

To create a tree or list in GTK, use the `GtkTreeModel` interface in
conjunction with the `GtkTreeView` widget. This widget is designed around
a _Model/View/Controller_ design and consists of four major parts:

- The tree view widget (GtkTreeView)
- The view column (GtkTreeViewColumn)
- The cell renderers (GtkCellRenderer etc.)
- The model interface (GtkTreeModel)

The _View_ is composed of the first three objects, while the last is the
_Model_. One of the prime benefits of the MVC design is that multiple views
can be created of a single model.  For example, a model mapping the file
system could be created for a file manager. Many views could be created
to display various parts of the file system, but only one copy need be
kept in memory.

The purpose of the cell renderers is to provide extensibility to the
widget and to allow multiple ways of rendering the same type of data.
For example, consider how to render a boolean variable. Should it
render it as a string of "True" or "False", "On" or "Off", or should
it be rendered as a checkbox?

## Creating a model

GTK provides two simple models that can be used: the `GtkListStore`
and the `GtkTreeStore`. `GtkListStore` is used to model list widgets,
while the `GtkTreeStore` models trees. It is possible to develop a new
type of model, but the existing models should be satisfactory for all
but the most specialized of situations. Creating the model is quite

``` {.c}
GtkListStore *store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
```

This creates a list store with two columns: a string column and a boolean
column. Typically the 2 is never passed directly like that; usually an
enum is created wherein the different columns are enumerated, followed by
a token that represents the total number of columns. The next example will
illustrate this, only using a tree store instead of a list store. Creating
a tree store operates almost exactly the same.

``` {.c}
enum
{
   TITLE_COLUMN,
   AUTHOR_COLUMN,
   CHECKED_COLUMN,
   N_COLUMNS
};

GtkTreeStore *store = gtk_tree_store_new (N_COLUMNS,       /* Total number of columns */
                                          G_TYPE_STRING,   /* Book title              */
                                          G_TYPE_STRING,   /* Author                  */
                                          G_TYPE_BOOLEAN); /* Is checked out?         */
```

Adding data to the model is done using gtk_tree_store_set() or
gtk_list_store_set(), depending upon which sort of model was
created. To do this, a `GtkTreeIter` must be acquired. The iterator
points to the location where data will be added.

Once an iterator has been acquired, gtk_tree_store_set() is used to
apply data to the part of the model that the iterator points to.
Consider the following example:

``` {.c}
GtkTreeIter iter;

gtk_tree_store_append (store, &iter, NULL);  /* Acquire an iterator */

gtk_tree_store_set (store, &iter,
                    TITLE_COLUMN, "The Principle of Reason",
                    AUTHOR_COLUMN, "Martin Heidegger",
                    CHECKED_COLUMN, FALSE,
                    -1);
```

Notice that the last argument is -1. This is always done because
this is a variable-argument function and it needs to know when to stop
processing arguments. It can be used to set the data in any or all
columns in a given row.

The third argument to gtk_tree_store_append() is the parent iterator.
It is used to add a row to a GtkTreeStore as a child of an existing row.
This means that the new row will only be visible when its parent is visible
and in its expanded state. Consider the following example:

``` {.c}
GtkTreeIter iter1;  /* Parent iter */
GtkTreeIter iter2;  /* Child iter  */

gtk_tree_store_append (store, &iter1, NULL);  /* Acquire a top-level iterator */
gtk_tree_store_set (store, &iter1,
                    TITLE_COLUMN, "The Art of Computer Programming",
                    AUTHOR_COLUMN, "Donald E. Knuth",
                    CHECKED_COLUMN, FALSE,
                    -1);

gtk_tree_store_append (store, &iter2, &iter1);  /* Acquire a child iterator */
gtk_tree_store_set (store, &iter2,
                    TITLE_COLUMN, "Volume 1: Fundamental Algorithms",
                    -1);

gtk_tree_store_append (store, &iter2, &iter1);
gtk_tree_store_set (store, &iter2,
                    TITLE_COLUMN, "Volume 2: Seminumerical Algorithms",
                    -1);

gtk_tree_store_append (store, &iter2, &iter1);
gtk_tree_store_set (store, &iter2,
                    TITLE_COLUMN, "Volume 3: Sorting and Searching",
                    -1);
```

## Creating the view component

While there are several different models to choose from, there is
only one view widget to deal with. It works with either the list
or the tree store. Setting up a `GtkTreeView` is not a difficult
matter. It needs a `GtkTreeModel` to know where to retrieve its data
from.

``` {.c}
GtkWidget *tree;

tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
```

## Columns and cell renderers

Once the `GtkTreeView` widget has a model, it will need to know how
to display the model. It does this with columns and cell renderers.

Cell renderers are used to draw the data in the tree model in a
way. There are a number of cell renderers that come with GTK,
including the `GtkCellRendererText`, `GtkCellRendererPixbuf` and
the `GtkCellRendererToggle`. It is relatively easy to write a
custom renderer.

A `GtkTreeViewColumn` is the object that `GtkTreeView` uses to organize
the vertical columns in the tree view. It needs to know the name of
the column to label for the user, what type of cell renderer to use,
and which piece of data to retrieve from the model for a given row.

``` {.c}
GtkCellRenderer *renderer;
GtkTreeViewColumn *column;

renderer = gtk_cell_renderer_text_new (<!-- -->);
column = gtk_tree_view_column_new_with_attributes ("Author",
                                                   renderer,
                                                   "text", AUTHOR_COLUMN,
                                                   NULL);
gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
```

At this point, all the steps in creating a displayable tree have been
covered. The model is created, data is stored in it, a tree view is
created and columns are added to it.

## Selection handling

Most applications will need to not only deal with displaying data,
but also receiving input events from users. To do this, simply get
a reference to a selection object and connect to the
`GtkTreeSelection::changed` signal.

``` {.c}
/* Prototype for selection handler callback */
static void tree_selection_changed_cb (GtkTreeSelection *selection, gpointer data);

/* Setup the selection handler */
GtkTreeSelection *select;

select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
g_signal_connect (G_OBJECT (select), "changed",
                  G_CALLBACK (tree_selection_changed_cb),
                  NULL);
```

Then to retrieve data for the row selected:

``` {.c}
static void
tree_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *author;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, AUTHOR_COLUMN, &author, -1);

      g_print ("You selected a book by %s\n", author);

      g_free (author);
    }
}
```

## Simple Example

Here is a simple example of using a `GtkTreeView` widget in context
of the other widgets. It simply creates a simple model and view,
and puts them together. Note that the model is never populated
with data — that is left as an exercise for the reader.
More information can be found on this in the `GtkTreeModel` section.

``` {.c}
enum
{
   TITLE_COLUMN,
   AUTHOR_COLUMN,
   CHECKED_COLUMN,
   N_COLUMNS
};

void
setup_tree (void)
{
   GtkTreeStore *store;
   GtkWidget *tree;
   GtkTreeViewColumn *column;
   GtkCellRenderer *renderer;

   /* Create a model.  We are using the store model for now, though we
    * could use any other GtkTreeModel */
   store = gtk_tree_store_new (N_COLUMNS,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_BOOLEAN);

   /* custom function to fill the model with data */
   populate_tree_model (store);

   /* Create a view */
   tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));

   /* The view now holds a reference.  We can get rid of our own
    * reference */
   g_object_unref (G_OBJECT (store));

   /* Create a cell render and arbitrarily make it red for demonstration
    * purposes */
   renderer = gtk_cell_renderer_text_new (<!-- -->);
   g_object_set (G_OBJECT (renderer),
                 "foreground", "red",
                 NULL);

   /* Create a column, associating the "text" attribute of the
    * cell_renderer to the first column of the model */
   column = gtk_tree_view_column_new_with_attributes ("Author", renderer,
                                                      "text", AUTHOR_COLUMN,
                                                      NULL);

   /* Add the column to the view. */
   gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

   /* Second column.. title of the book. */
   renderer = gtk_cell_renderer_text_new (<!-- -->);
   column = gtk_tree_view_column_new_with_attributes ("Title",
                                                      renderer,
                                                      "text", TITLE_COLUMN,
                                                      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

   /* Last column.. whether a book is checked out. */
   renderer = gtk_cell_renderer_toggle_new (<!-- -->);
   column = gtk_tree_view_column_new_with_attributes ("Checked out",
                                                      renderer,
                                                      "active", CHECKED_COLUMN,
                                                      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

   /* Now we can manipulate the view just like any other GTK widget */
   ...
}
```
