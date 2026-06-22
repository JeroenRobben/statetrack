#ifndef STATETRACK_H
#define STATETRACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STATETRACK_MAX_VARS
#define STATETRACK_MAX_VARS 256
#endif
#ifndef STATETRACK_NAME_LEN
#define STATETRACK_NAME_LEN 64
#endif
#ifndef STATETRACK_MAX_OBSERVERS
#define STATETRACK_MAX_OBSERVERS 8
#endif

typedef struct {
  void *ptr;
  size_t size;
  char name[STATETRACK_NAME_LEN];
} StateVar;

void statetrack_register(void *ptr, size_t size, const char *name);
void statetrack_unregister(void *ptr);
void statetrack_clear(void);

/* ---- observers --------------------------------------------------------- *
 * Notified when a state var is registered (on_register, after it enters the
 * registry) or unregistered (on_unregister, before it leaves). Either may be
 * NULL. */
typedef void (*statetrack_observer)(const StateVar *sv, void *user);
int statetrack_add_observer(statetrack_observer on_register,
                            statetrack_observer on_unregister, void *user);

/* ---- enumeration / lookup ---------------------------------------------- */

int statetrack_count(void);
const StateVar *statetrack_at(int index);
const StateVar *statetrack_find(const char *name);
int statetrack_find_next(const char *name, int from);

/* ---- value helpers ----------------------------------------------------- */

uint64_t statetrack_fingerprint(const StateVar *v);
uint64_t statetrack_stacktrace_xor(void);

#ifdef __cplusplus
}
#endif

#endif /* STATETRACK_H */
