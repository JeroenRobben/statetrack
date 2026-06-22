/* 022-indirection: the named constants reach the field only through a helper's
 * parameter (set_st(s, ST_A)), so they never appear at a syntactic use-site of
 * the field. Static analysis cannot see them here -> numeric. (Runtime would
 * later flag the observed values as unnamed.) */
#include "statetrack_annotate.h"

#define ST_A 1
#define ST_B 2

struct S { int st; };

static void set_st(struct S *s, int v) { s->st = v; }   /* RHS is a param, not a const */

void run(struct S *s)
{
    STATETRACK_REGISTER(s->st);
    set_st(s, ST_A);
    set_st(s, ST_B);
}
