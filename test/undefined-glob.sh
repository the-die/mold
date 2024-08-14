#!/bin/bash
. $(dirname $0)/common.inc

test_cflags -static || skip

cat <<EOF | $CC -o $t/a.o -c -xc -
int foo = 1;
EOF

cat <<EOF | $CC -o $t/b.o -c -xc -
int foobar = 1;
EOF

cat <<EOF | $CC -o $t/c.o -c -xc -
int baz = 1;
EOF

rm -f $t/d.a
ar cr $t/d.a $t/a.o $t/b.o $t/c.o

cat <<EOF | $CC -o $t/e.o -c -xc -
int main() {}
EOF

$CC -B. -o $t/exe1 $t/d.a $t/e.o
readelf -W --symbols $t/exe1 > $t/log1
! grep -q foo $t/log1 || false
! grep -q foobar $t/log1 || false
! grep -q baz $t/log1 || false

$CC -B. -o $t/exe2 $t/d.a $t/e.o -Wl,--undefined-glob='foo*'
readelf -W --symbols $t/exe2 > $t/log2
grep -q foo $t/log2
grep -q foobar $t/log2
! grep -q baz $t/log2 || false
