#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -c -o $t/a.o -fPIC -xassembler - 2> /dev/null || skip
.global foo
.type foo, %function
.variant_cc foo
foo:
  ret
EOF

$CC -B. -shared -o $t/b.so $t/a.o
readelf -W --dyn-syms $t/b.so | grep foo | grep -Fq '[VARIANT_CC]'

cat <<EOF | $CC -c -o $t/c.o -xc -
void foo();
int main() { foo(); }
EOF

$CC -B. -o $t/exe $t/c.o $t/b.so
readelf -W --dynamic $t/exe | grep -q RISCV_VARIANT_CC
