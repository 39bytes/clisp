#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <editline/readline.h>
#include <histedit.h>

#ifdef _WIN32
#include <string.h>

char *readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *copy = malloc(strlen(buffer) + 1);
    strcpy(copy, buffer);
    copy[strlen(copy) - 1] = '\0';
    return copy;
}

void add_history(char* unused) {}
#endif

int main(int argc, char** argv) {
    puts("Lispy version 0.1.0");

    while (true) {
        char *input = readline("~> ");
        add_history(input);
        printf("Echo: %s\n", input);
        free(input);
    }

    return 0;
}
