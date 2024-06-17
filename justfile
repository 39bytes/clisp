alias vg := valgrind

build:
    cc -std=c99 -Wall -Wextra -Werror \
        vendor/mpc.c src/* \
        -ledit -lm \
        -o lispy

build-debug:
    cc -g -std=c99 -Wall -Wextra -Werror \
        vendor/mpc.c src/* \
        -ledit -lm \
        -o lispy

run: build
    ./lispy

valgrind: build-debug
    valgrind ./lispy

debug: build-debug
    gdb ./lispy
