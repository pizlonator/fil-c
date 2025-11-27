GObject-Introspection annotations
---------------------------------

Symbol visibility
~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(skip)``
    - identifier
    - Omit the symbol from the introspected output.
    - :commit:`v0.6.4 <7549c8053d0229a12d9196cc8abae54a01a555d0>`
      :bzbug:`556628`
  * -
    - parameters, return value
    - Indicate that the parameter or return value is only useful in C and
      should be skipped.
    - :commit:`v1.29.0 <9c6797e0478b5025c3f2f37b1331c1328cf34f4d>`
      :bzbug:`649657`
  * - ``(rename-to SYMBOL)``
    - identifier
    - Rename the original symbol's name to ``SYMBOL``. If ``SYMBOL`` resolves
      to a symbol name that is already used, the original binding for that
      name is removed.
    - :commit:`v0.6.3 <23e6fa6993c046de032598127ea48d4a7ee00935>`
      :bzbug:`556475`


Memory and lifecycle management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(transfer MODE)``
    - identifier (only properties)
    - Transfer ownership for the property, (see below)
    - :commit:`v0.9.0 <22ae017ffd3052c0b81822b2ca6e41626b76b9c4>`
      :bzbug:`620484`
  * -
    - parameters, return value
    - Transfer mode for the parameter or return value (see below).
    - v0.5.0 unknown

Transfer modes:

* ``none``: the recipient does not own the value
* ``container``: the recipient owns the container, but not the elements.
  (Only meaningful for container types.)
* ``full``: the recipient owns the entire value. For a refcounted type,
  this means the recipient owns a ref on the value. For a container type,
  this means the recipient owns both container and elements.
* ``floating``: alias for none, can be used for floating objects.

``container`` is usually a pointer to a list or hash table, eg GList, GSList,
GHashTable etc.

``elements`` is what is contained inside the list: integers, strings, GObjects
etc.


Support for GObject objects
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(constructor)``
    - identifier
    - The annotated symbol should not become available as a static methods
      but as a constructor.
    - :commit:`v0.10.2 <2c36790c>`
      :bzbug:`561264`
  * - ``(method)``
    - identifier
    - This function is a method.
    - :commit:`v0.10.2 <09bca85d>`
      :bzbug:`639945`
  * - ``(virtual SLOT)``
    - identifier
    - This function is the invoker for a virtual method.
    - :commit:`v0.6.3 <fdbe3cc3>`
      :bzbug:`557383`
  * - ``(set-property NAME)``
    - identifier (only applies to methods)
    - This function is the setter method for the given GObject property.
      A setter function is defined as being the public function that is
      called by the ``GObjectClass.set_property`` implementation in a
      class.
    - :issue:`13`
  * - ``(get-property NAME)``
    - identifier (only applies to methods)
    - This function is the getter method for the given GObject property.
      A getter function is defined as being the public function that is
      called by the ``GObjectClass.get_property`` implementation in a
      class.
    - :issue:`13`
  * - ``(setter SYMBOL)``
    - identifier (only applies to properties)
    - This GObject property is accessed by the given setter function.
      A setter function is defined as being the public function that is
      called by the ``GObjectClass.set_property`` implementation in a
      class.
    - :issue:`13`
  * - ``(getter SYMBOL)``
    - identifier (only applies to properties)
    - This GObject property is accessed by the given getter function.
      A getter function is defined as being the public function that is
      called by the ``GObjectClass.get_property`` implementation in a
      class.
    - :issue:`13`
  * - ``(emitter METHOD)``
    - identifier (only applies to methods)
    - This signal is emitted by the given method
    -
  * - ``(default-value VALUE)``
    - identifier (only applies to properties)
    - The default value of a GObject property, as a freeform string
    - :issue:`4`


Support for GObject closures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(destroy)``
    - parameters
    - The parameter is part of a callback type and containing the ``destroy_data``.
    - :commit:`v0.6.3 <cf7621f3>`
      :bzbug:`574284`
  * - ``(destroy DESTROY)``
    - parameters
    - The parameter defines the ``destroy_data`` for a given callback. The
      ``DESTROY`` option points to the parameter that is the actual callback.
    -
  * - ``(closure)``
    - parameters
    - The parameter is part of a callback type and containing the ``user_data``.
    -
  * - ``(closure CLOSURE)``
    - parameters
    - The parameter defines the ``user_data`` for a given callback. The ``CLOSURE`` option
      points to the parameter that is the actual callback. Many bindings can pass ``NULL`` here.
    -


