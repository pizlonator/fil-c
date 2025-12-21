.function video_orc_blend_little
.flags 1d
.dest 4 d guint8
.source 4 s guint8
.temp 4 t
.temp 2 tw
.temp 1 tb
.temp 4 a
.temp 8 d_wide
.temp 8 s_wide
.temp 8 a_wide
.const 4 a_alpha 0x000000ff

loadl t, s
convlw tw, t
convwb tb, tw
splatbl a, tb
x4 convubw a_wide, a
x4 shruw a_wide, a_wide, 8
x4 convubw s_wide, t
loadl t, d
x4 convubw d_wide, t
x4 subw s_wide, s_wide, d_wide
x4 mullw s_wide, s_wide, a_wide
x4 div255w s_wide, s_wide
x4 addw d_wide, d_wide, s_wide
x4 convwb t, d_wide
orl t, t, a_alpha
storel d, t

.function video_orc_blend_big
.flags 1d
.dest 4 d guint8
.source 4 s guint8
.temp 4 t
.temp 4 t2
.temp 2 tw
.temp 1 tb
.temp 4 a
.temp 8 d_wide
.temp 8 s_wide
.temp 8 a_wide
.const 4 a_alpha 0xff000000

loadl t, s
shrul t2, t, 24
convlw tw, t2
convwb tb, tw
splatbl a, tb
x4 convubw a_wide, a
x4 shruw a_wide, a_wide, 8
x4 convubw s_wide, t
loadl t, d
x4 convubw d_wide, t
x4 subw s_wide, s_wide, d_wide
x4 mullw s_wide, s_wide, a_wide
x4 div255w s_wide, s_wide
x4 addw d_wide, d_wide, s_wide
x4 convwb t, d_wide
orl t, t, a_alpha
storel d, t

.function video_orc_unpack_I420
.dest 4 d guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.const 1 c255 255
.temp 2 uv
.temp 2 ay
.temp 1 tu
.temp 1 tv

loadupdb tu, u
loadupdb tv, v
mergebw uv, tu, tv
mergebw ay, c255, y
mergewl d, ay, uv


.function video_orc_pack_I420
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 8 ayuv guint8
.temp 4 ay
.temp 4 uv
.temp 2 uu
.temp 2 vv
.temp 1 t1
.temp 1 t2

x2 splitlw uv, ay, ayuv
x2 select1wb y, ay
x2 splitwb vv, uu, uv
select0wb u, uu
select0wb v, vv

.function video_orc_pack_Y
.dest 1 y guint8
.source 4 ayuv guint8
.temp 2 ay

select0lw ay, ayuv
select1wb y, ay

.function video_orc_pack_YA
.dest 1 y guint8
.dest 1 a guint8
.source 4 ayuv guint8
.temp 2 ay

select0lw ay, ayuv
select1wb y, ay
select0wb a, ay

.function video_orc_unpack_YUY2
.dest 8 ayuv guint8
.source 4 yuy2 guint8
.const 2 c255 0xff
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb uv, yy, yuy2
x2 mergebw ayay, c255, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_pack_YUY2
.dest 4 yuy2 guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
select0lw uv, uvuv
x2 select1wb yy, ayay
x2 mergebw yuy2, yy, uv


.function video_orc_pack_UYVY
.dest 4 yuy2 guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
select0lw uv, uvuv
x2 select1wb yy, ayay
x2 mergebw yuy2, uv, yy


.function video_orc_unpack_UYVY
.dest 8 ayuv guint8
.source 4 uyvy guint8
.const 2 c255 0xff
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb yy, uv, uyvy
x2 mergebw ayay, c255, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_pack_VYUY
.dest 4 vyuy guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 vu
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
select0lw vu, uvuv
x2 select1wb yy, ayay
swapw vu, vu
x2 mergebw vyuy, vu, yy


.function video_orc_unpack_VYUY
.dest 8 ayuv guint8
.source 4 vyuy guint8
.const 2 c255 0xff
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb yy, uv, vyuy
swapw uv, uv
x2 mergebw ayay, c255, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_unpack_YVYU
.dest 8 ayuv guint8
.source 4 uyvy guint8
.const 2 c255 0xff
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb uv, yy, uyvy
swapw uv, uv
x2 mergebw ayay, c255, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_pack_YVYU
.dest 4 yuy2 guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
select0lw uv, uvuv
x2 select1wb yy, ayay
swapw uv, uv
x2 mergebw yuy2, yy, uv


.function video_orc_unpack_YUV9
.dest 8 d guint8
.source 2 y guint8
.source 1 u guint8
.source 1 v guint8
.const 1 c255 255
.temp 2 tuv
.temp 4 ay
.temp 4 uv
.temp 1 tu
.temp 1 tv

loadupdb tu, u
loadupdb tv, v
mergebw tuv, tu, tv
mergewl uv, tuv, tuv
x2 mergebw ay, c255, y
x2 mergewl d, ay, uv


.function video_orc_unpack_Y42B
.dest 8 ayuv guint8
.source 2 yy guint8
.source 1 u guint8
.source 1 v guint8
.const 1 c255 255
.temp 2 uv
.temp 2 ay
.temp 4 uvuv
.temp 4 ayay

mergebw uv, u, v
x2 mergebw ayay, c255, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv

.function video_orc_pack_Y42B
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 8 ayuv guint8
.temp 4 ayay
.temp 4 uvuv
.temp 2 uv

x2 splitlw uvuv, ayay, ayuv
select0lw uv, uvuv
splitwb v, u, uv
x2 select1wb y, ayay


.function video_orc_unpack_Y444
.dest 4 ayuv guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.const 1 c255 255
.temp 2 uv
.temp 2 ay

mergebw uv, u, v
mergebw ay, c255, y
mergewl ayuv, ay, uv


.function video_orc_pack_Y444
.dest 1 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 ayuv guint8
.temp 2 ay
.temp 2 uv

splitlw uv, ay, ayuv
splitwb v, u, uv
select1wb y, ay

.function video_orc_unpack_GRAY8
.dest 4 ayuv guint8
.source 1 y guint8
.const 1 c255 255
.const 2 c0x8080 0x8080
.temp 2 ay

mergebw ay, c255, y
mergewl ayuv, ay, c0x8080


.function video_orc_pack_GRAY8
.dest 1 y guint8
.source 4 ayuv guint8
.temp 2 ay

select0lw ay, ayuv
select1wb y, ay


.function video_orc_unpack_BGRA
.dest 4 argb guint8
.source 4 bgra guint8

swapl argb, bgra

.function video_orc_pack_BGRA
.dest 4 bgra guint8
.source 4 argb guint8

swapl bgra, argb

.function video_orc_pack_RGBA_le
.dest 4 rgba guint8
.source 4 argb guint8
.temp 4 a
.temp 4 r

loadl r, argb
shrul a, r, 8
shll r, r, 24
orl rgba, r, a

.function video_orc_unpack_RGBA_le
.dest 4 argb guint8
.source 4 rgba guint8
.temp 4 a
.temp 4 r

loadl r, rgba
shll a, r, 8
shrul r, r, 24
orl argb, r, a

.function video_orc_pack_RGBA_be
.dest 4 rgba guint8
.source 4 argb guint8
.temp 4 a
.temp 4 r

loadl r, argb
shrul a, r, 24
shll r, r, 8
orl rgba, r, a

