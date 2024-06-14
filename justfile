run:
    cc -std=c99 -Wall -Wpedantic -Werror \
        vendor/mpc.c src/* \
        -ledit -lm \
        -o lispy \
    && ./lispy
