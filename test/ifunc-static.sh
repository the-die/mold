#!/bin/bash
. $(dirname $0)/common.inc

test_cflags -static || skip
supports_ifunc || skip

cat <<EOF | $CC -o $t/a.o -c -xc -
#include <stdio.h>

void foo() __attribute__((ifunc("resolve_foo")));

void hello() {
  printf("Hello world\n");
}

void *resolve_foo() {
  return hello;
}

int main() {
  foo();
  return 0;
}
EOF

$CC -B. -o $t/exe $t/a.o -static
$QEMU $t/exe | grep -q 'Hello world'
