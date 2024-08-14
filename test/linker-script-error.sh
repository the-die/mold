#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CC -o $t/a.o -c -xc -
int main() {}
EOF

echo 'VERSION { ver_x /*' > $t/b.script

! $CC -B. -o $t/exe $t/a.o $t/b.script 2> $t/log
grep -q 'unclosed comment' $t/log
