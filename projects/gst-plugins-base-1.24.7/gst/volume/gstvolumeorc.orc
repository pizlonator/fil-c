
.function volume_orc_scalarmultiply_f64_ns
.dest 8 d1 double
.doubleparam 8 p1

muld d1, d1, p1

.function volume_orc_scalarmultiply_f32_ns
.dest 4 d1 float
.floatparam 4 p1

mulf d1, d1, p1

.function volume_orc_process_int32
.dest 4 d1 gint32
.param 4 p1
.temp 8 t1

mulslq t1, d1, p1
shrsq t1, t1, 27
convql d1, t1

.function volume_orc_process_int32_clamp
.dest 4 d1 gint32
.param 4 p1
.temp 8 t1

mulslq t1, d1, p1
shrsq t1, t1, 27
convsssql d1, t1

.function volume_orc_process_int16
.dest 2 d1 gint16
.param 2 p1
.temp 4 t1

mulswl t1, d1, p1
shrsl t1, t1, 11
convlw d1, t1


.function volume_orc_process_int16_clamp
.dest 2 d1 gint16
.param 2 p1
.temp 4 t1

mulswl t1, d1, p1
shrsl t1, t1, 11
convssslw d1, t1

.function volume_orc_process_int8
.dest 1 d1 gint8
.param 1 p1
.temp 2 t1

mulsbw t1, d1, p1
shrsw t1, t1, 3
convwb d1, t1


.function volume_orc_process_int8_clamp
.dest 1 d1 gint8
.param 1 p1
.temp 2 t1

mulsbw t1, d1, p1
shrsw t1, t1, 3
convssswb d1, t1

.function volume_orc_memset_f64
.dest 8 d1 gdouble
.doubleparam 8 p1

copyq d1, p1

.function volume_orc_prepare_volumes
.dest 8 d1 gdouble
.source 4 s1 gboolean
.temp 8 t1

convld t1, s1
subd t1, 0x3FF0000000000000L, t1
muld d1, d1, t1

.function volume_orc_process_controlled_f64_1ch
.dest 8 d1 gdouble
.source 8 s1 gdouble

muld d1, d1, s1

.function volume_orc_process_controlled_f32_1ch
.dest 4 d1 gfloat
.source 8 s1 gdouble
.temp 4 t1

convdf t1, s1
mulf d1, d1, t1

.function volume_orc_process_controlled_f32_2ch
.dest 8 d1 gfloat
.source 8 s1 gdouble
.temp 4 t1
.temp 8 t2

convdf t1, s1
mergelq t2, t1, t1
x2 mulf d1, d1, t2

.function volume_orc_process_controlled_int32_1ch
.dest 4 d1 gint32
.source 8 s1 gdouble
.temp 8 t1

convld t1, d1
muld t1, t1, s1
convdl d1, t1

.function volume_orc_process_controlled_int16_1ch
.dest 2 d1 gint16
.source 8 s1 gdouble
.temp 4 t1
.temp 4 t2

convswl t1, d1
convlf t1, t1
convdf t2, s1
mulf t1, t1, t2
convfl t1, t1
convssslw d1, t1

.function volume_orc_process_controlled_int16_2ch
.dest 4 d1 gint16
.source 8 s1 gdouble
.temp 8 t1
.temp 4 t2
.temp 8 t3

x2 convswl t1, d1
x2 convlf t1, t1
convdf t2, s1
mergelq t3, t2, t2
x2 mulf t3, t3, t1
x2 convfl t3, t3
x2 convssslw d1, t3

.function volume_orc_process_controlled_int8_1ch
.dest 1 d1 gint8
.source 8 s1 gdouble
.temp 2 t1
.temp 4 t2
.temp 4 t3

convsbw t1, d1
convswl t2, t1
convlf t2, t2
convdf t3, s1
mulf t2, t2, t3
convfl t2, t2
convlw t1, t2
convssswb d1, t1

.function volume_orc_process_controlled_int8_2ch
.dest 2 d1 gint8
.source 8 s1 gdouble
.temp 4 t1
.temp 8 t2
.temp 8 t3

x2 convsbw t1, d1
x2 convswl t2, t1
x2 convlf t2, t2
convdf t1, s1
mergelq t3, t1, t1
x2 mulf t2, t2, t3
x2 convfl t2, t2
x2 convlw t1, t2
x2 convssswb d1, t1

