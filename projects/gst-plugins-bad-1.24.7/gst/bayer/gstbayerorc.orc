

.function bayer_orc_horiz_upsample_unaligned
.dest 2 d0 guint8
.dest 2 d1 guint8
.source 2 s guint8
.temp 2 t
.temp 1 b
.temp 1 c
.temp 1 d
.temp 1 e

splitwb c, b, s
loadoffw t, s, 1
splitwb e, d, t
avgub e, c, e
mergebw d0, c, e
avgub b, b, d
mergebw d1, b, d


.function bayer_orc_horiz_upsample
.dest 2 d0 guint8
.dest 2 d1 guint8
.source 2 s guint8
.temp 2 t
.temp 1 b
.temp 1 c
.temp 1 d
.temp 1 e

loadoffw t, s, -1
select1wb b, t
splitwb d, c, s
loadoffw t, s, 1
select0wb e, t
avgub e, c, e
mergebw d0, c, e
avgub b, b, d
mergebw d1, b, d


.function bayer_orc_merge_bg_bgra
.dest 8 d guint8
.source 2 g0 guint8
.source 2 r0 guint8
.source 2 b1 guint8
.source 2 g1 guint8
.source 2 g2 guint8
.source 2 r2 guint8
.temp 4 ra
.temp 4 bg
.temp 2 r
.temp 2 g
.temp 2 t

x2 avgub r, r0, r2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 255
andw t, t, 65280
orw g, t, g
x2 mergebw bg, b1, g
x2 mergebw ra, r, 255
x2 mergewl d, bg, ra


.function bayer_orc_merge_gr_bgra
.dest 8 d guint8
.source 2 b0 guint8
.source 2 g0 guint8
.source 2 g1 guint8
.source 2 r1 guint8
.source 2 b2 guint8
.source 2 g2 guint8
.temp 4 ra
.temp 4 bg
.temp 2 b
.temp 2 g
.temp 2 t

x2 avgub b, b0, b2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 65280
andw t, t, 255
orw g, t, g
x2 mergebw bg, b, g
x2 mergebw ra, r1, 255
x2 mergewl d, bg, ra


.function bayer_orc_merge_bg_abgr
.dest 8 d guint8
.source 2 g0 guint8
.source 2 r0 guint8
.source 2 b1 guint8
.source 2 g1 guint8
.source 2 g2 guint8
.source 2 r2 guint8
.temp 4 ab
.temp 4 gr
.temp 2 r
.temp 2 g
.temp 2 t

x2 avgub r, r0, r2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 255
andw t, t, 65280
orw g, t, g
x2 mergebw ab, 255, b1
x2 mergebw gr, g, r
x2 mergewl d, ab, gr


.function bayer_orc_merge_gr_abgr
.dest 8 d guint8
.source 2 b0 guint8
.source 2 g0 guint8
.source 2 g1 guint8
.source 2 r1 guint8
.source 2 b2 guint8
.source 2 g2 guint8
.temp 4 ab
.temp 4 gr
.temp 2 b
.temp 2 g
.temp 2 t

x2 avgub b, b0, b2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 65280
andw t, t, 255
orw g, t, g
x2 mergebw ab, 255, b
x2 mergebw gr, g, r1
x2 mergewl d, ab, gr


.function bayer_orc_merge_bg_rgba
.dest 8 d guint8
.source 2 g0 guint8
.source 2 r0 guint8
.source 2 b1 guint8
.source 2 g1 guint8
.source 2 g2 guint8
.source 2 r2 guint8
.temp 4 rg
.temp 4 ba
.temp 2 r
.temp 2 g
.temp 2 t

x2 avgub r, r0, r2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 255
andw t, t, 65280
orw g, t, g
x2 mergebw rg, r, g
x2 mergebw ba, b1, 255
x2 mergewl d, rg, ba


.function bayer_orc_merge_gr_rgba
.dest 8 d guint8
.source 2 b0 guint8
.source 2 g0 guint8
.source 2 g1 guint8
.source 2 r1 guint8
.source 2 b2 guint8
.source 2 g2 guint8
.temp 4 rg
.temp 4 ba
.temp 2 b
.temp 2 g
.temp 2 t

x2 avgub b, b0, b2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 65280
andw t, t, 255
orw g, t, g
x2 mergebw rg, r1, g
x2 mergebw ba, b, 255
x2 mergewl d, rg, ba


.function bayer_orc_merge_bg_argb
.dest 8 d guint8
.source 2 g0 guint8
.source 2 r0 guint8
.source 2 b1 guint8
.source 2 g1 guint8
.source 2 g2 guint8
.source 2 r2 guint8
.temp 4 ar
.temp 4 gb
.temp 2 r
.temp 2 g
.temp 2 t

x2 avgub r, r0, r2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 255
andw t, t, 65280
orw g, t, g
x2 mergebw ar, 255, r
x2 mergebw gb, g, b1
x2 mergewl d, ar, gb


.function bayer_orc_merge_gr_argb
.dest 8 d guint8
.source 2 b0 guint8
.source 2 g0 guint8
.source 2 g1 guint8
.source 2 r1 guint8
.source 2 b2 guint8
.source 2 g2 guint8
.temp 4 ar
.temp 4 gb
.temp 2 b
.temp 2 g
.temp 2 t

x2 avgub b, b0, b2
x2 avgub g, g0, g2
copyw t, g1
x2 avgub g, g, t
andw g, g, 65280
andw t, t, 255
orw g, t, g
x2 mergebw ar, 255, r1
x2 mergebw gb, g, b
x2 mergewl d, ar, gb


