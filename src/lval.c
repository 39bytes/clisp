#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../vendor/mpc.h"
#include <assert.h>
#include "builtins.h"

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

lval *lval_error(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERROR;
    v->data.error = malloc(strlen(m) + 1);
    strcpy(v->data.error, m);
    return v;
}

lval *lval_symbol(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYMBOL;
    v->data.symbol = malloc(strlen(s) + 1);
    strcpy(v->data.symbol, s);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->data.sexpr.count = 0;
    v->data.sexpr.cell = NULL;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->data.qexpr.count = 0;
    v->data.qexpr.cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_INT:
        case LVAL_DOUBLE:
            break;
        case LVAL_ERROR:
            free(v->data.error);
            break;
        case LVAL_SYMBOL:
            free(v->data.symbol);
            break;
        case LVAL_SEXPR:
            for (int i = 0; i < v->data.sexpr.count; i++) {
                lval_del(v->data.sexpr.cell[i]);
            }
            free(v->data.sexpr.cell);
            break;
        case LVAL_QEXPR:
            for (int i = 0; i < v->data.qexpr.count; i++) {
                lval_del(v->data.qexpr.cell[i]);
            }
            free(v->data.qexpr.cell);
            break;
    }

    free(v);
}

static lval *lval_read_int(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_int(x) : lval_error("invalid number");
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

    return errno != ERANGE ? lval_double(x) : lval_error("invalid floating point number");
}


void lval_expr_push_back(lval_expr_t* e, lval* x) {
    e->count++;
    e->cell = realloc(e->cell, sizeof(lval*) * e->count);
    e->cell[e->count - 1] = x;
}

void lval_expr_push_front(lval_expr_t* e, lval* x) {
    int n_bytes = sizeof(lval*) * e->count;
    e->count++;
    e->cell = realloc(e->cell, sizeof(lval*) * e->count);
    // Q-expr wasn't empty before
    // need to move everything forward one
    if (e->count > 1) {
        memmove(&e->cell[1], &e->cell[0], n_bytes);
    }
    e->cell[0] = x;
}

lval *lval_expr_pop(lval_expr_t* e, int i) {
    assert(i < e->count);

    lval *x = e->cell[i];

    int n_after = e->count - i - 1;
    memmove(&e->cell[i], &e->cell[i+1], sizeof(lval*) * n_after);

    e->count--;
    e->cell = realloc(e->cell, sizeof(lval*) * e->count);
    return x;
}

lval *lval_take(lval* v, int i) {
    assert(v->type == LVAL_SEXPR || v->type == LVAL_QEXPR);

    lval *x = NULL;

    if (v->type == LVAL_SEXPR) {
        x = lval_expr_pop(&v->data.sexpr, i);
    }
    else {
        x = lval_expr_pop(&v->data.qexpr, i);
    }

    lval_del(v);
    return x;
}

lval *lval_read(mpc_ast_t *t) {
    if (strstr(t->tag, "int")) { return lval_read_int(t); }
    if (strstr(t->tag, "double")) { return lval_read_double(t); }
    if (strstr(t->tag, "symbol")) { return lval_symbol(t->contents); }

    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) {
        x = lval_sexpr();

        for (int i = 0; i < t->children_num; i++) {
            if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
            if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
            if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
            lval_expr_push_back(&x->data.sexpr, lval_read(t->children[i]));
        }
    } else if (strstr(t->tag, "qexpr")) {
        x = lval_qexpr();

        for (int i = 0; i < t->children_num; i++) {
            if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
            if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
            if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
            lval_expr_push_back(&x->data.qexpr, lval_read(t->children[i]));
        }
    }
    

    return x;
}

static void lval_expr_print(lval *v, char open, char close) {
    assert(v->type == LVAL_SEXPR || v->type == LVAL_QEXPR);

    lval_expr_t *expr = v->type == LVAL_SEXPR ? &v->data.sexpr : &v->data.qexpr;

    putchar(open);
    for (int i = 0; i < expr->count; i++) {
        lval_print(expr->cell[i]);
        if (i != (expr->count-1)) {
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
        case LVAL_ERROR:
            printf("Error: %s", v->data.error);
            break;
        case LVAL_SYMBOL:
            printf("%s", v->data.symbol);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

void lval_println(lval *v){
    lval_print(v);
    putchar('\n');
}

static lval *lval_eval_sexpr(lval* v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;

    // Evaluate children
    for (int i = 0; i < sexpr->count; i++) {
        sexpr->cell[i] = lval_eval(sexpr->cell[i]);
    }

    // Error checking
    for (int i = 0; i < sexpr->count; i++) {
        if (sexpr->cell[i]->type == LVAL_ERROR) {
            return lval_take(v, i);
        }
    }

    if (sexpr->count == 0) { return v; }
    if (sexpr->count == 1) { return lval_take(v, 0); }

    lval *f = lval_expr_pop(sexpr, 0);
    if (f->type != LVAL_SYMBOL) {
        lval_del(f); 
        lval_del(v);
        return lval_error("S-expression does not start with symbol!");
    }
    
    lval *result = builtin(v, f->data.symbol);
    lval_del(f);
    return result;
}

lval *lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }

    return v;
}
