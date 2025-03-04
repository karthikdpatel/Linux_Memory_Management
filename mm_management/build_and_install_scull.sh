#!/bin/bash

MODULE_NAME="mm_simulatorko"

# Clean previous builds
echo "Cleaning previous builds..."
make clean

# Build the module
echo "Building the module..."
make

# Insert the module into the kernel
echo "Inserting the module into the kernel..."
sudo insmod ${MODULE_NAME}.ko


