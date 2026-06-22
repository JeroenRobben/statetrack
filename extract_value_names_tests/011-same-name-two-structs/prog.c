/* 011-same-name-two-structs: two different structs each have a field named
 * `state`. Keying by canonical USR (not field name) must keep their value
 * spaces separate -- no cross-contamination. */
#include "statetrack_annotate.h"

#define A_X 1
#define A_Y 2
#define B_P 10
#define B_Q 20

struct alpha { int state; };
struct beta  { int state; };

void alpha_init(struct alpha *a)
{
    STATETRACK_REGISTER(a->state);
    switch (a->state) { case A_X: ; case A_Y: ; }
}

void beta_init(struct beta *b)
{
    STATETRACK_REGISTER(b->state);
    switch (b->state) { case B_P: ; case B_Q: ; }
}
