#!/bin/bash

# Check if test name is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <test_name>"
    echo "Example: $0 01_variables"
    exit 1
fi

TEST_NAME=$1

# Check if build directory exists, if not create it
if [ ! -d "build" ]; then
    mkdir -p build
    echo "Created build directory"
fi

# Go to build directory
cd build

# Run cmake
echo "Running cmake..."
cmake ..

echo "Running test: $TEST_NAME..."
make -j$(nproc) && make run_$TEST_NAME

# Print status
echo "Test completed!" 