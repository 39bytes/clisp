#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "lval.h"
#include "dyn_string.h"

static inline bool valid_symbol_char(char c) {
    return strchr("abcdefghijklmnopqrstuvwxyz"
                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "0123456789_+-*\\/=<>!&",
                  c) != NULL;
}

static inline bool is_numeric(char c) {
    return strchr("-0123456789.", c) != NULL;
}

static lval *parse_int(char *s) {
    errno = 0;
    long x = strtol(s, NULL, 10);
    return errno != ERANGE ? lval_int(x) : lval_error("invalid number");
}

static lval *parse_double(char *s) {
    errno = 0;
    double x = strtod(s, NULL);
    free(s);

    return errno != ERANGE ? lval_double(x) : lval_error("invalid floating point number");
}

static int parse_symbol(lval *v, const char *s, int i) {
    dyn_string *str = dyn_string_new();

    while (valid_symbol_char(s[i]) && s[i] != '\0') {
        dyn_string_push(str, s[i]);
        i++;
    }

    bool numeric = true;
    bool is_double = false;
    for (size_t i = 0; i < str->len; i++) {
        char c = str->buf[i];
        if (!is_numeric(c)) {
            numeric = false;
            break;
        }
        if (c == '.') {
            is_double = true;
        }
    }

    if (numeric && strcmp(str->buf, "-") != 0) {
        if (is_double) {
            lval_expr_push_back(&v->sexpr, parse_double(str->buf));
        } else {
            lval_expr_push_back(&v->sexpr, parse_int(str->buf));
        }
    } else {
        // Parse bool
        if (strcmp(str->buf, "true") == 0) {
            lval_expr_push_back(&v->sexpr, lval_bool(true));
        } else if (strcmp(str->buf, "false") == 0) {
            lval_expr_push_back(&v->sexpr, lval_bool(false));
        } else {
            lval_expr_push_back(&v->sexpr, lval_symbol(str->buf));
        }
    }

    dyn_string_del(str);
    
    return i;
}

static int parse_string(lval *v, const char *s, int i) {
    dyn_string *str = dyn_string_new();

    while (s[i] != '"') {
        char c = s[i];
        if (c == '\0') {
            lval_expr_push_back(&v->sexpr, lval_error("Unexpected end of input while parsing string literal"));
            dyn_string_del(str);
            return i + 1;
        }

        if (c == '\\') {
            i++;

            if (strchr(lval_str_unescapable, s[i])) {
                c = lval_str_unescape(s[i]);
            } else {
                lval_expr_push_back(&v->sexpr, lval_error("Invalid escape character %c", c));
                dyn_string_del(str);
                return i + 1;
            }
        }

        dyn_string_push(str, c);
        i++;
    }

    lval_expr_push_back(&v->sexpr, lval_string(str->buf));
    dyn_string_del(str);

    return i+1;
}

int parse_expr(lval *v, const char *s, int i, char end) {
    while (s[i] != end) {
        if (s[i] == '\0') {
            lval_expr_push_back(&v->sexpr, lval_error("Missing %c at end of input", end));
            return strlen(s) + 1;
        }

        // Skip whitespace
        if (strchr(" \t\v\r\n", s[i])) {
            i++;
            continue;
        }

        // Read comment
        if (s[i] == ';') {
            while (s[i] != '\n' && s[i] != '\0') {
                i++;
            }
            i++;
            continue;
        }

        // Read S-expression
        if (s[i] == '(') {
            lval *x = lval_sexpr();
            lval_expr_push_back(&v->sexpr, x);
            i = parse_expr(x, s, i+1, ')');
            continue;
        }

        // Read Q-expression
        if (s[i] == '{') {
            lval *x = lval_qexpr();
            lval_expr_push_back(&v->sexpr, x);
            i = parse_expr(x, s, i+1, '}');
            continue;
        }

        // Read symbol
        if (valid_symbol_char(s[i])) {
            i = parse_symbol(v, s, i);
            continue;
        }

        // Read string
        if (s[i] == '"') {
            i = parse_string(v, s, i);
            continue;
        }

        lval_expr_push_back(&v->sexpr, lval_error("Unknown character %c", s[i]));
        return strlen(s) + 1;
    }

    return i+1;
}

lval *parse_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        lval *err = lval_error("Could not load file %s", filename);
        return err;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);

    fseek(f, 0, SEEK_SET);
    char *input = calloc(length+1, sizeof(char));
    fread(input, 1, length, f);

    fclose(f);

    lval *expr = lval_sexpr();
    parse_expr(expr, input, 0, '\0');
    free(input);

    return expr;
}
