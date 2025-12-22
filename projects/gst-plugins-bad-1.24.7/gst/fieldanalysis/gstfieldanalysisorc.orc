
.function fieldanalysis_orc_same_parity_sad_planar_yuv
.accumulator 4 a1 guint32
.source 1 s1
.source 1 s2
# noise threshold
.param 4 nt
.temp 2 t1
.temp 2 t2
.temp 4 t3
.temp 4 t4

convubw t1, s1
convubw t2, s2
subw t1, t1, t2
absw t1, t1
convuwl t3, t1
cmpgtsl t4, t3, nt
andl t3, t3, t4
accl a1, t3


.function fieldanalysis_orc_same_parity_ssd_planar_yuv
.accumulator 4 a1 guint32
.source 1 s1
.source 1 s2
# noise threshold
.param 4 nt
.temp 2 t1
.temp 2 t2
.temp 4 t3
.temp 4 t4

convubw t1, s1
convubw t2, s2
subw t1, t1, t2
mulswl t3, t1, t1
cmpgtsl t4, t3, nt
andl t3, t3, t4
accl a1, t3


.function fieldanalysis_orc_same_parity_3_tap_planar_yuv
.accumulator 4 a1 guint32
.source 1 s1
.source 1 s2
.source 1 s3
.source 1 s4
.source 1 s5
.source 1 s6
# noise threshold
.param 4 nt
.temp 2 t1
.temp 2 t2
.temp 2 t3
.temp 2 t4
.temp 2 t5
.temp 2 t6
.temp 4 t7
.temp 4 t8

convubw t1, s1
convubw t2, s2
convubw t3, s3
convubw t4, s4
convubw t5, s5
convubw t6, s6
shlw t2, t2, 2
shlw t5, t5, 2
addw t1, t1, t2
addw t1, t1, t3
addw t4, t4, t5
addw t4, t4, t6
subw t1, t1, t4
absw t1, t1
convuwl t7, t1
cmpgtsl t8, t7, nt
andl t7, t7, t8
accl a1, t7


.function fieldanalysis_orc_opposite_parity_5_tap_planar_yuv
.accumulator 4 a1 guint32
.source 1 s1
.source 1 s2
.source 1 s3
.source 1 s4
.source 1 s5
# noise threshold
.param 4 nt
.temp 2 t1
.temp 2 t2
.temp 2 t3
.temp 2 t4
.temp 2 t5
.temp 4 t6
.temp 4 t7

convubw t1, s1
convubw t2, s2
convubw t3, s3
convubw t4, s4
convubw t5, s5
shlw t3, t3, 2
mullw t2, t2, 3
mullw t4, t4, 3
subw t1, t1, t2
addw t1, t1, t3
subw t1, t1, t4
addw t1, t1, t5
absw t1, t1
convuwl t6, t1
cmpgtsl t7, t6, nt
andl t6, t6, t7
accl a1, t6

