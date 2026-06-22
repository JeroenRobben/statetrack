#include "statetrack.h"

#include <execinfo.h>
#include <stdio.h>
#include <string.h>

static StateVar      registry[STATETRACK_MAX_VARS];
static int           n_vars = 0;

/* ---- observers --------------------------------------------------------- */

static struct {
    statetrack_observer on_register;
    statetrack_observer on_unregister;
    void               *user;
} observers[STATETRACK_MAX_OBSERVERS];
static int n_observers = 0;

int statetrack_add_observer(statetrack_observer on_register,
                            statetrack_observer on_unregister, void *user)
{
    if (n_observers >= STATETRACK_MAX_OBSERVERS)
        return -1;
    observers[n_observers].on_register = on_register;
    observers[n_observers].on_unregister = on_unregister;
    observers[n_observers].user = user;
    ++n_observers;
    return 0;
}

static void notify_register(const StateVar *sv)
{
    for (int i = 0; i < n_observers; ++i)
        if (observers[i].on_register)
            observers[i].on_register(sv, observers[i].user);
}

static void notify_unregister(const StateVar *sv)
{
    for (int i = 0; i < n_observers; ++i)
        if (observers[i].on_unregister)
            observers[i].on_unregister(sv, observers[i].user);
}

/* ---- registration ------------------------------------------------------ */

void statetrack_register(void *ptr, const size_t size, const char *name)
{
    if (!ptr)
        return;
    for (int i = 0; i < n_vars; ++i) {                 /* update existing entry */
        if (registry[i].ptr == ptr) {
            registry[i].size = size;
            snprintf(registry[i].name, STATETRACK_NAME_LEN, "%s", name ? name : "?");
            notify_register(&registry[i]);             /* re-mark (name/size may have changed) */
            return;
        }
    }
    if (n_vars >= STATETRACK_MAX_VARS)
        return;
    registry[n_vars].ptr = ptr;
    registry[n_vars].size = size;
    snprintf(registry[n_vars].name, STATETRACK_NAME_LEN, "%s", name ? name : "?");
    ++n_vars;
    notify_register(&registry[n_vars - 1]);
}

void statetrack_unregister(void *ptr)
{
    if (!ptr)
        return;
    for (int i = 0; i < n_vars; ++i) {
        if (registry[i].ptr == ptr) {
            notify_unregister(&registry[i]);           /* before removal, while still valid */
            registry[i] = registry[n_vars - 1];    /* swap-with-last removal */
            --n_vars;
            return;
        }
    }
}

void statetrack_clear(void)
{
    n_vars = 0;
}

/* ---- enumeration / lookup ---------------------------------------------- */

int statetrack_count(void)
{
    return n_vars;
}

const StateVar *statetrack_at(int index)
{
    if (index < 0 || index >= n_vars)
        return NULL;
    return &registry[index];
}

const StateVar *statetrack_find(const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < n_vars; ++i)
        if (strcmp(registry[i].name, name) == 0)
            return &registry[i];
    return NULL;
}

int statetrack_find_next(const char *name, int from)
{
    if (!name)
        return -1;
    if (from < 0)
        from = 0;
    for (int i = from; i < n_vars; ++i)
        if (strcmp(registry[i].name, name) == 0)
            return i;
    return -1;
}

/* ---- value helpers ----------------------------------------------------- */

uint64_t statetrack_fingerprint(const StateVar *v)
{
    uint64_t fp = 1469598103934665603ULL;          /* FNV-1a 64-bit offset basis */
    if (!v || !v->ptr)
        return 0;
    const unsigned char *p = v->ptr;
    for (size_t i = 0; i < v->size; ++i) {
        fp ^= p[i];
        fp *= 1099511628211ULL;                    /* FNV prime */
    }
    return fp;
}

uint64_t statetrack_stacktrace_xor(void)
{
    void    *frames[32];
    uint64_t x = 0;
    const int n = backtrace(frames, sizeof frames / sizeof frames[0]);
    for (int i = 0; i < n; ++i)
        x = (x << 7 | x >> 57) ^ (uintptr_t)frames[i];
    return x;
}
