#name: Text Section Ordering (section-order-1b)
#source: section-order-1a.s
#source: section-order-1b.s
#source: start.s
#ld: --section-ordering-file section-order-1b.t
#nm: -n

#...
[0-9a-f]+ T yyy
#...
[0-9a-f]+ T bar
#...
[0-9a-f]+ T [_]+start
#...
[0-9a-f]+ T xxx
#...
[0-9a-f]+ T foo
#pass
