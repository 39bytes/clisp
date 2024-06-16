#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../vendor/mpc.h"
#include <assert.h>
#include "lval.h"


char *lval_type_name(enum LVAL_TYPE type) {
    switch (type) {
        case LVAL_INT: return "int";
        case LVAL_DOUBLE: return "double";
        case LVAL_SYMBOL: return "symbol";
        case LVAL_SEXPR: return "s-expression";
        case LVAL_QEXPR: return "q-expression";
        case LVAL_FUNC: return "function";
        case LVAL_ERROR: return "error";
    }

    return "unknown";
}

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

lval *lval_error(char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERROR;
    
    va_list va;
    va_start(va, fmt);
    v->data.error = malloc(512);

    vsnprintf(v->data.error, 511, fmt, va);
    v->data.error = realloc(v->data.error, strlen(v->data.error) + 1);
    va_end(va);

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

lval *lval_func(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUNC;
    v->data.func = func;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_INT:
        case LVAL_DOUBLE:
        case LVAL_FUNC:
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


void lval_expr_push_back(lval_expr* e, lval* x) {
    e->count++;
    e->cell = realloc(e->cell, sizeof(lval*) * e->count);
    e->cell[e->count - 1] = x;
}

void lval_expr_push_front(lval_expr* e, lval* x) {
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

lval *lval_expr_pop(lval_expr* e, int i) {
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

    lval_expr *expr = v->type == LVAL_SEXPR ? &v->data.sexpr : &v->data.qexpr;

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
        case LVAL_FUNC:
            printf("<function>");
            break;
    }
}

void lval_println(lval *v){
    lval_print(v);
    putchar('\n');
}

lval *lval_copy(lval* v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;
    switch (v->type) {
        case LVAL_INT:
            x->data._int = v->data._int;
            break;
        case LVAL_DOUBLE:
            x->data._double = v->data._double;
            break;
        case LVAL_FUNC:
            x->data.func = v->data.func;
            break;
        case LVAL_ERROR:
            x->data.error = malloc(strlen(v->data.error) + 1);
            strcpy(x->data.error, v->data.error);
            break;
        case LVAL_SYMBOL:
            x->data.symbol = malloc(strlen(v->data.symbol) + 1);
            strcpy(x->data.symbol, v->data.symbol);
            break;
        case LVAL_SEXPR: {
            lval_expr *sexpr = &x->data.sexpr;
            sexpr->count = v->data.sexpr.count;
            sexpr->cell = malloc(sizeof(lval*) * sexpr->count);
            for (int i = 0; i < sexpr->count; i++) {
                sexpr->cell[i] = lval_copy(v->data.sexpr.cell[i]);
            }
            break;
        }
        case LVAL_QEXPR: {
            lval_expr *qexpr = &x->data.qexpr;
            qexpr->count = v->data.qexpr.count;
            qexpr->cell = malloc(sizeof(lval*) * qexpr->count);
            for (int i = 0; i < qexpr->count; i++) {
                qexpr->cell[i] = lval_copy(v->data.qexpr.cell[i]);
            }
            break;
        }
    }

    return x;
}

lenv* lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->entries = NULL;
    return e;
}

void lenv_del(lenv *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->entries[i]->symbol);
        lval_del(e->entries[i]->val);
    }

    free(e->entries);
    free(e);
}

lenv_entry *lenv_lookup(lenv *e, char *k) {
    for (int i = 0; i < e->count; i++) {
        lenv_entry *entry = e->entries[i];
        if (strcmp(entry->symbol, k) == 0) {
            return entry;
        }
    }

    return NULL;
}

lval *lenv_get(lenv *e, char *k) {
    lenv_entry *entry = lenv_lookup(e, k);
    if (entry == NULL) {
        return lval_error("unbound symbol '%s'", k);
    }

    return lval_copy(entry->val);
}

void lenv_put(lenv *e, char *k, lval *v, bool builtin) {
    for (int i = 0; i < e->count; i++) {
        lenv_entry *entry = e->entries[i];
        if (strcmp(entry->symbol, k) == 0) {
            lval_del(entry->val);
            entry->val = lval_copy(v);
        }
    }

    e->count++;
    e->entries = realloc(e->entries, sizeof(lenv_entry*) * e->count);

    lenv_entry *entry = malloc(sizeof(lenv_entry));
    entry->symbol = malloc(strlen(k) + 1);
    strcpy(entry->symbol, k);
    entry->val = lval_copy(v);
    entry->builtin = builtin;

    e->entries[e->count - 1] = entry;
}


static lval *lval_eval_sexpr(lenv *e, lval* v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr *sexpr = &v->data.sexpr;

    // Evaluate children
    for (int i = 0; i < sexpr->count; i++) {
        sexpr->cell[i] = lval_eval(e, sexpr->cell[i]);
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
    if (f->type != LVAL_FUNC) {
        lval_del(f); 
        lval_del(v);
        return lval_error("S-expression does not start with a function!");
    }
    
    lval *result = f->data.func(e, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYMBOL) {
        lval *x = lenv_get(e, v->data.symbol);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }

    return v;
}