.function video_orc_unpack_RGBA_be
.dest 4 argb guint8
.source 4 rgba guint8
.temp 4 a
.temp 4 r

loadl r, rgba
shll a, r, 24
shrul r, r, 8
orl argb, r, a


.function video_orc_unpack_ABGR_le
.dest 4 argb guint8
.source 4 abgr guint8
.temp 4 a
.temp 4 r

swapl r, abgr
shll a, r, 8
shrul r, r, 24
orl argb, r, a

.function video_orc_pack_ABGR_le
.dest 4 abgr guint8
.source 4 argb guint8
.temp 4 a
.temp 4 r

swapl r, argb
shll a, r, 8
shrul r, r, 24
orl abgr, r, a

.function video_orc_unpack_ABGR_be
.dest 4 argb guint8
.source 4 abgr guint8
.temp 4 a
.temp 4 r

swapl r, abgr
shll a, r, 24
shrul r, r, 8
orl argb, r, a

.function video_orc_pack_ABGR_be
.dest 4 abgr guint8
.source 4 argb guint8
.temp 4 a
.temp 4 r

swapl r, argb
shll a, r, 24
shrul r, r, 8
orl abgr, r, a


.function video_orc_unpack_NV12
.dest 8 d guint8
.source 2 y guint8
.source 2 uv guint8
.const 1 c255 255
.temp 4 ay
.temp 4 uvuv

mergewl uvuv, uv, uv
x2 mergebw ay, c255, y
x2 mergewl d, ay, uvuv

.function video_orc_pack_NV12
.dest 2 y guint8
.dest 2 uv guint8
.source 8 ayuv guint8
.temp 4 ay
.temp 4 uvuv

x2 splitlw uvuv, ay, ayuv
x2 select1wb y, ay
select0lw uv, uvuv

.function video_orc_unpack_AV12
.dest 8 d guint8
.source 2 y guint8
.source 2 uv guint8
.source 2 a guint8
.temp 4 ay
.temp 4 uvuv

mergewl uvuv, uv, uv
x2 mergebw ay, a, y
x2 mergewl d, ay, uvuv

.function video_orc_pack_AV12
.dest 2 y guint8
.dest 2 uv guint8
.dest 2 a guint8
.source 8 ayuv guint8
.temp 4 ay
.temp 4 uvuv

x2 splitlw uvuv, ay, ayuv
x2 select1wb y, ay
x2 select0wb a, ay
select0lw uv, uvuv

.function video_orc_unpack_NV21
.dest 8 d guint8
.source 2 y guint8
.source 2 vu guint8
.const 1 c255 255
.temp 2 uv
.temp 4 ay
.temp 4 uvuv

swapw uv, vu
mergewl uvuv, uv, uv
x2 mergebw ay, c255, y
x2 mergewl d, ay, uvuv


.function video_orc_pack_NV21
.dest 2 y guint8
.dest 2 vu guint8
.source 8 ayuv guint8
.temp 4 ay
.temp 4 uvuv
.temp 2 uv

x2 splitlw uvuv, ay, ayuv
x2 select1wb y, ay
select0lw uv, uvuv
swapw vu, uv

.function video_orc_unpack_NV24
.dest 4 d guint8
.source 1 y guint8
.source 2 uv guint8
.const 1 c255 255
.temp 2 ay

mergebw ay, c255, y
mergewl d, ay, uv

.function video_orc_pack_NV24
.dest 1 y guint8
.dest 2 uv guint8
.source 4 ayuv guint8
.temp 2 ay

splitlw uv, ay, ayuv
select1wb y, ay

.function video_orc_unpack_A420
.dest 4 d guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.source 1 a guint8
.temp 2 uv
.temp 2 ay
.temp 1 tu
.temp 1 tv

loadupdb tu, u
loadupdb tv, v
mergebw uv, tu, tv
mergebw ay, a, y
mergewl d, ay, uv

.function video_orc_pack_A420
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.dest 2 a guint8
.source 8 ayuv guint8
.temp 4 ay
.temp 4 uv
.temp 2 uu
.temp 2 vv

x2 splitlw uv, ay, ayuv
x2 select1wb y, ay
x2 select0wb a, ay
x2 splitwb vv, uu, uv
select0wb u, uu
select0wb v, vv

.function video_orc_pack_AY
.dest 1 y guint8
.dest 1 a guint8
.source 4 ayuv guint8
.temp 2 ay

select0lw ay, ayuv
select1wb y, ay
select0wb a, ay


.function video_orc_unpack_VUYA
.dest 4 ayuv guint8
.source 4 vuya guint8

swapl ayuv, vuya


.function video_orc_pack_VUYA
.dest 4 vuya guint8
.source 4 ayuv guint8

swapl vuya, ayuv

.function video_orc_unpack_RGB15_le
.dest 4 argb guint32
.source 2 rgb15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, rgb15
andw r, t, 0x7c00
andw g, t, 0x03e0
andw b, t, 0x001f
shlw b, b, 5
mulhsw r, r, 0x0210
mulhsw g, g, 0x4200
mulhsw b, b, 0x4200
mergewl ag, 0xff, g
mergewl rb, r, b
shll rb, rb, 8
orl argb, ag, rb

.function video_orc_unpack_RGB15_be
.dest 4 argb guint32
.source 2 rgb15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, rgb15
andw r, t, 0x7c00
andw g, t, 0x03e0
andw b, t, 0x001f
shlw b, b, 5
mulhsw r, r, 0x0210
mulhsw g, g, 0x4200
mulhsw b, b, 0x4200
mergewl ag, 0xff, g
mergewl rb, r, b
shll ag, ag, 8
orl argb, ag, rb

.function video_orc_unpack_RGB15_le_trunc
.dest 4 argb guint32
.source 2 rgb15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, rgb15
andw r, t, 0x7c00
andw g, t, 0x03e0
andw b, t, 0x001f
shruw r, r, 7
shruw g, g, 2
shlw b, b, 3
mergewl ag, 0xff, g
mergewl rb, r, b
shll rb, rb, 8
orl argb, ag, rb

.function video_orc_unpack_RGB15_be_trunc
.dest 4 argb guint32
.source 2 rgb15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, rgb15
andw r, t, 0x7c00
andw g, t, 0x03e0
andw b, t, 0x001f
shruw r, r, 7
shruw g, g, 2
shlw b, b, 3
mergewl ag, 0xff, g
mergewl rb, r, b
shll ag, ag, 8
orl argb, ag, rb

.function video_orc_pack_RGB15_le
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf800
andl g, t, 0xf80000
andl b, t, 0xf8000000
shrul r, r, 1
shrul g, g, 14
shrul b, b, 27
orl t2, r, g
orl t2, t2, b
select0lw rgb15, t2

.function video_orc_pack_RGB15_be
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf80000
andl g, t, 0xf800
andl b, t, 0xf8
shrul r, r, 9
shrul g, g, 6
shrul b, b, 3
orl t2, r, g
orl t2, t2, b
select1lw rgb15, t2

.function video_orc_unpack_BGR15_le
.dest 4 argb guint32
.source 2 bgr15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, bgr15
andw b, t, 0x7c00
andw g, t, 0x03e0
andw r, t, 0x001f
shlw r, r, 5
mulhsw b, b, 0x0210
mulhsw g, g, 0x4200
mulhsw r, r, 0x4200
mergewl ag, 0xff, g
mergewl rb, r, b
shll rb, rb, 8
orl argb, ag, rb

