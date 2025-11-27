Introspection scanner
---------------------

- If you add a new warning to `annotationparser.py`, you should add new
  tests for it in `tests/scanner/annotationparser/gi`

- If you add a new warning elsewhere (`maintransformer.py`, etc), you
  should add a new test for it in `tests/warn/`

- Before pushing code to the repository, make sure to run `meson test`
