#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <editline/readline.h>
#include <histedit.h>
#include "../vendor/mpc.h"
#include "lval.h"
#include "builtins.h"

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

int ast_node_count(mpc_ast_t *t) { 
    if (t->children_num == 0) {
        return 1;
    }

    int total = 1;
    for (int i = 0; i < t->children_num; i++) {
        total += ast_node_count(t->children[i]);
    }
    return total;
}

int ast_leaf_count(mpc_ast_t *t) {
    if (t->children_num == 0) {
        return 1;
    }
    
    int total = 0;
    for (int i = 0; i < t->children_num; i++) {
        total += ast_leaf_count(t->children[i]);
    }
    return total;
}


int main(int argc, char** argv) {
    mpc_parser_t* Int = mpc_new("int");
    mpc_parser_t* Double = mpc_new("double");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                    \
            int      : /-?[0-9]+/ ;                                          \
            double   : /-?[0-9]+/ '.' /[0-9]+/ ;                             \
            number   : <double> | <int> ;                                    \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                    \
            sexpr    : '(' <expr>* ')' ;                                     \
            qexpr    : '{' <expr>* '}' ;                                     \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;             \
            lispy    : /^/ <expr>* /$/ ;                                     \
        ",
        Int, Double, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy version 0.1.0");

    lenv *e = lenv_base();

    while (true) {
        char *input = readline("~> ");
        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        add_history(input);
        free(input);
    }
    
    mpc_cleanup(8, Int, Double, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
