#include <assert.h>
#include "lval.h"
#include "utils.h"

#define MIN(x,y) (x) < (y) ? (x) : (y)
#define MAX(x,y) (x) > (y) ? (x) : (y)

#define LASSERT(arg, cond, ...) \
    if (!(cond)) { \
        lval *_e = lval_error(__VA_ARGS__); \
        lval_del(arg); \
        return _e; \
    }

#define LASSERT_ARG_COUNT(func_name, arg, expected_count) \
    LASSERT(arg, arg->sexpr.count == expected_count, \
            "Incorrect argument count for function '%s'. Expected %d, got %d", \
             func_name, expected_count, arg->sexpr.count);

#define LASSERT_ARG_TYPE(func_name, arg, arg_num, arg_type) \
    LASSERT(arg, arg->sexpr.cell[arg_num]->type = arg_type, \
            "Incorrect argument type for function '%s'. Expected type '%s' for argument '%d', got %s", \
             func_name, lval_type_name(arg_type), arg_num + 1, arg->sexpr.cell[arg_num]->type);

#define LASSERT_QEXPR_NOT_EMPTY(func_name, arg) \
    LASSERT(arg, arg->qexpr.count > 0, \
            "Function '%s' received empty Q-expression", func_name);

#define LASSERT_ARG_TYPES(lval, arg_type) \
    for (int i = 0; i < lval->sexpr.count; i++) { \
        LASSERT(lval, lval->sexpr.cell[i]->type == (arg_type), \
                "Expected type '%s', got type '%s'", \
                lval_type_name(arg_type), lval_type_name(lval->sexpr.cell[i]->type)); \
    }

static long powli(long x, long y) {
    long res = 1;
    while (y) {
        res *= x;
    }
    return res;
}

static lval *builtin_op(lval *v, char *op) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr *sexpr = &v->sexpr;
    enum LVAL_TYPE elem_type = sexpr->cell[0]->type;

    LASSERT(v, elem_type == LVAL_INT || elem_type == LVAL_DOUBLE,
            "Operator arguments must be numeric");
    LASSERT_ARG_TYPES(v, elem_type);

    lval *x = lval_expr_pop(sexpr, 0);
    
    // Unary '-'
    if ((strcmp(op, "-") == 0) && sexpr->count == 0) {
        if (elem_type == LVAL_INT) {
            x->_int = -x->_int;
        } else {
            x->_double = -x->_double;
        }
    }

    while (sexpr->count > 0) {
        lval *y = lval_expr_pop(sexpr, 0);

        if (elem_type == LVAL_INT) {
            if (strcmp(op, "+") == 0) { x->_int += y->_int; }
            else if (strcmp(op, "-") == 0) { x->_int -= y->_int; }
            else if (strcmp(op, "*") == 0) { x->_int *= y->_int; }
            else if (strcmp(op, "/") == 0) { 
                LASSERT(y, y->_int != 0, "division by zero");
                x->_int /= y->_int;
            }
            else if (strcmp(op, "%") == 0) { x->_int %= y->_int; }
            else if (strcmp(op, "^") == 0) { x->_int = powli(x->_int, y->_int); }
            else if (strcmp(op, "min") == 0) { x->_int = MIN(x->_int, y->_int); }
            else if (strcmp(op, "max") == 0) { x->_int = MAX(x->_int, y->_int); }
            else { 
                lval_del(y);
                return lval_error("invalid operator");
            }

            lval_del(y);
        } else {
            if (strcmp(op, "+") == 0) { x->_double += y->_double; }
            else if (strcmp(op, "-") == 0) { x->_double -= y->_double; }
            else if (strcmp(op, "*") == 0) { x->_double *= y->_double; }
            else if (strcmp(op, "/") == 0) { 
                LASSERT(y, y->_double != 0, "division by zero");
                x->_double /= y->_double;
            }
            else if (strcmp(op, "^") == 0) { x->_double = powl(x->_double, y->_double); }
            else if (strcmp(op, "min") == 0) { x->_double = MIN(x->_double, y->_double); }
            else if (strcmp(op, "max") == 0) { x->_double = MAX(x->_double, y->_double); }
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

lval *builtin_add(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "+");
}

lval *builtin_sub(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "-");
}

lval *builtin_mul(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "*");
}

lval *builtin_div(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "/");
}

lval *builtin_mod(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "%");
}

lval *builtin_pow(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "^");
}

lval *builtin_min(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "min");
}

lval *builtin_max(UNUSED lenv *e, lval *a) {
    return builtin_op(a, "max");
}

