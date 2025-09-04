#PR ld/31289
#source: pr22269.s
#ld: --no-dynamic-linker --fatal-warnings -z dynamic-undefined-weak
#readelf: -r -x .data.rel.ro
#error: unsupported option: -z dynamic-undefined-weak
#target: *-*-linux* *-*-gnu* *-*-nacl* arm*-*-uclinuxfdpiceabi
