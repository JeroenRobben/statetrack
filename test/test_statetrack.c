/* Unit test for the core statetrack registry (host gcc; `make check`). */
#include "../statetrack.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int32_t a = 0x11223344, b = 0x55667788, c = 99;
    int32_t dup1 = 1, dup2 = 2;
    unsigned char key[40];
    unsigned char zero[40] = {0};
    size_t i;
    const StateVar *v;

    for (i = 0; i < sizeof key; ++i)
        key[i] = (unsigned char)(i * 7 + 1);

    /* register: a, b, key, and two DISTINCT pointers sharing the name "dup" */
    statetrack_register(&a, sizeof a, "a");
    statetrack_register(&b, sizeof b, "b");
    statetrack_register(key, sizeof key, "key");
    statetrack_register(&dup1, sizeof dup1, "dup");
    statetrack_register(&dup2, sizeof dup2, "dup");
    assert(statetrack_count() == 5);

    /* re-registering the same ptr updates, does not add */
    statetrack_register(&a, sizeof a, "a");
    assert(statetrack_count() == 5);

    /* enumeration by index */
    v = statetrack_at(0); assert(v && v->ptr == &a && strcmp(v->name, "a") == 0);
    assert(statetrack_at(-1) == NULL);
    assert(statetrack_at(5) == NULL);

    /* find by name (first match) */
    v = statetrack_find("key");
    assert(v && v->ptr == key && v->size == sizeof key);
    assert(statetrack_find("nope") == NULL);

    /* duplicate names: find returns first, find_next walks the rest */
    {
        int i0 = statetrack_find_next("dup", 0);
        int i1 = statetrack_find_next("dup", i0 + 1);
        int i2 = statetrack_find_next("dup", i1 + 1);
        assert(i0 >= 0 && i1 > i0 && i2 == -1);
        assert(statetrack_at(i0)->ptr == &dup1);
        assert(statetrack_at(i1)->ptr == &dup2);
    }

    /* value helpers */
    /* fingerprint: nonzero for real data, deterministic, 0 for all-zero region */
    {
        StateVar fake = { zero, sizeof zero, "z" };
        uint64_t f1 = statetrack_fingerprint(statetrack_find("key"));
        uint64_t f2 = statetrack_fingerprint(statetrack_find("key"));
        assert(f1 != 0 && f1 == f2);
        /* all-zero region: FNV of zeros is a fixed nonzero value; just ensure
           it differs from the key's fingerprint */
        assert(statetrack_fingerprint(&fake) != f1);
    }

    /* stacktrace xor: returns a nonzero call-stack fingerprint */
    assert(statetrack_stacktrace_xor() != 0);

    /* register one more variable (count -> 6) */
    statetrack_register(&c, sizeof c, "c");
    assert(statetrack_count() == 6);
    assert(statetrack_find("c") != NULL);

    /* unregister by pointer + clear all */
    statetrack_unregister(&a);
    assert(statetrack_find("a") == NULL && statetrack_count() == 5);
    statetrack_clear();
    assert(statetrack_count() == 0);

    printf("test_statetrack: all assertions passed\n");
    return 0;
}
