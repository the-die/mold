#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -x assembler -
foo: jmp 0
EOF

cat <<EOF | $CC -o $t/b.o -c -xc -
int main() {}
EOF

$CC -B. -no-pie -o $t/exe $t/a.o $t/b.o
