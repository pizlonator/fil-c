# The GNU C Library Security Policy

This document describes the policy followed by the GNU C Library maintainers
to handle bugs that may have a security impact.  This includes determining if a
bug has a security impact, reporting such bugs to the community and handling
such bugs all the way to resolution.  This policy may evolve over time, so if
you're reading this from a release tarball, be sure to check the latest copy of
the [SECURITY.md in the
repository](https://sourceware.org/git/?p=glibc.git;a=blob;f=SECURITY.md),
especially for instructions on reporting issues privately.

## What is a security bug?

Most security vulnerabilities in the GNU C Library materialize only after an
application uses functionality in a specific way.  Therefore, it is sometimes
difficult to determine if a defect in the GNU C Library constitutes a
vulnerability as such.  The follow guidelines can help with a decision.

* Buffer overflows should be treated as security bugs if it is conceivable that
  the data triggering them can come from an untrusted source.
* Other bugs that cause memory corruption which is likely exploitable should be
  treated as security bugs.
* Information disclosure can be security bugs, especially if exposure through
  applications can be determined.
* Memory leaks and races are security bugs if they cause service breakage.
* Stack overflow through unbounded alloca calls or variable-length arrays are
  security bugs if it is conceivable that the data triggering the overflow
  could come from an untrusted source.
* Stack overflow through deep recursion and other crashes are security bugs if
  they cause service breakage.
* Bugs that cripple the whole system (so that it doesn't even boot or does not
  run most applications) are not security bugs because they will not be
  exploitable in practice, due to general system instability.
* Bugs that crash `nscd` are generally security bugs, except if they can only
  be triggered by a trusted data source (DNS is not trusted, but NIS and LDAP
  probably are).
* The [Security Exceptions](#SecurityExceptions) section below describes
  subsystems for which determining the security status of bugs is especially
  complicated.
* For consistency, if the bug has received a CVE name attributing it to the GNU
  C library, it should be flagged `security+`.
* Duplicates of security bugs (flagged with `security+`) should be flagged
  `security-`, to avoid cluttering the reporting.

In this context, _service breakage_ means client-side privilege escalation
(code execution) or server-side denial of service or privilege escalation
through actual, concrete, non-synthetic applications. Or put differently, if
the GNU C Library causes a security bug in an application (and the application
uses the library in a standard-conforming manner or according to the manual),
the GNU C Library bug should be treated as security-relevant.

### Security Exceptions

It may be especially complicated to determine the security status of bugs in
some subsystems in the GNU C Library.  This subsection describes such
subsystems and the special considerations applicable during security bug
classification in them.

#### Regular expression processing

Regular expression processing comes in two parts, compilation (through regcomp)
and execution (through regexec).

Implementing regular expressions efficiently, in a standard-conforming way, and
without denial-of-service vulnerabilities is very difficult and impossible for
Basic Regular Expressions. Most implementation strategies have issues dealing
with certain classes of patterns.

Consequently, certain issues which can be triggered only with crafted patterns
(either during compilation or execution) are treated as regular bugs and not
security issues.  Examples of such issues would include (but is not limited
to):

 * Running out of memory through valid use of malloc
 * Quadratic or exponential behaviour resulting in slow execution time
 * Stack overflows due to recursion when processing patterns

Crashes, infinite loops (and not merely exponential behavior), buffer overflows
and overreads, memory leaks and other bugs resulting from the regex
implementation relying on undefined behavior should be treated as security
vulnerabilities.

#### wordexp patterns

`wordexp` inherently has exponential memory consumption in terms of the input
size.  This means that denial of service flaws from crafted patterns are not
security issues (even if they lead to other issues, such as NULL pointer
dereferences).

#### Asynchronous I/O

The GNU C Library tries to implement asynchronous I/O without kernel support,
which means that several operations are not fully standard conforming.  Several
known races can cause crashes and resource leaks.  Such bugs are only treated
as security bugs if applications (as opposed to synthetic test cases) have
security exposures due to these bugs.

#### Asynchronous cancellation

The implementation of asynchronous cancellation is not fully
standard-conforming and has races and leaks.  Again, such bugs are only treated
as security bugs if applications (as opposed to synthetic test cases) have
security exposures due to these bugs.

#### Crafted binaries and ldd

The `ldd` tool is not expected to be used with untrusted executables.

#### Post-exploitation countermeasures

Certain features have been added to the library only to make exploitation of
security bugs (mainly for code execution) more difficult.  Examples includes
the stack smashing protector, function pointer obfuscation, vtable validation
for stdio stream handles, and various heap consistency checks.  Failure of such
countermeasures to stop exploitation of a different vulnerability is not a
security vulnerability in itself.  By their nature, these countermeasures are
based on heuristics and will never offer complete protection, so the original
vulnerability needs to be fixed anyway.

## Reporting security bugs

The process to report security bugs is documented on the glibc [security
page](https://sourceware.org/glibc/security.html).  In general, most security
bugs may be reported publicly in the [glibc
bugzilla](https://sourceware.org/glibc/bugs.html), but if in doubt, please feel
free to report security issues privately first.

## Triaging security bugs

This section is aimed at developers, not reporters.

Security-relevant bugs should be marked with `security+`, as per the [Bugzilla
security flag
documentation](https://sourceware.org/glibc/wiki/Bugzilla%20Procedures#security),
following the guidelines above.  If you set the `security+` flag, you should
make sure the following information is included in the bug (usually in a bug
comment):

* The first glibc version which includes the vulnerable code.  If the
  vulnerability was introduced before glibc 2.4 (released in 2006), this
  information is not necessary.
* The commit or commits (identified by hash) that fix this vulnerability in the
  master branch, and (for historic security bugs) the first release that
  includes this fix.
* The summary should include the CVE names (if any), in parentheses at the end.
* If there is a single CVE name assigned to this bug, it should be set as an
  alias.

The following links are helpful for finding untriaged bugs:

* [Unprocessed bugs](https://sourceware.org/bugzilla/buglist.cgi?f1=flagtypes.name&o1=notsubstring&product=glibc&query_format=advanced&v1=security)
* [`security?` review requests](https://sourceware.org/bugzilla/buglist.cgi?f1=flagtypes.name&o1=substring&product=glibc&query_format=advanced&v1=security%3f)
* [Open `security+` bugs](https://sourceware.org/bugzilla/buglist.cgi?bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=SUSPENDED&bug_status=WAITING&bug_status=REOPENED&bug_status=VERIFIED&f1=flagtypes.name&o1=substring&product=glibc&query_format=advanced&v1=security%2B)

## Fixing security bugs

For changes to master, the regular [consensus-driven
process](https://sourceware.org/glibc/wiki/Consensus) must be followed.  It
makes sense to obtain consensus in private, to ensure that the patch is likely
in a committable state, before disclosing an emboargoed vulnerability.

Security backports to release branches need to follow the
[release process](https://sourceware.org/glibc/wiki/Release#General_policy).

Contact the [website
maintainers](https://sourceware.org/glibc/wiki/MAINTAINERS#Maintainers_for_the_website)
and have them draft a news entry for the website frontpage to direct users to
the bug, the fix, or the mailing list discussions.

## CVE assignment

Security bugs flagged with `security+` should have [CVE
identifiers](http://cve.mitre.org/about/). Please reach out to the glibc
security team using the documented [security
process](https://sourceware.org/glibc/security.html) and they work on getting a
CVE number.
