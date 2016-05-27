#!/bin/sh
command -v cmake >/dev/null 2>&1 || (echo >&2 "build requires cmake but nothig is found" && exit)

script=$(readlink -f "$0")
route=$(dirname "$script")

sh lib_update_posix.sh

if [ -d "$route/../build" ]; then
    rm -r $route/../build
fi
mkdir $route/../build
cd $route/../build
cmake ..
make
