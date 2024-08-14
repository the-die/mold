#!/bin/bash
. $(dirname $0)/common.inc

# Skip if 32 bits as we use very large addresses in this test.
[ $MACHINE = i686 ] && skip
[ $MACHINE = riscv32 ] && skip
[ $MACHINE = m68k ] && skip

# It looks like SPARC's runtime can't handle PLT if it's too far from GOT.
[ $MACHINE = sparc64 ] && skip

# Current LoongArch compilers emit BL for function calls, but I believe
# they'll emit PCADDU18I + JIRL (which can address PC ± 128 GiB) in the
# future.
[[ $MACHINE = loongarch* ]] && skip

# qemu aborts with the "Unknown exception 0x5" error, although this
# test passes on a real POWER10 machine.
on_qemu && [ "$CPU" = power10 ] && skip

cat <<EOF > $t/a.c
#include <stdio.h>

void fn3();
void fn4();

__attribute__((section(".low"))) void fn1() { printf(" fn1"); fn3(); }
__attribute__((section(".low"))) void fn2() { printf(" fn2"); fn4(); }

int main() {
  printf(" main");
  fn1();
  printf("\n");
}
EOF

cat <<EOF > $t/b.c
#include <stdio.h>

void fn1();
void fn2();

__attribute__((section(".high"))) void fn3() { printf(" fn3"); fn2(); }
__attribute__((section(".high"))) void fn4() { printf(" fn4"); }
EOF

$CC -c -o $t/c.o $t/a.c -O0
$CC -c -o $t/d.o $t/b.c -O0

$CC -B. -o $t/exe1 $t/c.o $t/d.o \
  -Wl,--section-start=.low=0x10000000,--section-start=.high=0x20000000
$QEMU $t/exe1 | grep -q 'main fn1 fn3 fn2 fn4'

$CC -c -o $t/e.o $t/a.c -O2
$CC -c -o $t/f.o $t/b.c -O2

$CC -B. -o $t/exe2 $t/e.o $t/f.o \
  -Wl,--section-start=.low=0x10000000,--section-start=.high=0x20000000
$QEMU $t/exe2 | grep -q 'main fn1 fn3 fn2 fn4'
