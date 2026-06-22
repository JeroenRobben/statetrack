/* 004-if-and-assign: no switch. Constants come from == comparisons and
 * assignments to the registered field. `x == 5` (x not tracked, bare literal)
 * must contribute nothing. */
#include "statetrack_annotate.h"

#define MODE_OFF 0
#define MODE_ON  1
#define MODE_ERR 2

struct S { int mode; };

void set(struct S *s, int x)
{
    STATETRACK_REGISTER(s->mode);
    if (x == 5)
        s->mode = MODE_ON;
    else
        s->mode = MODE_OFF;
    if (s->mode == MODE_ERR)
        s->mode = MODE_OFF;
}
