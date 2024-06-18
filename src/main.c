#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <editline/readline.h>
#include <histedit.h>
#include "builtins.h"
#include "utils.h"
#include "parser.h"
#include "lval.h"

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
    lenv *e = lenv_base();

    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            load_file(e, argv[i]);
        }
    }

    while (true) {
        char *input = readline("~> ");

        lval *expr = lval_sexpr();
        parse_expr(expr, input, 0, '\0');
        lval *v = lval_eval(e, expr);
        lval_println(v);
        lval_del(v);
        
        add_history(input);
        free(input);
    }
    
    lenv_del(e);
    return 0;
}
