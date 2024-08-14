#!/bin/bash
. $(dirname $0)/common.inc

[ "$CC" = cc ] || skip

echo 'int main() {}' | $CC -flto -o /dev/null -xc - >& /dev/null \
  || skip

cat <<EOF | $CC -o $t/a.o -c -flto -xc -
#include <stdio.h>
void hello() {
  printf("Hello world\n");
}
EOF

cat <<EOF | $CC -o $t/b.o -c -flto -xc -
#include <stdio.h>
void howdy() {
  printf("Hello world\n");
}
EOF

rm -f $t/c.a
ar rc $t/c.a $t/a.o $t/b.o

cat <<EOF | $CC -o $t/d.o -c -flto -xc -
void hello();
int main() {
  hello();
}
EOF

$CC -B. -o $t/exe -flto $t/d.o $t/c.a
$QEMU $t/exe | grep -q 'Hello world'

nm $t/exe > $t/log
grep -q hello $t/log
! grep -q howdy $t/log || false
