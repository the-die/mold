#!/bin/bash -x
set -e

ver=$(grep '^VERSION =' $(dirname $0)/../Makefile | sed 's/.* = //')
dest=mold-$ver-ubuntu-20.04

cat <<'EOF' | docker build -t mold-build-ubuntu20 -
FROM ubuntu:20.04
RUN apt-get update && \
  TZ=Europe/London apt-get install -y tzdata && \
  apt-get install -y --no-install-recommends build-essential clang lld \
    bsdmainutils file gcc-multilib git pkg-config \
    cmake libstdc++-10-dev zlib1g-dev libssl-dev && \
  apt clean && \
  rm -rf /var/lib/apt/lists/*
EOF

docker run -it --rm -v "$(pwd):/mold:Z" -u "$(id -u):$(id -g)" \
  mold-build-ubuntu20 \
  bash -c "cp -r /mold /tmp/mold &&
cd /tmp/mold &&
make clean &&
make -j$(nproc) CXX=clang++ &&
make install PREFIX=/ DESTDIR=$dest &&
ln -sfr $dest/bin/mold $dest/libexec/mold/ld &&
tar czf /mold/$dest.tar.gz $dest"