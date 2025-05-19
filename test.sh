#!/bin/bash
cd $(dirname $0)
echo "Current directory: $(pwd)"
if [ "$1" == "rebuild" ]; then
    rm -rf build
    mkdir -p build
    cd build
    cmake ..
else
    cd build
fi

if [ "$1" == "gdb" ]; then
    make -j$(nproc) && gdb --args ./nova -j -D ../$2
fi
if [ "$1" == "run" ]; then
    make -j$(nproc) && ./nova -j -D ../$2
fi
if [ "$1" == "build" ]; then
    make -j$(nproc) && ./nova -D ../$2 && ./a.out
fi