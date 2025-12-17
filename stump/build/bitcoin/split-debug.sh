#!/bin/sh
set -e
if [ $# -ne 3 ];
    then echo "usage: $0 <input> <stripped-binary> <debug-binary>"
fi

/nix/store/v9qgwx232gw625shbqadsn90r2iczbm4-llvm-binutils-18.1.8/bin/llvm-objcopy --enable-deterministic-archives -p --only-keep-debug $1 $3
/nix/store/v9qgwx232gw625shbqadsn90r2iczbm4-llvm-binutils-18.1.8/bin/llvm-objcopy --enable-deterministic-archives -p --strip-debug $1 $2
/nix/store/v9qgwx232gw625shbqadsn90r2iczbm4-llvm-binutils-18.1.8/bin/llvm-strip --enable-deterministic-archives -p -s $2
/nix/store/v9qgwx232gw625shbqadsn90r2iczbm4-llvm-binutils-18.1.8/bin/llvm-objcopy --enable-deterministic-archives -p --add-gnu-debuglink=$3 $2
