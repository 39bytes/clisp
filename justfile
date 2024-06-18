alias vg := valgrind

flags := "-std=c99 -Wall -Wextra -Werror"
source := "src/*"
link := "-ledit -lm"
output := "lispy"

build:
    cc -g {{flags}} {{source}} {{link}} -o {{output}} 

build-release:
    cc {{flags}} -O3 {{source}} {{link}} -o {{output}} 

run: build
    ./lispy

valgrind: build
    valgrind --leak-check=full -s ./lispy 

debug: build
    gdb ./lispy
