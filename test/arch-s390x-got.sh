#!/bin/bash
. $(dirname $0)/common.inc

# GOT[0] must be set to the link-time address of .dynamic on s390x.

cat <<EOF | $CC -c -fPIC -o $t/a.o -xc -
#include <stdio.h>

extern char _DYNAMIC;
extern void *got[];

int main() {
  printf("%d %p %p\n", &_DYNAMIC == got[0], &_DYNAMIC, got[0]);
}
EOF

$CC -B. -o $t/exe $t/a.o -Wl,-defsym=got=_GLOBAL_OFFSET_TABLE_ -no-pie
$QEMU $t/exe | grep -Eq '^1'
