#!/bin/bash

if [ "$1" == "x86" ]; then
    for file in src/*
    do
        if [ -f $file ] && [[ $file == *.c ]]; then
            binary=$(echo $file | sed 's/\.c//' | sed 's/^src\///')
            mkdir -p bins/x86
            gcc -o ./bins/x86/$binary $file
        fi
    done
elif [ "$1" == "arm32" ]; then
    for file in src/*
    do
        if [ -f $file ] && [[ $file == *.c ]]; then
            binary=$(echo $file | sed 's/\.c//' | sed 's/^src\///')
            mkdir -p bins/arm32
            arm-linux-gnueabihf-gcc -o ./bins/arm32/$binary $file
        fi
    done
elif [ "$1" == "arm64" ]; then
    for file in src/*
    do
        if [ -f $file ] && [[ $file == *.c ]]; then
            binary=$(echo $file | sed 's/\.c//' | sed 's/^src\///')
            mkdir -p bins/arm64
            aarch64-linux-gnu-gcc -o ./bins/arm64/$binary $file
        fi
    done
else
    echo "Invalid argument. Usage: $0 [x86|arm32|arm64]"
fi
