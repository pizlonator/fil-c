
.function audio_orc_unpack_u8
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 0x80000000
.temp 4 t3

splatbl t3, s1
xorl d1, t3, c1

.function audio_orc_unpack_u8_trunc
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 0x80000000
.const 4 c2 24
.temp 4 t3

splatbl t3, s1
shll t3, t3, c2
xorl d1, t3, c1

.function audio_orc_unpack_s8
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 0x00808080
.temp 2 t2
.temp 4 t3

splatbl t3, s1
xorl d1, t3, c1

.function audio_orc_unpack_s8_trunc
.dest 4 d1 gint32
.source 1 s1 guint8
.const 4 c1 24
.temp 4 t3

splatbl t3, s1
shll d1, t3, c1

.function audio_orc_unpack_u16
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x80000000
.temp 4 t2

mergewl t2, s1, s1
xorl d1, t2, c1

.function audio_orc_unpack_u16_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c2 16
.const 4 c1 0x80000000
.temp 4 t2

mergewl t2, s1, s1
shll t2, t2, c2
xorl d1, t2, c1

.function audio_orc_unpack_s16
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x00008000
.temp 4 t2

mergewl t2, s1, s1
xorl d1, t2, c1

.function audio_orc_unpack_s16_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 16
.temp 4 t2

convuwl t2, s1
shll d1, t2, c1

.function audio_orc_unpack_u16_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 0x80000000
.temp 2 t1
.temp 4 t2

swapw t1, s1
mergewl t2, t1, t1
xorl d1, t2, c1

.function audio_orc_unpack_u16_swap_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c2 16
.const 4 c1 0x80000000
.temp 2 t1
.temp 4 t2

swapw t1, s1
convuwl t2, t1
shll t2, t2, c2
xorl d1, t2, c1

.function audio_orc_unpack_s16_swap
.dest 4 d1 gint32
.source 2 s1 guint8
.temp 2 t1

swapw t1, s1
mergewl d1, t1, t1

.function audio_orc_unpack_s16_swap_trunc
.dest 4 d1 gint32
.source 2 s1 guint8
.const 4 c1 16
.temp 2 t1
.temp 4 t2

swapw t1, s1
convuwl t2, t1
shll d1, t2, c1

.function audio_orc_unpack_u24_32
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c2 8
.const 4 c1 0x80000000
.temp 4 t1

shll t1, s1, c2
xorl d1, t1, c1

.function audio_orc_unpack_s24_32
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 8

shll d1, s1, c1

.function audio_orc_unpack_u24_32_swap
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c2 8
.const 4 c1 0x80000000
.temp 4 t1

swapl t1, s1
shll t1, t1, c2
xorl d1, t1, c1


.function audio_orc_unpack_s24_32_swap
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 8
.temp 4 t1

swapl t1, s1
shll d1, t1, c1


.function audio_orc_unpack_u32
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 0x80000000

xorl d1, s1, c1


.function audio_orc_unpack_u32_swap
.dest 4 d1 gint32
.source 4 s1 guint8
.const 4 c1 0x80000000
.temp 4 t1

swapl t1, s1
xorl d1, t1, c1

.function audio_orc_unpack_s32
.dest 4 d1 gint32
.source 4 s1 guint8

copyl d1, s1

.function audio_orc_unpack_s32_swap
.dest 4 d1 gint32
.source 4 s1 guint8

swapl d1, s1

.function audio_orc_unpack_f32
.dest 8 d1 gdouble
.source 4 s1 gfloat

convfd d1, s1

.function audio_orc_unpack_f32_swap
.dest 8 d1 gdouble
.source 4 s1 gfloat
.temp 4 t1

swapl t1, s1
convfd d1, t1

.function audio_orc_unpack_f64
.dest 8 d1 gdouble
.source 8 s1 gdouble

copyq d1, s1

.function audio_orc_unpack_f64_swap
.dest 8 d1 gdouble
.source 8 s1 gdouble

