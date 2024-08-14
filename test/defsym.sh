#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -fPIC -o $t/a.o -c -xc -
#include <stdio.h>
extern char foo;
extern char bar;
void baz();

void print() {
  printf("Hello %p %p\n", &foo, &bar);
}

int main() {
  baz();
}
EOF

$CC -B. -o $t/exe $t/a.o -pie -Wl,-defsym=foo=16 \
  -Wl,-defsym=bar=0x2000 -Wl,-defsym=baz=print

$QEMU $t/exe | grep -q '^Hello 0x10 0x2000$'
