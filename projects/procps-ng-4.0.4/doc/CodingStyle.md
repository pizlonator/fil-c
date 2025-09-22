Most developers find Linux coding style easy to read, and there is
really no reason to reinvent this practise, so procps-ng goes along
with others.

http://git.kernel.org/?p=linux/kernel/git/torvalds/linux.git;a=blob_plain;f=Documentation/CodingStyle

In addition to Linux coding style this project has few additional
wishes to contributors.

* Many small patches are favoured over one big. Break down is done on
  basis of logical functionality; for example #endif mark ups,
  compiler warning and exit codes fixes all should be individual
  small patches.

* Use 'FIXME: ' in code comments, manual pages, autotools files,
  scripts and so on to indicate something is wrong.  The reason we do
  is as simple as being able to find easily where problem areas are.

* In writing arithmetic comparisons, use "<" and "<=" rather than
  ">" and ">=".  For some justification, read this:
  http://thread.gmane.org/gmane.comp.version-control.git/3903/focus=4126

* Be nice to translators. Don't change translatable strings if you
  can avoid it.  If you must rearrange individual lines (e.g., in
  multi-line --help strings), extract and create new strings, rather
  than extracting and moving into existing blocks.  This avoids
  making unnecessary work for translators.
