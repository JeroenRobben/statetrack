/* Unit test for the ergonomic macros (host gcc/clang; `make check`).
   Verifies STATETRACK_REGISTER_SCOPED auto-unregisters on every scope exit
   (normal fallthrough, early return, goto-out) and that name/size are derived
   correctly. */
#include "../statetrack_annotate.h" /* registration macros */
#include "../statetrack.h"          /* statetrack_count/find for assertions */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Registers a scoped var, then returns -- possibly early.  Either way the
   cleanup must fire, leaving the registry at baseline. */
static void early_return(int take_early_exit) {
    int32_t x = 0xABCD;
    STATETRACK_REGISTER_SCOPED(x);
    assert(statetrack_find("x") != NULL);
    if (take_early_exit)
        return;
    (void)x;
}

int main(void) {
    assert(statetrack_count() == 0);

    /* scope-bound auto register/unregister; name + size derived from the token */
    {
        int32_t action = 0x4102;
        uint8_t ck[32];
        const StateVar *v;

        memset(ck, 0xA5, sizeof ck);
        STATETRACK_REGISTER_SCOPED(action);
        STATETRACK_REGISTER_SCOPED(ck);
        assert(statetrack_count() == 2);

        v = statetrack_find("action");
        assert(v && v->size == sizeof action && v->ptr == &action);
        v = statetrack_find("ck");
        assert(v && v->size == sizeof ck && v->ptr == ck);
    }
    assert(statetrack_count() == 0); /* both unregistered at block exit */

    /* early-return path */
    early_return(1);
    assert(statetrack_count() == 0);
    early_return(0);
    assert(statetrack_count() == 0);

    /* goto-out-of-scope path: cleanup fires when leaving the inner block */
    {
        int32_t g = 7;
        {
            STATETRACK_REGISTER_SCOPED(g);
            assert(statetrack_find("g") != NULL);
            goto done;
        }
    done:
        assert(statetrack_count() == 0);
        (void)g;
    }

    /* explicit register/unregister with an explicit label */
    {
        int32_t r = 1;
        STATETRACK_REGISTER_AS(r, "renamed");
        assert(statetrack_find("renamed") != NULL && statetrack_find("r") == NULL);
        STATETRACK_UNREGISTER(r);
        assert(statetrack_count() == 0);
    }

    /* STATETRACK_REGISTER_SCOPED_AS uses an explicit label and still auto-unregisters */
    {
        int32_t s = 1;
        STATETRACK_REGISTER_SCOPED_AS(s, "scoped_label");
        assert(statetrack_find("scoped_label") != NULL);
    }
    assert(statetrack_count() == 0);

    printf("test_statetrack_macros: all assertions passed\n");
    return 0;
}
