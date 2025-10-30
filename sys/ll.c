#include <ll.h>

void ll_add(ll_t **head, void *elem) {
    if (*head) {
        ll_t *last = NULL;

        ll_iterate(*head, ll_t *, curr, {
            if (!curr->next) {
                last = curr;
            }
        });

        if (!last) {
            last = *head;
        }

        last->next = elem;
    } else {
        *head = elem;
    }
}

void ll_destroy(ll_t **head, ll_destroy_cb_t cb) {
    void *tmp = NULL;
    ll_iterate(*head, ll_t *, curr, {
        if (cb) {
            cb(curr);
        }

        tmp = curr->next;

        free(curr);

        curr = (ll_t *)&tmp;
    });

    *head = NULL;
}