.function video_orc_unpack_BGR15_be
.dest 4 argb guint32
.source 2 bgr15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, bgr15
andw b, t, 0x7c00
andw g, t, 0x03e0
andw r, t, 0x001f
shlw r, r, 5
mulhsw b, b, 0x0210
mulhsw g, g, 0x4200
mulhsw r, r, 0x4200
mergewl ag, 0xff, g
mergewl rb, r, b
shll ag, ag, 8
orl argb, ag, rb

.function video_orc_unpack_BGR15_le_trunc
.dest 4 argb guint32
.source 2 bgr15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, bgr15
andw b, t, 0x7c00
andw g, t, 0x03e0
andw r, t, 0x001f
shruw b, b, 7
shruw g, g, 2
shlw r, r, 3
mergewl ag, 0xff, g
mergewl rb, r, b
shll rb, rb, 8
orl argb, ag, rb

.function video_orc_unpack_BGR15_be_trunc
.dest 4 argb guint32
.source 2 bgr15 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ag
.temp 4 rb

loadw t, bgr15
andw b, t, 0x7c00
andw g, t, 0x03e0
andw r, t, 0x001f
shruw b, b, 7
shruw g, g, 2
shlw r, r, 3
mergewl ag, 0xff, g
mergewl rb, r, b
shll ag, ag, 8
orl argb, ag, rb

.function video_orc_pack_BGR15_le
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf800
andl g, t, 0xf80000
andl b, t, 0xf8000000
shrul b, b, 17
shrul g, g, 14
shrul r, r, 11
orl t2, r, g
orl t2, t2, b
select0lw rgb15, t2

.function video_orc_pack_BGR15_be
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf80000
andl g, t, 0xf800
andl b, t, 0xf8
shll b, b, 7
shrul g, g, 6
shrul r, r, 19
orl t2, r, g
orl t2, t2, b
select1lw rgb15, t2

.function video_orc_unpack_RGB16
.dest 4 argb guint32
.source 2 rgb16 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ar
.temp 4 gb
.temp 8 t2

loadw t, rgb16
andw r, t, 0xf800
andw g, t, 0x07e0
andw b, t, 0x001f
shruw r, r, 6
shlw b, b, 5
mulhsw r, r, 0x4200
mulhsw g, g, 0x2080
mulhsw b, b, 0x4200
mergewl ar, 0xff, r
mergewl gb, g, b
mergelq t2, ar, gb
x4 convsuswb argb, t2

.function video_orc_unpack_RGB16_trunc
.dest 4 argb guint32
.source 2 rgb16 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ar
.temp 4 gb
.temp 8 t2

loadw t, rgb16
andw r, t, 0xf800
andw g, t, 0x07e0
andw b, t, 0x001f
shruw r, r, 8
shruw g, g, 3
shlw b, b, 3
mergewl ar, 0xff, r
mergewl gb, g, b
mergelq t2, ar, gb
x4 convsuswb argb, t2

.function video_orc_pack_RGB16_le
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf800
andl g, t, 0xfc0000
andl b, t, 0xf8000000
shrul g, g, 13
shrul b, b, 27
orl t2, r, g
orl t2, t2, b
select0lw rgb15, t2

.function video_orc_pack_RGB16_be
.dest 2 rgb16 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf80000
andl g, t, 0xfc00
andl b, t, 0xf8
shrul r, r, 8
shrul g, g, 5
shrul b, b, 3
orl t2, r, g
orl t2, t2, b
select1lw rgb16, t2

.function video_orc_unpack_BGR16
.dest 4 argb guint32
.source 2 bgr16 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ar
.temp 4 gb
.temp 8 t2

loadw t, bgr16
andw b, t, 0xf800
andw g, t, 0x07e0
andw r, t, 0x001f
shruw b, b, 6
shlw r, r, 5
mulhsw b, b, 0x4200
mulhsw g, g, 0x2080
mulhsw r, r, 0x4200
mergewl ar, 0xff, r
mergewl gb, g, b
mergelq t2, ar, gb
x4 convsuswb argb, t2

.function video_orc_unpack_BGR16_trunc
.dest 4 argb guint32
.source 2 bgr16 guint16
.temp 2 t
.temp 2 r
.temp 2 g
.temp 2 b
.temp 4 ar
.temp 4 gb
.temp 8 t2

loadw t, bgr16
andw b, t, 0xf800
andw g, t, 0x07e0
andw r, t, 0x001f
shruw b, b, 8
shruw g, g, 3
shlw r, r, 3
mergewl ar, 0xff, r
mergewl gb, g, b
mergelq t2, ar, gb
x4 convsuswb argb, t2

.function video_orc_pack_BGR16_le
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf800
andl g, t, 0xfc0000
andl b, t, 0xf8000000
shrul r, r, 11
shrul g, g, 13
shrul b, b, 16
orl t2, r, g
orl t2, t2, b
select0lw rgb15, t2

.function video_orc_pack_BGR16_be
.dest 2 rgb15 guint16
.source 4 argb guint32
.temp 4 t
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl r, t, 0xf80000
andl g, t, 0xfc00
andl b, t, 0xf8
shll b, b, 8
shrul g, g, 5
shrul r, r, 19
orl t2, r, g
orl t2, t2, b
select1lw rgb15, t2

.function video_orc_unpack_RBGA_le
.dest 4 argb guint8
.source 4 rbga guint8
.temp 4 t
.temp 4 a
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, rbga
andl a, t, 0xff000000
andl r, t, 0xff
andl g, t, 0xff0000
andl b, t, 0xff00
shrul a, a, 24
shll r, r, 8
shll b, b, 16
orl t2, a, r
orl t2, t2, g
orl argb, t2, b

.function video_orc_unpack_RBGA_be
.dest 4 argb guint8
.source 4 rbga guint8
.temp 4 t
.temp 4 a
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, rbga
andl a, t, 0xff
andl r, t, 0xff000000
andl g, t, 0xff00
andl b, t, 0xff0000
shll a, a, 24
shrul r, r, 8
shrul b, b, 16
orl t2, a, r
orl t2, t2, g
orl argb, t2, b

.function video_orc_pack_RBGA_le
.dest 4 rbga guint8
.source 4 argb guint8
.temp 4 t
.temp 4 a
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl a, t, 0xff
andl r, t, 0xff00
andl g, t, 0xff0000
andl b, t, 0xff000000
shll a, a, 24
shrul r, r, 8
shrul b, b, 16
orl t2, a, r
orl t2, t2, g
orl rbga, t2, b

.function video_orc_pack_RBGA_be
.dest 4 rbga guint8
.source 4 argb guint8
.temp 4 t
.temp 4 a
.temp 4 r
.temp 4 g
.temp 4 b
.temp 4 t2

loadl t, argb
andl a, t, 0xff000000
andl r, t, 0xff0000
andl g, t, 0xff00
andl b, t, 0xff
shrul a, a, 24
shll r, r, 8
shll b, b, 16
orl t2, a, r
orl t2, t2, g
orl rbga, t2, b

.function video_orc_resample_bilinear_u32
.dest 4 d1 guint8
.source 4 s1 guint8
.param 4 p1
.param 4 p2

ldreslinl d1, s1, p1, p2

