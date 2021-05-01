#!/bin/sh

set -e

git clone https://github.com/google/brotli.git
cd brotli
mkdir out
cd out
export CFLAGS="$(pg_config --cflags)"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./brotli_install ..
cmake --build . --config Release --target install
mv brotli_install ../../
