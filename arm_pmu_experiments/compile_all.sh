#!/bin/bash

# Find all .c files in the current directory
for file in *.c; do
  # Get the file name without the extension
  name=$(basename "$file" .c)
  # Compile the file using gcc and name the executable after the source file
  gcc -o "$name" "$file"
done
