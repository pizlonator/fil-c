.function audiomixer_orc_add_s32
.dest 4 d1 gint32
.source 4 s1 gint32

addssl d1, d1, s1


.function audiomixer_orc_add_s16
.dest 2 d1 gint16
.source 2 s1 gint16

addssw d1, d1, s1


.function audiomixer_orc_add_s8
.dest 1 d1 gint8
.source 1 s1 gint8

addssb d1, d1, s1


.function audiomixer_orc_add_u32
.dest 4 d1 guint32
.source 4 s1 guint32

addusl d1, d1, s1


.function audiomixer_orc_add_u16
.dest 2 d1 guint16
.source 2 s1 guint16

addusw d1, d1, s1


.function audiomixer_orc_add_u8
.dest 1 d1 guint8
.source 1 s1 guint8

addusb d1, d1, s1


.function audiomixer_orc_add_f32
.dest 4 d1 float
.source 4 s1 float

addf d1, d1, s1

.function audiomixer_orc_add_f64
.dest 8 d1 double
.source 8 s1 double

addd d1, d1, s1


.function audiomixer_orc_volume_u8
.dest 1 d1 guint8
.param 1 p1
.const 1 c1 0x80
.temp 2 t1
.temp 1 t2

xorb t2, d1, c1
mulsbw t1, t2, p1
shrsw t1, t1, 3
convssswb t2, t1
xorb d1, t2, c1


.function audiomixer_orc_add_volume_u8
.dest 1 d1 guint8
.source 1 s1 guint8
.param 1 p1
.const 1 c1 0x80
.temp 2 t1
.temp 1 t2

xorb t2, s1, c1
mulsbw t1, t2, p1
shrsw t1, t1, 3
convssswb t2, t1
xorb t2, t2, c1
addusb d1, d1, t2


.function audiomixer_orc_add_volume_s8
.dest 1 d1 gint8
.source 1 s1 gint8
.param 1 p1
.temp 2 t1
.temp 1 t2

mulsbw t1, s1, p1
shrsw t1, t1, 3
convssswb t2, t1
addssb d1, d1, t2


.function audiomixer_orc_add_volume_u16
.dest 2 d1 guint16
.source 2 s1 guint16
.param 2 p1
.const 2 c1 0x8000
.temp 4 t1
.temp 2 t2

xorw t2, s1, c1
mulswl t1, t2, p1
shrsl t1, t1, 11
convssslw t2, t1
xorw t2, t2, c1
addusw d1, d1, t2


.function audiomixer_orc_add_volume_s16
.dest 2 d1 gint16
.source 2 s1 gint16
.param 2 p1
.temp 4 t1
.temp 2 t2

mulswl t1, s1, p1
shrsl t1, t1, 11
convssslw t2, t1
addssw d1, d1, t2


.function audiomixer_orc_add_volume_u32
.dest 4 d1 guint32
.source 4 s1 guint32
.param 4 p1
.const 4 c1 0x80000000
.temp 8 t1
.temp 4 t2

xorl t2, s1, c1
mulslq t1, t2, p1
shrsq t1, t1, 27
convsssql t2, t1
xorl t2, t2, c1
addusl d1, d1, t2


.function audiomixer_orc_add_volume_s32
.dest 4 d1 gint32
.source 4 s1 gint32
.param 4 p1
.temp 8 t1
.temp 4 t2

mulslq t1, s1, p1
shrsq t1, t1, 27
convsssql t2, t1
addssl d1, d1, t2


.function audiomixer_orc_add_volume_f32
.dest 4 d1 float
.source 4 s1 float
.floatparam 4 p1
.temp 4 t1

mulf t1, s1, p1
addf d1, d1, t1


.function audiomixer_orc_add_volume_f64
.dest 8 d1 double
.source 8 s1 double
.doubleparam 8 p1
.temp 8 t1

muld t1, s1, p1
addd d1, d1, t1


