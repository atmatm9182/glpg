#!/bin/sh

set -xe

CXX=${CXX:-g++}
CXXFLAGS="$CXXFLAGS -g -lglfw -lGL -std=c++20"

SOURCES="main.cc"

$CXX -o main ${CXXFLAGS} main.cc
