#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -xc -
int two() { return 2; }
EOF

head -c 1 /dev/zero >> $t/a.o

cat <<EOF | $CC -o $t/b.o -c -xc -
int three() { return 3; }
EOF

cat <<EOF | $CC -o $t/c.o -c -xc -
#include <stdio.h>

int two();
int three();

int main() {
  printf("%d\n", two() + three());
}
EOF

rm -f $t/d.a
ar rcs $t/d.a $t/a.o $t/b.o

$CC -B. -o $t/exe $t/c.o $t/d.a
