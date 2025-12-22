
.function gaudi_orc_burn
.dest 4 dest guint32
.source 4 src guint32
.param 4 adj gint
.const 1 c255 255
.const 1 c7 7
.const 1 c1 1
.temp 4 tmp guint32
.temp 8 tmp2 guint32
.temp 8 a2 gint

x4 copyb tmp, src                  # tmp <- src
x4 convubw tmp2, tmp               # convert from size 1 to 2

x4 addw a2, tmp2, adj              # a = tmp + adjustment
x4 shruw a2, a2, c1                # a = a / 2
x4 subb tmp, c255, tmp             # tmp = 255 - tmp
#x4 mulubw tmp2, tmp, c127         # tmp = tmp * 127
x4 convubw tmp2, tmp               # convert from size 1 to 2
x4 shlw tmp2, tmp2, c7             # tmp = tmp * 128
x4 divluw tmp2, tmp2, a2           # tmp = tmp / a
x4 subw tmp2, c255, tmp2           # tmp = 255 - tmp

x4 convwb tmp, tmp2                # convert from size 2 to 1
storel dest, tmp
