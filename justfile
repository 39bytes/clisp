run:
    cc -std=c99 -Wall -Wpedantic -Werror \
        vendor/mpc.c main.c \
        -ledit -lm \
        -o lispy \
    && ./lispy
