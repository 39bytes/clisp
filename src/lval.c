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
        case LVAL_BUILTIN_FUNC: return "function";
        case LVAL_LAMBDA: return "function";
        case LVAL_ERROR: return "error";
    }

    return "unknown";
}

lval *lval_int(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_INT;
    v->_int = x;
    return v;
}

lval *lval_double(double x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DOUBLE;
    v->_double = x;
    return v;
}

lval *lval_error(char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERROR;
    
    va_list va;
    va_start(va, fmt);
    v->error = malloc(512);

    vsnprintf(v->error, 511, fmt, va);
    v->error = realloc(v->error, strlen(v->error) + 1);
    va_end(va);

    return v;
}

lval *lval_symbol(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYMBOL;
    v->symbol = malloc(strlen(s) + 1);
    strcpy(v->symbol, s);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->sexpr.count = 0;
    v->sexpr.cell = NULL;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->qexpr.count = 0;
    v->qexpr.cell = NULL;
    return v;
}

lval *lval_builtin_func(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_BUILTIN_FUNC;
    v->builtin_func = func;
    return v;
}

lval *lval_func(lval* formals, lval *body) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_LAMBDA;

    lval_lambda *lambda = &v->lambda;
    lambda->env = lenv_new();
    lambda->formals = formals;
    lambda->body = body;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_INT:
        case LVAL_DOUBLE:
        case LVAL_BUILTIN_FUNC:
            break;
        case LVAL_ERROR:
            free(v->error);
            break;
        case LVAL_SYMBOL:
            free(v->symbol);
            break;
        case LVAL_SEXPR:
            for (int i = 0; i < v->sexpr.count; i++) {
                lval_del(v->sexpr.cell[i]);
            }
            free(v->sexpr.cell);
            break;
        case LVAL_QEXPR:
            for (int i = 0; i < v->qexpr.count; i++) {
                lval_del(v->qexpr.cell[i]);
            }
            free(v->qexpr.cell);
            break;
        case LVAL_LAMBDA:
            lenv_del(v->lambda.env);
            lval_del(v->lambda.formals);
            lval_del(v->lambda.body);
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
    if (e->count == 0) {
        free(e->cell);
        e->cell = NULL;
    } else {
        e->cell = realloc(e->cell, sizeof(lval*) * e->count);
    }
    return x;
}

lval *lval_take(lval* v, int i) {
    assert(v->type == LVAL_SEXPR || v->type == LVAL_QEXPR);

    lval *x = NULL;

    if (v->type == LVAL_SEXPR) {
        x = lval_expr_pop(&v->sexpr, i);
    }
    else {
        x = lval_expr_pop(&v->qexpr, i);
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
            lval_expr_push_back(&x->sexpr, lval_read(t->children[i]));
        }
    } else if (strstr(t->tag, "qexpr")) {
        x = lval_qexpr();

        for (int i = 0; i < t->children_num; i++) {
            if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
            if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
            if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
            lval_expr_push_back(&x->qexpr, lval_read(t->children[i]));
        }
    }
    

    return x;
}

static void lval_expr_print(lval *v, char open, char close) {
    assert(v->type == LVAL_SEXPR || v->type == LVAL_QEXPR);

    lval_expr *expr = v->type == LVAL_SEXPR ? &v->sexpr : &v->qexpr;

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
            printf("%li", v->_int);
            break;
        case LVAL_DOUBLE:
            printf("%f", v->_double);
            break;
        case LVAL_ERROR:
            printf("Error: %s", v->error);
            break;
        case LVAL_SYMBOL:
            printf("%s", v->symbol);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_BUILTIN_FUNC:
            printf("<builtin>");
            break;
        case LVAL_LAMBDA:
            printf("(\\");
            lval_print(v->lambda.formals);
            putchar(' ');
            lval_print(v->lambda.body);
            putchar(')');
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
            x->_int = v->_int;
            break;
        case LVAL_DOUBLE:
            x->_double = v->_double;
            break;
        case LVAL_BUILTIN_FUNC:
            x->builtin_func = v->builtin_func;
            break;
        case LVAL_ERROR:
            x->error = malloc(strlen(v->error) + 1);
            strcpy(x->error, v->error);
            break;
        case LVAL_SYMBOL:
            x->symbol = malloc(strlen(v->symbol) + 1);
            strcpy(x->symbol, v->symbol);
            break;
        case LVAL_SEXPR: {
            lval_expr *sexpr = &x->sexpr;
            sexpr->count = v->sexpr.count;
            sexpr->cell = malloc(sizeof(lval*) * sexpr->count);
            for (int i = 0; i < sexpr->count; i++) {
                sexpr->cell[i] = lval_copy(v->sexpr.cell[i]);
            }
            break;
        }
        case LVAL_QEXPR: {
            lval_expr *qexpr = &x->qexpr;
            qexpr->count = v->qexpr.count;
            qexpr->cell = malloc(sizeof(lval*) * qexpr->count);
            for (int i = 0; i < qexpr->count; i++) {
                qexpr->cell[i] = lval_copy(v->qexpr.cell[i]);
            }
            break;
        }
        case LVAL_LAMBDA:
            x->lambda.env = lenv_copy(v->lambda.env);
            x->lambda.formals = lval_copy(v->lambda.formals);
            x->lambda.body = lval_copy(v->lambda.body);
            break;
    }

    return x;
}

