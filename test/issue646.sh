#!/bin/bash
. $(dirname $0)/common.inc

cat <<EOF | $CXX -o $t/a.o -c -xc++ -
#include <iostream>
#include <stdexcept>

class Foo : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

static void do_throw() {
  throw Foo("exception");
}

int main() {
  try {
    do_throw();
  } catch (const Foo &e) {
    std::cout << "error: " << e.what() << std::endl;
  }
}
EOF

$CXX -B. -o $t/exe $t/a.o
$QEMU $t/exe | grep -q 'error: exception'
