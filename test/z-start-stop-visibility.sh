#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -xc -
#include <stdio.h>

__attribute__((section("hello")))
static char msg[] = "Hello world";

int main() {
  printf("%s\n", msg);
}
EOF

$CC -B. -o $t/exe1 $t/a.o
readelf -W --dyn-syms $t/exe1 > $t/log1
! grep -q __start_hello $t/log1 || false
! grep -q __stop_hello $t/log1 || false

$CC -B. -o $t/exe2 $t/a.o -Wl,-z,start-stop-visibility=hidden
readelf -W --dyn-syms $t/exe2 > $t/log2
! grep -q __start_hello $t/log2 || false
! grep -q __stop_hello $t/log2 || false

$CC -B. -o $t/exe3 $t/a.o -Wl,-z,start-stop-visibility=protected
readelf -W --dyn-syms $t/exe3 > $t/log3
grep -q __start_hello $t/log3
grep -q __stop_hello $t/log3
