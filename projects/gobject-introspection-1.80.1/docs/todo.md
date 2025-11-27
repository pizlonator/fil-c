GIR XML format
----------
- Document the format better
- Add attributes to connect signals to their default handlers
  and wrappers to their vfuncs
- Add a way to specify default values
- Add a way to mark init functions like `gtk_init`
- For argument/return values etc, store only `c:identifier` in the
  `<class/>` definition. Requires a link to the references type in the 
  Type node.

Binary format
-------------
- Sort arrays so that we can use bsearch for lookup
- Maybe add hashes to lookup interfaces and functions in interfaces
- Add default values
- Make annotations more flexible (and typed)

Compiler
--------
- Cleanup, maybe turn the nodes in to objects to avoid the horrific
  type switches
- Add thorough error checking

Repository
----------
- Reconsider the current clunky API, maybe an iterator approach is better
- Add thorough error checking
- Use hashes
- Maybe allow populating repositories at runtime
- Think about a system-wide repository mapping namespace ids to
  libraries/metadata files
- Fix extra indirection for out parameters in invoke

Scanner
------
- Property.readable/writable, GLibProperty.blurb/nick
- Constants
- Annotations in external files which is not source code

C API
-------
- type SYMBOL is kind of inconvenient; would be nicer to go straight to 
  boxed, etc.
- no way to construct a boxed type
- no way to access struct fields (should have a way to go field -> GArgument)
- types namespaced GI, functions namespaced `g_`
- no way to get GType given a GIRegisteredTypeInfo
