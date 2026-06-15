/* Unit test for the heap interposer (host gcc/clang; `make check`).
   Built with -DSTATETRACK_HEAP_WRAP and linked with
   -Wl,--wrap=free -Wl,--wrap=realloc so free/realloc are deterministically
   routed through statetrack_heap.c.  Verifies that freeing a struct base
   unregisters registered fields inside it (range match), and that realloc
   re-points (on move) / drops (on shrink) registered fields. */
#include "../statetrack_annotate.h" /* registration macros */
#include "../statetrack.h"          /* statetrack_count/find for assertions */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct session {
    uint64_t nonce;
    uint8_t  ck[32];
    int      pad;
};

int main(void) {
    assert(statetrack_count() == 0);

    /* free(base) drops every registered field inside the freed block */
    {
        struct session *s = malloc(sizeof *s);
        assert(s);
        s->nonce = 1;
        memset(s->ck, 0x11, sizeof s->ck);
        STATETRACK_REGISTER(s->nonce);
        STATETRACK_REGISTER(s->ck);
        assert(statetrack_count() == 2);

        free(s); /* wrapped: range-unregisters &s->nonce and &s->ck */
        assert(statetrack_count() == 0);
    }

    /* realloc that grows (and likely moves): the field is re-pointed */
    {
        uint8_t *p = malloc(64);
        uint8_t *q;
        const StateVar *v;
        assert(p);
        p[0] = 0xEE;
        STATETRACK_REGISTER(p[0]); /* registers ptr == p, size 1 */
        v = statetrack_find("p[0]");
        assert(v && v->ptr == p);

        q = realloc(p, 1 << 20);
        assert(q);
        v = statetrack_find("p[0]");
        assert(v != NULL);     /* survived realloc */
        assert(v->ptr == q);   /* re-pointed to the new base (== p if not moved) */

        free(q);
        assert(statetrack_count() == 0);
    }

    /* realloc that shrinks: a field past the new end is dropped */
    {
        uint8_t *p = malloc(1 << 20);
        uint8_t *q;
        assert(p);
        STATETRACK_REGISTER_AS(p[1000], "tail"); /* registers ptr == p+1000 */
        assert(statetrack_find("tail") != NULL);

        q = realloc(p, 8); /* offset 1000 no longer fits -> dropped */
        assert(q);
        assert(statetrack_find("tail") == NULL);

        free(q);
        assert(statetrack_count() == 0);
    }

    printf("test_statetrack_heap: all assertions passed\n");
    return 0;
}
