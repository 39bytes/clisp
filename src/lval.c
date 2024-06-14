#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../vendor/mpc.h"
#include <assert.h>
#include "lval.h"

#define MIN(x,y) (x) < (y) ? (x) : (y)
#define MAX(x,y) (x) > (y) ? (x) : (y)


lval *lval_int(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_INT;
    v->data._int = x;
    return v;
}

lval *lval_double(double x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DOUBLE;
    v->data._double = x;
    return v;
}

lval *lval_err(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->data._err = malloc(strlen(m) + 1);
    return v;
}

lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->data._sym = malloc(strlen(s) + 1);
    strcpy(v->data._sym, s);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->data._sexpr.count = 0;
    v->data._sexpr.cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_INT:
        case LVAL_DOUBLE:
            break;
        case LVAL_ERR:
            free(v->data._err);
            break;
        case LVAL_SYM:
            free(v->data._sym);
            break;
        case LVAL_SEXPR:
            for (int i = 0; i < v->data._sexpr.count; i++) {
                lval_del(v->data._sexpr.cell[i]);
            }
            free(v->data._sexpr.cell);
            break;
    }

    free(v);
}

static lval *lval_read_int(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_int(x) : lval_err("invalid number");
}

static lval *lval_read_double(mpc_ast_t *t) {
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

    double x = strtod(buf, NULL);
    free(buf);

    return errno != ERANGE ? lval_double(x) : lval_err("invalid floating point number");
}


static lval *lval_add(lval* v, lval* x) {
    assert(v->type == LVAL_SEXPR);
    
    v->data._sexpr.count++;
    v->data._sexpr.cell = realloc(v->data._sexpr.cell, sizeof(lval*) * v->data._sexpr.count);
    v->data._sexpr.cell[v->data._sexpr.count - 1] = x;
    return v;
}

lval *lval_read(mpc_ast_t *t) {
    if (strstr(t->tag, "int")) { return lval_read_int(t); }
    if (strstr(t->tag, "double")) { return lval_read_double(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

static void lval_expr_print(lval *v, char open, char close) {
    assert(v->type == LVAL_SEXPR);

    putchar(open);
    for (int i = 0; i < v->data._sexpr.count; i++) {
        lval_print(v->data._sexpr.cell[i]);
        if (i != (v->data._sexpr.count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_INT:
            printf("%li", v->data._int);
            break;
        case LVAL_DOUBLE:
            printf("%f", v->data._double);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->data._err);
            break;
        case LVAL_SYM:
            printf("%s", v->data._sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

void lval_println(lval *v){
    lval_print(v);
    putchar('\n');
}

static long powli(long x, long y) {
    long res = 1;
    while (y) {
        res *= x;
    }
    return res;
}

static lval *lval_pop(lval* v, int i) {
    assert(v->type == LVAL_SEXPR);

    lval_sexpr_t *sexpr = &v->data._sexpr;

    lval *x = sexpr->cell[i];

    int n_after = sexpr->count - i - 1;
    memmove(&sexpr->cell[i], &sexpr->cell[i+1], sizeof(lval*) * n_after);

    sexpr->count--;
    sexpr->cell = realloc(sexpr->cell, sizeof(lval*) * sexpr->count);
    return x;
}

static lval *lval_take(lval* v, int i) {
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

static lval *builtin_op(lval *v, char *op) {
    assert(v->type == LVAL_SEXPR);
    
    lval_sexpr_t *sexpr = &v->data._sexpr;

    enum LVAL_TYPE elem_type = sexpr->cell[0]->type;

    if (elem_type != LVAL_INT && elem_type != LVAL_DOUBLE) {
        lval_del(v);
        return lval_err("Cannot operate on non-numeric type");
    }

    for (int i = 1; i < sexpr->count; i++) {
        if (sexpr->cell[i]->type != elem_type) {
            lval_del(v);
            return lval_err("Cannot mix integer and double types in expression");
        }
    }

    lval *x = lval_pop(v, 0);
    
    // Unary '-'
    if ((strcmp(op, "-") == 0) && sexpr->count == 0) {
        if (elem_type == LVAL_INT) {
            x->data._int = -x->data._int;
        } else {
            x->data._double = -x->data._double;
        }
    }

    while (sexpr->count > 0) {
        lval *y = lval_pop(v, 0);

        if (elem_type == LVAL_INT) {
            if (strcmp(op, "+") == 0) { x->data._int += y->data._int; }
            else if (strcmp(op, "-") == 0) { x->data._int -= y->data._int; }
            else if (strcmp(op, "*") == 0) { x->data._int *= y->data._int; }
            else if (strcmp(op, "/") == 0) { 
                if (y->data._int == 0) {
                    return lval_err("division by zero");
                }
                x->data._int /= y->data._int;
            }
            else if (strcmp(op, "%") == 0) { x->data._int %= y->data._int; }
            else if (strcmp(op, "^") == 0) { x->data._int = powli(x->data._int, y->data._int); }
            else if (strcmp(op, "min") == 0) { x->data._int = MIN(x->data._int, y->data._int); }
            else if (strcmp(op, "max") == 0) { x->data._int = MAX(x->data._int, y->data._int); }
            else { 
                lval_del(y);
                return lval_err("invalid operator");
            }

            lval_del(y);
        } else {
            if (strcmp(op, "+") == 0) { x->data._double += y->data._double; }
            else if (strcmp(op, "-") == 0) { x->data._double -= y->data._double; }
            else if (strcmp(op, "*") == 0) { x->data._double *= y->data._double; }
            else if (strcmp(op, "/") == 0) { 
                if (y->data._double == 0) {
                    return lval_err("division by zero");
                }
                x->data._double /= y->data._double;
            }
            else if (strcmp(op, "^") == 0) { x->data._double = powli(x->data._double, y->data._double); }
            else if (strcmp(op, "min") == 0) { x->data._double = MIN(x->data._double, y->data._double); }
            else if (strcmp(op, "max") == 0) { x->data._double = MAX(x->data._double, y->data._double); }
            else { 
                lval_del(y);
                return lval_err("invalid operator");
            }

            lval_del(y);
        }
    }

    lval_del(v);
    return x;
}

static lval *lval_eval_sexpr(lval* v) {
    assert(v->type == LVAL_SEXPR);

    // Evaluate children
    for (int i = 0; i < v->data._sexpr.count; i++) {
        v->data._sexpr.cell[i] = lval_eval(v->data._sexpr.cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->data._sexpr.count; i++) {
        if (v->data._sexpr.cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    if (v->data._sexpr.count == 0) { return v; }
    if (v->data._sexpr.count == 1) { return lval_take(v, 0); }

    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); 
        lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }
    
    lval *result = builtin_op(v, f->data._sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }

    return v;
}
