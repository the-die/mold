#!/bin/bash
. $(dirname $0)/common.inc

# OneTBB isn't tsan-clean
nm mold | grep -q '__tsan_init' && skip

cat <<EOF | $CC -c -o $t/a.o -xassembler -
.section .rodata.str1.1,"aMS",@progbits,1
val1:
.ascii "Hello \0"

.section .rodata.str1.1,"aMS",@progbits,1
val5:
.ascii "World \0"
EOF

./mold --relocatable -o $t/b.o $t/a.o

readelf -W -p .rodata.str1.1 $t/b.o | grep -Eq '\b0\b.*Hello'
readelf -W -p .rodata.str1.1 $t/b.o | grep -Eq '\b7\b.*World'