lval *builtin_comparison(lval *v, char *op) {
    LASSERT_ARG_COUNT(op, v, 2);

    lval *a = v->sexpr.cell[0];
    lval *b = v->sexpr.cell[1];
    
    LASSERT(v, a->type == LVAL_INT || a->type == LVAL_DOUBLE, 
            "Expected numeric type for comparison, got '%s'", lval_type_name(a->type));
    LASSERT(v, b->type == a->type, 
            "Expected type '%s' for second argument of comparison, got '%s'", 
            lval_type_name(a->type), lval_type_name(b->type));

    if (a->type == LVAL_INT && b->type == LVAL_INT) {
        if (strcmp(op, "<") == 0) { return lval_bool(a->_int < b->_int); }
        else if (strcmp(op, ">") == 0) { return lval_bool(a->_int > b->_int); }
        else if (strcmp(op, "<=") == 0) { return lval_bool(a->_int <= b->_int); }
        else if (strcmp(op, ">=") == 0) { return lval_bool(a->_int >= b->_int); }
        else if (strcmp(op, "==") == 0) { return lval_bool(a->_int == b->_int); }
        else if (strcmp(op, "!=") == 0) { return lval_bool(a->_int != b->_int); }
    } else {
        if (strcmp(op, "<") == 0) { return lval_bool(a->_double < b->_double); }
        else if (strcmp(op, ">") == 0) { return lval_bool(a->_double > b->_double); }
        else if (strcmp(op, "<=") == 0) { return lval_bool(a->_double <= b->_double); }
        else if (strcmp(op, ">=") == 0) { return lval_bool(a->_double >= b->_double); }
        else if (strcmp(op, "==") == 0) { return lval_bool(a->_double == b->_double); }
        else if (strcmp(op, "!=") == 0) { return lval_bool(a->_double != b->_double); }
    }

    return lval_error("invalid comparison operator!");
}

lval *builtin_lt(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, "<");
}

lval *builtin_gt(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, ">");
}

lval *builtin_lte(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, "<=");
}

lval *builtin_gte(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, ">=");
}

lval *builtin_eq(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, "==");
}

lval *builtin_neq(UNUSED lenv *e, lval *v) {
    return builtin_comparison(v, "!=");
}

