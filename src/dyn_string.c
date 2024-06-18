#include <stdlib.h>
#include "dyn_string.h"

dyn_string *dyn_string_new(void) {
    dyn_string *s = malloc(sizeof(dyn_string));
    s->len = 0;
    s->capacity = 4;
    s->buf = calloc(s->capacity + 1, sizeof(char));
    s->buf[0] = '\0';
    return s;
}

void dyn_string_del(dyn_string *s) {
    free(s->buf);
    free(s);
}

void dyn_string_push(dyn_string *s, char c) {
    if (s->len >= s->capacity) {
        s->capacity *= 2;
        s->buf = realloc(s->buf, s->capacity + 1);
    }
    s->buf[s->len] = c;
    s->buf[s->len + 1] = '\0';
    s->len++;
}