swapq d1, s1

.function audio_orc_pack_u8
.dest 1 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.temp 4 t1
.temp 2 t2

xorl t1, s1, c1
convhlw t2, t1
convhwb d1, t2

.function audio_orc_pack_s8
.dest 1 d1 guint8
.source 4 s1 gint32
.temp 2 t2

convhlw t2, s1
convhwb d1, t2

.function audio_orc_pack_u16
.dest 2 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.temp 4 t1

xorl t1, s1, c1
convhlw d1, t1

.function audio_orc_pack_s16
.dest 2 d1 guint8
.source 4 s1 gint32

convhlw d1, s1

.function audio_orc_pack_u16_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.temp 4 t1
.temp 2 t2

xorl t1, s1, c1
convhlw t2, t1
swapw d1, t2

.function audio_orc_pack_s16_swap
.dest 2 d1 guint8
.source 4 s1 gint32
.temp 2 t2

convhlw t2, s1
swapw d1, t2

.function audio_orc_pack_u24_32
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.const 4 c2 8
.temp 4 t1

xorl t1, s1, c1
shrul d1, t1, c2


.function audio_orc_pack_s24_32
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 8

shrsl d1, s1, c1


.function audio_orc_pack_u24_32_swap
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.const 4 c2 8
.temp 4 t1

xorl t1, s1, c1
shrul t1, t1, c2
swapl d1, t1


.function audio_orc_pack_s24_32_swap
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 8
.temp 4 t1

shrsl t1, s1, c1
swapl d1, t1


.function audio_orc_pack_u32
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000

xorl d1, s1, c1


.function audio_orc_pack_s32
.dest 4 d1 guint8
.source 4 s1 gint32

copyl d1, s1


.function audio_orc_pack_u32_swap
.dest 4 d1 guint8
.source 4 s1 gint32
.const 4 c1 0x80000000
.temp 4 t1

xorl t1, s1, c1
swapl d1, t1


.function audio_orc_pack_s32_swap
.dest 4 d1 guint8
.source 4 s1 gint32

swapl d1, s1

.function audio_orc_pack_f32
.dest 4 d1 gfloat
.source 8 s1 gdouble

convdf d1, s1

.function audio_orc_pack_f32_swap
.dest 4 d1 gfloat
.source 8 s1 gdouble
.temp 4 t1

convdf t1, s1
swapl d1, t1

.function audio_orc_pack_f64
.dest 8 d1 gdouble
.source 8 s1 gdouble

copyq d1, s1

.function audio_orc_pack_f64_swap
.dest 8 d1 gdouble
.source 8 s1 gdouble

swapq d1, s1

.function audio_orc_splat_u16
.dest 2 d1 guint16
.param 2 p1

copyw d1, p1

.function audio_orc_splat_u32
.dest 4 d1 guint32
.param 4 p1

copyl d1, p1

.function audio_orc_splat_u64
.dest 8 d1 guint64
.param 8 p1

copyq d1, p1

.function audio_orc_int_bias
.dest 4 d1 gint32
.source 4 s1 gint32
.param 4 bias gint32
.param 4 mask gint32
.temp 4 t1

addssl t1, s1, bias
andl d1, t1, mask

.function audio_orc_int_dither
.dest 4 d1 gint32
.source 4 s1 gint32
.source 4 dither gint32
.param 4 mask gint32
.temp 4 t1

addssl t1, s1, dither
andl d1, t1, mask

.function audio_orc_update_rand
.dest 4 r guint32
.temp 4 t

mulll t, r, 1103515245
addl r, t, 12345

.function audio_orc_s32_to_double
.dest 8 d1 gdouble
.source 4 s1 gint32
.temp 8 t1

convld t1, s1
divd d1, t1, 2147483648.0L

.function audio_orc_double_to_s32
.dest 4 d1 gint32
.source 8 s1 gdouble
.temp 8 t1

muld t1, s1, 2147483648.0L
convdl d1, t1