lval *builtin_head(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("head", v, 1);
    LASSERT_ARG_TYPE("head", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("head", arg)

    while (arg->qexpr.count > 1) {
        lval_del(lval_expr_pop(&arg->qexpr, 1));
    }
    return arg;
}

lval *builtin_tail(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("tail", v, 1);
    LASSERT_ARG_TYPE("tail", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("tail", arg)

    lval_del(lval_expr_pop(&arg->qexpr, 0));
    return arg;
}

lval *builtin_list(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    v->type = LVAL_QEXPR;
    v->qexpr = v->sexpr;
    return v;
}

lval *builtin_eval(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("eval", v, 1);
    LASSERT_ARG_TYPE("eval", v, 0, LVAL_QEXPR);

    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    x->sexpr = x->qexpr;
    return lval_eval(e, x);
}

lval *builtin_join(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    lval_expr *sexpr = &v->sexpr;
    LASSERT_ARG_TYPES(v, LVAL_QEXPR);
    
    lval *x = lval_expr_pop(sexpr, 0);

    while (sexpr->count > 0) {
        lval *y = lval_expr_pop(sexpr, 0);
        lval_expr *xq = &x->qexpr;
        lval_expr *yq = &y->qexpr;

        while (yq->count > 0) {
            lval_expr_push_back(xq, lval_expr_pop(yq, 0));
        }
        lval_del(y);
    }

    lval_del(v);
    return x;
}

lval *builtin_cons(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    lval_expr *sexpr = &v->sexpr;
    LASSERT(v, sexpr->count == 2, "Function 'cons' expected exactly two arguments");
    LASSERT_ARG_COUNT("cons", v, 2);
    LASSERT_ARG_TYPE("cons", v, 1, LVAL_QEXPR);

    lval *arg1 = lval_expr_pop(sexpr, 0);
    lval *arg2 = lval_take(v, 0);
    lval_expr_push_front(&arg2->qexpr, arg1);
    return arg2;
}

lval *builtin_len(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    LASSERT_ARG_COUNT("len", v, 1);
    LASSERT_ARG_TYPE("len", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    int len = arg->qexpr.count;
    lval_del(arg);

    return lval_int(len);
}

lval *builtin_init(UNUSED lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);
    
    LASSERT_ARG_COUNT("init", v, 1);
    LASSERT_ARG_TYPE("init", v, 0, LVAL_QEXPR);

    lval *arg = lval_take(v, 0);
    LASSERT_QEXPR_NOT_EMPTY("init", arg)

    lval *x = lval_expr_pop(&arg->qexpr, arg->qexpr.count - 1);
    lval_del(x);

    return arg;
}

lval *builtin_lambda(UNUSED lenv *e, lval *v) {
    LASSERT_ARG_COUNT("\\", v, 2);
    LASSERT_ARG_TYPE("\\", v, 0, LVAL_QEXPR);
    LASSERT_ARG_TYPE("\\", v, 1, LVAL_QEXPR);

    lval_expr *sexpr = &v->sexpr;
    lval_expr *symbols = &sexpr->cell[0]->qexpr;
    for (int i = 0; i < symbols->count; i++) {
        LASSERT(v, symbols->cell[i]->type == LVAL_SYMBOL, 
                "Lambda parameter list must be a list of symbols. Got %s instead.", 
                lval_type_name(symbols->cell[i]->type));
    }

    lval *formals = lval_expr_pop(sexpr, 0);
    lval *body = lval_expr_pop(sexpr, 0);
    lval_del(v);

    return lval_func(formals, body);
}

lval *builtin_var(UNUSED lenv *e, lval *v, char *func) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_TYPE(func, v, 0, LVAL_QEXPR);

    lval_expr *symbols = &v->sexpr.cell[0]->qexpr;
    for (int i = 0; i < symbols->count; i++) {
        LASSERT(v, symbols->cell[i]->type == LVAL_SYMBOL, 
                "Function '%s' requires a list of symbols as first argument", func);
    }
    LASSERT(v, symbols->count == v->sexpr.count - 1, 
            "Function '%s' requires the same number of values as is given in the symbol list", func);

    for (int i = 0; i < symbols->count; i++) {
        lenv_entry *entry = lenv_lookup(e, symbols->cell[i]->symbol);
        if (entry != NULL && entry->builtin) {
            return lval_error("Cannot redefine builtin function '%s'", entry->symbol);
        }
    }


    for (int i = 0; i < symbols->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_def(e, symbols->cell[i]->symbol, v->sexpr.cell[i+1]);
        }
        if (strcmp(func, "=") == 0) {
            lenv_put(e, symbols->cell[i]->symbol, v->sexpr.cell[i+1], false);
        }
    }

    lval_del(v);
    return lval_sexpr();
}

lval *builtin_def(lenv *e, lval *v) {
    return builtin_var(e, v, "def");
}

lval *builtin_put(lenv *e, lval *v) {
    return builtin_var(e, v, "=");
}

lval *builtin_fun(lenv *e, lval *v) {
    assert(v->type == LVAL_SEXPR);

    LASSERT_ARG_COUNT("fun", v, 2);
    LASSERT_ARG_TYPE("fun", v, 0, LVAL_QEXPR);
    LASSERT_ARG_TYPE("fun", v, 1, LVAL_QEXPR);

    lval *symbol_list = v->sexpr.cell[0];

    LASSERT(v, symbol_list->qexpr.count >= 2,
            "Function 'fun' requires at least one function argument in function declaration.");

    for (int i = 0; i < symbol_list->qexpr.count; i++) { \
        LASSERT(v, symbol_list->qexpr.cell[i]->type == LVAL_SYMBOL,
                "Expected type '%s', got type '%s'",
                lval_type_name(LVAL_SYMBOL), lval_type_name(v->sexpr.cell[i]->type));
    }

    lval *fun_name = lval_expr_pop(&symbol_list->qexpr, 0);
    lval *params = symbol_list;
    lval *body = v->sexpr.cell[1];
    
    lval *func = lval_func(params, body);
    lenv_def(e, fun_name->symbol, func);

    return lval_sexpr();
}

lval *builtin_exit(UNUSED lenv *e, UNUSED lval *v) {
    printf("Exiting REPL\n");
    exit(0);
}

static void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *v = lval_builtin_func(func);
    lenv_put(e, name, v, true);
    lval_del(v);
}

static void lenv_add_builtins(lenv *e) {
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "fun", builtin_fun);
    lenv_add_builtin(e, "\\", builtin_lambda);

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

    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, "<=", builtin_lte);
    lenv_add_builtin(e, ">=", builtin_gte);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_neq);

    lenv_add_builtin(e, "exit", builtin_exit);
}

lenv *lenv_base(void) {
    lenv *e = lenv_new();
    lenv_add_builtins(e);
    return e;
}
