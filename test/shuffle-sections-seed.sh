#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -ffunction-sections -c -xc -
#include <stdio.h>
int main() {
  printf("Hello world\n");
}
EOF

# Create a lot of sections to lower the probability that
# we get the identical output as a result of shuffling.
for i in `seq 1 1000`; do echo "void fn$i() {}"; done | \
  $CC -o $t/b.o -ffunction-sections -c -xc -

$CC -B. -o $t/exe1 $t/a.o $t/b.o
$QEMU $t/exe1 | grep -q 'Hello world'

$CC -B. -o $t/exe2 $t/a.o $t/b.o -Wl,-shuffle-sections=42
$QEMU $t/exe2 | grep -q 'Hello world'

$CC -B. -o $t/exe3 $t/a.o $t/b.o -Wl,-shuffle-sections=42
$QEMU $t/exe3 | grep -q 'Hello world'

$CC -B. -o $t/exe4 $t/a.o $t/b.o -Wl,-shuffle-sections=5
$QEMU $t/exe4 | grep -q 'Hello world'

! diff $t/exe1 $t/exe2 >& /dev/null || false
diff $t/exe2 $t/exe3
! diff $t/exe3 $t/exe4 >& /dev/null || false