.function video_orc_merge_linear_u8
.dest 1 d1
.source 1 s1
.source 1 s2
.param 1 p1
.temp 2 t1
.temp 2 t2
.temp 1 a
.temp 1 t

loadb a, s1
convubw t1, s1
convubw t2, s2
subw t2, t2, t1
mullw t2, t2, p1
addw t2, t2, 128
convhwb t, t2
addb d1, t, a


.function video_orc_memset_2d
.flags 2d
.dest 1 d1 guint8
.param 1 p1

storeb d1, p1

.function video_orc_memcpy_2d
.flags 2d
.dest 1 d1 guint8
.source 1 s1 guint8

copyb d1, s1

.function video_orc_convert_u16_to_u8
.source 2 s guint16
.dest 1 d guint8

convhwb d, s

.function video_orc_convert_u8_to_u16
.source 1 s guint8
.dest 2 d guint16

mergebw d, s, s

.function video_orc_splat_u16
.dest 2 d1 guint8
.param 2 p1

storew d1, p1

.function video_orc_splat_u32
.dest 4 d1 guint8
.param 4 p1

storel d1, p1

.function video_orc_splat_u64
.dest 8 d1 guint8
.longparam 8 p1

storeq d1, p1

.function video_orc_splat2_u64
.dest 8 d1 guint8
.param 4 p1
.temp 4 p

loadpl p, p1
x4 mergebw d1, p, p

.function video_orc_convert_I420_UYVY
.dest 4 d1 guint8
.dest 4 d2 guint8
.source 2 y1 guint8
.source 2 y2 guint8
.source 1 u guint8
.source 1 v guint8
.temp 2 uv

mergebw uv, u, v
x2 mergebw d1, uv, y1
x2 mergebw d2, uv, y2


.function video_orc_convert_I420_YUY2
.dest 4 d1 guint8
.dest 4 d2 guint8
.source 2 y1 guint8
.source 2 y2 guint8
.source 1 u guint8
.source 1 v guint8
.temp 2 uv

mergebw uv, u, v
x2 mergebw d1, y1, uv
x2 mergebw d2, y2, uv



.function video_orc_convert_I420_AYUV
.dest 4 d1 guint8
.dest 4 d2 guint8
.source 1 y1 guint8
.source 1 y2 guint8
.source 1 u guint8
.source 1 v guint8
.param 1 alpha
.temp 2 uv
.temp 2 ay
.temp 1 tu
.temp 1 tv

loadupdb tu, u
loadupdb tv, v
mergebw uv, tu, tv
mergebw ay, alpha, y1
mergewl d1, ay, uv
mergebw ay, alpha, y2
mergewl d2, ay, uv


.function video_orc_convert_YUY2_I420
.dest 2 y1 guint8
.dest 2 y2 guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 yuv1 guint8
.source 4 yuv2 guint8
.temp 2 t1
.temp 2 t2
.temp 2 ty

x2 splitwb t1, ty, yuv1
storew y1, ty
x2 splitwb t2, ty, yuv2
storew y2, ty
x2 avgub t1, t1, t2
splitwb v, u, t1


.function video_orc_convert_UYVY_YUY2
.flags 2d
.dest 4 yuy2 guint8
.source 4 uyvy guint8

x2 swapw yuy2, uyvy


.function video_orc_planar_chroma_420_422
.flags 2d
.dest 1 d1 guint8
.dest 1 d2 guint8
.source 1 s guint8

copyb d1, s
copyb d2, s


.function video_orc_planar_chroma_420_444
.flags 2d
.dest 2 d1 guint8
.dest 2 d2 guint8
.source 1 s guint8
.temp 2 t

splatbw t, s
storew d1, t
storew d2, t


.function video_orc_planar_chroma_422_444
.flags 2d
.dest 2 d1 guint8
.source 1 s guint8
.temp 2 t

splatbw t, s
storew d1, t


.function video_orc_planar_chroma_444_422
.flags 2d
.dest 1 d guint8
.source 2 s guint8
.temp 1 t1
.temp 1 t2

splitwb t1, t2, s
avgub d, t1, t2


.function video_orc_planar_chroma_444_420
.flags 2d
.dest 1 d guint8
.source 2 s1 guint8
.source 2 s2 guint8
.temp 2 t
.temp 1 t1
.temp 1 t2

x2 avgub t, s1, s2
splitwb t1, t2, t
avgub d, t1, t2


.function video_orc_planar_chroma_422_420
.flags 2d
.dest 1 d guint8
.source 1 s1 guint8
.source 1 s2 guint8

avgub d, s1, s2


.function video_orc_convert_YUY2_AYUV
.flags 2d
.dest 8 ayuv guint8
.source 4 yuy2 guint8
.param 1 alpha
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb uv, yy, yuy2
x2 mergebw ayay, alpha, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_convert_UYVY_AYUV
.flags 2d
.dest 8 ayuv guint8
.source 4 uyvy guint8
.param 1 alpha
.temp 2 yy
.temp 2 uv
.temp 4 ayay
.temp 4 uvuv

x2 splitwb yy, uv, uyvy
x2 mergebw ayay, alpha, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_convert_YUY2_Y42B
.flags 2d
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 yuy2 guint8
.temp 2 uv

x2 splitwb uv, y, yuy2
splitwb v, u, uv


.function video_orc_convert_UYVY_Y42B
.flags 2d
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 uyvy guint8
.temp 2 uv

x2 splitwb y, uv, uyvy
splitwb v, u, uv


.function video_orc_convert_YUY2_Y444
.flags 2d
.dest 2 y guint8
.dest 2 uu guint8
.dest 2 vv guint8
.source 4 yuy2 guint8
.temp 2 uv
.temp 1 u
.temp 1 v

x2 splitwb uv, y, yuy2
splitwb v, u, uv
splatbw uu, u
splatbw vv, v


.function video_orc_convert_UYVY_Y444
.flags 2d
.dest 2 y guint8
.dest 2 uu guint8
.dest 2 vv guint8
.source 4 uyvy guint8
.temp 2 uv
.temp 1 u
.temp 1 v

x2 splitwb y, uv, uyvy
splitwb v, u, uv
splatbw uu, u
splatbw vv, v


.function video_orc_convert_UYVY_I420
.dest 2 y1 guint8
.dest 2 y2 guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 yuv1 guint8
.source 4 yuv2 guint8
.temp 2 t1
.temp 2 t2
.temp 2 ty

x2 splitwb ty, t1, yuv1
storew y1, ty
x2 splitwb ty, t2, yuv2
storew y2, ty
x2 avgub t1, t1, t2
splitwb v, u, t1



.function video_orc_convert_AYUV_I420
.flags 2d
.dest 2 y1 guint8
.dest 2 y2 guint8
.dest 1 u guint8
.dest 1 v guint8
.source 8 ayuv1 guint8
.source 8 ayuv2 guint8
.temp 4 ay
.temp 4 uv1
.temp 4 uv2
.temp 4 uv
.temp 2 uu
.temp 2 vv
.temp 1 t1
.temp 1 t2

x2 splitlw uv1, ay, ayuv1
x2 select1wb y1, ay
x2 splitlw uv2, ay, ayuv2
x2 select1wb y2, ay
x4 avgub uv, uv1, uv2
x2 splitwb vv, uu, uv
splitwb t1, t2, uu
avgub u, t1, t2
splitwb t1, t2, vv
avgub v, t1, t2



