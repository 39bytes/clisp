#define UNUSED __attribute__((unused))

typedef struct lenv lenv;

void load_file(lenv *e, char *filename);
