#name: globalize and weaken 'foo'
#PROG: objcopy
#objcopy: --globalize-symbol foo -W foo
#source: symbols-5.s
#nm: -n

#...
0+ [VW] foo