.function video_orc_convert_AYUV_YUY2
.flags 2d
.dest 4 yuy2 guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 uv1
.temp 2 uv2
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
splitlw uv1, uv2, uvuv
x2 avgub uv1, uv1, uv2
x2 select1wb yy, ayay
x2 mergebw yuy2, yy, uv1


.function video_orc_convert_AYUV_UYVY
.flags 2d
.dest 4 yuy2 guint8
.source 8 ayuv guint8
.temp 2 yy
.temp 2 uv1
.temp 2 uv2
.temp 4 ayay
.temp 4 uvuv

x2 splitlw uvuv, ayay, ayuv
splitlw uv1, uv2, uvuv
x2 avgub uv1, uv1, uv2
x2 select1wb yy, ayay
x2 mergebw yuy2, uv1, yy



.function video_orc_convert_AYUV_Y42B
.flags 2d
.dest 2 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 8 ayuv guint8
.temp 4 ayay
.temp 4 uvuv
.temp 2 uv1
.temp 2 uv2

x2 splitlw uvuv, ayay, ayuv
splitlw uv1, uv2, uvuv
x2 avgub uv1, uv1, uv2
splitwb v, u, uv1
x2 select1wb y, ayay


.function video_orc_convert_AYUV_Y444
.flags 2d
.dest 1 y guint8
.dest 1 u guint8
.dest 1 v guint8
.source 4 ayuv guint8
.temp 2 ay
.temp 2 uv

splitlw uv, ay, ayuv
splitwb v, u, uv
select1wb y, ay


.function video_orc_convert_Y42B_YUY2
.flags 2d
.dest 4 yuy2 guint8
.source 2 y guint8
.source 1 u guint8
.source 1 v guint8
.temp 2 uv

mergebw uv, u, v
x2 mergebw yuy2, y, uv


.function video_orc_convert_Y42B_UYVY
.flags 2d
.dest 4 uyvy guint8
.source 2 y guint8
.source 1 u guint8
.source 1 v guint8
.temp 2 uv

mergebw uv, u, v
x2 mergebw uyvy, uv, y


.function video_orc_convert_Y42B_AYUV
.flags 2d
.dest 8 ayuv guint8
.source 2 yy guint8
.source 1 u guint8
.source 1 v guint8
.param 1 alpha
.temp 2 uv
.temp 2 ay
.temp 4 uvuv
.temp 4 ayay

mergebw uv, u, v
x2 mergebw ayay, alpha, yy
mergewl uvuv, uv, uv
x2 mergewl ayuv, ayay, uvuv


.function video_orc_convert_Y444_YUY2
.flags 2d
.dest 4 yuy2 guint8
.source 2 y guint8
.source 2 u guint8
.source 2 v guint8
.temp 2 uv
.temp 4 uvuv
.temp 2 uv1
.temp 2 uv2

x2 mergebw uvuv, u, v
splitlw uv1, uv2, uvuv
x2 avgub uv, uv1, uv2
x2 mergebw yuy2, y, uv


.function video_orc_convert_Y444_UYVY
.flags 2d
.dest 4 uyvy guint8
.source 2 y guint8
.source 2 u guint8
.source 2 v guint8
.temp 2 uv
.temp 4 uvuv
.temp 2 uv1
.temp 2 uv2

x2 mergebw uvuv, u, v
splitlw uv1, uv2, uvuv
x2 avgub uv, uv1, uv2
x2 mergebw uyvy, uv, y


.function video_orc_convert_Y444_AYUV
.flags 2d
.dest 4 ayuv guint8
.source 1 yy guint8
.source 1 u guint8
.source 1 v guint8
.param 1 alpha
.temp 2 uv
.temp 2 ay

mergebw uv, u, v
mergebw ay, alpha, yy
mergewl ayuv, ay, uv



.function video_orc_convert_AYUV_ARGB
.flags 2d
.dest 4 argb guint8
.source 4 ayuv guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 1 a
.temp 1 y
.temp 1 u
.temp 1 v
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128

x4 subb x, ayuv, c128
splitlw wv, wy, x
splitwb y, a, wy
splitwb v, u, wv

splatbw wy, y
splatbw wu, u
splatbw wv, v

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, a, r

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, g, b
mergewl x, wr, wb
x4 addb argb, x, c128

.function video_orc_convert_AYUV_BGRA
.flags 2d
.dest 4 bgra guint8
.source 4 ayuv guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 1 a
.temp 1 y
.temp 1 u
.temp 1 v
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128

x4 subb x, ayuv, c128
splitlw wv, wy, x
splitwb y, a, wy
splitwb v, u, wv

splatbw wy, y
splatbw wu, u
splatbw wv, v

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, r, a

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, b, g
mergewl x, wb, wr
x4 addb bgra, x, c128


.function video_orc_convert_AYUV_ABGR
.flags 2d
.dest 4 argb guint8
.source 4 ayuv guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 1 a
.temp 1 y
.temp 1 u
.temp 1 v
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128

x4 subb x, ayuv, c128
splitlw wv, wy, x
splitwb y, a, wy
splitwb v, u, wv

splatbw wy, y
splatbw wu, u
splatbw wv, v

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb
mergebw wb, a, b

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wr, g, r
mergewl x, wb, wr
x4 addb argb, x, c128

.function video_orc_convert_AYUV_RGBA
.flags 2d
.dest 4 argb guint8
.source 4 ayuv guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 1 a
.temp 1 y
.temp 1 u
.temp 1 v
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128

x4 subb x, ayuv, c128
splitlw wv, wy, x
splitwb y, a, wy
splitwb v, u, wv

splatbw wy, y
splatbw wu, u
splatbw wv, v

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb
mergebw wb, b, a

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wr, r, g
mergewl x, wr, wb
x4 addb argb, x, c128

.function video_orc_convert_I420_BGRA
.dest 4 argb guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128
.const 4 c4128 128

subb r, y, c128
splatbw wy, r
loadupdb r, u
subb r, r, c128
splatbw wu, r
loadupdb r, v
subb r, r, c128
splatbw wv, r

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, r, 127

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, b, g
mergewl x, wb, wr
x4 addb argb, x, c4128

.function video_orc_convert_I420_ARGB
.dest 4 argb guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 4 x
.const 1 c128 128
.const 4 c4128 128

subb r, y, c128
splatbw wy, r
loadupdb r, u
subb r, r, c128
splatbw wu, r
loadupdb r, v
subb r, r, c128
splatbw wv, r

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, 127, r

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, g, b
mergewl x, wr, wb
x4 addb argb, x, c4128

.function video_orc_convert_A420_ARGB
.dest 4 argb guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.source 1 a guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 1 da
.temp 4 x
.const 1 c128 128

subb r, y, c128
splatbw wy, r
loadupdb r, u
subb r, r, c128
splatbw wu, r
loadupdb r, v
subb r, r, c128
splatbw wv, r
subb da, a, c128

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, da, r

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, g, b
mergewl x, wr, wb
x4 addb argb, x, c128

