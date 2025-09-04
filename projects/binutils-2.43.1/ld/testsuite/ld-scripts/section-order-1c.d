#name: Data Section Ordering (section-order-1c)
#source: section-order-1b.s
#source: section-order-1a.s
#source: start.s
#ld: --section-ordering-file section-order-1a.t
#nm: -n

#...
[0-9a-f]+ D small
#...
[0-9a-f]+ D big
#...
[0-9a-f]+ D ba.*
#pass
