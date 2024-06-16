#include <assert.h>
#include "lval.h"

#define MIN(x,y) (x) < (y) ? (x) : (y)
#define MAX(x,y) (x) > (y) ? (x) : (y)

#define LASSERT(arg, cond, ...) \
    if (!(cond)) { \
        lval *_e = lval_error(__VA_ARGS__); \
        lval_del(arg); \
        return _e; \
    }

#define LASSERT_ARG_COUNT(func_name, arg, expected_count) \
    LASSERT(arg, arg->data.sexpr.count == expected_count, \
            "Incorrect argument count for function '%s'. Expected %d, got %d", \
             func_name, expected_count, arg->data.sexpr.count);

#define LASSERT_ARG_TYPE(func_name, arg, arg_num, arg_type) \
    lval *_a = arg->data.sexpr.cell[arg_num]; \
    LASSERT(arg, _a->type = arg_type, \
            "Incorrect argument type for function '%s'. Expected type '%s' for argument '%d', got %s", \
             func_name, lval_type_name(arg_type), arg_num + 1, _a->type);

#define LASSERT_QEXPR_NOT_EMPTY(func_name, arg) \
    LASSERT(arg, arg->data.qexpr.count > 0, \
            "Function '%s' received empty Q-expression", func_name);

#define LASSERT_ARG_TYPES(lval, arg_type) \
    for (int i = 0; i < lval->data.sexpr.count; i++) { \
        enum LVAL_TYPE _t = lval->data.sexpr.cell[i]->type; \
        LASSERT(lval, _t == (arg_type), \
                "Expected type '%s', got type '%s'", \
                lval_type_name(arg_type), lval_type_name(_t)); \
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
    
    lval_expr *sexpr = &v->data.sexpr;
    enum LVAL_TYPE elem_type = sexpr->cell[0]->type;

    LASSERT(v, elem_type == LVAL_INT || elem_type == LVAL_DOUBLE,
            "Operator arguments must be numeric");
    LASSERT_ARG_TYPES(v, elem_type);

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

static lval *builtin_add(lenv *e, lval *a) {
    return builtin_op(e, a, "+");
}

static lval *builtin_sub(lenv *e, lval *a) {
    return builtin_op(e, a, "-");
}

static lval *builtin_mul(lenv *e, lval *a) {
    return builtin_op(e, a, "*");
}

static lval *builtin_div(lenv *e, lval *a) {
    return builtin_op(e, a, "/");
}

static lval *builtin_mod(lenv *e, lval *a) {
    return builtin_op(e, a, "%");
}

static lval *builtin_pow(lenv *e, lval *a) {
    return builtin_op(e, a, "^");
}

static lval *builtin_min(lenv *e, lval *a) {
    return builtin_op(e, a, "min");
}

static lval *builtin_max(lenv *e, lval *a) {
    return builtin_op(e, a, "max");
}

static lval *builtin_head(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("head", v, 1);
    LASSERT_ARG_TYPE("head", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("head", arg)

    while (arg->data.qexpr.count > 1) {
        lval_del(lval_expr_pop(&arg->data.qexpr, 1));
    }
    return arg;
}

static lval *builtin_tail(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("tail", v, 1);
    LASSERT_ARG_TYPE("tail", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("tail", arg)

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

    LASSERT_ARG_COUNT("eval", v, 1);
    LASSERT_ARG_TYPE("eval", v, 0, LVAL_QEXPR);

    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    x->data.sexpr = x->data.qexpr;
    return lval_eval(e, x);
}

static lval *builtin_join(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr *sexpr = &v->data.sexpr;
    LASSERT_ARG_TYPES(v, LVAL_QEXPR);
    
    lval *x = lval_expr_pop(sexpr, 0);

    while (sexpr->count > 0) {
        lval *y = lval_expr_pop(sexpr, 0);
        lval_expr *xq = &x->data.qexpr;
        lval_expr *yq = &y->data.qexpr;

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
    
    lval_expr *sexpr = &v->data.sexpr;
    LASSERT(v, sexpr->count == 2, "Function 'cons' expected exactly two arguments");
    LASSERT_ARG_COUNT("cons", v, 2);
    LASSERT_ARG_TYPE("cons", v, 1, LVAL_QEXPR);

    lval *arg1 = lval_expr_pop(sexpr, 0);
    lval *arg2 = lval_take(v, 0);
    lval_expr_push_front(&arg2->data.qexpr, arg1);
    return arg2;
}

static lval *builtin_len(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    LASSERT_ARG_COUNT("len", v, 1);
    LASSERT_ARG_TYPE("len", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    int len = arg->data.qexpr.count;
    lval_del(arg);

    return lval_int(len);
}

static lval *builtin_init(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    LASSERT_ARG_COUNT("init", v, 1);
    LASSERT_ARG_TYPE("init", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("init", arg)

    lval *x = lval_expr_pop(&arg->data.qexpr, arg->data.qexpr.count - 1);
    lval_del(x);

    return arg;
}

static lval *builtin_def(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr *sexpr = &v->data.sexpr;
    LASSERT_ARG_TYPE("def", v, 0, LVAL_QEXPR);

    lval_expr *symbols = &sexpr->cell[0]->data.qexpr;
    for (int i = 0; i < symbols->count; i++) {
        LASSERT(v, symbols->cell[i]->type == LVAL_SYMBOL, 
                "Function 'def' requires a list of symbols as first argument");
    }
    LASSERT(v, symbols->count == sexpr->count - 1, 
            "Function 'def' requires the same number of values as is given in the symbol list");

    for (int i = 0; i < symbols->count; i++) {
        lenv_entry *entry = lenv_lookup(e, symbols->cell[i]->data.symbol);
        if (entry != NULL && entry->builtin) {
            return lval_error("Cannot redefine builtin function '%s'", entry->symbol);
        }
    }
    for (int i = 0; i < symbols->count; i++) {
        lenv_put(e, symbols->cell[i]->data.symbol, sexpr->cell[i+1], false);
    }

    lval_del(v);
    return lval_sexpr();
}

static lval *builtin_exit(lenv *e, lval *v) {
    printf("Exiting REPL\n");
    exit(0);
}

static void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *v = lval_func(func);
    lenv_put(e, name, v, true);
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

    lenv_add_builtin(e, "exit", builtin_exit);
}

lenv *lenv_base(void) {
    lenv *e = lenv_new();
    lenv_add_builtins(e);
    return e;
}