.function video_orc_convert_A420_BGRA
.dest 4 argb guint8
.source 1 y guint8
.source 1 u guint8
.source 1 v guint8
.source 1 a guint8
.param 2 p1
.param 2 p2
.param 2 p3
.param 2 p4
.param 2 p5
.temp 2 wy
.temp 2 wu
.temp 2 wv
.temp 2 wr
.temp 2 wg
.temp 2 wb
.temp 1 r
.temp 1 g
.temp 1 b
.temp 1 da
.temp 4 x
.const 1 c128 128

subb r, y, c128
splatbw wy, r
loadupdb r, u
subb r, r, c128
splatbw wu, r
loadupdb r, v
subb r, r, c128
splatbw wv, r
subb da, a, c128

mulhsw wy, wy, p1

mulhsw wr, wv, p2
addw wr, wy, wr
convssswb r, wr
mergebw wr, r, da

mulhsw wb, wu, p3
addw wb, wy, wb
convssswb b, wb

mulhsw wg, wu, p4
addw wg, wy, wg
mulhsw wy, wv, p5
addw wg, wg, wy

convssswb g, wg

mergebw wb, b, g
mergewl x, wb, wr
x4 addb argb, x, c128

.function video_orc_matrix8
.backup _custom_video_orc_matrix8
.source 4 argb guint8
.dest 4 ayuv guint8
.longparam 8 p1
.longparam 8 p2
.longparam 8 p3
.longparam 8 p4
.const 1 c128 128
.temp 2 w1
.temp 2 w2
.temp 1 b1
.temp 1 b2
.temp 4 l1
.temp 4 ayuv2
.temp 8 aq
.temp 8 q1
.temp 8 pr1
.temp 8 pr2
.temp 8 pr3

loadpq pr1, p1
loadpq pr2, p2
loadpq pr3, p3

x4 subb l1, argb, c128

select0lw w1, l1
select1lw w2, l1
select0wb b1, w1
select1wb b2, w1

convubw w1, b1
convuwl l1, w1
x4 mergebw aq, l1, l1

splatbl l1, b2
mergelq q1, l1, l1
x4 mulhsw q1, q1, pr1
x4 addw aq, aq, q1

select0wb b1, w2
splatbl l1,b1
mergelq q1, l1, l1
x4 mulhsw q1, q1, pr2
x4 addw aq, aq, q1

select1wb b2, w2
splatbl l1, b2
mergelq q1, l1, l1
x4 mulhsw q1, q1, pr3
x4 addw aq, aq, q1

x4 convssswb ayuv2, aq
x4 addb ayuv, ayuv2, c128

#.function video_orc_resample_h_near_u32
#.source 4 src guint32
#.source 4 idx
#.dest 4 dest guint32
#.temp 4 t
#
#loadidxl t, src, idx
#storel dest, t

.function video_orc_resample_h_near_u32_lq
.dest 4 d1 guint32
.source 4 s1 guint32
.param 4 p1
.param 4 p2

ldresnearl d1, s1, p1, p2

.function video_orc_resample_h_2tap_1u8_lq
.dest 1 d1 guint8
.source 1 s1 guint8
.param 4 p1
.param 4 p2

ldreslinb d1, s1, p1, p2

.function video_orc_resample_h_2tap_4u8_lq
.dest 4 d1 guint32
.source 4 s1 guint32
.param 4 p1
.param 4 p2

ldreslinl d1, s1, p1, p2

.function video_orc_resample_h_2tap_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 2 t1 gint16
.source 2 t2 gint16
.dest 1 d guint8
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
addw w1, w1, 32
shrsw w1, w1, 6
convsuswb d, w1

.function video_orc_resample_h_2tap_u16
.source 2 s1 guint16
.source 2 s2 guint16
.source 2 t1 gint16
.source 2 t2 gint16
.dest 2 d guint16
.temp 4 w1
.temp 4 w2
.temp 4 tl1
.temp 4 tl2

convuwl w1, s1
convswl tl1, t1
mulll w1, w1, tl1
convuwl w2, s2
convswl tl2, t2
mulll w2, w2, tl2
addl w1, w1, w2
addl w1, w1, 4096
shrsl w1, w1, 12
convsuslw d, w1

.function video_orc_resample_v_2tap_u8_lq
.source 1 src1 guint8
.source 1 src2 guint8
.dest 1 dest guint8
.param 2 p1 gint16
.temp 1 t
.temp 2 w1
.temp 2 w2

convubw w1, src1
convubw w2, src2
subw w2, w2, w1
mullw w2, w2, p1
addw w2, w2, 128
convhwb t, w2
addb dest, t, src1

.function video_orc_resample_v_2tap_u16
.source 2 src1 guint16
.source 2 src2 guint16
.dest 2 dest guint16
.param 2 p1 gint16
.temp 4 l1
.temp 4 l2
.temp 4 l3

convuwl l1, src1
convuwl l2, src2
subl l2, l2, l1
convuwl l3, p1
mulll l2, l2, l3
addl l2, l2, 4096
shrsl l2, l2, 12
addl l1, l1, l2
convsuslw dest, l1

.function video_orc_resample_v_2tap_u8
.source 1 s1 guint8
.source 1 s2 guint8
.dest 1 d1 guint8
.param 2 p1 gint16
.temp 1 t
.temp 2 w1
.temp 2 w2
.temp 4 t1
.temp 4 t2

convubw w1, s1
convubw w2, s2
subw w2, w2, w1
mulswl t2, w2, p1
addl t2, t2, 4095
shrsl t2, t2, 12
convlw w2, t2
addw w2, w2, w1
convsuswb d1, w2

.function video_orc_resample_v_4tap_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 1 s4 guint8
.dest 1 d1 guint8
.param 2 p1 gint16
.param 2 p2 gint16
.param 2 p3 gint16
.param 2 p4 gint16
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, p1
convubw w2, s2
mullw w2, w2, p2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, p3
addw w1, w1, w2
convubw w2, s4
mullw w2, w2, p4
addw w1, w1, w2
addw w1, w1, 32
shrsw w1, w1, 6
convsuswb d1, w1

.function video_orc_resample_v_4tap_u8
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 1 s4 guint8
.dest 1 d1 guint8
.param 2 p1 gint16
.param 2 p2 gint16
.param 2 p3 gint16
.param 2 p4 gint16
.temp 2 w1
.temp 2 w2
.temp 4 t1
.temp 4 t2

convubw w1, s1
mulswl t1, w1, p1
convubw w2, s2
mulswl t2, w2, p2
addl t1, t1, t2
convubw w2, s3
mulswl t2, w2, p3
addl t1, t1, t2
convubw w2, s4
mulswl t2, w2, p4
addl t1, t1, t2
addl t1, t1, 4095
shrsl t1, t1, 12
convsuslw w1, t1
convsuswb d1, w1

# crashes ORC for now but is potentially faster
#.function video_orc_resample_h_4tap_u8
#.source 1 s1 guint8
#.source 1 s2 guint8
#.source 1 s3 guint8
#.source 1 s4 guint8
#.source 2 t1 gint16
#.source 2 t2 gint16
#.source 2 t3 gint16
#.source 2 t4 gint16
#.dest 1 d1 guint8
#.temp 2 w1
#.temp 2 w2
#.temp 4 l1
#.temp 4 l2
#
#convubw w1, s1
#mulswl l1, w1, t1
#convubw w2, s2
#mulswl l2, w2, t2
#addl l1, l1, l2
#convubw w2, s3
#mulswl l2, w2, t3
#addl l1, l1, l2
#convubw w2, s4
#mulswl l2, w2, t4
#addl l1, l1, l2
#addl l1, l1, 4095
#shrsl l1, l1, 12
#convsuslw w1, l1
#convsuswb d1, w1

