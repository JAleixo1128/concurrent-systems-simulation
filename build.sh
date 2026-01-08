#!/bin/bash

set -e

build_version () {
    echo "Building $1..."
    cd $1
    g++ -std=c++17 \
        main.cpp \
        gl_frontEnd.cpp \
        utils.cpp \
        -o final \
        -framework OpenGL \
        -framework GLUT
    cd ..
}

build_version Version1
build_version Version2
build_version Version3
build_version Version4
build_version Version5

echo "Build complete."

