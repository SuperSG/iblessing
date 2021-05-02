#!/bin/bash

# todo: use fakeroot
cd "$(dirname "$0")"

set -xe
cd submodules/capstone
make
cd ../..

cd submodules/unicorn
UNICORN_ARCHS="arm aarch64 x86" ./make.sh
cd ../..

cd submodules/keystone
mkdir -p build
cd build
../make-lib.sh
cd ../../..

cp submodules/capstone/libcapstone.a                iblessing/iblessing/vendor/libs/
cp submodules/unicorn/libunicorn.a                  iblessing/iblessing/vendor/libs/
cp submodules/keystone/build/llvm/lib/libkeystone.a iblessing/iblessing/vendor/libs/

mkdir -p cmake-build
cd cmake-build

if [[ "$OSTYPE" == "darwin"* ]]; then
    cmake --clean-first -Diblessing.PLATFORM=macos ../iblessing 
else
    cmake --clean-first -Diblessing.PLATFORM=linux ../iblessing 
fi

cmake --build .

if [[ "$OSTYPE" == "darwin"* ]]; then
    mv iblessing iblessing-darwin
else
    mv iblessing iblessing-linux
fi

cd ..
sh ./copy-headers.sh

cd cmake-build
tar -czvf iblessing-framework.tar.gz ./libiblessing-core.dylib include

cd ..
echo "[+] iblessing is in cmake-build directory~"
