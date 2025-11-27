============
Architecture
============

.. figure:: images/architecture.svg
    :align: center


::

    BUILD TIME:

              +-----------------------------------------------------------+
              |   foo.c                                                   |
              |   foo.h                                                   |
              |                                                           |
              | Library sources, with type annotations                    |
              +-----------------------------------------------------------+
                  |                                      |
                 gcc                                 g-ir-scanner
                  |                                      |
                  |                                      V
                  |                              +------------------------+
                  |                              |    Foo.gir             |
                  |                              |                        |
                  |                              |    <GI-name>.gir       |
                  |                              |                        |
                  |                              | XML file               |
                  |                              |                        |
                  |                              | Invocation information |
                  |                              | Required .gir files    |
                  |                              | API docs               |
                  |                              |                        |
                  |                              +------------------------+
                  |                                          |
                  |                                     g-ir-compiler
                  |                                          |
      DEPLOYMENT TIME:                                       |
                  |                                          |
                  V                                          V
            +-----------------------------+      +---------------------------+
            |   libfoo.so                 |      |    Foo.typelib            |
            |                             |      |                           |
            |                             |      | Binary version of the     |
            |  ELF file                   |      | invocation info and       |
            |                             |      | required .typelib files   |
            |  Machine code, plus         |      +---------------------------+
            | dynamic linkage information |                  A
            | (DWARF debug data, etc)     |                  |
            +-----------------------------+                  |
                         A                                   |
                         |                       +---------------------------+
                         |                       |  libgirepository.so       |
                    +-----------+                |                           |
                    | libffi.so |                |  Can read typelibs and    |
                    |           |<-------+------>|  present them in a        |
                    +-----------+        |       |  libffi-based way         |
                                         |       |                           |
                                         |       +---------------------------+
                                         |
                                         |
                               +----------------------------+
                               | Specific language bindings |
                               +----------------------------+
