typedef struct lval lval;

int parse_expr(lval *v, const char *s, int i, char end);
lval *parse_file(const char *filename);

