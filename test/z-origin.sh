#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -c -o $t/a.o -xc -
#include <stdio.h>

int main() {
  printf("Hello world\n");
}
EOF

$CC -B. -o $t/exe $t/a.o -Wl,-z,origin

readelf --dynamic $t/exe | grep -Eq '\(FLAGS\)\s+ORIGIN'
readelf --dynamic $t/exe | grep -Eq 'Flags:.*ORIGIN'