Support for non-GObject fundamental objects
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(ref-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to ref a struct, must be a GTypeInstance
    - :commit:`v0.9.2 <1e9822c7>`
      :bzbug:`568913`
  * - ``(unref-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to unref a struct, must be a GTypeInstance
    -
  * - ``(get-value-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to convert a struct from a GValue,
      must be a GTypeInstance
    -
  * - ``(set-value-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to convert from a struct to a GValue,
      must be a GTypeInstance
    -
  * - ``(copy-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to copy a struct or a union
    - 1.76
  * - ``(free-func FUNC)``
    - identifier
    - ``FUNC`` is the function used to free a struct or a union
    - 1.76


Type signature
~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(nullable)``
    - parameters, return value
    - Indicates that ``NULL`` may be a valid value for a parameter
      (in, out, inout), or return value (though note that return and out values which
      are only ``NULL`` when throwing an error should not be annotated as
      ``(nullable)``).
    - :commit:`1.42 <1459ff3e>`
      :bzbug:`660879`
  * - ``(not nullable)``
    - parameters, return value
    - Indicates that ``NULL`` is not a valid value for a parameter
      (in, out, inout), or return value.
    - :commit:`1.48 <10cb665f>`
      :bzbug:`729660`
  * - ``(optional)``
    - parameters
    - For ``(out)`` or ``(inout)`` parameters, signifies that the caller
      can pass ``NULL`` to ignore this output parameter.
    - :commit:`1.42 <1459ff3e>`
      :bzbug:`660879`
  * - ``(not optional)``
    - parameters
    - For ``(out)`` or ``(inout)`` parameters, signifies that the caller
      cannot pass ``NULL`` to ignore this output parameter.
    - v1.80
  * - ``(in)``
    - parameters
    - In parameter.
    - v0.5.0
      unknown
  * - ``(out)``
    - parameters
    - Out parameter (automatically determine allocation).
    - v0.5.0
      unknown
  * - ``(out caller-allocates)``
    - parameters
    - Out parameter, where the calling code must allocate storage.
    - :commit:`v0.6.13 <5589687a>`
      :bzbug:`604749`
  * - ``(out callee-allocates)``
    - parameters
    - Out parameter, where the receiving function must allocate storage.
    -
  * - ``(inout)``
    - parameters
    - In/out parameter.
    - v0.5.0
      unknown
  * - ``(type TYPE)``
    - identifier
    - Override the default type, used for properties
    - :commit:`v0.6.2 <6de1b296>`
      :bzbug:`546739`
  * -
    - parameters, return value
    - override the parsed C type with given type
    -
  * - ``(array)``
    - parameters, return value
    - Arrays.
    - v0.5.0
      unknown
  * - ``(array fixed-size=N)``
    - parameters, return value
    - array of fixed length N
    - v0.5.0
      unknown
  * - ``(array length=PARAM)``
    - parameters, return value
    - array, fetch the length from parameter PARAM
    - v0.5.0
      unknown
  * - ``(array zero-terminated=1)``
    - parameters, return value
    - array which is NULL terminated
    - :commit:`v0.6.0 <d15f8cde>`
      :bzbug:`557786`
  * - ``(element-type TYPE)``
    - parameters, return value
    - Specify the type of the element inside a container.
      Can be used in combination with (array).
    - v0.5.0
      unknown
  * - ``(element-type KTYPE VTYPE)``
    - parameters, return value
    - Specify the types of the keys and values in a dictionary-like container
      (eg, ``GHashTable``).
    - v0.5.0
      unknown
  * - ``(foreign)``
    - identifier
    - The annotated symbol is a foreign struct, meaning it is not available
      in a g-i supported library.
    - :commit:`v0.6.12 <1edeccd2>`
      :bzbug:`619450`
  * - ``(sync-func FUNC)``
    - identifier
    - ``FUNC`` is the identifier of the function which implements the synchronous
      version of the asynchronous function referred to by the annotated symbol.
    -
  * - ``(async-func FUNC)``
    - identifier
    - ``FUNC`` is the identifier of the function which implements the asynchronous
      version of the synchronous function referred to by the annotated symbol.
    -
  * - ``(finish-func FUNC)``
    - identifier
    - ``FUNC`` is the identifier of the function which returns the result of the
      asynchronous function referred to by the annotated symbol.
    -
  * - ``(scope TYPE)``
    - parameters
    - The parameter is a callback, the ``TYPE`` option indicates the lifetime
      of the call. It is mainly used by language bindings wanting to know when
      the resources required to do the call (for instance ffi closures) can be
      freed.
    - :commit:`v0.6.2 <bc88ef7b>`
      :bzbug:`556489`

Scope types:

* ``call`` (default) - Only valid for the duration of the call.
  Can be called multiple times during the call.
* ``async`` - Only valid for the duration of the first callback invocation.
  Can only be called once.
* ``notified`` - valid until the GDestroyNotify argument is called.
  Can be called multiple times before the GDestroyNotify is called.
* ``forever`` - valid until the process terminates.

An example of a function using the ``call`` scope is ``g_slist_foreach()``.
For ``async`` there is ``g_file_read_async()`` and for notified
``g_idle_add_full()``.

Default Annotations
~~~~~~~~~~~~~~~~~~~

The introspection framework provides default values for parameters, return
values, and certain types.

* ``(in)`` parameters: ``(transfer none)``
* ``(inout)`` and ``(out)`` parameters: ``(transfer full)``

  * if ``(caller allocates)`` is set: ``(transfer none)``

* ``gchar*`` means ``(type utf8)``
* return values: ``(transfer full)``

  * ``gchar*`` means ``(type utf8) (transfer full)``
  * ``const gchar*`` means ``(type utf8) (transfer none)``
  * ``GObject*`` defaults to ``(transfer full)``


Data annotations
~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(value VALUE)``
    - identifier
    - Used to override constants for defined values,
      VALUE contains the evaluated value
    - v0.5.0
      unknown
  * - ``(attributes my.key=val my.key2)``
    - identifier, parameters, return value
    - Attributes are free-form "key=value" annotations. When present, at least
      one key has to be specified. Assigning values to keys is optional.
    - :commit:`v0.9.0 <11cfe386>`
      :bzbug:`571548`


Deprecated GObject-Introspection annotations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
  :header-rows: 1
  :widths: 1 10 1

  * - Annotation
    - Description
    - Since
  * - ``(null-ok)``
    - Replaced by ``(allow-none)``
    - :commit:`v0.6.0 <dc651812>`
      :bzbug:`557405`
  * - ``(in-out)``
    - Replaced by ``(inout)``
    - :commit:`1.39.0 <a2b22ce7>`
      :bzbug:`688897`
  * - ``(allow-none)``
    - Replaced by ``(nullable)`` and ``(optional)``
    - :commit:`1.42 <1459ff3e>`
      :bzbug:`660879`


Possible future GObject-Introspection annotations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These proposed additions are currently being discussed and in various stages
of development.

.. list-table::
  :header-rows: 1
  :widths: 1 1 10 1

  * - Annotation
    - Applies to
    - Description
    - Since
  * - ``(default VALUE)``
    - parameters
    - Default value for a parameter.
    - :bzbug:`558620`
  * - ``(error-domains DOM1 DOM2)``
    - parameters
    - Typed errors, similar to ``throws`` in Java.
    - unknown


Default Basic Types
-------------------

Basic types:

* gpointer: pointer to anything
* gboolean:boolean
* gint[8,16,32,64]: integer
* guint[8,16,32,64]: unsigned integer
* glong: long
* gulong: unsigned long
* GType: a gtype
* gfloat: float
* gdouble: double
* utf8: string encoded in UTF-8, not containing any embedded nuls
* filename: filename string (see below)
* guint8 array: binary data

Filename type:

The filename type represents an utf-8 string on Windows and a zero terminated
guint8 array on Unix. It should be used for filenames, environment variables
and process arguments.


Reference to Object Instances
-----------------------------

Instances:

* Object: a GObject instance
* Gtk.Button: a Gtk.Button instance


Examples
--------

Transfer
~~~~~~~~

::

    /**
     * mylib_get_constant1:
     *
     * Returns: (transfer full): a constant, free when you used it
     */
    gchar *
    mylib_get_constant1 (void)
    {
       return g_strdup("a constant");
    }

::

  /**
   * mylib_get_constant2:
   *
   * Returns: (transfer none): another constant
   */
  const gchar *
  mylib_get_string2 (void)
  {
     return "another constant";
  }

::

  /**
   * mylib_get_string_list1:
   *
   * Returns: (element-type utf8) (transfer full): list of constants,
   *          free the list with g_slist_free and the elements with g_free when done.
   */
  GSList *
  mylib_get_string_list1 (void)
  {
     GSList *l = NULL;
     l = g_slist_append (l, g_strdup ("foo"));
     l = g_slist_append (l, g_strdup ("bar"));
     return l;
  }

::

  /**
   * mylib_get_string_list2:
   *
   * Returns: (element-type utf8) (transfer container): list of constants
   *          free the list with g_slist_free when done.
   */
  GSList *
  mylib_get_string_list2 (void)
  {
     GSList *l = NULL;
     l = g_slist_append (l, "foo");
     l = g_slist_append (l, "bar");
     return l;
  }


Array length
~~~~~~~~~~~~

::

  /**
   * gtk_list_store_set_column_types:
   * @store: a #GtkListStore
   * @n_columns: Length of @types
   * @types: (array length=n_columns): List of types
   */
  void
  gtk_list_store_set_column_types (GtkListStore *list_store,
                                   gint          n_columns,
                                   GType        *types);


Nullable parameters
~~~~~~~~~~~~~~~~~~~

A number of things are nullable by convention, which means that you do not
have to add a ``(nullable)`` annotation to your code for them to be marked as
nullable in a GIR file. If you need to mark a parameter or return value as not
nullable, use ``(not nullable)`` to override the convention. Conventionally,
the following are automatically nullable:

* ``(closure)`` parameters and their corresponding user data parameters
* ``gpointer`` parameters and return types, unless also annotated with
  ``(type)``

::

  /**
   * gtk_link_button_new_with_label:
   * @uri: A URI
   * @label: (nullable): A piece of text or NULL
   */
  GtkWidget *
  gtk_link_button_new_with_label (const gchar *uri,
                                  const gchar *label);

::

  /**
   * g_source_add_unix_fd:
   * @source: a #GSource
   * @fd: the fd to monitor
   * @events: an event mask
   *
   * Returns: (not nullable): an opaque tag
   */
  gpointer
  g_source_add_unix_fd (GSource      *source,
                        gint          fd,
                        GIOCondition  events);

  /**
   * g_source_remove_unix_fd:
   * @source: a #GSource
   * @tag: (not nullable): the tag from g_source_add_unix_fd()
   */
  void
  g_source_remove_unix_fd (GSource  *source,
                           gpointer  tag);


G(S)List contained types
~~~~~~~~~~~~~~~~~~~~~~~~

::

  /**
   * gtk_container_get_children:
   * @container: A #GtkContainer
   *
   * Returns: (element-type Gtk.Widget) (transfer container): List of #GtkWidget
   */
  GList*
  gtk_container_get_children (GtkContainer *container);

::

  /**
   * FooBar:alist: (type GSList(NiceObj))
   *
   * This property is a GSList of NiceObj GOjects.
   */
      g_object_class_install_property (object_class,
                                       FOO_BAR_PROP_ALIST,
                                       g_param_spec_pointer ("alist",
                                                             "Alist",
                                                             "A list of nice objects",
                                                             G_PARAM_READWRITE));


Direction
~~~~~~~~~

::

  /**
   * gtk_widget_get_size_request:
   * @width: (out): Int to store width in
   * @height: (out): Int to store height in
   */


Out parameters
~~~~~~~~~~~~~~

This is a callee-allocates example; the (out) annotation automatically infers
this from the fact that there's a double indirection on a structure parameter.


::

  typedef struct _FooSubObj FooSubObj

  /**
   * foo_obj_get_sub_obj:
   * @obj: A #FooObj
   * @subobj: (out): A #FooSubObj
   *
   * Get a sub object.
   */
  void
  foo_obj_get_sub_obj (FooObj     *obj,
                       FooSubObj **subobj)
  {
    *subobj = foo_sub_object_new ();
  }

This is a caller-allocates example; the (out) annotation automatically infers
this from the fact that there's only a single indirection on a structure
parameter.

::

  typedef struct _FooIter FooIter;

  /**
   * foo_obj_get_iter:
   * @obj: A #FooObj
   * @iter: (out): An iterator, will be initialized
   *
   * Get an iterator.
   */
  void
  foo_obj_get_iter (FooObj *obj,
                    FooIter *iter)
  {
    iter->state = 0;
  }

An example which demonstrates an (optional) parameter: an (out) parameter
where the caller can pass NULL if they don’t want to receive the (out) value.
Note that if the GError is returned set, the value of contents and length might
be unspecified and should therefore not be used or freed.

::

  /**
   * g_file_get_contents:
   * @filename: name of a file to read contents from, in the GLib file name encoding
   * @contents: (out): location to store an allocated string, use g_free() to free the returned string
   * @length: (out) (optional): location to store length in bytes of the contents, or NULL
   * @error: return location for a GError, or NULL
   *
   * [...]
   *
   * Returns: TRUE on success, FALSE if an error occurred
   */
  gboolean g_file_get_contents (const gchar *filename,
                                gchar **contents,
                                gsize *length,
                                GError **error);

  /* this is valid because length has (optional) */
  g_file_get_contents ("/etc/motd", &motd, NULL, &error); // VALID
  /* but this is not valid, according to those annotations */
  g_file_get_contents ("/etc/motd", NULL, NULL, &error); // NOT VALID


mylib_hash_table_iter_next() demonstrates the difference between (nullable) and
(optional) for (out) parameters. For an (out) or (inout) parameter, (optional) indicates
that NULL may be passed by the caller to indicate they don’t want to receive
the (out) value. (nullable) indicates that NULL may be passed out by the
callee as the returned value.

::

  /**
   * mylib_hash_table_iter_next:
   * @iter: an initialized #MylibHashTableIter
   * @key: (out) (optional): a location to store the key
   * @value: (out) (optional) (nullable): a location to store the value
   *
   * [...]
   *
   * Returns: %FALSE if the end of the #MylibHashTable has been reached.
   */
  gboolean
  mylib_hash_table_iter_next (MylibHashTableIter *iter,
                              gpointer           *key,
                              gpointer           *value);

  /* this is valid because value and key have (optional) */
  mylib_hash_table_iter_next (iter, NULL, NULL);

  gpointer key, value;
  mylib_hash_table_iter_next (iter, &key, &value);

  if (value == NULL)
    /* this is valid because value has (nullable) */
  if (key == NULL)
    /* this is NOT VALID because key does not have (nullable) */


Rename to
~~~~~~~~~

Rename to is an advisory annotation. It's not required to fulfil the advisory
when generating or making a language binding. The way it is currently
implemented, if you rename a function to a name already in use, it will remove
the other binding. This is useful to eliminate unwanted/deprecated functions
from the binding.

Another (currently unimplemented) use for the rename annotation would be
overloading; for example, overloading of constructors or, like in this
example, overloading a method to be both an asynchronous and a synchronous one
(depending on the amount and what kind of parameters).

::

  /**
   * my_type_perform_async: (rename-to my_type_perform)
   * @self: The this ptr
   * @data: data
   * @callback: callback when async operation finished
   * @user_data: user_data for @callback
   *
   * Asynchronously perform
   **/
  void
  my_type_perform_async (MyType *self, gpointer data,
                         GFunc callback,
                         gpointer user_data);

  /**
   * my_type_perform:
   * @self: The this ptr
   * @data: data
   *
   * Perform
   **/
  void
  my_type_perform (MyType *self, gpointer data);

In a language supporting method overloading, because we advised to rename to
perform, and because we have another perform already, this could be bound like
this:

::

  class MyType {
    public void perform (Pointer data) { }
    public void perform (Pointer data, GFunc callback, Pointer user_data) { }
  }

However, currently the generated gir/typelib will only contain information
about my_type_perform_async, which will shadow (ie, remove) the binding of
my_type_perform.


Attributes
~~~~~~~~~~

Attributes are arbitrary key/value pairs that can be attached to almost any
item including classes, methods, signals, properties, parameters and return
values. These attributes appear in both the .gir and the .typelib files.
Attributes can serve as a mechanism for software higher in the toolchain.
Attributes are name-spaced using dot as a separator. At least one dot must
appear in the key name.

::

  /**
   * my_frobnicator_poke_path: (attributes gdbus.method PokePath)
   * @frobnicator: A #MyFrobnicator
   * @object_path: (gdbus.signature o): An object path.
   *
   * Manipulate an object path.
   *
   * Returns: (gdbus.signature o): A new object path. Free with g_free().
   */
  gchar *
  my_frobnicator_poke_path (MyFrobnicator *frobnicator,
                            const gchar   *object_path)


Constants
~~~~~~~~~

::

  /**
   * MY_CONSTANT: (value 100)
   * A constant.
   */
  #define MY_CONSTANT 10 * 10
