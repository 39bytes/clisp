#include <assert.h>
#include "lval.h"

#define MIN(x,y) (x) < (y) ? (x) : (y)
#define MAX(x,y) (x) > (y) ? (x) : (y)

#define LASSERT(lval, cond, err) \
    if (!(cond)) { lval_del(lval); return lval_error(err); }

#define LASSERT_ARG_TYPES(lval, arg_type, err) \
    for (int i = 0; i < lval->data.sexpr.count; i++) { \
        LASSERT(lval, lval->data.sexpr.cell[i]->type == (arg_type), err); \
    }

static long powli(long x, long y) {
    long res = 1;
    while (y) {
        res *= x;
    }
    return res;
}

static lval *builtin_op(lenv *e, lval *v, char *op) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr_t *sexpr = &v->data.sexpr;
    enum LVAL_TYPE elem_type = sexpr->cell[0]->type;

    LASSERT(v, elem_type == LVAL_INT || elem_type == LVAL_DOUBLE,
            "Operator arguments must be numeric");
    LASSERT_ARG_TYPES(v, elem_type, "Cannot mix integer and double types in expression");

    lval *x = lval_expr_pop(sexpr, 0);
    
    // Unary '-'
    if ((strcmp(op, "-") == 0) && sexpr->count == 0) {
        if (elem_type == LVAL_INT) {
            x->data._int = -x->data._int;
        } else {
            x->data._double = -x->data._double;
        }
    }

    while (sexpr->count > 0) {
        lval *y = lval_expr_pop(sexpr, 0);

        if (elem_type == LVAL_INT) {
            if (strcmp(op, "+") == 0) { x->data._int += y->data._int; }
            else if (strcmp(op, "-") == 0) { x->data._int -= y->data._int; }
            else if (strcmp(op, "*") == 0) { x->data._int *= y->data._int; }
            else if (strcmp(op, "/") == 0) { 
                LASSERT(y, y->data._int != 0, "division by zero");
                x->data._int /= y->data._int;
            }
            else if (strcmp(op, "%") == 0) { x->data._int %= y->data._int; }
            else if (strcmp(op, "^") == 0) { x->data._int = powli(x->data._int, y->data._int); }
            else if (strcmp(op, "min") == 0) { x->data._int = MIN(x->data._int, y->data._int); }
            else if (strcmp(op, "max") == 0) { x->data._int = MAX(x->data._int, y->data._int); }
            else { 
                lval_del(y);
                return lval_error("invalid operator");
            }

            lval_del(y);
        } else {
            if (strcmp(op, "+") == 0) { x->data._double += y->data._double; }
            else if (strcmp(op, "-") == 0) { x->data._double -= y->data._double; }
            else if (strcmp(op, "*") == 0) { x->data._double *= y->data._double; }
            else if (strcmp(op, "/") == 0) { 
                LASSERT(y, y->data._double != 0, "division by zero");
                x->data._double /= y->data._double;
            }
            else if (strcmp(op, "^") == 0) { x->data._double = powl(x->data._double, y->data._double); }
            else if (strcmp(op, "min") == 0) { x->data._double = MIN(x->data._double, y->data._double); }
            else if (strcmp(op, "max") == 0) { x->data._double = MAX(x->data._double, y->data._double); }
            else { 
                lval_del(y);
                return lval_error("invalid operator");
            }

            lval_del(y);
        }
    }

    lval_del(v);
    return x;
}

lval *builtin_add(lenv *e, lval *a) {
    return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a) {
    return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a) {
    return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a) {
    return builtin_op(e, a, "/");
}

lval *builtin_mod(lenv *e, lval *a) {
    return builtin_op(e, a, "%");
}

lval *builtin_pow(lenv *e, lval *a) {
    return builtin_op(e, a, "^");
}

lval *builtin_min(lenv *e, lval *a) {
    return builtin_op(e, a, "min");
}

lval *builtin_max(lenv *e, lval *a) {
    return builtin_op(e, a, "max");
}

static lval *builtin_head(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 1, 
            "Function 'head' expected exactly one argument");

    lval *arg = sexpr->cell[0];
    LASSERT(v, arg->type == LVAL_QEXPR, 
            "Function 'head' expected type Q-expression for argument");
    LASSERT(v, arg->data.qexpr.count > 0, 
            "Function 'head' received empty Q-expression");

    arg = lval_take(v, 0);
    while (arg->data.qexpr.count > 1) {
        lval_del(lval_expr_pop(&arg->data.qexpr, 1));
    }
    return arg;
}

