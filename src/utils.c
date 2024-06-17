#include "lval.h"
#include "builtins.h"

void load_file(lenv *e, char *filename) {
    lval *args = lval_sexpr();
    lval_expr_push_back(&args->sexpr, lval_string(filename));

    lval *x = builtin_load(e, args);

    if (x->type == LVAL_ERROR) {
        lval_println(x);
    }
    lval_del(x);
}
