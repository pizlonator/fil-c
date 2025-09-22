# All public include headers should be usable "standalone".

. $srcdir/test-subr.sh

echo '#include "libelf.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf -xc -
echo '#include "gelf.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf -xc -

echo '#include "dwarf.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf \
        -I ${abs_srcdir}/../libdw -xc -
echo '#include "libdw.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf \
        -I ${abs_srcdir}/../libdw -xc -

echo '#include "libdwfl.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf \
    -I ${abs_srcdir}/../libdw -I ${abs_srcdir}/../libdwfl -xc -
echo '#include "libdwelf.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf \
    -I ${abs_srcdir}/../libdw -I ${abs_srcdir}/../libdwelf -xc -

echo '#include "libasm.h"' \
  | ${CC} -c -o /dev/null -I ${abs_srcdir}/../libelf \
    -I ${abs_srcdir}/../libasm -xc -
