#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CXX -c -o $t/a.o -xc++ -fPIC -
int main() {
  try {
    throw 0;
  } catch (int x) {
    return x;
  }
  return 1;
}
EOF

$CXX -B. -o $t/exe $t/a.o -mcmodel=large
$QEMU $t/exe

if test_cxxflags -static; then
  $CXX -B. -o $t/exe $t/a.o -static -mcmodel=large
  $QEMU $t/exe
fi
