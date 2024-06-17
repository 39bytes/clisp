#include "../vendor/mpc.h"
#include "lval.h"

extern mpc_parser_t *Lispy;


void init_parser(void);
void cleanup_parser(void);
lval *parse_expr(const char *filename, const char *string);

