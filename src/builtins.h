typedef struct lenv lenv;
typedef struct lval lval;

lenv *lenv_base(void);
lval *builtin_load(lenv *e, lval *v);
