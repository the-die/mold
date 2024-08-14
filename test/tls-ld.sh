#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -fPIC -ftls-model=local-dynamic -c -o $t/a.o -xc -
#include <stdio.h>

extern _Thread_local int foo;
static _Thread_local int bar;

int *get_foo_addr() { return &foo; }
int *get_bar_addr() { return &bar; }

int main() {
  bar = 5;

  printf("%d %d %d %d\n", *get_foo_addr(), *get_bar_addr(), foo, bar);
  return 0;
}
EOF

cat <<EOF | $GCC -fPIC -ftls-model=local-dynamic -c -o $t/b.o -xc -
_Thread_local int foo = 3;
EOF

$CC -B. -o $t/exe1 $t/a.o $t/b.o -Wl,-relax
$QEMU $t/exe1 | grep -q '3 5 3 5'

$CC -B. -o $t/exe2 $t/a.o $t/b.o -Wl,-no-relax
$QEMU $t/exe2 | grep -q '3 5 3 5'