lenv* lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->parent = NULL;
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
        if (e->parent == NULL) {
            return lval_error("unbound symbol '%s'", k);
        } else {
            return lenv_get(e->parent, k);
        }
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

lenv *lenv_copy(lenv *e) {
    lenv *copy = malloc(sizeof(lenv));
    copy->parent = e->parent;
    copy->count = e->count;
    copy->entries = malloc(sizeof(lenv_entry*) * e->count);

    for (int i = 0; i < e->count; i++) {
        lenv_entry *entry = malloc(sizeof(lenv_entry));
        lenv_entry *existing = e->entries[i];

        entry->builtin = existing->builtin;
        entry->symbol = malloc(strlen(existing->symbol) + 1);
        strcpy(entry->symbol, existing->symbol);
        entry->val = lval_copy(existing->val);
        
        copy->entries[i] = entry;
    }
    
    return copy;
}

void lenv_def(lenv *e, char *k, lval *v) {
    while (e->parent != NULL) {
        e = e->parent;
    }

    lenv_put(e, k, v, false);
}

lval *lval_eval(lenv* e, lval* v);
lval *builtin_list(lenv *e, lval *v);

static lval *lval_call(lenv *e, lval *f, lval *a) {
    assert(f->type == LVAL_BUILTIN_FUNC || f->type == LVAL_LAMBDA);
    assert(a->type == LVAL_SEXPR);
    
    if (f->type == LVAL_BUILTIN_FUNC) {
        return f->builtin_func(e, a);
    }

    lval_lambda *func = &f->lambda;
    lval_expr *params = &func->formals->qexpr;
    
    int passed = a->sexpr.count;
    int expected = params->count;

    while (a->sexpr.count > 0) {
        if (params->count == 0) {
            lval_del(a);
            return lval_error("Function received too many arguments. Got %i, expected %i", passed, expected);
        }

        lval *symbol = lval_expr_pop(params, 0);

        if (strcmp(symbol->symbol, "&") == 0) {
            if (params->count != 1) {
                lval_del(a);
                return lval_error("Syntax error: expected a single symbol after '&'");
            }
            
            lval *rest = lval_expr_pop(params, 0);
            lenv_put(func->env, rest->symbol, builtin_list(e, a), false);
            lval_del(symbol);
            lval_del(rest);
            break;
        }

        lval *val = lval_expr_pop(&a->sexpr, 0);

        lenv_put(func->env, symbol->symbol, val, false);

        lval_del(symbol);
        lval_del(val);
    }

    lval_del(a);

    if (params->count > 0 && 
        strcmp(params->cell[0]->symbol, "&") == 0) {
        
        if (params->count != 2) {
            return lval_error("Syntax error: expected a single symbol after '&'");
        }
        
        // Delete '&'
        lval_del(lval_expr_pop(params, 0));

        // Pop next symbol and create empty list
        lval *rest = lval_expr_pop(params, 0);
        lval *val = lval_qexpr();
        
        lenv_put(func->env, rest->symbol, val, false);
        lval_del(rest);
        lval_del(val);
    }

    if (func->formals->qexpr.count == 0) {
        func->env->parent = e;
        
        lval *body = lval_copy(func->body);
        body->type = LVAL_SEXPR;
        body->sexpr = body->qexpr;
        return lval_eval(func->env, body);
    }
    
    return lval_copy(f);
}


static lval *lval_eval_sexpr(lenv *e, lval* v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr *sexpr = &v->sexpr;

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
    if (f->type != LVAL_BUILTIN_FUNC && f->type != LVAL_LAMBDA) {
        lval_del(f); 
        lval_del(v);
        return lval_error("S-expression does not start with a function!");
    }
    
    lval *result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYMBOL) {
        lval *x = lenv_get(e, v->symbol);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }

    return v;
}