static lval *builtin_tail(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 1, 
            "Function 'tail' expected exactly one argument");

    lval *arg = sexpr->cell[0];
    LASSERT(v, arg->type == LVAL_QEXPR, 
            "Function 'tail' expected type Q-expression for argument");
    LASSERT(v, arg->data.qexpr.count > 0, 
            "Function 'tail' received empty Q-expression");

    arg = lval_take(v, 0);
    lval_del(lval_expr_pop(&arg->data.qexpr, 0));
    return arg;
}

static lval *builtin_list(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    v->type = LVAL_QEXPR;
    v->data.qexpr = v->data.sexpr;
    return v;
}

static lval *builtin_eval(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 1,
            "Function 'eval' expected exactly one argument");
    LASSERT(v, sexpr->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' expected type Q-expression for argument");
    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    x->data.sexpr = x->data.qexpr;
    return lval_eval(e, x);
}

static lval *builtin_join(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT_ARG_TYPES(v, LVAL_QEXPR, "Function 'join' expects Q-expressions as arguments");
    
    lval *x = lval_expr_pop(sexpr, 0);

    while (sexpr->count > 0) {
        lval *y = lval_expr_pop(sexpr, 0);
        lval_expr_t *xq = &x->data.qexpr;
        lval_expr_t *yq = &y->data.qexpr;

        while (yq->count > 0) {
            lval_expr_push_back(xq, lval_expr_pop(yq, 0));
        }
        lval_del(y);
    }

    lval_del(v);
    return x;
}

static lval *builtin_cons(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 2, "Function 'cons' expected exactly two arguments");

    lval *arg1 = lval_expr_pop(sexpr, 0);
    lval *arg2 = lval_take(v, 0);
    LASSERT(v, arg2->type == LVAL_QEXPR, "Function 'cons' expected a Q-expression as second argument");
    
    lval_expr_push_front(&arg2->data.qexpr, arg1);
    return arg2;
}

static lval *builtin_len(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 1, "Function 'len' expected exactly one argument");

    lval *arg = lval_take(v, 0);
    LASSERT(v, arg->type == LVAL_QEXPR, "Function 'len' expected a Q-expression as argument");

    int len = arg->data.qexpr.count;
    lval_del(arg);

    return lval_int(len);
}

static lval *builtin_init(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr_t *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 1, "Function 'init' expected exactly one argument");

    lval *arg = lval_take(v, 0);
    LASSERT(v, arg->type == LVAL_QEXPR, "Function 'init' expected a Q-expression as argument");

    lval_expr_t *qexpr = &arg->data.qexpr;
    LASSERT(arg, qexpr->count > 0, "Function 'init' received empty Q-expression");
    lval *x = lval_expr_pop(qexpr, qexpr->count - 1);
    lval_del(x);

    return arg;
}

static lval *builtin_def(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr_t *sexpr = &v->data.sexpr;

    LASSERT(v, sexpr->cell[0]->type == LVAL_QEXPR, 
            "Function 'def' passed incorrect type!");

    lval_expr_t *symbols = &sexpr->cell[0]->data.qexpr;
    for (int i = 0; i < symbols->count; i++) {
        LASSERT(v, symbols->cell[i]->type == LVAL_SYMBOL, 
                "Function 'def' requires a list of symbols as first argument");
    }
    LASSERT(v, symbols->count == sexpr->count - 1, 
            "Function 'def' requires the same number of values as is given in the symbol list");
    for (int i = 0; i < symbols->count; i++) {
        lenv_put(e, symbols->cell[i]->data.symbol, sexpr->cell[i+1]);
    }

    lval_del(v);
    return lval_sexpr();
}

static void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *v = lval_func(func);
    lenv_put(e, name, v);
    lval_del(v);
}

static void lenv_add_builtins(lenv *e) {
    lenv_add_builtin(e, "def", builtin_def);

    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "eval", builtin_eval);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);
}


lenv *lenv_base(void) {
    lenv *e = lenv_new();
    lenv_add_builtins(e);
    return e;
}

lval *builtin(lenv *e, lval *v, char *func) {
    if (strcmp("list", func) == 0) { return builtin_list(e, v); }
    if (strcmp("head", func) == 0) { return builtin_head(e, v); }
    if (strcmp("tail", func) == 0) { return builtin_tail(e, v); }
    if (strcmp("join", func) == 0) { return builtin_join(e, v); }
    if (strcmp("cons", func) == 0) { return builtin_cons(e, v); }
    if (strcmp("len", func) == 0) { return builtin_len(e, v); }
    if (strcmp("init", func) == 0) { return builtin_init(e, v); }
    if (strcmp("eval", func) == 0) { return builtin_eval(e, v); }
    if (strcmp("min", func) == 0) { return builtin_op(e, v, "min"); }
    if (strcmp("max", func) == 0) { return builtin_op(e, v, "max"); }
    if (strstr("+-/*%^", func)) { return builtin_op(e, v, func); }

    lval_del(v);
    return lval_error("Unknown function");
}
