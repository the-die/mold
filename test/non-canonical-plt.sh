#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.so -fPIC -shared -xc -
void *foo() {
  return foo;
}

void *bar() {
  return bar;
}
EOF

cat <<EOF | $CC -o $t/b.o -c -xc - -fPIC
void *bar();

void *baz() {
  return bar;
}
EOF

cat <<EOF | $CC -o $t/c.o -c -xc - -fPIC
#include <stdio.h>

void *foo();
void *bar();
void *baz();

int main() {
  printf("%d %d %d\n", foo == foo(), bar == bar(), bar == baz());
}
EOF

$CC -B. -no-pie -o $t/exe $t/a.so $t/b.o $t/c.o
$QEMU $t/exe | grep -q '^1 1 1$'

readelf --dyn-syms $t/exe | grep -q '00000000 .* foo'
readelf --dyn-syms $t/exe | grep -q '00000000 .* bar'
