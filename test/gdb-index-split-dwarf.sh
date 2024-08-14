#!/bin/bash
. $(dirname $0)/common.inc

on_qemu && skip
[ $MACHINE = riscv64 -o $MACHINE = riscv32 -o $MACHINE = sparc64 ] && skip

command -v gdb >& /dev/null || skip

test_cflags -gdwarf-5 -g || skip

cat <<EOF > $t/a.c
void fn3();

static void fn2() {
  fn3();
}

void fn1() {
  fn2();
}
EOF

cat <<EOF > $t/b.c
void fn5();

static void fn4() {
  fn5();
}

void fn3() {
  fn4();
}
EOF

cat <<EOF > $t/c.c
void fn7();

static void fn6() {
  fn7();
}

void fn5() {
  fn6();
}
EOF

cat <<EOF > $t/d.c
#include <stdio.h>
void trap() {}

static void fn8() {
  printf("Hello world\n");
  trap();
}

void fn7() {
  fn8();
}
EOF

$CC -c -o $t/a.o $t/a.c -fPIC -g -ggnu-pubnames -gdwarf-5 -gsplit-dwarf
$CC -c -o $t/b.o $t/b.c -fPIC -g -ggnu-pubnames -gdwarf-4 -gsplit-dwarf
$CC -c -o $t/c.o $t/c.c -fPIC -g -ggnu-pubnames -gdwarf-5
$CC -c -o $t/d.o $t/d.c -fPIC -g -ggnu-pubnames -gdwarf-5 -gsplit-dwarf

$CC -B. -shared -o $t/e.so $t/a.o $t/b.o $t/c.o $t/d.o -Wl,--gdb-index
readelf -WS $t/e.so 2> /dev/null | grep -Fq .gdb_index

cat <<EOF | $CC -c -o $t/f.o -fPIC -g -ggnu-pubnames -gdwarf-5 -gsplit-dwarf -xc - -gz
void fn1();

int main() {
  fn1();
}
EOF

$CC -B. -o $t/exe $t/e.so $t/f.o -Wl,--gdb-index
readelf -WS $t/exe 2> /dev/null | grep -Fq .gdb_index

$QEMU $t/exe | grep -q 'Hello world'
