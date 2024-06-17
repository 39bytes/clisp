#include <stdbool.h>
#include "../vendor/mpc.h"

enum LVAL_TYPE { 
    LVAL_INT, 
    LVAL_DOUBLE, 
    LVAL_BOOL,
    LVAL_STRING,
    LVAL_SYMBOL,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_BUILTIN_FUNC,
    LVAL_LAMBDA,
    LVAL_ERROR,
};

char *lval_type_name(enum LVAL_TYPE type);

typedef struct {
    int count; 
    struct lval** cell;
} lval_expr;

struct lval;
typedef struct lval lval;

typedef struct {
    char *symbol;
    lval *val;
    bool builtin;
} lenv_entry;

typedef struct lenv lenv;
struct lenv {
    lenv *parent;
    int count;
    lenv_entry **entries;
};

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct {
    lenv *env;
    lval *formals;
    lval *body;
} lval_lambda;

struct lval {
    enum LVAL_TYPE type;
    union {
        long _int; 
        double _double;
        bool _bool;
        char *string;
        char *error;
        char *symbol;
        lval_expr sexpr;
        lval_expr qexpr;
        lbuiltin builtin_func;
        lval_lambda lambda;
    };
};

lenv *lenv_new(void);
void lenv_del(lenv *e);
lenv_entry *lenv_lookup(lenv *e, char *k);
lval *lenv_get(lenv *e, char *k);
void lenv_put(lenv *e, char *k, lval *v, bool builtin);
void lenv_def(lenv *e, char *k, lval *v);
lenv *lenv_copy(lenv *e);

lenv *lenv_base(void);

lval *lval_int(long x);
lval *lval_double(double x);
lval *lval_bool(bool x);
lval *lval_string(char *s);
lval *lval_error(char *fmt, ...);
lval *lval_symbol(char *s);
lval *lval_sexpr(void);
lval *lval_qexpr(void);
lval *lval_builtin_func(lbuiltin func);
lval *lval_func(lval *formals, lval *body);
bool lval_eq(lval* a, lval *b);
void lval_del(lval* v);

lval *lval_read(mpc_ast_t *t);

void lval_print(lval *v);
void lval_println(lval *v);

lval *lval_eval(lenv *e, lval *v);

lval *lval_expr_pop(lval_expr* e, int i);
void lval_expr_push_back(lval_expr* e, lval* x);
void lval_expr_push_front(lval_expr* e, lval* x);
lval *lval_take(lval* v, int i);
