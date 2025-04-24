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

# Compile the project
echo "Compiling the project..."
make -j$(nproc)

# Capture expected output for all tests
echo "Capturing expected output for all tests..."
make capture_expected_output

# Print status
echo "Expected output captured for all tests!" 