.function video_orc_resample_h_multaps_u8
.source 1 s guint8
.source 2 t gint16
.dest 4 d gint32
.temp 2 w1

convubw w1, s
mulswl d, w1, t

.function video_orc_resample_h_muladdtaps_u8
.flags 2d
.source 1 s guint8
.source 2 t gint16
.dest 4 d gint32
.temp 2 w1
.temp 4 t1

convubw w1, s
mulswl t1, w1, t
addl d, d, t1

.function video_orc_resample_scaletaps_u8
.source 4 s gint32
.dest 1 d guint8
.temp 2 w1
.temp 4 t1

addl t1, s, 4095
shrsl t1, t1, 12
convsuslw w1, t1
convsuswb d, w1

.function video_orc_resample_h_multaps_u8_lq
.source 1 s guint8
.source 2 t gint16
.dest 2 d gint16
.temp 2 w1

convubw w1, s
mullw d, w1, t

.function video_orc_resample_h_muladdtaps_u8_lq
.flags 2d
.source 1 s guint8
.source 2 t gint16
.dest 2 d gint16
.temp 2 w1

convubw w1, s
mullw w1, w1, t
addw d, d, w1

.function video_orc_resample_h_multaps3_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 2 t1 gint16
.source 2 t2 gint16
.source 2 t3 gint16
.dest 2 d gint16
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw d, w1, w2

.function video_orc_resample_h_muladdtaps3_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 2 t1 gint16
.source 2 t2 gint16
.source 2 t3 gint16
.dest 2 d gint16
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw w1, w1, w2
addw d, d, w1

.function video_orc_resample_h_muladdscaletaps3_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 2 t1 gint16
.source 2 t2 gint16
.source 2 t3 gint16
.source 2 temp gint16
.dest 1 d guint8
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw w1, w1, w2
addw w1, w1, temp
addw w1, w1, 32
shrsw w1, w1, 6
convsuswb d, w1

.function video_orc_resample_scaletaps_u8_lq
.source 2 s gint16
.dest 1 d guint8
.temp 2 w1

addw w1, s, 32
shrsw w1, w1, 6
convsuswb d, w1

.function video_orc_resample_h_multaps_u16
.source 2 s guint16
.source 2 t gint16
.dest 4 d gint32
.temp 4 l1
.temp 4 l2

convuwl l1, s
convswl l2, t
mulll d, l1, l2

.function video_orc_resample_h_muladdtaps_u16
.flags 2d
.source 2 s guint16
.source 2 t gint16
.dest 4 d gint32
.temp 4 l1
.temp 4 l2

convuwl l1, s
convswl l2, t
mulll l1, l1, l2
addl d, d, l1

.function video_orc_resample_scaletaps_u16
.source 4 s gint32
.dest 2 d guint16
.temp 4 t1

addl t1, s, 4095
shrsl t1, t1, 12
convsuslw d, t1

.function video_orc_resample_v_multaps_u8
.source 1 s guint8
.param 2 t gint16
.dest 4 d gint32
.temp 2 w1

convubw w1, s
mulswl d, w1, t

.function video_orc_resample_v_muladdtaps_u8
.source 1 s guint8
.param 2 t gint16
.dest 4 d gint32
.temp 2 w1
.temp 4 t1

convubw w1, s
mulswl t1, w1, t
addl d, d, t1

.function video_orc_resample_v_multaps_u16
.source 2 s guint16
.param 2 t gint16
.dest 4 d gint32
.temp 4 l1

convuwl l1, s
mulll d, l1, t

.function video_orc_resample_v_muladdtaps_u16
.source 2 s guint16
.param 2 t gint16
.dest 4 d gint32
.temp 4 t1
.temp 4 t2

convuwl t1, s
convswl t2, t
mulll t1, t1, t2
addl d, d, t1

.function video_orc_resample_v_multaps_u8_lq
.source 1 s guint8
.param 2 t gint16
.dest 2 d gint16
.temp 2 w1

convubw w1, s
mullw d, w1, t

.function video_orc_resample_v_multaps4_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 1 s4 guint8
.param 2 t1 gint16
.param 2 t2 gint16
.param 2 t3 gint16
.param 2 t4 gint16
.dest 2 d gint16
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw w1, w1, w2
convubw w2, s4
mullw w2, w2, t4
addw d, w1, w2

.function video_orc_resample_v_muladdtaps_u8_lq
.source 1 s guint8
.param 2 t gint16
.dest 2 d gint16
.temp 2 w1

convubw w1, s
mullw w1, w1, t
addw d, d, w1

.function video_orc_resample_v_muladdtaps4_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 1 s4 guint8
.param 2 t1 gint16
.param 2 t2 gint16
.param 2 t3 gint16
.param 2 t4 gint16
.dest 2 d gint16
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw w1, w1, w2
convubw w2, s4
mullw w2, w2, t4
addw w1, w1, w2
addw d, d, w1

.function video_orc_resample_v_muladdscaletaps4_u8_lq
.source 1 s1 guint8
.source 1 s2 guint8
.source 1 s3 guint8
.source 1 s4 guint8
.source 2 temp gint16
.param 2 t1 gint16
.param 2 t2 gint16
.param 2 t3 gint16
.param 2 t4 gint16
.dest 1 d guint8
.temp 2 w1
.temp 2 w2

convubw w1, s1
mullw w1, w1, t1
convubw w2, s2
mullw w2, w2, t2
addw w1, w1, w2
convubw w2, s3
mullw w2, w2, t3
addw w1, w1, w2
convubw w2, s4
mullw w2, w2, t4
addw w1, w1, w2
addw w1, w1, temp
addw w1, w1, 32
shrsw w1, w1, 6
convsuswb d, w1

.function video_orc_chroma_down_h2_u8
.source 8 s guint8
.dest 8 d guint8
.temp 4 ayuv1
.temp 4 ayuv2
.temp 2 ay1
.temp 2 uv1
.temp 2 uv2

splitql ayuv2, ayuv1, s
splitlw uv1, ay1, ayuv1
select1lw uv2, ayuv2
x2 avgub uv1, uv1, uv2
mergewl ayuv1, ay1, uv1
mergelq d, ayuv1, ayuv2

#.function video_orc_chroma_up_h2_cs_u8
#.source 8 s guint8
#.source 4 s1 guint8
#.dest 8 d guint8
#.temp 4 ayuv1
#.temp 4 ayuv2
#.temp 4 ayuv3
#.temp 2 ay2
#.temp 2 uv2
#.temp 2 uv3
#
#splitql ayuv2, ayuv1, s
#ldresnearl ayuv3, s1, 0x20000, 0x20000
#splitlw uv2, ay2, ayuv2
#select1lw uv3, ayuv3
#x2 avgub uv2, uv2, uv3
#mergewl ayuv2, ay2, uv2
#mergelq d, ayuv1, ayuv2

.function video_orc_chroma_down_v2_u8
.source 4 s1 guint8
.source 4 s2 guint8
.dest 4 d guint8
.temp 2 ay1
.temp 2 uv1
.temp 2 uv2

splitlw uv1, ay1, s1
select1lw uv2, s2
x2 avgub uv1, uv1, uv2
mergewl d, ay1, uv1

