This document describes how maintainership works on GNOME Settings. It is intended to be a guideline
for future reference.

The list of current maintainers can be found at the [gnome-control-center.doap][doap] DOAP file.

# General Rules

The purpose of the shared maintainership model in GNOME Settings is to avoid as much as possible
pushing unreviewed code in the main repository. Not only it is a good engineering practice, but it
also increases the code quality and reduces the number of bugs.

GNOME Settings has two types of maintainers:

 * **General Maintainer**: take care of the whole codebase and of panels without a specific maintainer.
 * **Panel Maintainer**: take care of a specific panel with a stronger authority over General
   Maintainers.


## For Contributors

Panel Maintainers have a stronger authority over their panels than a General Maintainer. If you are
contributing to a specific panel, and that panel has a dedicate maintainer, they should be the main
point of contact.

In the rare case of Panel Maintainers not being responsive, it is allowed to contact General
Maintainers.

## For Maintainers

If you are a Panel Maintainer, your merge requests will be reviewed by General Maintainer. The
opposite is true as well - General Maintainers contributing to a specific panel will have their
merge requests reviewed by the Panel Maintainer of that panel.

If you are a General Maintainer contributing to an unmaintained panel, or to the main codebase, your
merge requests will be reviewed by another General Maintainer.

Avoid pushing commits without an explicit review, except in the following cases:

 * The commit is a translation commit
 * The commit is trivial
 * The commit is urgent and no reviewers were available in time

When accepting a merge request and allowing the other maintainer to merge, avoid misunderstandings
by being explicit. Suggested acceptance phrase:

`I approve this merge request. You are allowed to merge it.`

### Urgency Commits

Urgency commits should never happen, but in case they're needed, they are defined by the following
criteria:

 * On stable branches (or `main` right after a stable release)
 * Symptoms:
   * Always OR often reproducible; AND
   * Crash; OR
   * Data loss; OR
   * System corruption
 * Quickly followed by an emergency release (at most 2 days after the commit)


[doap]: https://gitlab.gnome.org/GNOME/gnome-control-center/blob/main/gnome-control-center.doap
