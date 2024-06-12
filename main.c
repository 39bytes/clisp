#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <editline/readline.h>
#include <histedit.h>
#include "vendor/mpc.h"

#define MIN(x,y) (x) < (y) ? (x) : (y)
#define MAX(x,y) (x) > (y) ? (x) : (y)

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

enum LVAL_TYPE { 
    LVAL_NUM, 
    LVAL_DOUBLE, 
    LVAL_ERR
};

enum LVAL_ERROR_TYPE { 
    LERR_DIV_ZERO, 
    LERR_BAD_OP, 
    LERR_BAD_NUM,
    LERR_MISMATCHED_TYPES,
};

typedef union {
    long num; 
    double doub;
    enum LVAL_ERROR_TYPE err;
} lval_data;

typedef struct {
    enum LVAL_TYPE type;
    lval_data data;
} lval;

lval lval_num(long x) {
    lval res;
    res.type = LVAL_NUM;
    res.data.num = x;
    return res;
}

lval lval_doub(double x) {
    lval res;
    res.type = LVAL_DOUBLE;
    res.data.doub = x;
    return res;
}

lval lval_err(enum LVAL_ERROR_TYPE err) {
    lval res;
    res.type = LVAL_ERR;
    res.data.err = err;
    return res;
}

void lval_print(lval v) {
    switch (v.type) {
        case LVAL_NUM:
            printf("%li", v.data.num);
            break;
        case LVAL_DOUBLE:
            printf("%f", v.data.doub);
            break;
        case LVAL_ERR:
            switch (v.data.err) {
                case LERR_DIV_ZERO:
                    printf("Error: Division by zero!");
                    break;
                case LERR_BAD_OP:
                    printf("Error: Invalid operator!");
                    break;
                case LERR_BAD_NUM:
                    printf("Error: Invalid number!");
                    break;
                case LERR_MISMATCHED_TYPES:
                    printf("Error: Mismatched types!");
                    break;
            }
            break;
    }
}

void lval_println(lval v){
    lval_print(v);
    putchar('\n');
}

long powli(long x, long y) {
    long res = 1;
    while (y) {
        res *= x;
    }
    return res;
}

lval eval_op(lval x, char* op, lval y) {
    if (x.type == LVAL_ERR) {
        return x;
    }
    if (y.type == LVAL_ERR) {
        return y;
    }

    if (x.type != y.type) {
        return lval_err(LERR_MISMATCHED_TYPES);
    }

    if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
        long xv = x.data.num;
        long yv = y.data.num;

        if (strcmp(op, "+") == 0) { return lval_num(xv + yv); }
        if (strcmp(op, "-") == 0) { return lval_num(xv - yv); }
        if (strcmp(op, "*") == 0) { return lval_num(xv * yv); }
        if (strcmp(op, "/") == 0) { 
            if (yv == 0) {
                return lval_err(LERR_DIV_ZERO);
            }
            return lval_num(xv / yv);
        }
        if (strcmp(op, "%") == 0) { return lval_num(xv % yv); }
        if (strcmp(op, "^") == 0) { return lval_num(powli(xv, yv)); }
        if (strcmp(op, "min") == 0) { return lval_num(MIN(xv, yv)); }
        if (strcmp(op, "max") == 0) { return lval_num(MAX(xv, yv)); }
    } else {
        double xv = x.data.doub;
        double yv = y.data.doub;

        if (strcmp(op, "+") == 0) { return lval_doub(xv + yv); }
        if (strcmp(op, "-") == 0) { return lval_doub(xv - yv); }
        if (strcmp(op, "*") == 0) { return lval_doub(xv * yv); }
        if (strcmp(op, "/") == 0) { 
            if (yv == 0.0) {
                return lval_err(LERR_DIV_ZERO);
            }
            return lval_doub(xv / yv);
        }
        if (strcmp(op, "^") == 0) { return lval_doub(pow(xv, yv)); }
        if (strcmp(op, "min") == 0) { return lval_doub(MIN(xv, yv)); }
        if (strcmp(op, "max") == 0) { return lval_doub(MAX(xv, yv)); }
    }
    

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "int")) {
       errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }
    if (strstr(t->tag, "double")) {
        errno = 0;
        // Double consists of 3 children
        // (0) Leading digits before decimal
        // (1) Decimal
        // (2) Decimal digits
        char *buf = malloc(strlen(t->children[0]->contents) + 1 + strlen(t->children[2]->contents) + 1);
        strcpy(buf, t->children[0]->contents);
        strcat(buf, t->children[1]->contents);
        strcat(buf, t->children[2]->contents);
        strcat(buf, "\0");
        printf("Buffer: %s\n", buf);

        double x = strtod(buf, NULL);
        free(buf);

        return errno != ERANGE ? lval_doub(x) : lval_err(LERR_BAD_NUM);
    }

    char* op = t->children[1]->contents;
    lval x = eval(t->children[2]);

    // In case we have '-' as a unary operator
    // if (x.type == LVAL_NUM && 
    //     strcmp(op, "-") == 0 && 
    //     t->children_num == 4) 
    // {
    //     x.data.num = -x.data.num;
    //     return x;
    // }

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv) {
    mpc_parser_t* Int = mpc_new("int");
    mpc_parser_t* Double = mpc_new("double");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                \
            int      : /-?[0-9]+/ ;                                      \
            double   : /-?[0-9]+/ '.' /[0-9]+/ ;                         \
            number   : <double> | <int> ;                                \
            operator : '+' | '-' | '*' | '/' | '%' | \"min\" | \"max\" ; \
            expr     : <number> | '(' <operator> <expr>+ ')' ;           \
            lispy    : /^/ <operator> <expr>+ /$/ ;                      \
        ",
        Int, Double, Number, Operator, Expr, Lispy);

    puts("Lispy version 0.1.0");

    while (true) {
        char *input = readline("~> ");
        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval res = eval(r.output);
            mpc_ast_print(r.output);
            lval_println(res);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        add_history(input);
        free(input);
    }
    
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
