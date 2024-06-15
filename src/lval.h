#include "../vendor/mpc.h"

enum LVAL_TYPE { 
    LVAL_INT, 
    LVAL_DOUBLE, 
    LVAL_SYMBOL,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_ERROR,
};

typedef struct {
    int count; 
    struct lval** cell;
} lval_expr_t;

typedef union {
    long _int; 
    double _double;
    char *error;
    char *symbol;
    lval_expr_t sexpr;
    lval_expr_t qexpr;
} lval_data;

struct lval {
    enum LVAL_TYPE type;
    lval_data data;
};

typedef struct lval lval;

lval *lval_int(long x);
lval *lval_double(double x);
lval *lval_error(char *m);
lval *lval_symbol(char *s);
lval *lval_sexpr(void);
void lval_del(lval* v);

lval *lval_read(mpc_ast_t *t);

void lval_print(lval *v);
void lval_println(lval *v);

lval *lval_eval(lval *v);

lval *lval_expr_pop(lval_expr_t* e, int i);
void lval_expr_push_back(lval_expr_t* e, lval* x);
void lval_expr_push_front(lval_expr_t* e, lval* x);
lval *lval_take(lval* v, int i);
