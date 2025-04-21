#!/bin/bash
if [ "$1" == "rebuild" ]; then
    rm -rf build
    mkdir -p build
fi

cd build
cmake ..
make
./nova ../test.nova
