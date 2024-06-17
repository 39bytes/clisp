#include "../vendor/mpc.h"
#include "lval.h"
#include "assert.h"

mpc_parser_t* Int;
mpc_parser_t* Double;
mpc_parser_t* Number;
mpc_parser_t* Bool;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Symbol;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;

void init_parser(void) {
    Int = mpc_new("int");
    Double = mpc_new("double");
    Number = mpc_new("number");
    Bool = mpc_new("bool");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Symbol = mpc_new("symbol");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                   \
            int      : /-?[0-9]+/ ;                                         \
            double   : /-?[0-9]+/ '.' /[0-9]+/ ;                            \
            number   : <double> | <int> ;                                   \
            bool     : \"true\" | \"false\" ;                               \
            string   : /\"(\\\\.|[^\"])*\"/ ;                               \
            comment  : /;[^\\r\\n]*/ ;                                      \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&\\|]+/ ;                \
            sexpr    : '(' <expr>* ')' ;                                    \
            qexpr    : '{' <expr>* '}' ;                                    \
            expr     : <number> | <bool> | <symbol> | <string> | <comment> | <sexpr> | <qexpr> ; \
            lispy    : /^/ <expr>* /$/ ;                                    \
        ",
        Int, Double, Number, Bool, String, Comment, Symbol, Sexpr, Qexpr, Expr, Lispy);
}

void cleanup_parser(void) {
    mpc_cleanup(11, Int, Double, Number, Bool, String, Comment, Symbol, Sexpr, Qexpr, Expr, Lispy);
}


lval *parse_expr(const char *string) {
    mpc_result_t r;

    if (!mpc_parse("<stdin>", string, Lispy, &r)) {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
        return NULL;
    }
    return lval_read(r.output);
}

lval *parse_file(const char *filename) {
    mpc_result_t r;
    
    if (!mpc_parse_contents(filename, Lispy, &r)) {
        char *err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        
        lval *err = lval_error("Could not load file %s", err_msg);
        free(err_msg);
        return err;
    } 

    lval *expr = lval_read(r.output);
    assert(expr->type == LVAL_SEXPR);
    mpc_ast_delete(r.output);

    return expr;
}
