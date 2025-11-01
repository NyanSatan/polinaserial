/* stupid code for (single) linked lists */

#ifndef POLINA_LL_H
#define POLINA_LL_H

#include <stdlib.h>

struct ll {
    struct ll *next;
};

typedef struct ll ll_t;

typedef void (*ll_destroy_cb_t)(void *);

void ll_add(ll_t **head, void *elem);
void ll_destroy(ll_t **head, ll_destroy_cb_t cb);

#define ll_iterate(_head, _type, _name, _code) \
    do { \
        for (_type _name = _head; _name != NULL; _name = (_type)((ll_t *)_name)->next) \
            _code \
    } while (0);

#endif
