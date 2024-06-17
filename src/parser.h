#include "../vendor/mpc.h"

extern mpc_parser_t *Lispy;
typedef struct lval lval;


void init_parser(void);
void cleanup_parser(void);
lval *parse_expr(const char *string);
lval *parse_file(const char *filename);

