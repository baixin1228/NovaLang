#!/bin/bash
if [ "$1" == "rebuild" ]; then
    rm -rf build
    mkdir -p build
fi

cd build
cmake ..
if [ "$1" == "gdb" ]; then
    make -j$(nproc) && gdb --args ./nova -j -D ../test.nova
else
    make -j$(nproc) && ./nova -j -D ../test.nova
fi
