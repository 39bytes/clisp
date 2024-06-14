#include "../vendor/mpc.h"

enum LVAL_TYPE { 
    LVAL_INT, 
    LVAL_DOUBLE, 
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_ERR,
};

typedef struct {
    int count; 
    struct lval** cell;
} lval_sexpr_t;

typedef struct {
    int count; 
    struct lval** cell;
} lval_qexpr_t;

typedef union {
    long _int; 
    double _double;
    char *err;
    char *symbol;
    lval_sexpr_t sexpr;
    lval_qexpr_t qexpr;
} lval_data;

struct lval {
    enum LVAL_TYPE type;
    lval_data data;
};

typedef struct lval lval;

lval *lval_int(long x);
lval *lval_double(double x);
lval *lval_err(char *m);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
void lval_del(lval* v);

lval *lval_read(mpc_ast_t *t);

void lval_print(lval *v);
void lval_println(lval *v);

lval *lval_eval(lval *v);
