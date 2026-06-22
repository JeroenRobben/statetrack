/* 001-enum-typed: enum-typed fields -> family from the type alone.
 * phase (typedef enum) and role (named enum) decode; counter (no enum,
 * no use-sites) degrades to numeric. */
#include "statetrack_annotate.h"

typedef enum { ST_INIT = 0, ST_RUN = 1, ST_DONE = 2 } phase_t;
enum role { ROLE_CLIENT = 10, ROLE_SERVER = 11 };

struct S {
    phase_t       phase;
    enum role     role;
    unsigned long counter;
};

void init(struct S *s)
{
    STATETRACK_REGISTER(s->phase);
    STATETRACK_REGISTER(s->role);
    STATETRACK_REGISTER(s->counter);
}
