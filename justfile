alias vg := valgrind

flags := "-std=c99 -Wall -Wextra -Werror"
source := "src/*"
link := "-ledit -lm"
output := "clisp"

build:
    cc -g {{flags}} {{source}} {{link}} -o {{output}} 

build-release:
    cc {{flags}} -O3 {{source}} {{link}} -o {{output}} 

run: build
    ./{{output}}

valgrind: build
    valgrind --leak-check=full -s ./{{output}}

debug: build
    gdb ./{{output}}
