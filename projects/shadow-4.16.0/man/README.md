The [official releases](https://github.com/shadow-maint/shadow/releases) ship
with pre-built manpages.

The content of the man pages however is dependent on compile flags. So the
pre-built ones might not fit your version of shadow.  To build them yourself use
`--enable-man`. Furthermore the following build requirements will be needed:
- xsltproc
- docbook 4
- docbook stylesheets
- itstool
