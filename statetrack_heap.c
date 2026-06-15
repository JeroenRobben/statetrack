/*
 * statetrack_heap - Optional, link this file in only if you want heap lifecycle tracking.
 *
 * This file hooks free() and realloc() to automatically unregister tracked state variables when they are freed
 *
 *     free(p)        drop every StateVar pointing inside the freed block
 *     realloc(p, n)  moved  -> re-point entries to the new address
 *                    shrank -> drop entries past the new end
 *                    n == 0 -> same as free
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <malloc.h>

#include "statetrack.h"

/* True iff ptr lies within the block [base, base + n). */
static int statetrack_heap_in_block(const void *ptr, const void *base, size_t n) {
    const char *p = (const char *)ptr;
    const char *lo = (const char *)base;
    return p >= lo && p < lo + n;
}

/* Unregister every entry inside [base, base + n). */
static void statetrack_heap_unregister_range(void *base, size_t n) {
    int i;
    for (i = statetrack_count() - 1; i >= 0; i--) {
        const StateVar *v = statetrack_at(i);
        if (v && statetrack_heap_in_block(v->ptr, base, n))
            statetrack_unregister(v->ptr);
    }
}

/* ---- the real allocator (named the same in both modes) -------------------- *
 * --wrap mode:     __real_free / __real_realloc are functions supplied by the
 *                  linker (-Wl,--wrap=free -Wl,--wrap=realloc).
 * LD_PRELOAD mode: __real_free / __real_realloc are function pointers resolved 
 *                  lazily using dlsym from the object in the chain. */

#ifdef STATETRACK_HEAP_WRAP
extern void  __real_free(void *p);
extern void *__real_realloc(void *p, size_t n);
#else
#include <dlfcn.h>
static void  (*__real_free)(void *);
static void *(*__real_realloc)(void *, size_t);
#endif

/* ---- shared interposer bodies --------------------------------------------- */

static void statetrack_heap_on_free(void *p) {
    if (p)
        statetrack_heap_unregister_range(p, malloc_usable_size(p));
    __real_free(p);
}

static void *statetrack_heap_on_realloc(void *old, size_t n) {
    size_t oldn = old ? malloc_usable_size(old) : 0;
    void *nw = __real_realloc(old, n);
    size_t newn;
    int i;

    if (!nw) {                /* realloc(p, 0) frees p and returns NULL; a genuine
                                 failure leaves the old block valid, so reconcile
                                 the free only when n == 0. */
        if (old && n == 0)
            statetrack_heap_unregister_range(old, oldn);
        return nw;
    }
    if (!old)
        return nw; /* realloc(NULL, n) is a fresh malloc: nothing registered yet */

    newn = malloc_usable_size(nw);

    if (nw == old) {
        /* Stayed put: existing addresses are still valid; only a shrink can
           orphan a field that now sits past the end of the block. */
        if (newn < oldn)
            statetrack_heap_unregister_range((char *)old + newn, oldn - newn);
        return nw;
    }

    /* Moved: relocate each entry inside the old block to the same offset in the
       new block, dropping any that no longer fits. */
    for (i = statetrack_count() - 1; i >= 0; i--) {
        const StateVar *cur = statetrack_at(i);
        StateVar v;
        if (!cur || !statetrack_heap_in_block(cur->ptr, old, oldn))
            continue;
        v = *cur; /* copy out: (un)register shuffles the registry underneath us */
        const size_t offset = (size_t) ((char *) v.ptr - (char *) old);
        statetrack_unregister(v.ptr);
        if (offset + v.size <= newn)
            statetrack_register((char *)nw + offset, v.size, v.name);
    }
    return nw;
}

#ifdef STATETRACK_HEAP_WRAP

void __wrap_free(void *p) {
    statetrack_heap_on_free(p);
}

void *__wrap_realloc(void *old, size_t n) {
    return statetrack_heap_on_realloc(old, n);
}

#else

void free(void *p) {
    if (!__real_free)
        __real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    statetrack_heap_on_free(p);
}

void *realloc(void *old, size_t n) {
    if (!__real_realloc)
        __real_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    return statetrack_heap_on_realloc(old, n);
}

#endif /* STATETRACK_HEAP_WRAP */
