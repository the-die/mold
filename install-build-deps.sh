#!/bin/sh
# This script installs binary packages needed to build mold.
# Feel free to send me a PR if your OS is not on this list.

set -e
. /etc/os-release

set -x

case "$ID" in
ubuntu | pop | linuxmint | debian | raspbian | neon)
  apt-get update
  apt-get install -y cmake gcc g++
  if [ "$ID-$VERSION_ID" = ubuntu-20.04 ]; then apt-get install -y g++-10; fi
  ;;
fedora | amzn | rhel)
  dnf install -y gcc-g++ cmake glibc-static libstdc++-static diffutils util-linux
  ;;
opensuse-*)
  zypper install -y make cmake gcc-c++ glibc-devel-static tar diffutils util-linux
  ;;
gentoo)
  emerge-webrsync
  FEATURES='getbinpkg binpkg-request-signature' emerge dev-build/cmake
  ;;
arch | archarm | artix | endeavouros)
  pacman -Sy --needed --noconfirm base-devel cmake util-linux
  ;;
void)
  xbps-install -Sy xbps bash make cmake gcc tar diffutils util-linux
  ;;
alpine)
  apk update
  apk add bash make linux-headers cmake gcc g++
  ;;
clear-linux-os)
  swupd update
  swupd bundle-add c-basic diffutils
  ;;
almalinux)
  dnf install -y gcc-toolset-13-gcc-c++ gcc-toolset-13-libstdc++-devel cmake diffutils
  ;;
freebsd)
  pkg update
  pkg install -y cmake bash binutils gcc
  ;;
*)
  echo "Error: don't know anything about build dependencies on $ID-$VERSION_ID"
  exit 1
esac
