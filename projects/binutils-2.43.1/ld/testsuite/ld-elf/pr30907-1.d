#name: --rosegment (pr30907-1) (does not have two read-only segments)
#source: pr22393-1.s
#ld: -shared -z separate-code -z relro --rosegment
#readelf: -l --wide
#target: *-*-linux-gnu *-*-gnu* *-*-nacl* arm*-*-uclinuxfdpiceabi
#xfail: ![check_shared_lib_support] 
#xfail: ![check_relro_support]

#failif
#...
[ ]+LOAD[ 	]+0x[0-9a-f x]+R[ ]+0x.*
#...
[ ]+LOAD[ 	]+0x[0-9a-f x]+R[ ]+0x.*
#...
