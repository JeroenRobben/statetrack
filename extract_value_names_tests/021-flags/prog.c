/* 021-flags: a bitmask field. `|=` (compound) and `&` (bitwise test) must NOT
 * be read as a 1:1 value space -> the field stays numeric. Only `=` and `==`
 * harvest, and here the only `=` RHS is a bare literal. */
#include "statetrack_annotate.h"

#define F_A 0x1
#define F_B 0x2
#define F_C 0x4

struct S { int flags; };

void upd(struct S *s)
{
    STATETRACK_REGISTER(s->flags);
    s->flags |= F_A;
    s->flags |= F_B;
    if (s->flags & F_C)
        s->flags = 0;
}
