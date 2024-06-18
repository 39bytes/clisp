#include <stdlib.h>

typedef struct {
    size_t len;
    size_t capacity;
    char *buf;
} dyn_string;

dyn_string *dyn_string_new(void);
void dyn_string_del(dyn_string *s);
void dyn_string_push(dyn_string *s, char c);
