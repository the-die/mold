#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -xassembler - 2> /dev/null || skip
.section .my_section,"a",@0x80000000
EOF

! $CC -B. -o $t/exe $t/a.o >& $t/log1
grep -q 'unsupported section type: 0x80000000' $t/log1
