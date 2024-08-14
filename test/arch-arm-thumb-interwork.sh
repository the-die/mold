#!/bin/bash
. $(dirname $0)/common.inc

echo 'int foo() { return 0; }' | $CC -o /dev/null -c -xc - -mthumb 2> /dev/null || skip

cat <<EOF | $CC -o $t/a.o -c -xc - -mthumb
#include <stdio.h>
int bar();
int foo() {
  printf(" foo");
  bar();
}
EOF

cat <<EOF | $CC -o $t/b.o -c -xc - -marm
#include <stdio.h>
int bar() {
  printf(" bar\n");
}

int foo();

int main() {
  printf("main");
  foo();
}
EOF

$CC -B. -o $t/exe $t/a.o $t/b.o
$QEMU $t/exe | grep -q 'main foo bar'
