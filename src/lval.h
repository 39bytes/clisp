#include "../vendor/mpc.h"

enum LVAL_TYPE { 
    LVAL_INT, 
    LVAL_DOUBLE, 
    LVAL_SYMBOL,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_FUNC,
    LVAL_ERROR,
};

typedef struct {
    int count; 
    struct lval** cell;
} lval_expr_t;

struct lval;
typedef struct lval lval;

struct lenv {
    int count;
    char **syms;
    lval **vals;
};
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef union {
    long _int; 
    double _double;
    char *error;
    char *symbol;
    lval_expr_t sexpr;
    lval_expr_t qexpr;
    lbuiltin func;
} lval_data;

struct lval {
    enum LVAL_TYPE type;
    lval_data data;
};

lenv *lenv_new(void);
void lenv_del(lenv *e);
lval *lenv_get(lenv *e, char *k);
void lenv_put(lenv *e, char *k, lval *v);

lenv *lenv_base(void);

lval *lval_int(long x);
lval *lval_double(double x);
lval *lval_error(char *m);
lval *lval_symbol(char *s);
lval *lval_sexpr(void);
lval *lval_qexpr(void);
lval *lval_func(lbuiltin func);
void lval_del(lval* v);

lval *lval_read(mpc_ast_t *t);

void lval_print(lval *v);
void lval_println(lval *v);

lval *lval_eval(lenv *e, lval *v);

lval *lval_expr_pop(lval_expr_t* e, int i);
void lval_expr_push_back(lval_expr_t* e, lval* x);
void lval_expr_push_front(lval_expr_t* e, lval* x);
lval *lval_take(lval* v, int i);
