#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -xassembler -
.section .rodata.str1.1, "aMS", @progbits, 1
.section .rodata.foo, "aMS", @progbits, 1
EOF

cat <<EOF | $CC -o $t/b.o -c -xassembler -
.section .rodata.str1.1, "aMS", @progbits, 1
.string "foo"
.string "bar"
EOF

cat <<EOF | $CC -o $t/c.o -c -xc -
int main() {}
EOF

$CC -B. -o $t/exe $t/a.o $t/b.o $t/c.o
$t/exe

readelf -SW $t/exe > $t/log
grep -q '.rodata.str1.1 .* AMS ' $t/log
! grep -q '.rodata.str1.1 .* AM ' $t/log || false
