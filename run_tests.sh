#!/bin/bash

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

echo "Running all tests..."
make -j$(nproc) && make run_all_tests

# Print status
echo "All tests completed!" 