# 10..16 bit bayer handling
.function bayer16_orc_horiz_upsample_le
.dest 4 d0 guint16
.dest 4 d1 guint16
.source 4 s guint16
.temp 4 t

.temp 2 b
.temp 2 c
.temp 2 d
.temp 2 e

splitlw c, b, s
loadoffl t, s, 1
splitlw e, d, t
avguw e, c, e
mergewl d0, c, e
avguw b, b, d
mergewl d1, b, d

.function bayer16_orc_horiz_upsample_be
.dest 4 d0 guint16
.dest 4 d1 guint16
.source 4 s guint16
.temp 4 t

.temp 2 b
.temp 2 c
.temp 2 d
.temp 2 e

splitlw c, b, s
swapw b, b
swapw c, c
loadoffl t, s, 1
splitlw e, d, t
swapw d, d
swapw e, e
avguw e, c, e
mergewl d0, c, e
avguw b, b, d
mergewl d1, b, d

.function bayer16_orc_merge_bg_bgra
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 g0 guint8
.source 4 r0 guint8
.source 4 b1 guint8
.source 4 g1 guint8
.source 4 g2 guint8
.source 4 r2 guint8
.temp 4 r
.temp 4 g
.temp 4 t


x2 avguw r, r0, r2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 65535
andl t, t, 4294901760
orl g, t, g
x2 mergewl d1, b1, g
x2 mergewl d2, r, 65535


.function bayer16_orc_merge_gr_bgra
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 b0 guint8
.source 4 g0 guint8
.source 4 g1 guint8
.source 4 r1 guint8
.source 4 b2 guint8
.source 4 g2 guint8
.temp 4 b
.temp 4 g
.temp 4 t


x2 avguw b, b0, b2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 4294901760
andl t, t, 65535
orl g, t, g
x2 mergewl d1, b, g
x2 mergewl d2, r1, 65535


.function bayer16_orc_merge_bg_abgr
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 g0 guint8
.source 4 r0 guint8
.source 4 b1 guint8
.source 4 g1 guint8
.source 4 g2 guint8
.source 4 r2 guint8
.temp 4 r
.temp 4 g
.temp 4 t


x2 avguw r, r0, r2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 65535
andl t, t, 4294901760
orl g, t, g
x2 mergewl d1, 65535, b1
x2 mergewl d2, g, r


.function bayer16_orc_merge_gr_abgr
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 b0 guint8
.source 4 g0 guint8
.source 4 g1 guint8
.source 4 r1 guint8
.source 4 b2 guint8
.source 4 g2 guint8
.temp 4 b
.temp 4 g
.temp 4 t


x2 avguw b, b0, b2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 4294901760
andl t, t, 65535
orl g, t, g
x2 mergewl d1, 65535, b
x2 mergewl d2, g, r1


.function bayer16_orc_merge_bg_rgba
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 g0 guint8
.source 4 r0 guint8
.source 4 b1 guint8
.source 4 g1 guint8
.source 4 g2 guint8
.source 4 r2 guint8
.temp 4 r
.temp 4 g
.temp 4 t


x2 avguw r, r0, r2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 65535
andl t, t, 4294901760
orl g, t, g
x2 mergewl d1, r, g
x2 mergewl d2, b1, 65535


.function bayer16_orc_merge_gr_rgba
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 b0 guint8
.source 4 g0 guint8
.source 4 g1 guint8
.source 4 r1 guint8
.source 4 b2 guint8
.source 4 g2 guint8
.temp 4 b
.temp 4 g
.temp 4 t


x2 avguw b, b0, b2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 4294901760
andl t, t, 65535
orl g, t, g
x2 mergewl d1, r1, g
x2 mergewl d2, b, 65535


.function bayer16_orc_merge_bg_argb
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 g0 guint8
.source 4 r0 guint8
.source 4 b1 guint8
.source 4 g1 guint8
.source 4 g2 guint8
.source 4 r2 guint8
.temp 4 r
.temp 4 g
.temp 4 t


x2 avguw r, r0, r2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 65535
andl t, t, 4294901760
orl g, t, g
x2 mergewl d1, 65535, r
x2 mergewl d2, g, b1


.function bayer16_orc_merge_gr_argb
.dest 8 d1 guint16
.dest 8 d2 guint16
.source 4 b0 guint8
.source 4 g0 guint8
.source 4 g1 guint8
.source 4 r1 guint8
.source 4 b2 guint8
.source 4 g2 guint8
.temp 4 b
.temp 4 g
.temp 4 t


x2 avguw b, b0, b2
x2 avguw g, g0, g2
copyl t, g1
x2 avguw g, g, t
andl g, g, 4294901760
andl t, t, 65535
orl g, t, g
x2 mergewl d1, 65535, r1
x2 mergewl d2, g, b


.function bayer16to16_orc_reorder
.dest 8 d guint8
.source 4 s1 guint32
.source 4 s2 guint32
.param 4 shift
.temp 4 u
.temp 4 v
.temp 8 q

x2 muluwl q, s1, 0xffff
x2 shrul q, q, shift
x2 convuuslw u, q
x2 muluwl q, s2, 0xffff
x2 shrul q, q, shift
x2 convuuslw v, q
mergelq d, u, v

.function bayer16to8_orc_reorder
.dest 4 d guint8
.source 4 s1 guint32
.source 4 s2 guint32
.param 4 shift
.temp 2 u
.temp 2 v
.temp 4 l

x2 shruw l, s1, shift
x2 convuuswb u, l
x2 shruw l, s2, shift
x2 convuuswb v, l
mergewl d, u, v

.function bayer8to16_orc_reorder
.dest 8 d guint8
.source 4 s guint32

x4 splatbw d, s
