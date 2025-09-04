tblq z0.s, {z0.b}, z0.b
tblq z31.b, z0.b, z0.b
tblq z0.s, {z0.s, z1.s}, z0.s
tblq z0.s, {z0.s - z1.s}, z0.s
tblq z0.s, {z31.s}, z0.b
tblq z0.b, {z0.b}, {z31.b}
tbxq z0.s, {z0.b}, z0.b
tbxq z31.s, z0.b, z0.h
tbxq z0.s, {z0.s, z1.s}, z0.s
tbxq z0.h, {z0.h - z1.h}, z0.h
tbxq {z0.s}, z31.s, z0.b
tbxq z0.b, {z0.b}, {z31.b}
uzpq1 z0.s, {z0.b}, z0.b
uzpq1 z31.s, z0.b, z0.h
uzpq1 z0.s, {z0.s, z1.s}, z0.s
uzpq1 z0.h, {z0.h - z1.h}, z0.h
uzpq1 {z0.s}, z31.s, z0.b
uzpq1 z0.b, {z0.b}, {z31.b}
uzpq2 z0.s, {z0.b}, z0.b
uzpq2 z31.s, z0.b, z0.h
uzpq2 z0.s, {z0.s, z1.s}, z0.s
uzpq2 z0.h, {z0.h - z1.h}, z0.h
uzpq2 {z0.s}, z31.s, z0.b
uzpq2 z0.b, {z0.b}, {z31.b}
zipq1 z0.s, {z0.b}, z0.b
zipq1 z31.s, z0.b, z0.h
zipq1 z0.s, {z0.s, z1.s}, z0.s
zipq1 z0.h, {z0.b - z1.h}, z0.h
zipq1 {z0.s}, z31.s, z0.b
zipq1 z0.b, {z0.b}, {z31.b}
zipq2 z0.s, {z0.b}, z0.b
zipq2 z31.s, z0.b, z0.h
zipq2 z0.s, {z0.s, z1.s}, z0.s
zipq2 z0.h, {z0.b - z1.h}, z0.h
zipq2 {z0.s}, z31.s, z0.b
zipq2 z0.b, {z0.b}, {z31.b}
