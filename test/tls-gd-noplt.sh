#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $GCC -fPIC -fno-plt -c -o $t/a.o -xc -
#include <stdio.h>

__attribute__((tls_model("global-dynamic"))) static _Thread_local int x1 = 1;
__attribute__((tls_model("global-dynamic"))) static _Thread_local int x2;
__attribute__((tls_model("global-dynamic"))) extern _Thread_local int x3;
__attribute__((tls_model("global-dynamic"))) extern _Thread_local int x4;
int get_x5();
int get_x6();

int main() {
  x2 = 2;

  printf("%d %d %d %d %d %d\n", x1, x2, x3, x4, get_x5(), get_x6());
  return 0;
}
EOF

cat <<EOF | $GCC -fPIC -fno-plt -c -o $t/b.o -xc -
__attribute__((tls_model("global-dynamic"))) _Thread_local int x3 = 3;
__attribute__((tls_model("global-dynamic"))) static _Thread_local int x5 = 5;
int get_x5() { return x5; }
EOF

cat <<EOF | $GCC -fPIC -fno-plt -c -o $t/c.o -xc -
__attribute__((tls_model("global-dynamic"))) _Thread_local int x4 = 4;
__attribute__((tls_model("global-dynamic"))) static _Thread_local int x6 = 6;
int get_x6() { return x6; }
EOF

$CC -B. -shared -o $t/d.so $t/b.o
$CC -B. -shared -o $t/e.so $t/c.o -Wl,--no-relax

$CC -B. -o $t/exe $t/a.o $t/d.so $t/e.so
$QEMU $t/exe | grep -q '1 2 3 4 5 6'

$CC -B. -o $t/exe $t/a.o $t/d.so $t/e.so -Wl,-no-relax
$QEMU $t/exe | grep -q '1 2 3 4 5 6'