.function video_orc_chroma_up_v2_u8
.source 4 s1 guint8
.source 4 s2 guint8
.dest 4 d1 guint8
.dest 4 d2 guint8
.temp 2 ay1
.temp 2 ay2
.temp 2 uv1
.temp 2 uv2
.temp 4 uuvv1
.temp 4 uuvv2
.temp 4 uuvv3

splitlw uv1, ay1, s1
splitlw uv2, ay2, s2
x2 convubw uuvv1, uv1
x2 convubw uuvv2, uv2

x2 mullw uuvv3, uuvv1, 3
x2 addw uuvv3, uuvv3, uuvv2
x2 addw uuvv3, uuvv3, 2
x2 shruw uuvv3, uuvv3, 2
x2 convsuswb uv1, uuvv3
mergewl d1, ay1, uv1

x2 mullw uuvv3, uuvv2, 3
x2 addw uuvv3, uuvv3, uuvv1
x2 addw uuvv3, uuvv3, 2
x2 shruw uuvv3, uuvv3, 2
x2 convsuswb uv2, uuvv3
mergewl d2, ay2, uv2

.function video_orc_chroma_up_v2_u16
.source 8 s1 guint16
.source 8 s2 guint16
.dest 8 d1 guint16
.dest 8 d2 guint16
.temp 4 ay1
.temp 4 ay2
.temp 4 uv1
.temp 4 uv2
.temp 8 uuvv1
.temp 8 uuvv2
.temp 8 uuvv3

splitql uv1, ay1, s1
splitql uv2, ay2, s2
x2 convuwl uuvv1, uv1
x2 convuwl uuvv2, uv2

x2 mulll uuvv3, uuvv1, 3
x2 addl uuvv3, uuvv3, uuvv2
x2 addl uuvv3, uuvv3, 2
x2 shrul uuvv3, uuvv3, 2
x2 convsuslw uv1, uuvv3
mergelq d1, ay1, uv1

x2 mulll uuvv3, uuvv2, 3
x2 addl uuvv3, uuvv3, uuvv1
x2 addl uuvv3, uuvv3, 2
x2 shrul uuvv3, uuvv3, 2
x2 convsuslw uv2, uuvv3
mergelq d2, ay2, uv2

.function video_orc_chroma_down_v2_u16
.source 8 s1 guint16
.source 8 s2 guint16
.dest 8 d guint16
.temp 4 ay1
.temp 4 uv1
.temp 4 uv2

splitql uv1, ay1, s1
select1ql uv2, s2
x2 avguw uv1, uv1, uv2
mergelq d, ay1, uv1


.function video_orc_chroma_down_v4_u8
.source 4 s1 guint8
.source 4 s2 guint8
.source 4 s3 guint8
.source 4 s4 guint8
.dest 4 d guint8
.temp 2 ay1
.temp 2 uv1
.temp 4 uuvv1
.temp 4 uuvv2
.temp 4 uuvv3

splitlw uv1, ay1, s1
x2 convubw uuvv1, uv1
select1lw uv1, s4
x2 convubw uuvv2, uv1
x2 addw uuvv3, uuvv1, uuvv2
select1lw uv1, s2
x2 convubw uuvv1, uv1
select1lw uv1, s3
x2 convubw uuvv2, uv1
x2 addw uuvv1, uuvv1, uuvv2
x2 shlw uuvv2, uuvv1, 1
x2 addw uuvv1, uuvv1, uuvv2
x2 addw uuvv3, uuvv3, uuvv1
x2 addw uuvv3, uuvv3, 4
x2 shruw uuvv3, uuvv3, 3
x2 convsuswb uv1, uuvv3
mergewl d, ay1, uv1

.function video_orc_chroma_down_v4_u16
.source 8 s1 guint16
.source 8 s2 guint16
.source 8 s3 guint16
.source 8 s4 guint16
.dest 8 d guint16
.temp 4 ay1
.temp 4 uv1
.temp 8 uuvv1
.temp 8 uuvv2
.temp 8 uuvv3

splitql uv1, ay1, s1
x2 convuwl uuvv1, uv1
select1ql uv1, s4
x2 convuwl uuvv2, uv1
x2 addl uuvv3, uuvv1, uuvv2
select1ql uv1, s2
x2 convuwl uuvv1, uv1
select1ql uv1, s3
x2 convuwl uuvv2, uv1
x2 addl uuvv1, uuvv1, uuvv2
x2 shll uuvv2, uuvv1, 1
x2 addl uuvv1, uuvv1, uuvv2
x2 addl uuvv3, uuvv3, uuvv1
x2 addl uuvv3, uuvv3, 4
x2 shrul uuvv3, uuvv3, 3
x2 convsuslw uv1, uuvv3
mergelq d, ay1, uv1

.function video_orc_dither_none_4u8_mask
.dest 4 p guint8
.param 4 masks
.temp 4 m

loadpl m, masks
x4 andnb p, m, p

.function video_orc_dither_none_4u16_mask
.dest 8 p guint16
.longparam 8 masks
.temp 8 m

loadpq m, masks
x4 andnw p, m, p

.function video_orc_dither_verterr_4u8_mask
.dest 4 p guint8
.dest 8 e guint16
.longparam 8 masks
.temp 8 m
.temp 8 t1

loadpq m, masks
x4 convubw t1, p
x4 addw t1, e, t1
x4 andw e, m, t1
x4 andnw t1, m, t1
x4 convsuswb p, t1

.function video_orc_dither_fs_muladd_u8
.dest 2 e guint16
.temp 2 t1
.temp 2 t2

loadoffw t2, e, 4
mullw t2, t2, 5
addw t1, t2, e
loadoffw t2, e, 8
mullw t2, t2, 3
addw e, t1, t2

# due to error propagation we should disable
# loop_shift for this function and only work on
# 4 pixels at a time.
#.function video_orc_dither_fs_add_4u8_mask
#.flags no-unroll
#.dest 4 d guint8
#.dest 8 e1 guint16
#.dest 8 e2 guint16
#.longparam 8 masks
#.temp 8 p
#.temp 8 t1
#.temp 8 t2
#
#x4 mullw t1, e1, 7
#x4 addw t1, t1, e2
#x4 shruw t1, t1, 4
#x4 convubw p, d
#x4 addw t1, t1, p
#x4 andnw p, masks, t1
#x4 convsuswb d, p
#x4 andw e2, t1, masks

.function video_orc_dither_ordered_u8
.source 1 e guint8
.dest 1 d guint8

addusb d, d, e

.function video_orc_dither_ordered_4u8_mask
.source 8 e1 guint16
.dest 4 d guint8
.longparam 8 masks
.temp 8 p
.temp 8 m

loadpq m, masks
x4 convubw p, d
x4 addw p, p, e1
x4 andnw p, m, p
x4 convsuswb d, p

.function video_orc_dither_ordered_4u16_mask
.source 8 e1 guint16
.dest 8 d guint16
.longparam 8 masks
.temp 8 p
.temp 8 m

loadpq m, masks
x4 addusw p, d, e1
x4 andnw d, m, p

.function video_orc_convert_UYVY_GRAY8
.flags 2d
.dest 1 d guint8
.source 2 s
.temp 1 t1
.temp 2 t2

loadw t2, s
convhwb t1, t2
storeb d, t1
