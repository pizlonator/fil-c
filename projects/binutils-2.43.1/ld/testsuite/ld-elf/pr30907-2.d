#name: --rosegment (pr30907-2) (has at least one RX, one R and one RW segments)
#source: pr22393-1.s
#ld: -shared -z separate-code -z relro --rosegment
#readelf: -l --wide
#target: *-*-linux-gnu *-*-gnu* *-*-nacl* arm*-*-uclinuxfdpiceabi
#xfail: ![check_shared_lib_support] 
#xfail: ![check_relro_support]

#...
[ ]+LOAD[ 	]+0x[0-9a-f x]+R E[ ]+0x.*
#...
[ ]+LOAD[ 	]+0x[0-9a-f x]+R[ ]+0x.*
#...
[ ]+LOAD[ 	]+0x[0-9a-f x]+RW[ ]+0x.*
#...
